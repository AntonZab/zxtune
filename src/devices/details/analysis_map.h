/**
* 
* @file
*
* @brief  PSG-based devices analysis helper
*
* @author vitamin.caig@gmail.com
*
**/

#pragma once

//local includes
#include "freq_table.h"
//boost includes
#include <boost/array.hpp>

namespace Devices
{
  namespace Details
  {
    class AnalysisMap
    {
    public:
      AnalysisMap()
        : ClockRate()
      {
      }

      void SetClockRate(uint64_t clock)
      {
        if (ClockRate == clock)
        {
          return;
        }
        ClockRate = clock;
        for (uint_t halftone = 0; halftone != Details::FreqTable::SIZE; ++halftone)
        {
          const Details::Frequency freq = Details::FreqTable::GetHalftoneFrequency(halftone);
          const uint_t period = static_cast<uint_t>(clock * freq.PRECISION / freq.Raw());
          Lookup[Details::FreqTable::SIZE - halftone - 1] = period;
        }
      }
      
      uint_t GetBandByPeriod(uint_t period) const
      {
        const uint_t maxBand = static_cast<uint_t>(Lookup.size() - 1);
        const uint_t currentBand = static_cast<uint_t>(Lookup.end() - std::lower_bound(Lookup.begin(), Lookup.end(), period));
        return std::min(currentBand, maxBand);
      }
    private:
      uint64_t ClockRate;
      typedef boost::array<uint_t, FreqTable::SIZE> NoteTable;
      NoteTable Lookup;
    };
  }
}
