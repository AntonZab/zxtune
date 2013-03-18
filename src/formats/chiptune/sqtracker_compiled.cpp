/*
Abstract:
  SQTracker compiled format implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "container.h"
#include "sqtracker.h"
//common includes
#include <byteorder.h>
#include <contract.h>
#include <crc.h>
#include <range_checker.h>
//library includes
#include <binary/container_factories.h>
#include <binary/typed_container.h>
#include <debug/log.h>
#include <math/numeric.h>
//std includes
#include <set>
//boost includes
#include <boost/array.hpp>
#include <boost/make_shared.hpp>
//text includes
#include <formats/text/chiptune.h>

namespace
{
  const Debug::Stream Dbg("Formats::Chiptune::SQTracker");
}

namespace Formats
{
namespace Chiptune
{
  namespace SQTracker
  {
    const std::size_t MAX_MODULE_SIZE = 0x3600;
    const std::size_t MAX_POSITIONS_COUNT = 120;
    const std::size_t MIN_PATTERN_SIZE = 11;
    const std::size_t MAX_PATTERN_SIZE = 64;
    const std::size_t MAX_PATTERNS_COUNT = 99;
    const std::size_t MAX_SAMPLES_COUNT = 26;
    const std::size_t MAX_ORNAMENTS_COUNT = 26;
    const std::size_t SAMPLE_SIZE = 32;
    const std::size_t ORNAMENT_SIZE = 32;

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
    PACK_PRE struct RawSample
    {
      uint8_t Loop;
      uint8_t LoopSize;
      PACK_PRE struct Line
      {
        //nnnnvvvv
        uint8_t VolumeAndNoise;
        //nTNDdddd
        //n- low noise bit
        //d- high delta bits
        //D- delta sign (1 for positive)
        //N- enable noise
        //T- enable tone
        uint8_t Flags;
        //dddddddd - low delta bits
        uint8_t Delta;

        uint_t GetLevel() const
        {
          return VolumeAndNoise & 15;
        }

        uint_t GetNoise() const
        {
          return ((VolumeAndNoise & 240) >> 3) | ((Flags & 128) >> 7);
        }

        bool GetEnableNoise() const
        {
          return 0 != (Flags & 32);
        }

        bool GetEnableTone() const
        {
          return 0 != (Flags & 64);
        }

        int_t GetToneDeviation() const
        {
          const int delta = int_t(Delta) | ((Flags & 15) << 8);
          return 0 != (Flags & 16) ? delta : -delta;
        }
      } PACK_POST;
      Line Data[SAMPLE_SIZE];
    } PACK_POST;

    PACK_PRE struct RawPosEntry
    {
      PACK_PRE struct Channel
      {
        //0 is end
        //Eppppppp
        //p - pattern idx
        //E - enable effect
        uint8_t Pattern;
        //TtttVVVV
        //signed transposition
        uint8_t TranspositionAndAttenuation;

        uint_t GetPattern() const
        {
          return Pattern & 127;
        }

        int_t GetTransposition() const
        {
          return static_cast<int8_t>(TranspositionAndAttenuation) >> 4;
        }

        uint_t GetAttenuation() const
        {
          return TranspositionAndAttenuation & 15;
        }

        bool GetEnableEffects() const
        {
          return 0 != (Pattern & 128);
        }
      } PACK_POST;

      Channel ChannelC;
      Channel ChannelB;
      Channel ChannelA;
      uint8_t Tempo;
    } PACK_POST;

    PACK_PRE struct RawOrnament
    {
      uint8_t Loop;
      uint8_t LoopSize;
      boost::array<int8_t, ORNAMENT_SIZE> Lines;
    } PACK_POST;

    PACK_PRE struct RawHeader
    {
      uint16_t Size;
      uint16_t SamplesOffset;
      uint16_t OrnamentsOffset;
      uint16_t PatternsOffset;
      uint16_t PositionsOffset;
      uint16_t LoopPositionOffset;
    } PACK_POST;
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

    class StubBuilder : public Builder
    {
    public:
      virtual void SetProgram(const String& /*program*/) {}
      //samples+ornaments
      virtual void SetSample(uint_t /*index*/, const Sample& /*sample*/) {}
      virtual void SetOrnament(uint_t /*index*/, const Ornament& /*ornament*/) {}
      //patterns
      virtual void SetPositions(const std::vector<PositionEntry>& /*positions*/, uint_t /*loop*/) {}
      //! @invariant Patterns are built sequentally
      virtual void StartPattern(uint_t /*index*/) {}
      virtual void FinishPattern(uint_t /*size*/) {}
      //! @invariant Lines are built sequentally
      virtual void StartLine(uint_t /*index*/) {}
      virtual void SetTempo(uint_t /*tempo*/) {}
      virtual void SetTempoAddon(uint_t /*add*/) {}
      virtual void SetRest() {}
      virtual void SetNote(uint_t /*note*/) {}
      virtual void SetNoteAddon(int_t /*add*/) {}
      virtual void SetSample(uint_t /*sample*/) {}
      virtual void SetOrnament(uint_t /*ornament*/) {}
      virtual void SetEnvelope(uint_t /*type*/, uint_t /*value*/) {}
      virtual void SetGlissade(int_t /*step*/) {}
      virtual void SetAttenuation(uint_t /*att*/) {}
      virtual void SetAttenuationAddon(int_t /*add*/) {}
      virtual void SetGlobalAttenuation(uint_t /*att*/) {}
      virtual void SetGlobalAttenuationAddon(int_t /*add*/) {}
    };

    typedef std::set<uint_t> Indices;

    class StatisticCollectingBuilder : public Builder
    {
    public:
      explicit StatisticCollectingBuilder(Builder& delegate)
        : Delegate(delegate)
      {
      }

      virtual void SetProgram(const String& program)
      {
        return Delegate.SetProgram(program);
      }

      virtual void SetSample(uint_t index, const Sample& sample)
      {
        assert(UsedSamples.count(index));
        return Delegate.SetSample(index, sample);
      }

      virtual void SetOrnament(uint_t index, const Ornament& ornament)
      {
        assert(UsedOrnaments.count(index));
        return Delegate.SetOrnament(index, ornament);
      }

      virtual void SetPositions(const std::vector<PositionEntry>& positions, uint_t loop)
      {
        Indices patterns;
        for (std::vector<PositionEntry>::const_iterator it = positions.begin(), lim = positions.end(); it != lim; ++it)
        {
          for (uint_t chan = 0; chan != 3; ++chan)
          {
            patterns.insert(it->Channels[chan].Pattern);
          }
        }
        UsedPatterns.swap(patterns);
        return Delegate.SetPositions(positions, loop);
      }

      virtual void StartPattern(uint_t index)
      {
        assert(UsedPatterns.count(index));
        return Delegate.StartPattern(index);
      }

      virtual void FinishPattern(uint_t size)
      {
        return Delegate.FinishPattern(size);
      }

      virtual void StartLine(uint_t index)
      {
        return Delegate.StartLine(index);
      }

      virtual void SetTempo(uint_t tempo)
      {
        return Delegate.SetTempo(tempo);
      }

      virtual void SetTempoAddon(uint_t add)
      {
        return Delegate.SetTempoAddon(add);
      }

      virtual void SetRest()
      {
        return Delegate.SetRest();
      }

      virtual void SetNote(uint_t note)
      {
        return Delegate.SetNote(note);
      }

      virtual void SetNoteAddon(int_t add)
      {
        return Delegate.SetNoteAddon(add);
      }

      virtual void SetSample(uint_t sample)
      {
        UsedSamples.insert(sample);
        return Delegate.SetSample(sample);
      }

      virtual void SetOrnament(uint_t ornament)
      {
        UsedOrnaments.insert(ornament);
        return Delegate.SetOrnament(ornament);
      }

      virtual void SetEnvelope(uint_t type, uint_t value)
      {
        return Delegate.SetEnvelope(type, value);
      }

      virtual void SetGlissade(int_t step)
      {
        return Delegate.SetGlissade(step);
      }

      virtual void SetAttenuation(uint_t att)
      {
        return Delegate.SetAttenuation(att);
      }

      virtual void SetAttenuationAddon(int_t add)
      {
        return Delegate.SetAttenuationAddon(add);
      }

      virtual void SetGlobalAttenuation(uint_t att)
      {
        return Delegate.SetGlobalAttenuation(att);
      }

      virtual void SetGlobalAttenuationAddon(int_t add)
      {
        return Delegate.SetGlobalAttenuationAddon(add);
      }

      const Indices& GetUsedPatterns() const
      {
        return UsedPatterns;
      }

      const Indices& GetUsedSamples() const
      {
        return UsedSamples;
      }

      const Indices& GetUsedOrnaments() const
      {
        return UsedOrnaments;
      }
    private:
      Builder& Delegate;
      Indices UsedPatterns;
      Indices UsedSamples;
      Indices UsedOrnaments;
    };

    class RangesMap
    {
    public:
      explicit RangesMap(std::size_t limit)
        : ServiceRanges(RangeChecker::CreateSimple(limit))
        , TotalRanges(RangeChecker::CreateSimple(limit))
        , FixedRanges(RangeChecker::CreateSimple(limit))
      {
      }

      void AddService(std::size_t offset, std::size_t size) const
      {
        Require(ServiceRanges->AddRange(offset, size));
        Add(offset, size);
      }

      void AddFixed(std::size_t offset, std::size_t size) const
      {
        Require(FixedRanges->AddRange(offset, size));
        Add(offset, size);
      }

      void Add(std::size_t offset, std::size_t size) const
      {
        Dbg(" Affected range %1%..%2%", offset, offset + size);
        Require(TotalRanges->AddRange(offset, size));
      }

      std::size_t GetSize() const
      {
        return TotalRanges->GetAffectedRange().second;
      }

      RangeChecker::Range GetFixedArea() const
      {
        return FixedRanges->GetAffectedRange();
      }
    private:
      const RangeChecker::Ptr ServiceRanges;
      const RangeChecker::Ptr TotalRanges;
      const RangeChecker::Ptr FixedRanges;
    };

    std::size_t GetFixDelta(const RawHeader& hdr)
    {
      const uint_t samplesAddr = fromLE(hdr.SamplesOffset);
      //since ornaments,samples and patterns tables are 1-based, real delta is 2 bytes shorter
      const uint_t delta = offsetof(RawHeader, LoopPositionOffset);
      Require(samplesAddr >= delta);
      return samplesAddr - delta;
    }

    class Format
    {
    public:
      explicit Format(const Binary::TypedContainer& data)
        : Delegate(data)
        , Ranges(data.GetSize())
        , Source(*Delegate.GetField<RawHeader>(0))
        , Delta(GetFixDelta(Source))
      {
        Ranges.AddService(0, sizeof(Source));
      }

      void ParseCommonProperties(Builder& builder) const
      {
        builder.SetProgram(Text::SQTRACKER_DECODER_DESCRIPTION);
      }

      void ParsePositions(Builder& builder) const
      {
        const std::size_t loopPositionOffset = fromLE(Source.LoopPositionOffset) - Delta;
        std::size_t posOffset = fromLE(Source.PositionsOffset) - Delta;
        std::vector<PositionEntry> result;
        uint_t loopPos = 0;
        for (uint_t pos = 0;; ++pos)
        {
          if (posOffset == loopPositionOffset)
          {
            loopPos = pos;
          }
          if (const RawPosEntry* fullEntry = Delegate.GetField<RawPosEntry>(posOffset))
          {
            Ranges.AddService(posOffset, sizeof(*fullEntry));
            if (0 == fullEntry->ChannelC.GetPattern()
             || 0 == fullEntry->ChannelB.GetPattern()
             || 0 == fullEntry->ChannelA.GetPattern())
            {
              break;
            }
            PositionEntry dst;
            ParsePositionChannel(fullEntry->ChannelC, dst.Channels[2]);
            ParsePositionChannel(fullEntry->ChannelB, dst.Channels[1]);
            ParsePositionChannel(fullEntry->ChannelA, dst.Channels[0]);
            dst.Tempo = fullEntry->Tempo;
            posOffset += sizeof(*fullEntry);
            result.push_back(dst);
          }
          else
          {
            const RawPosEntry::Channel& partialEntry = GetServiceObject<RawPosEntry::Channel>(posOffset);
            Require(0 == partialEntry.GetPattern());
            const std::size_t tailSize = std::min(sizeof(RawPosEntry), Delegate.GetSize() - posOffset);
            Ranges.Add(posOffset, tailSize);
            break;
          }
        }
        builder.SetPositions(result, loopPos);
        Dbg("Positions: %1% entries, loop to %2%", result.size(), loopPos);
      }

      void ParsePatterns(const Indices& pats, Builder& builder) const
      {
        Require(!pats.empty());
        for (Indices::const_iterator it = pats.begin(), lim = pats.end(); it != lim; ++it)
        {
          const uint_t patIndex = *it;
          Require(Math::InRange<uint_t>(patIndex + 1, 1, MAX_PATTERNS_COUNT));
          Dbg("Parse pattern %1%", patIndex);
          const std::size_t src = GetPatternOffset(patIndex);
          builder.StartPattern(patIndex);
          ParsePattern(src, builder);
        }
      }

      void ParseSamples(const Indices& samples, Builder& builder) const
      {
        Require(!samples.empty());
        Require(0 == samples.count(0));
        for (Indices::const_iterator it = samples.begin(), lim = samples.end(); it != lim; ++it)
        {
          const uint_t samIdx = *it;
          Require(Math::InRange<uint_t>(samIdx, 1, MAX_SAMPLES_COUNT));
          Dbg("Parse sample %1%", samIdx);
          Sample result;
          const RawSample& src = GetSample(samIdx);
          ParseSample(src, result);
          builder.SetSample(samIdx, result);
        }
      }

      void ParseOrnaments(const Indices& ornaments, Builder& builder) const
      {
        if (ornaments.empty())
        {
          Dbg("No ornaments used");
          return;
        }
        Require(0 == ornaments.count(0));
        for (Indices::const_iterator it = ornaments.begin(), lim = ornaments.end(); it != lim; ++it)
        {
          const uint_t ornIdx = *it;
          Require(Math::InRange<uint_t>(ornIdx, 1, MAX_ORNAMENTS_COUNT));
          Dbg("Parse ornament %1%", ornIdx);
          Ornament result;
          const RawOrnament& src = GetOrnament(ornIdx);
          ParseOrnament(src, result);
          builder.SetOrnament(ornIdx, result);
        }
      }

      std::size_t GetSize() const
      {
        return Ranges.GetSize();
      }

      RangeChecker::Range GetFixedArea() const
      {
        return Ranges.GetFixedArea();
      }
    private:
      template<class T>
      const T& GetServiceObject(std::size_t offset) const
      {
        const T* const src = Delegate.GetField<T>(offset);
        Require(src != 0);
        Ranges.AddService(offset, sizeof(T));
        return *src;
      }

      template<class T>
      const T& GetObject(std::size_t offset) const
      {
        const T* const src = Delegate.GetField<T>(offset);
        Require(src != 0);
        Ranges.Add(offset, sizeof(T));
        return *src;
      }

      uint8_t PeekByte(std::size_t offset) const
      {
        const uint8_t* const data = Delegate.GetField<uint8_t>(offset);
        Require(data != 0);
        return *data;
      }

      std::size_t GetPatternOffset(uint_t index) const
      {
        const std::size_t entryAddr = fromLE(Source.PatternsOffset) + index * sizeof(uint16_t);
        Require(entryAddr >= Delta);
        const std::size_t patternAddr = fromLE(GetServiceObject<uint16_t>(entryAddr - Delta));
        Require(patternAddr >= Delta);
        return patternAddr - Delta;
      }

      const RawSample& GetSample(uint_t index) const
      {
        const std::size_t entryAddr = fromLE(Source.SamplesOffset) + index * sizeof(uint16_t);
        Require(entryAddr >= Delta);
        const std::size_t sampleAddr = fromLE(GetServiceObject<uint16_t>(entryAddr - Delta));
        Require(sampleAddr >= Delta);
        return GetObject<RawSample>(sampleAddr - Delta);
      }

      const RawOrnament& GetOrnament(uint_t index) const
      {
        const std::size_t entryAddr = fromLE(Source.OrnamentsOffset) + index * sizeof(uint16_t);
        Require(entryAddr >= Delta);
        const std::size_t ornamentAddr = fromLE(GetServiceObject<uint16_t>(entryAddr - Delta));
        Require(ornamentAddr >= Delta);
        return GetObject<RawOrnament>(ornamentAddr - Delta);
      }

      struct ParserState
      {
        uint_t Period;
        std::size_t Cursor;
        std::size_t LastNoteStart;
        bool RepeatLastNote;

        ParserState(std::size_t cursor)
          : Period()
          , Cursor(cursor)
          , LastNoteStart()
          , RepeatLastNote()
        {
        }
      };

      void ParsePattern(std::size_t patOffset, Builder& builder) const
      {
        const std::size_t patSize = PeekByte(patOffset);
        Require(patSize <= MAX_PATTERN_SIZE);
        ParserState state(patOffset + 1);
        uint_t lineIdx = 0;
        for (uint_t counter = 0; lineIdx < patSize; ++lineIdx)
        {
          if (counter)
          {
            --counter;
            if (state.RepeatLastNote)
            {
              builder.StartLine(lineIdx);
              ParseNote(state.LastNoteStart, builder);
            }
          }
          else
          {
            builder.StartLine(lineIdx);
            ParseLine(state, builder);
            counter = state.Period;
          }
        }
        builder.FinishPattern(lineIdx);
        const std::size_t start = patOffset;
        if (start >= Delegate.GetSize())
        {
          Dbg("Invalid offset (%1%)", start);
        }
        else
        {
          const std::size_t stop = std::min(Delegate.GetSize(), state.Cursor);
          Ranges.AddFixed(start, stop - start);
        }
      }

      void ParseLine(ParserState& state, Builder& builder) const
      {
        state.RepeatLastNote = false;
        const uint_t cmd = PeekByte(state.Cursor++);
        if (cmd <= 0x5f)
        {
          state.LastNoteStart = state.Cursor - 1;
          builder.SetNote(cmd);
          state.Cursor = ParseNoteParameters(state.Cursor, builder);
        }
        else if (cmd <= 0x6e)
        {
          ParseEffect(cmd - 0x60, PeekByte(state.Cursor++), builder);
        }
        else if (cmd == 0x6f)
        {
          builder.SetRest();
        }
        else if (cmd <= 0x7f)
        {
          builder.SetRest();
          ParseEffect(cmd - 0x6f, PeekByte(state.Cursor++), builder);
        }
        else if (cmd <= 0x9f)
        {
          const int_t addon = cmd & 15;
          builder.SetNoteAddon(cmd & 16 ? -addon : addon);
          ParseNote(state.LastNoteStart, builder);
        }
        else if (cmd <= 0xbf)
        {
          state.Period = cmd & 15;
          if (cmd & 16)
          {
            state.RepeatLastNote |= state.Period != 0;
            ParseNote(state.LastNoteStart, builder);
          }
        }
        else
        {
          state.LastNoteStart = state.Cursor - 1;
          builder.SetSample(cmd & 31);
        }
      }

      void ParseNote(std::size_t cursor, Builder& builder) const
      {
        const uint_t cmd = PeekByte(cursor);
        if (cmd < 0x80)
        {
          ParseNoteParameters(cursor + 1, builder);
        }
        else
        {
          builder.SetSample(cmd & 31);
        }
      }

      std::size_t ParseNoteParameters(std::size_t start, Builder& builder) const
      {
        std::size_t cursor = start;
        const uint_t cmd = PeekByte(cursor++);
        if (0 != (cmd & 128))
        {
          if (const uint_t sample = (cmd >> 1) & 31)
          {
            builder.SetSample(sample);
          }
          if (0 != (cmd & 64))
          {
            const uint_t param = PeekByte(cursor++);
            if (const uint_t ornament = (param >> 4) | ((cmd & 1) << 4))
            {
              builder.SetOrnament(ornament);
            }
            if (const uint_t effect = param & 15)
            {
              ParseEffect(effect, PeekByte(cursor++), builder);
            }
          }
        }
        else
        {
          ParseEffect(cmd, PeekByte(cursor++), builder);
        }
        return cursor;
      }

      static void ParseEffect(uint_t code, uint_t param, Builder& builder)
      {
        switch (code - 1)
        {
        case 0:
          builder.SetAttenuation(param & 15);
          break;
        case 1:
          builder.SetAttenuationAddon(static_cast<int8_t>(param));
          break;
        case 2:
          builder.SetGlobalAttenuation(param);
          break;
        case 3:
          builder.SetGlobalAttenuationAddon(static_cast<int8_t>(param));
          break;
        case 4:
          builder.SetTempo(param & 31 ? param & 31 : 32);
          break;
        case 5:
          builder.SetTempoAddon(param);
          break;
        case 6:
          builder.SetGlissade(-int_t(param));
          break;
        case 7:
          builder.SetGlissade(int_t(param));
          break;
        default:
          builder.SetEnvelope((code - 1) & 15, param);
          break;
        }
      }

      static void ParsePositionChannel(const RawPosEntry::Channel& src, PositionEntry::Channel& dst)
      {
        dst.Pattern = src.GetPattern();
        dst.Transposition = src.GetTransposition();
        dst.Attenuation = src.GetAttenuation();
        dst.EnabledEffects = src.GetEnableEffects();
        Require(Math::InRange<uint_t>(dst.Pattern, 1, MAX_PATTERNS_COUNT + 1));
      }

      static void ParseSample(const RawSample& src, Sample& dst)
      {
        dst.Lines.resize(SAMPLE_SIZE);
        for (uint_t idx = 0; idx < SAMPLE_SIZE; ++idx)
        {
          const RawSample::Line& line = src.Data[idx];
          Sample::Line& res = dst.Lines[idx];
          res.Level = line.GetLevel();
          res.Noise = line.GetNoise();
          res.EnableNoise = line.GetEnableNoise();
          res.EnableTone = line.GetEnableTone();
          res.ToneDeviation = line.GetToneDeviation();
        }
        dst.Loop = std::min<uint_t>(src.Loop, SAMPLE_SIZE);
        dst.LoopSize = src.LoopSize;
      }

      static void ParseOrnament(const RawOrnament& src, Ornament& dst)
      {
        dst.Lines.assign(src.Lines.begin(), src.Lines.end());
        dst.Loop = std::min<uint_t>(src.Loop, ORNAMENT_SIZE);
        dst.LoopSize = src.LoopSize;
      }
    private:
      const Binary::TypedContainer Delegate;
      RangesMap Ranges;
      const RawHeader& Source;
      const std::size_t Delta;
    };

    enum AreaTypes
    {
      HEADER,
      SAMPLES_OFFSETS,
      ORNAMENTS_OFFSETS,
      PATTERNS_OFFSETS,
      SAMPLES,
      ORNAMENTS,
      PATTERNS,
      POSITIONS,
      LOOPED_POSITION,
      END
    };

    struct Areas : public AreaController<AreaTypes, 1 + END, std::size_t>
    {
      Areas(const Binary::TypedContainer& data, std::size_t unfixDelta, std::size_t size)
      {
        const RawHeader& hdr = *data.GetField<RawHeader>(0);
        AddArea(HEADER, 0);
        const std::size_t sample1AddrOffset = fromLE(hdr.SamplesOffset) + sizeof(uint16_t) - unfixDelta;
        AddArea(SAMPLES_OFFSETS, sample1AddrOffset);
        const std::size_t ornament1AddrOffset = fromLE(hdr.OrnamentsOffset) + sizeof(uint16_t) - unfixDelta;
        if (hdr.OrnamentsOffset != hdr.PatternsOffset)
        {
          AddArea(ORNAMENTS_OFFSETS, ornament1AddrOffset);
        }
        const std::size_t pattern1AddOffset = fromLE(hdr.PatternsOffset) + sizeof(uint16_t) - unfixDelta;
        AddArea(PATTERNS_OFFSETS, pattern1AddOffset);
        AddArea(POSITIONS, fromLE(hdr.PositionsOffset) - unfixDelta);
        if (hdr.PositionsOffset != hdr.LoopPositionOffset)
        {
          AddArea(LOOPED_POSITION, fromLE(hdr.LoopPositionOffset) - unfixDelta);
        }
        AddArea(END, size);
        if (!CheckHeader())
        {
          return;
        }
        if (CheckSamplesOffsets())
        {
          const std::size_t sample1Addr = fromLE(*data.GetField<uint16_t>(sample1AddrOffset)) - unfixDelta;
          AddArea(SAMPLES, sample1Addr);
        }
        if (CheckOrnamentsOffsets())
        {
          const std::size_t ornament1Addr = fromLE(*data.GetField<uint16_t>(ornament1AddrOffset)) - unfixDelta;
          AddArea(ORNAMENTS, ornament1Addr);
        }
      }

      bool CheckHeader() const
      {
        return sizeof(RawHeader) == GetAreaSize(HEADER) && Undefined == GetAreaSize(END);
      }

      bool CheckSamples() const
      {
        const std::size_t size = GetAreaSize(SAMPLES);
        return size != Undefined && size >= sizeof(RawSample);
      }

      bool CheckOrnaments() const
      {
        if (GetAreaAddress(ORNAMENTS_OFFSETS) == Undefined)
        {
          return true;
        }
        else if (!CheckOrnamentsOffsets())
        {
          return false;
        }
        const std::size_t ornamentsSize = GetAreaSize(ORNAMENTS);
        return ornamentsSize != Undefined && ornamentsSize >= sizeof(RawOrnament);
      }

      bool CheckPatterns() const
      {
        const std::size_t size = GetAreaSize(PATTERNS_OFFSETS);
        return size != Undefined && size >= sizeof(uint16_t);
      }

      bool CheckPositions() const
      {
        if (GetAreaAddress(LOOPED_POSITION) != Undefined)
        {
          const std::size_t baseSize = GetAreaSize(POSITIONS);
          if (baseSize == Undefined || 0 != baseSize % sizeof(RawPosEntry))
          {
            return false;
          }
          const std::size_t restSize = GetAreaSize(LOOPED_POSITION);
          return restSize != Undefined && restSize >= sizeof(RawPosEntry) + sizeof(RawPosEntry::Channel);
        }
        else
        {
          const std::size_t baseSize = GetAreaSize(POSITIONS);
          return baseSize != Undefined && baseSize >= sizeof(RawPosEntry) + sizeof(RawPosEntry::Channel);
        }
      }
    private:
      bool CheckSamplesOffsets() const
      {
        const std::size_t size = GetAreaSize(SAMPLES_OFFSETS);
        return size != Undefined && size >= sizeof(uint16_t);
      }

      bool CheckOrnamentsOffsets() const
      {
        const std::size_t size = GetAreaSize(ORNAMENTS_OFFSETS);
        return size != Undefined && size >= sizeof(uint16_t);
      }
    };

    bool FastCheck(const Areas& areas)
    {
      if (!areas.CheckHeader())
      {
        return false;
      }
      if (!areas.CheckSamples())
      {
        return false;
      }
      if (!areas.CheckOrnaments())
      {
        return false;
      }
      if (!areas.CheckPatterns())
      {
        return false;
      }
      if (!areas.CheckPositions())
      {
        return false;
      }
      return true;
    }

    Binary::TypedContainer CreateContainer(const Binary::Container& rawData)
    {
      return Binary::TypedContainer(rawData, std::min(rawData.Size(), MAX_MODULE_SIZE));
    }

    bool FastCheck(const Binary::TypedContainer& data)
    {
      const RawHeader* const hdr = data.GetField<RawHeader>(0);
      if (0 == hdr)
      {
        return false;
      }
      const std::size_t minSamplesAddr = offsetof(RawHeader, LoopPositionOffset);
      const std::size_t samplesAddr = fromLE(hdr->SamplesOffset);
      const std::size_t ornamentAddr = fromLE(hdr->OrnamentsOffset);
      const std::size_t patternAddr = fromLE(hdr->PatternsOffset);
      const std::size_t positionAddr = fromLE(hdr->PositionsOffset);
      const std::size_t loopPosAddr = fromLE(hdr->LoopPositionOffset);
      if (samplesAddr < minSamplesAddr
       || samplesAddr >= ornamentAddr
       || ornamentAddr > patternAddr
       || patternAddr >= positionAddr
       || positionAddr > loopPosAddr)
      {
        return false;
      }
      const std::size_t compileAddr = samplesAddr - minSamplesAddr;
      const Areas areas(data, compileAddr, data.GetSize());
      return FastCheck(areas);
    }

    bool FastCheck(const Binary::Container& rawData)
    {
      const Binary::TypedContainer data = CreateContainer(rawData);
      return FastCheck(data);
    }

    //TODO: size may be <256
    const std::string FORMAT(
      "?01-30"       //uint16_t Size;
      "?00|60-fb"    //uint16_t SamplesOffset;
      "?00|60-fb"    //uint16_t OrnamentsOffset;
      "?00|60-fb"    //uint16_t PatternsOffset;
      "?01-2d|61-ff" //uint16_t PositionsOffset;
      "?01-30|61-ff" //uint16_t LoopPositionOffset;
      //sample1 offset
      "?00|60-fb"
      //pattern1 offset minimal
      "?00-01|60-fc"
    );

    class Decoder : public Formats::Chiptune::Decoder
    {
    public:
      Decoder()
        : Format(Binary::Format::Create(FORMAT))
      {
      }

      virtual String GetDescription() const
      {
        return Text::SQTRACKER_DECODER_DESCRIPTION;
      }

      virtual Binary::Format::Ptr GetFormat() const
      {
        return Format;
      }

      virtual bool Check(const Binary::Container& rawData) const
      {
        return Format->Match(rawData) && FastCheck(rawData);
      }

      virtual Formats::Chiptune::Container::Ptr Decode(const Binary::Container& rawData) const
      {
        Builder& stub = GetStubBuilder();
        return ParseCompiled(rawData, stub);
      }
    private:
      const Binary::Format::Ptr Format;
    };

    Formats::Chiptune::Container::Ptr ParseCompiled(const Binary::Container& rawData, Builder& target)
    {
      const Binary::TypedContainer data = CreateContainer(rawData);

      if (!FastCheck(data))
      {
        return Formats::Chiptune::Container::Ptr();
      }

      try
      {
        const Format format(data);

        format.ParseCommonProperties(target);

        StatisticCollectingBuilder statistic(target);
        format.ParsePositions(statistic);
        const Indices& usedPatterns = statistic.GetUsedPatterns();
        format.ParsePatterns(usedPatterns, statistic);
        const Indices& usedSamples = statistic.GetUsedSamples();
        format.ParseSamples(usedSamples, statistic);
        const Indices& usedOrnaments = statistic.GetUsedOrnaments();
        format.ParseOrnaments(usedOrnaments, target);

        const Binary::Container::Ptr subData = rawData.GetSubcontainer(0, format.GetSize());
        const RangeChecker::Range fixedRange = format.GetFixedArea();
        return CreateCalculatingCrcContainer(subData, fixedRange.first, fixedRange.second - fixedRange.first);
      }
      catch (const std::exception&)
      {
        Dbg("Failed to create");
        return Formats::Chiptune::Container::Ptr();
      }
    }

    Builder& GetStubBuilder()
    {
      static StubBuilder stub;
      return stub;
    }
  }// namespace SQTracker

  Decoder::Ptr CreateSQTrackerDecoder()
  {
    return boost::make_shared<SQTracker::Decoder>();
  }
}// namespace Chiptune
}// namespace Formats
