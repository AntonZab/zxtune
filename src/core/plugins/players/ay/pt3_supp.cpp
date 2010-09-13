/*
Abstract:
  PT3 modules playback support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "ay_conversion.h"
#include "aym_parameters_helper.h"
#include <core/plugins/detect_helper.h>
#include <core/plugins/utils.h>
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
#include <sound/render_params.h>
//boost includes
#include <boost/bind.hpp>
#include <boost/enable_shared_from_this.hpp>
//text includes
#include <core/text/core.h>
#include <core/text/plugins.h>
#include <core/text/warnings.h>

#define FILE_TAG 3CBC0BBC

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  const Char PT3_PLUGIN_ID[] = {'P', 'T', '3', 0};
  const String PT3_PLUGIN_VERSION(FromStdString("$Rev$"));

  const std::size_t MAX_MODULE_SIZE = 16384;
  const uint_t MAX_PATTERNS_COUNT = 85;
  const uint_t MAX_PATTERN_SIZE = 256;//really no limit for PT3.58+
  const uint_t MAX_SAMPLES_COUNT = 32;
  const uint_t MAX_SAMPLE_SIZE = 64;
  const uint_t MAX_ORNAMENTS_COUNT = 16;
  const uint_t MAX_ORNAMENT_SIZE = 64;

  //checkers
  static const DetectFormatChain DETECTORS[] = {
    //PT3.4 without signatures
    {
      "21??"    // ld hl,xxxx
      "18?"     // jr xx
      "c3??"    // jp xxxx
      "f3"      // di
      "ed73??"  // ld (xxxx),sp
      "22??"    // ld (xxxx),hl
      "e5"      // push hl
      "01??"    // ld bc,xxxx(0x63)
      "09"      // add hl,bc
      ,
      3559
    },
    //PT3x
    {
      "21??"    // ld hl,xxxx
      "18?"     // jr xx
      "c3??"    // jp xxxx
      "c3+35+"  // jp xx:ds 33
      "f3"      // di
      "22??"    // ld (xxxx),hl
      "22??"
      "22??"
      "22??"
      "016400"  // ld bc,0x64
      "09"      // add hl,bc
      ,
      3617
    },
    //PT3.5x
    {
      "21??"    // ld hl,xxxx
      "18?"     // jr xx
      "c3??"    // jp xxxx
      "c3+37+"  // jp xx:ds 35
      "f3"      // di
      "ed73??"  // ld (xxxx),sp
      "22??"    // ld (xxxx),hl
      "22??"
      "22??"
      "22??"
      "016400"  // ld bc,0x64
      "09"      // add hl,bc
      ,
      3462
    },
    //PT3.5 without player text
    {
      "21??"    // ld hl,xxxx
      "18?"     // jr xx
      "c3??"    // jp xxxx
      "18?"     // jr xx
      "f3"      // di
      "ed73??"  // ld (xxxx),sp
      "22??"    // ld (xxxx),hl
      "44"      // ld b,h
      "4d"      // ld c,l
      "11??"    // ld de,xxxx
      "19"      // add hl,de
      "7e"      // ld a,(hl)
      "23"      // inc hl
      "32??"    // ld (xxxx),a
      "f9"      // ld sp,hl
      "19"      // add hl,de
      "22??"    // ld (xxxx),hl
      "f1"      // pop af
      "5f"      // ld e,a
      "19"      // add hl,de
      "22??"    // ld (xxxx),hl
      "e1"      // pop hl
      "09"      // add hl,bc
      ,
      0xc89
    },
    //Vortex1
    {
      "21??"    // ld hl,xxxx
      "18?"     // jr xx
      "c3??"    // jp xxxx
      "18+25+"  // jr xx:ds 24
      "21??"    // ld hl,xxxx
      "cbfe"    // set 7,(hl)
      "cb46"    // bit 0,(hl)
      "c8"      // ret z
      "e1"      // pop hl
      "21??"    // ld hl,xxxx
      "34"      // inc (hl)
      "21??"    // ld hl,xxxx
      "34"      // inc (hl)
      "af"      // xor a
      "67"      // ld h,a
      "6f"      // ld l,a
      "32??"    // ld (xxxx),a
      "22??"    // ld (xxxx),hl
      "c3"      // jp xxxx
      ,
      0x86e
    },
    //Vortex2
    {
      "21??"    // ld hl,xxxx
      "18?"     // jr xx
      "c3??"    // jp xxxx
      "18?"     // jr xx
      "?"       // db xx
      "f3"      // di
      "ed73??"  // ld (xxxx),sp
      "22??"    // ld (xxxx),hl
      "44"      // ld b,h
      "4d"      // ld c,l
      "11??"    // ld de,xxxx
      "19"      // add hl,de
      "7e"      // ld a,(hl)
      "23"      // inc hl
      "32??"    // ld (xxxx),a
      "f9"      // ld sp,hl
      "19"      // add hl,de
      "22??"    // ld (xxxx),hl
      "f1"      // pop af
      "5f"      // ld e,a
      "19"      // add hl,de
      "22??"    // ld (xxxx),hl
      "e1"      // pop hl
      "09"      // add hl,bc
      ,
      0xc00
    },
    //Vortex1.0
    {
      "21??"    // ld hl,xxxx
      "18?"     // jr xx
      "c3??"    // jp xxxx
      "18?"     // jr xx
      "f3"      // di
      "ed73??"  // ld (xxxx),sp
      "22??"    // ld (xxxx),hl
      "44"      // ld b,h
      "4d"      // ld c,l
      "11??"    // ld de,xxxx
      "19"      // add hl,de
      "7e"      // ld a,(hl)
      "23"      // inc hl
      "32??"    // ld (xxxx),hl
      "f9"      // ld sp,hl
      "19"      // add hl,de
      "22??"    // ld (xxxx),hl
      "f1"      // pop af
      "5f"      // ld e,a
      "19"      // add hl,de
      "22??"    // ld (xxxx),hl
      "e1"      // pop hl
      "09"      // add hl,bc
      ,
      0xc00
    }
  };
  //////////////////////////////////////////////////////////////////////////
  //possible values for Mode field
  const uint_t AY_TRACK = 0x20;
  //all other are TS patterns count

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
  PACK_PRE struct PT3Header
  {
    uint8_t Id[13];        //'ProTracker 3.'
    uint8_t Subversion;
    uint8_t Optional1[16]; //' compilation of '
    char TrackName[32];
    uint8_t Optional2[4]; //' by '
    char TrackAuthor[32];
    uint8_t Mode;
    uint8_t FreqTableNum;
    uint8_t Tempo;
    uint8_t Length;
    uint8_t Loop;
    uint16_t PatternsOffset;
    boost::array<uint16_t, MAX_SAMPLES_COUNT> SamplesOffsets;
    boost::array<uint16_t, MAX_ORNAMENTS_COUNT> OrnamentsOffsets;
    uint8_t Positions[1]; //finished by marker
  } PACK_POST;

  PACK_PRE struct PT3Pattern
  {
    boost::array<uint16_t, 3> Offsets;

    bool Check() const
    {
      return Offsets[0] && Offsets[1] && Offsets[2];
    }
  } PACK_POST;

  PACK_PRE struct PT3Sample
  {
    uint8_t Loop;
    uint8_t Size;

    uint_t GetSize() const
    {
      return sizeof(*this) + (Size - 1) * sizeof(Data[0]);
    }

    static uint_t GetMinimalSize()
    {
      return sizeof(PT3Sample) - sizeof(PT3Sample::Line);
    }

    PACK_PRE struct Line
    {
      //SUoooooE
      //NkKTLLLL
      //OOOOOOOO
      //OOOOOOOO
      // S - vol slide
      // U - vol slide up
      // o - noise or env offset
      // E - env mask
      // L - level
      // T - tone mask
      // K - keep noise or env offset
      // k - keep tone offset
      // N - noise mask
      // O - tone offset
      uint8_t VolSlideEnv;
      uint8_t LevelKeepers;
      int16_t ToneOffset;
      
      bool GetEnvelopeMask() const
      {
        return 0 != (VolSlideEnv & 1);
      }
      
      int_t GetNoiseOrEnvelopeOffset() const
      {
        const uint8_t noeoff = (VolSlideEnv & 62) >> 1;
        return static_cast<int8_t>(noeoff & 16 ? noeoff | 0xf0 : noeoff);
      }
      
      int_t GetVolSlide() const
      {
        return (VolSlideEnv & 128) ? ((VolSlideEnv & 64) ? +1 : -1) : 0;
      }
      
      uint_t GetLevel() const
      {
        return LevelKeepers & 15;
      }
      
      bool GetToneMask() const
      {
        return 0 != (LevelKeepers & 16);
      }
      
      bool GetKeepNoiseOrEnvelopeOffset() const
      {
        return 0 != (LevelKeepers & 32);
      }
      
      bool GetKeepToneOffset() const
      {
        return 0 != (LevelKeepers & 64);
      }
      
      bool GetNoiseMask() const
      {
        return 0 != (LevelKeepers & 128);
      }
      
      int_t GetToneOffset() const
      {
        return fromLE(ToneOffset);
      }
    } PACK_POST;
    Line Data[1];
  } PACK_POST;

  PACK_PRE struct PT3Ornament
  {
    uint8_t Loop;
    uint8_t Size;
    int8_t Data[1];
    
    uint_t GetSize() const
    {
      return sizeof(*this) + (Size - 1) * sizeof(Data[0]);
    }

    static uint_t GetMinimalSize()
    {
      return sizeof(PT3Ornament) - sizeof(int8_t);
    }
  } PACK_POST;
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  BOOST_STATIC_ASSERT(sizeof(PT3Header) == 202);
  BOOST_STATIC_ASSERT(sizeof(PT3Pattern) == 6);
  BOOST_STATIC_ASSERT(sizeof(PT3Sample) == 6);
  BOOST_STATIC_ASSERT(sizeof(PT3Ornament) == 3);

  inline Vortex::Sample::Line ParseSampleLine(const PT3Sample::Line& line)
  {
    Vortex::Sample::Line res;
    res.Level = line.GetLevel();
    res.VolumeSlideAddon = line.GetVolSlide();
    res.ToneMask = line.GetToneMask();
    res.ToneOffset = line.GetToneOffset();
    res.KeepToneOffset = line.GetKeepToneOffset();
    res.NoiseMask = line.GetNoiseMask();
    res.EnvMask = line.GetEnvelopeMask();
    res.NoiseOrEnvelopeOffset = line.GetNoiseOrEnvelopeOffset();
    res.KeepNoiseOrEnvelopeOffset = line.GetKeepNoiseOrEnvelopeOffset();
    return res;
  }
  
  inline Vortex::Sample ParseSample(const IO::FastDump& data, uint16_t offset, std::size_t& rawSize)
  {
    const uint_t off(fromLE(offset));
    const PT3Sample* const sample(safe_ptr_cast<const PT3Sample*>(&data[off]));
    if (0 == offset || !sample->Size)
    {
      return Vortex::Sample();//safe
    }
    std::vector<Vortex::Sample::Line> tmpData(sample->Size);
    std::transform(sample->Data, sample->Data + sample->Size, tmpData.begin(), ParseSampleLine);
    Vortex::Sample tmp(sample->Loop, tmpData.begin(), tmpData.end());
    rawSize = std::max<std::size_t>(rawSize, off + sample->GetSize());
    return tmp;
  }

  inline SimpleOrnament ParseOrnament(const IO::FastDump& data, uint16_t offset, std::size_t& rawSize)
  {
    const uint_t off(fromLE(offset));
    const PT3Ornament* const ornament(safe_ptr_cast<const PT3Ornament*>(&data[off]));
    if (0 == offset || !ornament->Size)
    {
      return SimpleOrnament();//safe version
    }
    rawSize = std::max<std::size_t>(rawSize, off + ornament->GetSize());
    return SimpleOrnament(ornament->Loop, ornament->Data, ornament->Data + ornament->Size);
  }
  
  class PT3Holder : public Holder
  {
    typedef boost::array<PatternCursor, AYM::CHANNELS> PatternCursors;
    
    void ParsePattern(const IO::FastDump& data
      , PatternCursors& cursors
      , Vortex::Track::Line& line
      , Log::MessagesCollector& warner
      , uint_t& noiseBase
      )
    {
      bool wasEnvelope(false), wasNoisebase(false);
      assert(line.Channels.size() == cursors.size());
      Vortex::Track::Line::ChannelsArray::iterator channel(line.Channels.begin());
      for (PatternCursors::iterator cur = cursors.begin(); cur != cursors.end(); ++cur, ++channel)
      {
        if (cur->Counter--)
        {
          continue;//has to skip
        }

        Log::ParamPrefixedCollector channelWarner(warner, Text::CHANNEL_WARN_PREFIX, std::distance(line.Channels.begin(), channel));
        for (;;)
        {
          const uint_t cmd(data[cur->Offset++]);
          const std::size_t restbytes = data.Size() - cur->Offset;
          if (cmd == 1)//gliss
          {
            channel->Commands.push_back(Vortex::Track::Command(Vortex::GLISS));
          }
          else if (cmd == 2)//portamento
          {
            channel->Commands.push_back(Vortex::Track::Command(Vortex::GLISS_NOTE));
          }
          else if (cmd == 3)//sample offset
          {
            channel->Commands.push_back(Vortex::Track::Command(Vortex::SAMPLEOFFSET, -1));
          }
          else if (cmd == 4)//ornament offset
          {
            channel->Commands.push_back(Vortex::Track::Command(Vortex::ORNAMENTOFFSET));
          }
          else if (cmd == 5)//vibrate
          {
            channel->Commands.push_back(Vortex::Track::Command(Vortex::VIBRATE));
          }
          else if (cmd == 8)//slide envelope
          {
            channel->Commands.push_back(Vortex::Track::Command(Vortex::SLIDEENV));
          }
          else if (cmd == 9)//tempo
          {
            channel->Commands.push_back(Vortex::Track::Command(Vortex::TEMPO));
          }
          else if ((cmd >= 0x10 && cmd <= 0x1f) ||
                   (cmd >= 0xb2 && cmd <= 0xbf) ||
                    cmd >= 0xf0)
          {
            const bool hasEnv(cmd >= 0x11 && cmd <= 0xbf);
            const bool hasOrn(cmd >= 0xf0);
            const bool hasSmp(cmd < 0xb2 || cmd > 0xbf);
            
            if (restbytes < std::size_t(2 * hasEnv + hasSmp))
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            
            if (hasEnv) //has envelope command
            {
              const uint_t envPeriod(data[cur->Offset + 1] + (uint_t(data[cur->Offset]) << 8));
              cur->Offset += 2;
              Log::Assert(channelWarner, !wasEnvelope, Text::WARNING_DUPLICATE_ENVELOPE);
              channel->Commands.push_back(Vortex::Track::Command(Vortex::ENVELOPE, cmd - (cmd >= 0xb2 ? 0xb1 : 0x10), envPeriod));
              wasEnvelope = true;
            }
            else
            {
              channel->Commands.push_back(Vortex::Track::Command(Vortex::NOENVELOPE));
            }
           
            if (hasOrn) //has ornament command
            {
              const uint_t num(cmd - 0xf0);
              Log::Assert(channelWarner, !num || Data->Ornaments[num].GetSize(), Text::WARNING_INVALID_ORNAMENT);
              Log::Assert(channelWarner, !channel->OrnamentNum, Text::WARNING_DUPLICATE_ORNAMENT);
              channel->OrnamentNum = num;
            }
            
            if (hasSmp)
            {
              const uint_t doubleSampNum(data[cur->Offset++]);
              const bool sampValid(doubleSampNum < MAX_SAMPLES_COUNT * 2 && 0 == (doubleSampNum & 1));
              Log::Assert(channelWarner, sampValid, Text::WARNING_INVALID_SAMPLE);
              Log::Assert(channelWarner, !channel->SampleNum, Text::WARNING_DUPLICATE_SAMPLE);
              channel->SampleNum = sampValid ? (doubleSampNum / 2) : 0;
            }
          }
          else if (cmd >= 0x20 && cmd <= 0x3f)
          {
            //noisebase should be in channel B
            //Log::Assert(channelWarner, chan == 1, Text::WARNING_INVALID_NOISE_BASE);
            Log::Assert(channelWarner, !wasNoisebase, Text::WARNING_DUPLICATE_NOISEBASE);
            noiseBase = cmd - 0x20;
            wasNoisebase = true;
          }
          else if (cmd >= 0x40 && cmd <= 0x4f)
          {
            const uint_t num(cmd - 0x40);
            Log::Assert(channelWarner, !num || Data->Ornaments[num].GetSize(), Text::WARNING_INVALID_ORNAMENT);
            Log::Assert(channelWarner, !channel->OrnamentNum, Text::WARNING_DUPLICATE_ORNAMENT);
            channel->OrnamentNum = num;
          }
          else if (cmd >= 0x50 && cmd <= 0xaf)
          {
            const uint_t note(cmd - 0x50);
            Vortex::Track::CommandsArray::iterator it(std::find(channel->Commands.begin(), channel->Commands.end(), Vortex::GLISS_NOTE));
            if (channel->Commands.end() != it)
            {
              it->Param3 = note;
            }
            else
            {
              Log::Assert(channelWarner, !channel->Note, Text::WARNING_DUPLICATE_NOTE);
              channel->Note = note;
            }
            Log::Assert(channelWarner, !channel->Enabled, Text::WARNING_DUPLICATE_STATE);
            channel->Enabled = true;
            break;
          }
          else if (cmd == 0xb0)
          {
            channel->Commands.push_back(Vortex::Track::Command(Vortex::NOENVELOPE));
          }
          else if (cmd == 0xb1)
          {
            if (restbytes < 1)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            cur->Period = data[cur->Offset++] - 1;
          }
          else if (cmd == 0xc0)
          {
            Log::Assert(channelWarner, !channel->Enabled, Text::WARNING_DUPLICATE_STATE);
            channel->Enabled = false;
            break;
          }
          else if (cmd >= 0xc1 && cmd <= 0xcf)
          {
            Log::Assert(channelWarner, !channel->Volume, Text::WARNING_DUPLICATE_VOLUME);
            channel->Volume = cmd - 0xc0;
          }
          else if (cmd == 0xd0)
          {
            break;
          }
          else if (cmd >= 0xd1 && cmd <= 0xef)
          {
            //TODO: check for empty sample
            Log::Assert(channelWarner, !channel->SampleNum, Text::WARNING_DUPLICATE_SAMPLE);
            channel->SampleNum = cmd - 0xd0;
          }
        }
        //parse parameters
        const std::size_t restbytes = data.Size() - cur->Offset;
        for (Vortex::Track::CommandsArray::reverse_iterator it = channel->Commands.rbegin(), lim = channel->Commands.rend();
          it != lim; ++it)
        {
          switch (it->Type)
          {
          case Vortex::TEMPO:
            if (restbytes < 1)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            Log::Assert(channelWarner, !line.Tempo, Text::WARNING_DUPLICATE_TEMPO);
            line.Tempo = data[cur->Offset++];
            break;
          case Vortex::SLIDEENV:
          case Vortex::GLISS:
            if (restbytes < 3)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            it->Param1 = data[cur->Offset++];
            it->Param2 = static_cast<int16_t>(256 * data[cur->Offset + 1] + data[cur->Offset]);
            cur->Offset += 2;
            break;
          case Vortex::VIBRATE:
            if (restbytes < 2)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            it->Param1 = data[cur->Offset++];
            it->Param2 = data[cur->Offset++];
            break;
          case Vortex::ORNAMENTOFFSET:
          {
            if (restbytes < 1)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            const uint_t offset(data[cur->Offset++]);
            const bool isValid(offset < (channel->OrnamentNum ?
              Data->Ornaments[*channel->OrnamentNum].GetSize() : MAX_ORNAMENT_SIZE));
            Log::Assert(channelWarner, isValid, Text::WARNING_INVALID_ORNAMENT_OFFSET);
            it->Param1 = isValid ? offset : 0;
            break;
          }
          case Vortex::SAMPLEOFFSET:
            if (-1 == it->Param1)
            {
              if (restbytes < 1)
              {
                throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
              }
              const uint_t offset(data[cur->Offset++]);
              const bool isValid(offset < (channel->SampleNum ?
                Data->Samples[*channel->SampleNum].GetSize() : MAX_SAMPLE_SIZE));
              Log::Assert(channelWarner, isValid, Text::WARNING_INVALID_SAMPLE_OFFSET);
              it->Param1 = isValid ? offset : 0;
            }
            break;
          case Vortex::GLISS_NOTE:
            if (restbytes < 5)
            {
              throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
            }
            it->Param1 = data[cur->Offset++];
            //skip limiter
            cur->Offset += 2;
            it->Param2 = static_cast<int16_t>(256 * data[cur->Offset + 1] + data[cur->Offset]);
            cur->Offset += 2;
            break;
          }
        }
        cur->Counter = cur->Period;
      }
      if (noiseBase)
      {
        //place to channel B
        line.Channels[1].Commands.push_back(Vortex::Track::Command(Vortex::NOISEBASE, noiseBase));
      }
    }
  public:
    PT3Holder(Plugin::Ptr plugin, const MetaContainer& container, ModuleRegion& region)
      : SrcPlugin(plugin)
      , Data(Vortex::Track::ModuleData::Create())
      , Info(Vortex::Track::ModuleInfo::Create(Data))
      , Version()
      , TSPatternBase(0)
    {
      //assume all data is correct
      const IO::FastDump& data = IO::FastDump(*container.Data, region.Offset);
      const PT3Header* const header(safe_ptr_cast<const PT3Header*>(&data[0]));

      Log::MessagesCollector::Ptr warner(Log::MessagesCollector::Create());

      std::size_t rawSize(0);
      //fill samples
      Data->Samples.reserve(header->SamplesOffsets.size());
      std::transform(header->SamplesOffsets.begin(), header->SamplesOffsets.end(),
        std::back_inserter(Data->Samples), boost::bind(&ParseSample, boost::cref(data), _1, boost::ref(rawSize)));
      //fill ornaments
      Data->Ornaments.reserve(header->OrnamentsOffsets.size());
      std::transform(header->OrnamentsOffsets.begin(), header->OrnamentsOffsets.end(),
        std::back_inserter(Data->Ornaments), boost::bind(&ParseOrnament, boost::cref(data), _1, boost::ref(rawSize)));

      //fill patterns
      Data->Patterns.resize(MAX_PATTERNS_COUNT);
      uint_t index(0);
      Data->Patterns.resize(1 + *std::max_element(header->Positions, header->Positions + header->Length) / 3);
      const PT3Pattern* patterns(safe_ptr_cast<const PT3Pattern*>(&data[fromLE(header->PatternsOffset)]));
      for (const PT3Pattern* pattern = patterns; pattern->Check() && index < Data->Patterns.size(); ++pattern, ++index)
      {
        Log::ParamPrefixedCollector patternWarner(*warner, Text::PATTERN_WARN_PREFIX, index);
        Vortex::Track::Pattern& pat(Data->Patterns[index]);
        
        PatternCursors cursors;
        std::transform(pattern->Offsets.begin(), pattern->Offsets.end(), cursors.begin(), &fromLE<uint16_t>);
        pat.reserve(MAX_PATTERN_SIZE);
        uint_t noiseBase(0);
        do
        {
          const uint_t patternSize = pat.size();
          if (patternSize > MAX_PATTERN_SIZE)
          {
            throw Error(THIS_LINE, ERROR_INVALID_FORMAT);//no details
          }
          Log::ParamPrefixedCollector patLineWarner(patternWarner, Text::LINE_WARN_PREFIX, patternSize);
          pat.push_back(Vortex::Track::Line());
          Vortex::Track::Line& line(pat.back());
          ParsePattern(data, cursors, line, patLineWarner, noiseBase);
          //skip lines
          if (const uint_t linesToSkip = std::min_element(cursors.begin(), cursors.end(), PatternCursor::CompareByCounter)->Counter)
          {
            std::for_each(cursors.begin(), cursors.end(), std::bind2nd(std::mem_fun_ref(&PatternCursor::SkipLines), linesToSkip));
            pat.resize(pat.size() + linesToSkip);//add dummies
          }
        }
        while (data[cursors.front().Offset] || cursors.front().Counter);
        //as warnings
        Log::Assert(patternWarner, 0 == std::max_element(cursors.begin(), cursors.end(), PatternCursor::CompareByCounter)->Counter,
          Text::WARNING_PERIODS);
        Log::Assert(patternWarner, pat.size() <= MAX_PATTERN_SIZE, Text::WARNING_INVALID_PATTERN_SIZE);
        rawSize = std::max<std::size_t>(rawSize, std::max_element(cursors.begin(), cursors.end(), PatternCursor::CompareByOffset)->Offset + 1);
      }
      //fill order
      for (const uint8_t* curPos = header->Positions; curPos != header->Positions + header->Length; ++curPos)
      {
        if (0 == *curPos % 3 && !Data->Patterns[*curPos / 3].empty())
        {
          Data->Positions.push_back(*curPos / 3);
        }
      }
      Log::Assert(*warner, header->Length == Data->Positions.size(), Text::WARNING_INVALID_LENGTH);

      //fill region
      region.Size = rawSize;
      region.Extract(*container.Data, RawData);
      
      //meta properties
      Info->SetType(PT3_PLUGIN_ID);
      Info->SetContainer(container);
      Info->SetData(*container.Data, region);
      {
        const std::size_t fixedOffset(sizeof(PT3Header) + header->Length - 1);
        const ModuleRegion fixedRegion(fixedOffset, rawSize -  fixedOffset);
        Info->SetFixedData(*container.Data, region);
      }
      Info->SetTitle(OptimizeString(FromStdString(header->TrackName)));
      Info->SetAuthor(OptimizeString(FromStdString(header->TrackAuthor)));
      Info->SetProgram(OptimizeString(String(header->Id, header->Optional1)));
      Info->SetWarnings(*warner);
      
      //tracking properties
      Version = std::isdigit(header->Subversion) ? header->Subversion - '0' : 6;
      FreqTableName = Vortex::GetFreqTable(static_cast<Vortex::NoteTable>(header->FreqTableNum), Version);

      //TODO: proper calculating
      Info->SetLoopPosition(header->Loop);
      Info->SetTempo(header->Tempo);
      if (header->Mode != AY_TRACK)
      {
        TSPatternBase = header->Mode;
        Info->SetLogicalChannels(Info->LogicalChannels() * 2);
      }
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
      return TSPatternBase ?
        Vortex::CreateTSPlayer(Info, Data, Version, FreqTableName, TSPatternBase, AYM::CreateChip(), AYM::CreateChip())
        :
        Vortex::CreatePlayer(Info, Data, Version, FreqTableName, AYM::CreateChip());
    }
    
    virtual Error Convert(const Conversion::Parameter& param, Dump& dst) const
    {
      using namespace Conversion;
      Error result;
      if (parameter_cast<RawConvertParam>(&param))
      {
        dst = RawData;
        return Error();
      }
      else if (TSPatternBase == 0)//only on usual modules
      {
        if (ConvertAYMFormat(boost::bind(&Vortex::CreatePlayer, boost::cref(Info), boost::cref(Data), Version, FreqTableName, _1),
          param, dst, result))
        {
          return result;
        }
        else if (ConvertVortexFormat(*Data, *Info, param, Version, FreqTableName, dst, result))
        {
          return result;
        }
      }
      return Error(THIS_LINE, ERROR_MODULE_CONVERT, Text::MODULE_ERROR_CONVERSION_UNSUPPORTED);
    }
  private:
    const Plugin::Ptr SrcPlugin;
    const Vortex::Track::ModuleData::RWPtr Data;
    const Vortex::Track::ModuleInfo::Ptr Info;
    Dump RawData;
    uint_t Version;
    String FreqTableName;
    uint_t TSPatternBase;
  };

  //////////////////////////////////////////////////
  bool CheckPT3Module(const uint8_t* data, std::size_t size) 
  {
    const PT3Header* const header(safe_ptr_cast<const PT3Header*>(data));
    if (size < sizeof(*header))
    {
      return false;
    }
    const std::size_t patOff(fromLE(header->PatternsOffset));
    if (!header->Length ||
        patOff >= size || patOff < sizeof(*header) ||
        0xff != data[patOff - 1] ||
        &header->Positions[header->Length] != data + patOff - 1 ||
        &data[patOff - 1] != std::find_if(header->Positions, data + patOff - 1,
          std::bind2nd(std::modulus<uint8_t>(), 3))
      )
    {
      return false;
    }
    //check patterns
    const uint_t patternsCount = 1 + *std::max_element(header->Positions, header->Positions + header->Length) / 3;
    if (!patternsCount || patternsCount > MAX_PATTERNS_COUNT)
    {
      return false;
    }
    //patOff is start of patterns and other data
    RangeChecker::Ptr checker(RangeChecker::CreateShared(size));
    checker->AddRange(0, patOff);

    //check samples
    for (const uint16_t* sampOff = header->SamplesOffsets.begin(); sampOff != header->SamplesOffsets.end(); ++sampOff)
    {
      if (const uint_t offset = fromLE(*sampOff))
      {
        if (offset + PT3Sample::GetMinimalSize() > size)
        {
          return false;
        }
        const PT3Sample* const sample(safe_ptr_cast<const PT3Sample*>(data + offset));
        if (!checker->AddRange(offset, sample->GetSize()))
        {
          return false;
        }
      }
    }
    //check ornaments
    for (const uint16_t* ornOff = header->OrnamentsOffsets.begin(); ornOff != header->OrnamentsOffsets.end(); ++ornOff)
    {
      if (const uint_t offset = fromLE(*ornOff))
      {
        if (offset + PT3Ornament::GetMinimalSize() > size)
        {
          return false;
        }
        const PT3Ornament* const ornament(safe_ptr_cast<const PT3Ornament*>(data + offset));
        if (!checker->AddRange(offset, ornament->GetSize()))
        {
          return false;
        }
      }
    }
    const PT3Pattern* patPos(safe_ptr_cast<const PT3Pattern*>(data + patOff));
    for (uint_t patIdx = 0; patPos->Check() && patIdx < patternsCount; ++patPos, ++patIdx)
    {
      if (!checker->AddRange(patOff + sizeof(PT3Pattern) * patIdx, sizeof(PT3Pattern)))
      {
        return false;
      }
      //at least 1 byte for pattern
      if (patPos->Offsets.end() != std::find_if(patPos->Offsets.begin(), patPos->Offsets.end(),
        !boost::bind(&RangeChecker::AddRange, checker.get(), boost::bind(&fromLE<uint16_t>, _1), 1)))
      {
        return false;
      }
    }
    return true;
  }
  
  Holder::Ptr CreatePT3Module(Plugin::Ptr plugin, const MetaContainer& container, ModuleRegion& region)
  {
    try
    {
      const Holder::Ptr holder(new PT3Holder(plugin, container, region));
#ifdef SELF_TEST
      holder->CreatePlayer();
#endif
      return holder;
    }
    catch (const Error&/*e*/)
    {
      Log::Debug("Core::PT3Supp", "Failed to create holder");
    }
    return Holder::Ptr();
  }

  //////////////////////////////////////////////////////////////////////////
  class PT3Plugin : public PlayerPlugin
                  , public boost::enable_shared_from_this<PT3Plugin>
  {
  public:
    virtual String Id() const
    {
      return PT3_PLUGIN_ID;
    }

    virtual String Description() const
    {
      return Text::PT3_PLUGIN_INFO;
    }

    virtual String Version() const
    {
      return PT3_PLUGIN_VERSION;
    }

    virtual uint_t Capabilities() const
    {
      return CAP_STOR_MODULE | CAP_DEV_AYM | CAP_CONV_RAW |
        GetSupportedAYMFormatConvertors() | GetSupportedVortexFormatConvertors();
    }

    virtual bool Check(const IO::DataContainer& inputData) const
    {
      return PerformCheck(&CheckPT3Module, DETECTORS, ArrayEnd(DETECTORS), inputData);
    }

    virtual Module::Holder::Ptr CreateModule(const Parameters::Map& /*parameters*/,
                                             const MetaContainer& container,
                                             ModuleRegion& region) const
    {
      const Plugin::Ptr plugin = shared_from_this();
      return PerformCreate(&CheckPT3Module, &CreatePT3Module, DETECTORS, ArrayEnd(DETECTORS),
        plugin, container, region);
    }
  };
}

namespace ZXTune
{
  void RegisterPT3Support(PluginsEnumerator& enumerator)
  {
    const PlayerPlugin::Ptr plugin(new PT3Plugin());
    enumerator.RegisterPlugin(plugin);
  }
}