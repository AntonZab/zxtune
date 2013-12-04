/**
*
* @file
*
* @brief  Format detector implementation
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "details.h"
#include "expression.h"
//common includes
#include <contract.h>
//library includes
#include <binary/format_factories.h>
#include <math/numeric.h>
//std includes
#include <limits>
#include <vector>
//boost includes
#include <boost/array.hpp>
#include <boost/make_shared.hpp>
#include <boost/ref.hpp>

namespace
{
  using namespace Binary;

  class StaticToken
  {
  public:
    StaticToken(Token::Ptr tok)
      : Mask()
      , Count()
      , Last()
    {
      for (uint_t idx = 0; idx != 256; ++idx)
      {
        if (tok->Match(idx))
        {
          Set(idx);
        }
      }
    }

    explicit StaticToken(uint_t val)
      : Mask()
      , Count()
      , Last()
    {
      Set(val);
    }

    bool Match(uint_t val) const
    {
      return Get(val);
    }

    bool IsAny() const
    {
      return Count == 256;
    }

    const uint_t* GetSingle() const
    {
      return Count == 1
        ? &Last
        : 0;
    }

    static bool AreIntersected(const StaticToken& lh, const StaticToken& rh)
    {
      if (lh.IsAny() || rh.IsAny())
      {
        return true;
      }
      else
      {
        for (uint_t idx = 0; idx != ElementsCount; ++idx)
        {
          if (0 != (lh.Mask[idx] & rh.Mask[idx]))
          {
            return true;
          }
        }
        return false;
      }
    }
  private:
    void Set(uint_t idx)
    {
      Require(!Get(idx));
      const uint_t bit = idx % BitsPerElement;
      const std::size_t offset = idx / BitsPerElement;
      const ElementType mask = ElementType(1) << bit;
      Mask[offset] |= mask;
      ++Count;
      Last = idx;
    }

    bool Get(uint_t idx) const
    {
      const uint_t bit = idx % BitsPerElement;
      const std::size_t offset = idx / BitsPerElement;
      const ElementType mask = ElementType(1) << bit;
      return 0 != (Mask[offset] & mask);
    }
  private:
    typedef uint_t ElementType;
    static const std::size_t BitsPerElement = 8 * sizeof(ElementType);
    static const std::size_t ElementsCount = 256 / BitsPerElement;
    boost::array<ElementType, ElementsCount> Mask;
    uint_t Count;
    uint_t Last;
  };

  class StaticPattern
  {
  public:
    explicit StaticPattern(ObjectIterator<Token::Ptr>::Ptr iter)
    {
      for (; iter->IsValid(); iter->Next())
      {
        Data.push_back(StaticToken(iter->Get()));
      }
    }

    std::size_t GetSize() const
    {
      return Data.size();
    }

    const StaticToken& Get(std::size_t idx) const
    {
      return Data[idx];
    }

    //return back offset
    std::size_t FindSuffix(std::size_t suffixSize) const
    {
      const StaticToken* const suffixBegin = End() - suffixSize;
      const StaticToken* const suffixEnd = End();
      const std::size_t patternSize = GetSize();
      const std::size_t startOffset = 1;
      const StaticToken* start = suffixBegin - startOffset;
      for (std::size_t offset = startOffset; ; ++offset, --start)
      {
        const std::size_t windowSize = suffixSize + offset;
        if (patternSize >= windowSize)
        {
          //pattern:  ......sssssss
          //suffix:        xssssss
          //offset=1
          if (std::equal(suffixBegin, suffixEnd, start, &StaticToken::AreIntersected))
          {
            return offset;
          }
        }
        else
        {
          if (patternSize == offset)
          {
            //all suffix is out of pattern
            return offset;
          }
          //pattern:   .......
          //suffix:  xssssss
          //out of pattern=2
          const std::size_t outOfPattern = windowSize - patternSize;
          if (std::equal(suffixBegin + outOfPattern, suffixEnd, Begin(), &StaticToken::AreIntersected))
          {
            return offset;
          }
        }
      }
    }

    std::size_t FindPrefix(std::size_t prefixSize) const
    {
      const StaticToken* const prefixBegin = Begin();
      const StaticToken* const prefixEnd = Begin() + prefixSize;
      const std::size_t patternSize = GetSize();
      const std::size_t startOffset = 1;
      const StaticToken* start = prefixBegin + startOffset;
      for (std::size_t offset = startOffset; ; ++offset, ++start)
      {
        const std::size_t windowSize = prefixSize + offset;
        if (patternSize >= windowSize)
        {
          //pattern: ssssss...
          //prefix:   sssssx
          //offset=1
          if (std::equal(prefixBegin, prefixEnd, start, &StaticToken::AreIntersected))
          {
            return offset;
          }
        }
        else
        {
          if (patternSize == offset)
          {
            //all prefix is out of patter
            return offset;
          }
          //pattern: ....
          //prefix:    sssx
          //out of pattern=2
          const std::size_t outOfPattern = windowSize - patternSize;
          if (std::equal(prefixBegin, prefixEnd - outOfPattern, start, &StaticToken::AreIntersected))
          {
            return offset;
          }
        }
      }
    }
  private:
    const StaticToken* Begin() const
    {
      return &Data.front();
    }

    const StaticToken* End() const
    {
      return &Data.back() + 1;
    }
  private:
    std::vector<StaticToken> Data;
  };

  class FuzzyFormat : public FormatDetails
  {
  public:
    typedef boost::array<uint8_t, 256> PatternRow;
    typedef std::vector<PatternRow> PatternMatrix;

    FuzzyFormat(const PatternMatrix& mtx, std::size_t offset, std::size_t minSize, std::size_t minScanStep)
      : Offset(offset)
      , MinSize(std::max(minSize, mtx.size() + offset))
      , MinScanStep(minScanStep)
      , Pat(mtx.rbegin(), mtx.rend())
      , PatRBegin(&Pat[0])
      , PatREnd(PatRBegin + Pat.size())
    {
    }

    virtual bool Match(const Data& data) const
    {
      if (data.Size() < MinSize)
      {
        return false;
      }
      const std::size_t endOfPat = Offset + Pat.size();
      const uint8_t* typedDataLast = static_cast<const uint8_t*>(data.Start()) + endOfPat - 1;
      return 0 == SearchBackward(typedDataLast);
    }

    virtual std::size_t NextMatchOffset(const Data& data) const
    {
      const std::size_t size = data.Size();
      if (size < MinSize)
      {
        return size;
      }
      const uint8_t* const typedData = static_cast<const uint8_t*>(data.Start());
      const std::size_t endOfPat = Offset + Pat.size();
      const uint8_t* const scanStart = typedData + endOfPat - 1;
      const uint8_t* const scanStop = typedData + size;
      const std::size_t firstMatch = SearchBackward(scanStart);
      const std::size_t initialOffset = firstMatch != 0 ? firstMatch : MinScanStep;
      for (const uint8_t* scanPos = scanStart + initialOffset; scanPos < scanStop; )
      {
        if (const std::size_t offset = SearchBackward(scanPos))
        {
          scanPos += offset;
        }
        else
        {
          return scanPos - scanStart;
        }
      }
      return size;
    }

    virtual std::size_t GetMinSize() const
    {
      return MinSize;
    }

    static Ptr Create(const StaticPattern& pattern, std::size_t startOffset, std::size_t minSize)
    {
      const std::size_t patternSize = pattern.GetSize();
      PatternMatrix tmp(patternSize);
      for (uint_t sym = 0; sym != 256; ++sym)
      {
        for (std::size_t pos = 0, offset = 1; pos != patternSize; ++pos, ++offset)
        {
          const StaticToken& tok = pattern.Get(pos);
          if (tok.Match(sym))
          {
            offset = 0;
          }
          else
          {
            Require(Math::InRange<std::size_t>(offset, 0, std::numeric_limits<PatternRow::value_type>::max()));
            tmp[pos][sym] = static_cast<PatternRow::value_type>(offset);
          }
        }
      }
      /*
       Increase offset using suffixes.
       Average offsets (standard big file test):
         without increasing: 2.17
         with increasing: 2.27
         with precise increasing: 2.28

       Due to high complexity of precise detection, simple increasing is used
      */
      for (std::size_t pos = 0; pos != patternSize - 1; ++pos)
      {
        FuzzyFormat::PatternRow& row = tmp[pos];
        const std::size_t suffixLen = patternSize - pos - 1;
        const std::size_t offset = pattern.FindSuffix(suffixLen);
        const std::size_t availOffset = std::min<std::size_t>(offset, std::numeric_limits<PatternRow::value_type>::max());
        for (uint_t sym = 0; sym != 256; ++sym)
        {
          if (uint8_t& curOffset = row[sym])
          {
            curOffset = std::max(curOffset, static_cast<PatternRow::value_type>(availOffset));
          }
        }
      }
      //Each matrix element specifies forward movement of reversily matched pattern for specified symbol. =0 means symbol match
      const std::size_t minScanStep = pattern.FindPrefix(patternSize);
      return boost::make_shared<FuzzyFormat>(boost::cref(tmp), startOffset, minSize, minScanStep);
    }
  private:
    std::size_t SearchBackward(const uint8_t* data) const
    {
      if (const std::size_t offset = (*PatRBegin)[*data])
      {
        return offset;
      }
      --data;
      for (const PatternRow* it = PatRBegin + 1; it != PatREnd; ++it, --data)
      {
        const PatternRow& row = *it;
        if (const std::size_t offset = row[*data])
        {
          return offset;
        }
      }
      return 0;
    }
  private:
    const std::size_t Offset;
    const std::size_t MinSize;
    const std::size_t MinScanStep;
    const PatternMatrix Pat;
    const PatternRow* const PatRBegin;
    const PatternRow* const PatREnd;
  };

  class ExactFormat : public FormatDetails
  {
  public:
    typedef std::vector<uint8_t> PatternMatrix;

    ExactFormat(const PatternMatrix& mtx, std::size_t offset, std::size_t minSize)
      : Offset(offset)
      , MinSize(std::max(minSize, mtx.size() + offset))
      , Pattern(mtx)
    {
    }

    virtual bool Match(const Data& data) const
    {
      if (data.Size() < MinSize)
      {
        return false;
      }
      const uint8_t* const patternStart = &Pattern.front();
      const uint8_t* const patternEnd = patternStart + Pattern.size();
      const uint8_t* const typedDataStart = static_cast<const uint8_t*>(data.Start()) + Offset;
      return std::equal(patternStart, patternEnd, typedDataStart);
    }

    virtual std::size_t NextMatchOffset(const Data& data) const
    {
      const std::size_t size = data.Size();
      if (size < MinSize)
      {
        return size;
      }
      const uint8_t* const patternStart = &Pattern.front();
      const uint8_t* const patternEnd = patternStart + Pattern.size();
      const uint8_t* const typedDataStart = static_cast<const uint8_t*>(data.Start());
      const uint8_t* const typedDataEnd = typedDataStart + size;
      const uint8_t* const matched = std::search(typedDataStart + Offset + 1, typedDataEnd, patternStart, patternEnd);
      return matched != typedDataEnd
        ? matched - typedDataStart - Offset
        : size;
    }

    virtual std::size_t GetMinSize() const
    {
      return MinSize;
    }

    static Ptr TryCreate(const StaticPattern& pattern, std::size_t startOffset, std::size_t minSize)
    {
      const std::size_t patternSize = pattern.GetSize();
      PatternMatrix tmp(patternSize);
      for (std::size_t idx = 0; idx != patternSize; ++idx)
      {
        const StaticToken& tok = pattern.Get(idx);
        if (const uint_t* single = tok.GetSingle())
        {
          tmp[idx] = *single;
        }
        else
        {
          return Ptr();
        }
      }
      return boost::make_shared<ExactFormat>(boost::cref(tmp), startOffset, minSize);
    }
  private:
    const std::size_t Offset;
    const std::size_t MinSize;
    const PatternMatrix Pattern;
  };

  Format::Ptr CreateFormatFromTokens(const Expression& expr, std::size_t minSize)
  {
    const StaticPattern pattern(expr.Tokens());
    const std::size_t startOffset = expr.StartOffset();
    if (const Format::Ptr exact = ExactFormat::TryCreate(pattern, startOffset, minSize))
    {
      return exact;
    }
    else
    {
      return FuzzyFormat::Create(pattern, startOffset, minSize);
    }
  }
}

namespace Binary
{
  Format::Ptr CreateFormat(const std::string& pattern)
  {
    return CreateFormat(pattern, 0);
  }

  Format::Ptr CreateFormat(const std::string& pattern, std::size_t minSize)
  {
    const Expression::Ptr expr = Expression::Parse(pattern);
    return CreateFormatFromTokens(*expr, minSize);
  }
}