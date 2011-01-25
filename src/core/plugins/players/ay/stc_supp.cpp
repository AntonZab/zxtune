/*
Abstract:
  STC modules playback support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "ay_base.h"
#include "ay_conversion.h"
#include <core/plugins/detect_helper.h>
#include <core/plugins/utils.h>
#include <core/plugins/players/module_properties.h>
//common includes
#include <byteorder.h>
#include <error_tools.h>
#include <logging.h>
#include <messages_collector.h>
#include <range_checker.h>
#include <tools.h>
//library includes
#include <core/convert_parameters.h>
#include <core/core_parameters.h>
#include <core/error_codes.h>
#include <core/module_attrs.h>
#include <core/plugin_attrs.h>
#include <io/container.h>
//boost includes
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/make_shared.hpp>
//text includes
#include <core/text/core.h>
#include <core/text/plugins.h>
#include <core/text/warnings.h>

#define FILE_TAG D9281D1D

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  //plugin attributes
  const Char STC_PLUGIN_ID[] = {'S', 'T', 'C', 0};
  const String STC_PLUGIN_VERSION(FromStdString("$Rev$"));

  //hints
  const std::size_t MAX_STC_MODULE_SIZE = 16384;
  const uint_t MAX_SAMPLES_COUNT = 16;
  const uint_t MAX_ORNAMENTS_COUNT = 16;
  const uint_t MAX_PATTERN_SIZE = 64;
  const uint_t MAX_PATTERN_COUNT = 32;
  const uint_t SAMPLE_ORNAMENT_SIZE = 32;

  //detectors
  static const DataPrefix PLAYERS[] =
  {
    {
      "21??"    // ld hl,xxxx
      "c3??"    // jp xxxx
      "c3??"    // jp xxxx
      "f3"      // di
      "7e"      // ld a,(hl)
      "32??"    // ld (xxxx),a
      "22??"    // ld (xxxx),hl
      "23"      // inc hl
      "cd??"    // call xxxx
      "1a"      // ld a,(de)
      "13"      // inc de
      "3c"      // inc a
      "32??"    // ld (xxxx),a
      "ed53??"  // ld (xxxx),de
      "cd??"    // call xxxx
      "ed53??"  // ld (xxxx),de
      "d5"      // push de
      "cd??"    // call xxxx
      "ed53??"  // ld (xxxx),de
      "21??"    // ld hl,xxxx
      "cd??"    // call xxxx
      "eb"      // ex de,hl
      "22??"    // ld (xxxx),hl
      "21??"    // ld hl,xxxx
      "22??"    // ld(xxxx),hl
      "21??"    // ld hl,xxxx
      "11??"    // ld de,xxxx
      "01??"    // ld bc,xxxx
      "70"      // ld (hl),b
      "edb0"    // ldir
      "e1"      // pop hl
      "01??"    // ld bc,xxxx
      "af"      // xor a
      "cd??"    // call xxxx
      "3d"      // dec a
      "32??"    // ld (xxxx),a
      "32??"    // ld (xxxx),a
      "32??"    // ld (xxxx),a
      "3e?"     // ld a,x
      "32??"    // ld (xxxx),a
      "23"      // inc hl
      "22??"    // ld (xxxx),hl
      "22??"    // ld (xxxx),hl
      "22??"    // ld (xxxx),hl
      "cd??"    // call xxxx
      "fb"      // ei
      "c9"      // ret
      ,
      0x43c
    }
  };


//////////////////////////////////////////////////////////////////////////
#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
  PACK_PRE struct STCHeader
  {
    uint8_t Tempo;
    uint16_t PositionsOffset;
    uint16_t OrnamentsOffset;
    uint16_t PatternsOffset;
    char Identifier[18];
    uint16_t Size;
  } PACK_POST;

  PACK_PRE struct STCPositions
  {
    uint8_t Lenght;
    PACK_PRE struct STCPosEntry
    {
      uint8_t PatternNum;
      int8_t PatternHeight;
    } PACK_POST;
    STCPosEntry Data[1];
  } PACK_POST;

  PACK_PRE struct STCPattern
  {
    uint8_t Number;
    boost::array<uint16_t, AYM::CHANNELS> Offsets;

    operator bool () const
    {
      return 0xff != Number;
    }
  } PACK_POST;

  PACK_PRE struct STCOrnament
  {
    uint8_t Number;
    int8_t Data[SAMPLE_ORNAMENT_SIZE];
  } PACK_POST;

  PACK_PRE struct STCSample
  {
    uint8_t Number;
    // EEEEaaaa
    // NESnnnnn
    // eeeeeeee
    // Ee - effect
    // a - level
    // N - noise mask, n- noise value
    // E - envelope mask
    // S - effect sign
    PACK_PRE struct Line
    {
      uint8_t EffHiAndLevel;
      uint8_t NoiseAndMasks;
      uint8_t EffLo;

      uint_t GetLevel() const
      {
        return EffHiAndLevel & 15;
      }

      int_t GetEffect() const
      {
        const int_t val = int_t(EffHiAndLevel & 240) * 16 + EffLo;
        return (NoiseAndMasks & 32) ? val : -val;
      }

      uint_t GetNoise() const
      {
        return NoiseAndMasks & 31;
      }

      bool GetNoiseMask() const
      {
        return 0 != (NoiseAndMasks & 128);
      }

      bool GetEnvelopeMask() const
      {
        return 0 != (NoiseAndMasks & 64);
      }
    } PACK_POST;
    Line Data[SAMPLE_ORNAMENT_SIZE];
    uint8_t Loop;
    uint8_t LoopSize;
  } PACK_POST;

#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  BOOST_STATIC_ASSERT(sizeof(STCHeader) == 27);
  BOOST_STATIC_ASSERT(sizeof(STCPositions) == 3);
  BOOST_STATIC_ASSERT(sizeof(STCPattern) == 7);
  BOOST_STATIC_ASSERT(sizeof(STCOrnament) == 33);
  BOOST_STATIC_ASSERT(sizeof(STCSample) == 99);

  // currently used sample
  struct Sample
  {
    Sample() : Loop(), LoopLimit(), Lines()
    {
    }

    explicit Sample(const STCSample& sample)
      : Loop(std::min<uint_t>(sample.Loop, ArraySize(sample.Data)))
      , LoopLimit(std::min<uint_t>(sample.Loop + sample.LoopSize + 1, ArraySize(sample.Data)))
      , Lines(sample.Data, ArrayEnd(sample.Data))
    {
    }

    struct Line
    {
      Line() : Level(), Noise(), NoiseMask(), EnvelopeMask(), Effect()
      {
      }

      /*explicit*/Line(const STCSample::Line& line)
        : Level(line.GetLevel()), Noise(line.GetNoise()), NoiseMask(line.GetNoiseMask())
        , EnvelopeMask(line.GetEnvelopeMask()), Effect(line.GetEffect())
      {
      }
      uint_t Level;//0-15
      uint_t Noise;//0-31
      bool NoiseMask;
      bool EnvelopeMask;
      int_t Effect;
    };

    uint_t GetLoop() const
    {
      return Loop;
    }

    uint_t GetLoopLimit() const
    {
      return LoopLimit;
    }

    uint_t GetSize() const
    {
      return Lines.size();
    }

    const Line& GetLine(uint_t idx) const
    {
      static const Line STUB;
      return Lines.size() > idx ? Lines[idx] : STUB;
    }
  private:
    uint_t Loop;
    uint_t LoopLimit;
    std::vector<Line> Lines;
  };

  enum CmdType
  {
    EMPTY,
    ENVELOPE,     //2p
    NOENVELOPE,   //0p
  };

  typedef TrackingSupport<AYM::CHANNELS, Sample> STCTrack;
  typedef std::vector<int_t> STCTransposition;

  typedef PatternCursorSet<AYM::CHANNELS> PatternCursors;

  class STCAreas
  {
  public:
    enum AreaTypes
    {
      HEADER,
      SAMPLES,
      POSITIONS,
      ORNAMENTS,
      PATTERNS,
      FREE,
      END
    };

    explicit STCAreas(const IO::FastDump& data)
      : Data(data)
    {
      const STCHeader* const header(safe_ptr_cast<const STCHeader*>(Data.Data()));

      Areas.AddArea(HEADER, 0);
      Areas.AddArea(SAMPLES, sizeof(*header));
      Areas.AddArea(POSITIONS, fromLE(header->PositionsOffset));
      Areas.AddArea(ORNAMENTS, fromLE(header->OrnamentsOffset));
      Areas.AddArea(PATTERNS, fromLE(header->PatternsOffset));
      Areas.AddArea(END, data.Size());
    }

    const IO::FastDump& GetOriginalData() const
    {
      return Data;
    }

    const STCHeader& GetHeader() const
    {
      return *safe_ptr_cast<const STCHeader*>(Data.Data());
    }

    RangeIterator<const STCSample*> GetSamples() const
    {
      return GetIterator<STCSample>(SAMPLES);
    }

    uint_t GetSamplesLimit() const
    {
      return GetLimit<STCSample>(SAMPLES);
    }

    RangeIterator<const STCOrnament*> GetOrnaments() const
    {
      return GetIterator<STCOrnament>(ORNAMENTS);
    }

    uint_t GetOrnamentsLimit() const
    {
      return GetLimit<STCOrnament>(ORNAMENTS);
    }

    RangeIterator<const STCPositions::STCPosEntry*> GetPositions() const
    {
      const uint_t offset = Areas.GetAreaAddress(POSITIONS);
      const STCPositions* const positions = safe_ptr_cast<const STCPositions*>(&Data[offset]);
      const STCPositions::STCPosEntry* const entry = positions->Data;
      return RangeIterator<const STCPositions::STCPosEntry*>(entry, entry + positions->Lenght + 1);
    }

    uint_t GetPositionsLimit() const
    {
      const uint_t offset = Areas.GetAreaAddress(POSITIONS);
      const STCPositions* const positions = safe_ptr_cast<const STCPositions*>(&Data[offset]);
      const uint_t size = sizeof(*positions) + sizeof(positions->Data[0]) * positions->Lenght;
      return offset + size;
    }

    RangeIterator<const STCPattern*> GetPatterns() const
    {
      const std::pair<const STCPattern*, const STCPattern*> patRange = GetPatternsRange();
      return RangeIterator<const STCPattern*>(patRange.first, patRange.second);
    }

    uint_t GetPatternsLimit() const
    {
      const uint_t offset = Areas.GetAreaAddress(PATTERNS);
      const std::pair<const STCPattern*, const STCPattern*> patRange = GetPatternsRange();
      const uint_t size = 1 + sizeof(*patRange.first) * (patRange.second - patRange.first);
      return offset + size;
    }
  private:
    template<class T>
    RangeIterator<const T*> GetIterator(AreaTypes id) const
    {
      const uint_t offset = Areas.GetAreaAddress(id);
      const uint_t count = Areas.GetAreaSize(id) / sizeof(T);
      const T* const begin = safe_ptr_cast<const T*>(&Data[offset]);
      return RangeIterator<const T*>(begin, begin + count);
    }

    template<class T>
    uint_t GetLimit(AreaTypes id) const
    {
      const uint_t offset = Areas.GetAreaAddress(id);
      const uint_t count = Areas.GetAreaSize(id) / sizeof(T);
      return offset + count * sizeof(T);
    }

    std::pair<const STCPattern*, const STCPattern*> GetPatternsRange() const
    {
      const uint_t offset = Areas.GetAreaAddress(PATTERNS);
      const STCPattern* const begin = safe_ptr_cast<const STCPattern*>(&Data[offset]);
      const STCPattern* const end = begin + Areas.GetAreaSize(PATTERNS) / sizeof(*begin);
      const STCPattern* const lastPattern = std::find_if(begin, end, std::logical_not<STCPattern>());
      return std::make_pair(begin, lastPattern);
    }
  protected:
    const IO::FastDump& Data;
    AreaController<AreaTypes, 1 + END, uint_t> Areas;
  };

  struct STCModuleData : public STCTrack::ModuleData
  {
    typedef boost::shared_ptr<STCModuleData> RWPtr;
    typedef boost::shared_ptr<const STCModuleData> Ptr;

    STCModuleData()
      : STCTrack::ModuleData()
    {
    }

    ModuleProperties::Ptr ParseInformation(const STCAreas& areas)
    {
      const STCHeader& header = areas.GetHeader();
      InitialTempo = header.Tempo;
      const ModuleProperties::Ptr props = ModuleProperties::Create(STC_PLUGIN_ID);
      props->SetProgram(OptimizeString(FromCharArray(header.Identifier)));
      return props;
    }

    uint_t ParseSamples(const STCAreas& areas, Log::MessagesCollector& warner)
    {
      Samples.resize(MAX_SAMPLES_COUNT);
      for (RangeIterator<const STCSample*> iter = areas.GetSamples(); iter; ++iter)
      {
        const STCSample& sample = *iter;
        if (CheckSample(sample))
        {
          AddSample(sample);
        }
        else
        {
          warner.AddMessage(Text::WARNING_INVALID_SAMPLE);
        }
      }
      return areas.GetSamplesLimit();
    }

    uint_t ParseOrnaments(const STCAreas& areas, Log::MessagesCollector& warner)
    {
      Ornaments.resize(MAX_ORNAMENTS_COUNT);
      for (RangeIterator<const STCOrnament*> iter = areas.GetOrnaments(); iter; ++iter)
      {
        const STCOrnament& ornament = *iter;
        if (CheckOrnament(ornament))
        {
          AddOrnament(ornament);
        }
        else
        {
          warner.AddMessage(Text::WARNING_INVALID_ORNAMENT);
        }
      }
      return areas.GetOrnamentsLimit();
    }

    uint_t ParsePositions(const STCAreas& areas, Log::MessagesCollector& warner)
    {
      assert(!Patterns.empty());
      uint_t positionsCount = 0;
      for (RangeIterator<const STCPositions::STCPosEntry*> iter = areas.GetPositions();
        iter; ++iter, ++positionsCount)
      {
        const STCPositions::STCPosEntry& entry = *iter;
        if (CheckPosition(entry))
        {
          AddPosition(entry);
        }
        else
        {
          warner.AddMessage(Text::WARNING_INVALID_POSITIONS);
        }
      }
      if (Positions.empty())
      {
        throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
      }
      Log::Assert(warner, Positions.size() == positionsCount, Text::WARNING_INVALID_POSITIONS);
      return areas.GetPositionsLimit();
    }

    uint_t ParsePatterns(const STCAreas& areas, Log::MessagesCollector& warner)
    {
      assert(!Samples.empty());
      assert(!Ornaments.empty());
      uint_t resLimit = 0;
      Patterns.resize(MAX_PATTERN_COUNT);
      for (RangeIterator<const STCPattern*> iter = areas.GetPatterns(); iter; ++iter)
      {
        const STCPattern& pattern = *iter;
        assert(pattern);
        if (CheckPattern(pattern))
        {
          Log::ParamPrefixedCollector patternWarner(warner, Text::PATTERN_WARN_PREFIX, pattern.Number - 1);
          STCTrack::Pattern& dst(Patterns[pattern.Number - 1]);
          const uint_t patLimit = ParsePattern(areas.GetOriginalData(), pattern, dst, patternWarner);
          resLimit = std::max(resLimit, patLimit);
        }
        else
        {
          warner.AddMessage(Text::WARNING_INVALID_PATTERN);
        }
      }
      return resLimit;
    }
  private:
    bool CheckSample(const STCSample& sample) const
    {
      return sample.Number < Samples.size();
    }

    void AddSample(const STCSample& sample)
    {
      assert(CheckSample(sample));
      Samples[sample.Number] = Sample(sample);
    }

    bool CheckOrnament(const STCOrnament& ornament) const
    {
      return ornament.Number < Ornaments.size();
    }

    void AddOrnament(const STCOrnament& ornament)
    {
      assert(CheckOrnament(ornament));
      Ornaments[ornament.Number] =
        STCTrack::Ornament(ArraySize(ornament.Data), ornament.Data, ArrayEnd(ornament.Data));
    }

    bool CheckPosition(const STCPositions::STCPosEntry& entry) const
    {
      const uint_t patNum = entry.PatternNum - 1;
      return patNum < MAX_PATTERN_COUNT && !Patterns[patNum].empty();
    }

    void AddPosition(const STCPositions::STCPosEntry& entry)
    {
      assert(CheckPosition(entry));
      Positions.push_back(entry.PatternNum - 1);
      Transpositions.push_back(entry.PatternHeight);
    }

    bool CheckPattern(const STCPattern& pattern) const
    {
      return 0 != pattern.Number && pattern.Number < MAX_PATTERN_COUNT;
    }

    uint_t ParsePattern(const IO::FastDump& data, const STCPattern& pattern, STCTrack::Pattern& dst, Log::MessagesCollector& warner)
    {
      AYMPatternCursors cursors;
      std::transform(pattern.Offsets.begin(), pattern.Offsets.end(), cursors.begin(), &fromLE<uint16_t>);
      dst.reserve(MAX_PATTERN_SIZE);
      do
      {
        const uint_t curPatternSize = dst.size();
        Log::ParamPrefixedCollector lineWarner(warner, Text::LINE_WARN_PREFIX, curPatternSize);
        dst.push_back(STCTrack::Line());
        STCTrack::Line& line(dst.back());
        ParsePatternLine(data, cursors, line, lineWarner);
        //skip lines
        if (const uint_t linesToSkip = cursors.GetMinCounter())
        {
          cursors.SkipLines(linesToSkip);
          dst.resize(dst.size() + linesToSkip);//add dummies
        }
      }
      while (0xff != data[cursors.front().Offset] ||
             0 != cursors.front().Counter);
      Log::Assert(warner, 0 == cursors.GetMaxCounter(), Text::WARNING_PERIODS);
      Log::Assert(warner, dst.size() <= MAX_PATTERN_SIZE, Text::WARNING_INVALID_PATTERN_SIZE);
      const uint_t maxOffset = 1 + cursors.GetMaxOffset();
      return maxOffset;
    }

    void ParsePatternLine(const IO::FastDump& data, AYMPatternCursors& cursors, STCTrack::Line& line, Log::MessagesCollector& warner)
    {
      assert(line.Channels.size() == cursors.size());
      STCTrack::Line::ChannelsArray::iterator channel(line.Channels.begin());
      for (AYMPatternCursors::iterator cur = cursors.begin(); cur != cursors.end(); ++cur, ++channel)
      {
        Log::ParamPrefixedCollector channelWarner(warner, Text::CHANNEL_WARN_PREFIX, std::distance(line.Channels.begin(), channel));
        ParsePatternLineChannel(data, *cur, *channel, channelWarner);
      }
    }

    void ParsePatternLineChannel(const IO::FastDump& data, PatternCursor& cur, STCTrack::Line::Chan& channel,
      Log::MessagesCollector& warner)
    {
      if (cur.Counter--)
      {
        return;//has to skip
      }

      for (const std::size_t dataSize = data.Size(); cur.Offset < dataSize;)
      {
        const uint_t cmd(data[cur.Offset++]);
        const std::size_t restbytes = dataSize - cur.Offset;
        //ornament==0 and sample==0 are valid - no ornament and no sample respectively
        //ornament==0 and sample==0 are valid - no ornament and no sample respectively
        if (cmd <= 0x5f)//note
        {
          channel.SetNote(cmd, warner);
          channel.SetEnabled(true, warner);
          break;
        }
        else if (cmd >= 0x60 && cmd <= 0x6f)//sample
        {
          const uint_t num = cmd - 0x60;
          channel.SetSample(num, warner);
          Log::Assert(warner, !num || Samples[num].GetSize(), Text::WARNING_INVALID_SAMPLE);
        }
        else if (cmd >= 0x70 && cmd <= 0x7f)//ornament
        {
          Log::Assert(warner, !channel.FindCommand(ENVELOPE), Text::WARNING_DUPLICATE_ENVELOPE);
          channel.Commands.push_back(STCTrack::Command(NOENVELOPE));
          const uint_t num = cmd - 0x70;
          channel.SetOrnament(num, warner);
          Log::Assert(warner, !num || Ornaments[num].GetSize(), Text::WARNING_INVALID_ORNAMENT);
        }
        else if (cmd == 0x80)//reset
        {
          channel.SetEnabled(false, warner);
          break;
        }
        else if (cmd == 0x81)//empty
        {
          break;
        }
        else if (cmd >= 0x82 && cmd <= 0x8e)//orn 0, without/with envelope
        {
          channel.SetOrnament(0, warner);
          Log::Assert(warner, !channel.FindCommand(ENVELOPE) && !channel.FindCommand(NOENVELOPE),
            Text::WARNING_DUPLICATE_ENVELOPE);
          if (cmd == 0x82)
          {
            channel.Commands.push_back(STCTrack::Command(NOENVELOPE));
          }
          else if (!restbytes)
          {
            throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
          }
          else
          {
            channel.Commands.push_back(STCTrack::Command(ENVELOPE, cmd - 0x80, data[cur.Offset++]));
          }
        }
        else if (cmd < 0xa1 || !restbytes)
        {
          throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
        }
        else //skip
        {
          cur.Period = cmd - 0xa1;
        }
      }
      cur.Counter = cur.Period;
    }
  public:
    STCTransposition Transpositions;
  };

  Player::Ptr CreateSTCPlayer(Information::Ptr info, STCModuleData::Ptr data, AYM::Chip::Ptr device);

  class STCHolder : public Holder
  {
  public:
    STCHolder(Plugin::Ptr plugin, Parameters::Accessor::Ptr parameters,
        const MetaContainer& container, ModuleRegion& region)
      : SrcPlugin(plugin)
      , Data(boost::make_shared<STCModuleData>())
      , Info(TrackInfo::Create(Data))
    {
      //assume that data is ok
      const IO::FastDump& data = IO::FastDump(*container.Data, region.Offset);
      const STCAreas areas(data);

      Log::MessagesCollector::Ptr warner(Log::MessagesCollector::Create());

      const ModuleProperties::Ptr props = Data->ParseInformation(areas);
      const uint_t smpLim = Data->ParseSamples(areas, *warner);
      const uint_t ornLim = Data->ParseOrnaments(areas, *warner);
      const uint_t patLim = Data->ParsePatterns(areas, *warner);
      const uint_t posLim = Data->ParsePositions(areas, *warner);

      const std::size_t maxLim = std::max(std::max(smpLim, ornLim), std::max(patLim, posLim));
      //fill region
      region.Size = std::min(data.Size(), maxLim);
        
      RawData = region.Extract(*container.Data);

      //meta properties
      {
        const ModuleRegion fixedRegion(sizeof(STCHeader), region.Size - sizeof(STCHeader));
        props->SetSource(RawData, fixedRegion);
      }
      props->SetPlugins(container.Plugins);
      props->SetPath(container.Path);
      props->SetWarnings(warner);

      Info->SetLogicalChannels(AYM::LOGICAL_CHANNELS);
      Info->SetModuleProperties(Parameters::CreateMergedAccessor(parameters, props));
    }

    virtual Plugin::Ptr GetPlugin() const
    {
      return SrcPlugin;
    }

    virtual Information::Ptr GetModuleInformation() const
    {
      return Info;
    }

    virtual Player::Ptr CreatePlayer() const
    {
      return CreateSTCPlayer(Info, Data, AYM::CreateChip());
    }

    virtual Error Convert(const Conversion::Parameter& param, Dump& dst) const
    {
      using namespace Conversion;
      Error result;
      if (parameter_cast<RawConvertParam>(&param))
      {
        const uint8_t* const data = static_cast<const uint8_t*>(RawData->Data());
        dst.assign(data, data + RawData->Size());
      }
      else if (!ConvertAYMFormat(boost::bind(&CreateSTCPlayer, boost::cref(Info), boost::cref(Data), _1),
        param, dst, result))
      {
        return Error(THIS_LINE, ERROR_MODULE_CONVERT, Text::MODULE_ERROR_CONVERSION_UNSUPPORTED);
      }
      return result;
    }
  private:
    const Plugin::Ptr SrcPlugin;
    const STCModuleData::RWPtr Data;
    const TrackInfo::Ptr Info;
    IO::DataContainer::Ptr RawData;
  };

  class STCChannelSynthesizer
  {
  public:
    STCChannelSynthesizer(uint_t transposition, AYMTrackSynthesizer& synth, uint_t chanNum)
      : Transposition(transposition)
      , MainSynth(synth)
      , ChanSynth(synth.GetChannel(chanNum))
    {
    }

    void SetLevel(uint_t level)
    {
      ChanSynth.SetLevel(level);
    }

    void EnableEnvelope()
    {
      ChanSynth.EnableEnvelope();
    }

    void SetTone(int_t halftones, int_t offset)
    {
      ChanSynth.SetTone(halftones + Transposition, offset);
      ChanSynth.EnableTone();
    }

    void SetNoise(int_t level)
    {
      MainSynth.SetNoise(level);
      ChanSynth.EnableNoise();
    }

  private:
    const uint_t Transposition;
    AYMTrackSynthesizer& MainSynth;
    const AYMChannelSynthesizer ChanSynth;
  };

  struct STCChannelState
  {
    explicit STCChannelState(STCModuleData::Ptr data)
      : Data(data)
      , Enabled(false), Envelope(false)
      , Note()
      , CurSample(GetStubSample()), PosInSample(0), LoopedInSample(false)
      , CurOrnament(GetStubOrnament())
    {
    }

    void Reset()
    {
      Enabled = false;
      Envelope = false;
      Note = 0;
      CurSample = GetStubSample();
      PosInSample = 0;
      LoopedInSample = false;
      CurOrnament = GetStubOrnament();
    }

    void SetNewState(const STCTrack::Line::Chan& src)
    {
      if (src.Enabled)
      {
        SetEnabled(*src.Enabled);
      }
      if (src.Note)
      {
        SetNote(*src.Note);
      }
      if (src.SampleNum)
      {
        SetSample(*src.SampleNum);
      }
      if (src.OrnamentNum)
      {
        SetOrnament(*src.OrnamentNum);
      }
      if (!src.Commands.empty())
      {
        ApplyCommands(src.Commands);
      }
    }

    void Synthesize(STCChannelSynthesizer& synthesizer) const
    {
      if (!Enabled)
      {
        synthesizer.SetLevel(0);
        return;
      }

      const STCTrack::Sample::Line& curSampleLine = CurSample->GetLine(PosInSample);

      //apply level
      synthesizer.SetLevel(curSampleLine.Level);
      //apply envelope
      if (Envelope)
      {
        synthesizer.EnableEnvelope();
      }
      //apply tone
      const int_t halftones = int_t(Note) + CurOrnament->GetLine(PosInSample);
      if (!curSampleLine.EnvelopeMask)
      {
        synthesizer.SetTone(halftones, curSampleLine.Effect);
      }
      //apply noise
      if (!curSampleLine.NoiseMask)
      {
        synthesizer.SetNoise(curSampleLine.Noise);
      }
    }

    void Iterate()
    {
      if (++PosInSample >= (LoopedInSample ? CurSample->GetLoopLimit() : CurSample->GetSize()))
      {
        if (CurSample->GetLoop() && CurSample->GetLoop() < CurSample->GetSize())
        {
          PosInSample = CurSample->GetLoop();
          LoopedInSample = true;
        }
        else
        {
          Enabled = false;
        }
      }
    }
  private:
    static const STCTrack::Sample* GetStubSample()
    {
      static const STCTrack::Sample stubSample;
      return &stubSample;
    }

    static const STCTrack::Ornament* GetStubOrnament()
    {
      static const STCTrack::Ornament stubOrnament;
      return &stubOrnament;
    }

    void SetEnabled(bool enabled)
    {
      Enabled = enabled;
      if (!Enabled)
      {
        PosInSample = 0;
      }
    }

    void SetNote(uint_t note)
    {
      Note = note;
      PosInSample = 0;
      LoopedInSample = false;
    }

    void SetSample(uint_t sampleNum)
    {
      CurSample = sampleNum < Data->Samples.size() ? &Data->Samples[sampleNum] : GetStubSample();
      PosInSample = 0;
    }

    void SetOrnament(uint_t ornamentNum)
    {
      CurOrnament = ornamentNum < Data->Ornaments.size() ? &Data->Ornaments[ornamentNum] : GetStubOrnament();
      PosInSample = 0;
    }

    void ApplyCommands(const STCTrack::CommandsArray& commands)
    {
      std::for_each(commands.begin(), commands.end(), boost::bind(&STCChannelState::ApplyCommand, this, _1));
    }

    void ApplyCommand(const STCTrack::Command& command)
    {
      if (command == ENVELOPE)
      {
        Envelope = true;
      }
      else if (command == NOENVELOPE)
      {
        Envelope = false;
      }
    }
  private:
    const STCModuleData::Ptr Data;
    bool Enabled;
    bool Envelope;
    uint_t Note;
    const STCTrack::Sample* CurSample;
    uint_t PosInSample;
    bool LoopedInSample;
    const STCTrack::Ornament* CurOrnament;
  };

  void ProcessEnvelopeCommands(const STCTrack::CommandsArray& commands, AYMTrackSynthesizer& synthesizer)
  {
    const STCTrack::CommandsArray::const_iterator it = std::find(commands.begin(), commands.end(), ENVELOPE);
    if (it != commands.end())
    {
      synthesizer.SetEnvelopeType(it->Param1);
      synthesizer.SetEnvelopeTone(it->Param2);
    }
  }

  class STCDataRenderer : public AYMDataRenderer
  {
  public:
    explicit STCDataRenderer(STCModuleData::Ptr data)
      : Data(data)
      , StateA(Data)
      , StateB(Data)
      , StateC(Data)
    {
    }

    virtual void Reset()
    {
      StateA.Reset();
      StateB.Reset();
      StateC.Reset();
    }

    virtual void SynthesizeData(const TrackState& state, AYMTrackSynthesizer& synthesizer)
    {
      if (0 == state.Quirk())
      {
        GetNewLineState(state, synthesizer);
      }
      SynthesizeChannelsData(state, synthesizer);
      StateA.Iterate();
      StateB.Iterate();
      StateC.Iterate();
    }

  private:
    void GetNewLineState(const TrackState& state, AYMTrackSynthesizer& synthesizer)
    {
      const STCTrack::Line& line(Data->Patterns[state.Pattern()][state.Line()]);
      if (const STCTrack::Line::Chan& src = line.Channels[0])
      {
        StateA.SetNewState(src);
        ProcessEnvelopeCommands(src.Commands, synthesizer);
      }
      if (const STCTrack::Line::Chan& src = line.Channels[1])
      {
        StateB.SetNewState(src);
        ProcessEnvelopeCommands(src.Commands, synthesizer);
      }
      if (const STCTrack::Line::Chan& src = line.Channels[2])
      {
        StateC.SetNewState(src);
        ProcessEnvelopeCommands(src.Commands, synthesizer);
      }
    }

    void SynthesizeChannelsData(const TrackState& state, AYMTrackSynthesizer& synthesizer)
    {
      const uint_t transposition = Data->Transpositions[state.Position()];
      {
        STCChannelSynthesizer synth(transposition, synthesizer, 0);
        StateA.Synthesize(synth);
      }
      {
        STCChannelSynthesizer synth(transposition, synthesizer, 1);
        StateB.Synthesize(synth);
      }
      {
        STCChannelSynthesizer synth(transposition, synthesizer, 2);
        StateC.Synthesize(synth);
      }
    }
  private:
    const STCModuleData::Ptr Data;
    STCChannelState StateA;
    STCChannelState StateB;
    STCChannelState StateC;
  };

  Player::Ptr CreateSTCPlayer(Information::Ptr info, STCModuleData::Ptr data, AYM::Chip::Ptr device)
  {
    const AYMDataRenderer::Ptr renderer = boost::make_shared<STCDataRenderer>(data);
    return CreateAYMTrackPlayer(info, data, renderer, device, TABLE_SOUNDTRACKER);
  }

  class STCAreasChecker : public STCAreas
  {
  public:
    explicit STCAreasChecker(const IO::FastDump& data)
      : STCAreas(data)
    {
    }

    bool CheckLayout() const
    {
      return sizeof(STCHeader) == Areas.GetAreaSize(STCAreas::HEADER) &&
             0 == Areas.GetAreaSize(STCAreas::END);
    }

    bool CheckSamples() const
    {
      const uint_t size = Areas.GetAreaSize(STCAreas::SAMPLES);
      return size != 0 && 0 == size % sizeof(STCSample);
    }

    bool CheckOrnaments() const
    {
      const uint_t size = Areas.GetAreaSize(STCAreas::ORNAMENTS);
      return size != 0 && 0 == size % sizeof(STCOrnament);
    }

    bool CheckPositions() const
    {
      const uint_t size = Areas.GetAreaSize(STCAreas::POSITIONS);
      if (0 == size ||
          0 != (size - 1) % sizeof(STCPositions::STCPosEntry))
      {
        return false;
      }
      const uint_t limit = Areas.GetAreaAddress(STCAreas::END);
      const uint_t offset = Areas.GetAreaAddress(STCAreas::POSITIONS);
      if (offset > limit)
      {
        return false;
      }
      const STCPositions* const positions = safe_ptr_cast<const STCPositions*>(&Data[offset]);
      return positions->Lenght == (size - 1) / sizeof(STCPositions::STCPosEntry) - 1;
    }

    bool CheckPatterns() const
    {
      const uint_t size = Areas.GetAreaSize(STCAreas::PATTERNS);
      if (0 == size)
      {
        return false;
      }
      const uint_t limit = Areas.GetAreaAddress(STCAreas::END);
      const uint_t offset = Areas.GetAreaAddress(STCAreas::PATTERNS);
      if (offset > limit)
      {
        return false;
      }
      const STCPattern* const patterns = safe_ptr_cast<const STCPattern*>(&Data[offset]);
      const STCPattern* const patternsLimit = patterns + size / sizeof(STCPattern);
      for (const STCPattern* pattern = patterns; *pattern && pattern != patternsLimit; ++pattern)
      {
        if (!pattern->Number || pattern->Number >= MAX_PATTERN_COUNT)
        {
          continue;
        }
        if (pattern->Offsets.end() != std::find_if(pattern->Offsets.begin(), pattern->Offsets.end(),
          boost::bind(&fromLE<uint16_t>, _1) > limit - 1))
        {
          return false;
        }
      }
      return true;
    }
  };

  bool CheckSTCModule(const uint8_t* data, std::size_t limit)
  {
    if (limit < sizeof(STCHeader))
    {
      return false;
    }

    const IO::FastDump dump(data, limit);
    const STCAreasChecker areas(dump);

    return areas.CheckLayout() &&
           areas.CheckSamples() &&
           areas.CheckOrnaments() &&
           areas.CheckPositions() &&
           areas.CheckPatterns();
  }

  //////////////////////////////////////////////////////////////////////////
  class STCPlugin : public PlayerPlugin
                  , public boost::enable_shared_from_this<STCPlugin>
                  , private ModuleDetector
  {
  public:
    virtual String Id() const
    {
      return STC_PLUGIN_ID;
    }

    virtual String Description() const
    {
      return Text::STC_PLUGIN_INFO;
    }

    virtual String Version() const
    {
      return STC_PLUGIN_VERSION;
    }

    virtual uint_t Capabilities() const
    {
      return CAP_STOR_MODULE | CAP_DEV_AYM | CAP_CONV_RAW | GetSupportedAYMFormatConvertors();
    }

    virtual bool Check(const IO::DataContainer& inputData) const
    {
      return CheckDataFormat(*this, inputData);
    }

    Module::Holder::Ptr CreateModule(Parameters::Accessor::Ptr parameters, const MetaContainer& container, ModuleRegion& region) const
    {
      return CreateModuleFromData(*this, parameters, container, region);
    }
  private:
    virtual DataPrefixIterator GetPrefixes() const
    {
      return DataPrefixIterator(PLAYERS, ArrayEnd(PLAYERS));
    }

    virtual bool CheckData(const uint8_t* data, std::size_t size) const
    {
      return CheckSTCModule(data, size);
    }

    virtual Holder::Ptr TryToCreateModule(Parameters::Accessor::Ptr parameters,
      const MetaContainer& container, ModuleRegion& region) const
    {
      const Plugin::Ptr plugin = shared_from_this();
      try
      {
        const Holder::Ptr holder(new STCHolder(plugin, parameters, container, region));
#ifdef SELF_TEST
        holder->CreatePlayer();
#endif
        return holder;
      }
      catch (const Error&/*e*/)
      {
        Log::Debug("Core::STCSupp", "Failed to create holder");
      }
      return Holder::Ptr();
    }
  };
}

namespace ZXTune
{
  void RegisterSTCSupport(PluginsEnumerator& enumerator)
  {
    const PlayerPlugin::Ptr plugin(new STCPlugin());
    enumerator.RegisterPlugin(plugin);
  }
}
