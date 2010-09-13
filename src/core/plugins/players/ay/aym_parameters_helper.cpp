/*
Abstract:
  AYM parameters helper implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "freq_tables_internal.h"
#include "aym_parameters_helper.h"
//common includes
#include <error_tools.h>
#include <tools.h>
//library includes
#include <core/error_codes.h>
#include <core/core_parameters.h>
#include <devices/aym.h>
//std includes
#include <numeric>
//text includes
#include <core/text/core.h>

#define FILE_TAG 6972CAAF

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::AYM;

  //duty-cycle related parameter: accumulate letters to bitmask functor
  inline uint8_t LetterToMask(uint8_t val, const Char letter)
  {
    static const Char LETTERS[] = {'A', 'B', 'C', 'N', 'E'};
    static const uint8_t MASKS[] =
    {
      DataChunk::DUTY_CYCLE_MASK_A,
      DataChunk::DUTY_CYCLE_MASK_B,
      DataChunk::DUTY_CYCLE_MASK_C,
      DataChunk::DUTY_CYCLE_MASK_N,
      DataChunk::DUTY_CYCLE_MASK_E
    };
    BOOST_STATIC_ASSERT(sizeof(LETTERS) / sizeof(*LETTERS) == sizeof(MASKS) / sizeof(*MASKS));
    const std::size_t pos = std::find(LETTERS, ArrayEnd(LETTERS), letter) - LETTERS;
    if (pos == ArraySize(LETTERS))
    {
      throw MakeFormattedError(THIS_LINE, Module::ERROR_INVALID_PARAMETERS,
        Text::MODULE_ERROR_INVALID_DUTY_CYCLE_MASK_ITEM, String(1, letter));
    }
    return val | MASKS[pos];
  }

  uint_t String2Layout(const String& str)
  {
    if (str == Text::MODULE_LAYOUT_ABC)
    {
      return LAYOUT_ABC;
    }
    else if (str == Text::MODULE_LAYOUT_ACB)
    {
      return LAYOUT_ACB;
    }
    else if (str == Text::MODULE_LAYOUT_BAC)
    {
      return LAYOUT_BAC;
    }
    else if (str == Text::MODULE_LAYOUT_BCA)
    {
      return LAYOUT_BCA;
    }
    else if (str == Text::MODULE_LAYOUT_CBA)
    {
      return LAYOUT_CBA;
    }
    else if (str == Text::MODULE_LAYOUT_CAB)
    {
      return LAYOUT_CAB;
    }
    else
    {
      throw MakeFormattedError(THIS_LINE, Module::ERROR_INVALID_PARAMETERS,
        Text::MODULE_ERROR_INVALID_LAYOUT, str);
    }
  }

  class ParametersHelperImpl : public ParametersHelper
  {
  public:
    explicit ParametersHelperImpl(const String& freqTableName)
      : Chunk()
    {
      ThrowIfError(Module::GetFreqTable(freqTableName, FreqTable));
    }

    virtual void SetParameters(const Parameters::Map& params)
    {
      Parameters::IntType intParam = 0;
      Parameters::StringType strParam;
      // chip type parameter
      if (Parameters::FindByName(params, Parameters::ZXTune::Core::AYM::TYPE, intParam))
      {
        if (intParam)
        {
          Chunk.Mask |= AYM::DataChunk::YM_CHIP;
        }
        else
        {
          Chunk.Mask &= ~AYM::DataChunk::YM_CHIP;
        }
      }
      // interpolation parameter
      if (Parameters::FindByName(params, Parameters::ZXTune::Core::AYM::INTERPOLATION, intParam))
      {
        if (intParam)
        {
          Chunk.Mask |= AYM::DataChunk::INTERPOLATE;
        }
        else
        {
          Chunk.Mask &= ~AYM::DataChunk::INTERPOLATE;
        }
      }
      // freqtable parameter
      if (Parameters::FindByName(params, Parameters::ZXTune::Core::AYM::TABLE, strParam))
      {
        // as name
        ThrowIfError(Module::GetFreqTable(strParam, FreqTable));
      }
      else if (const Parameters::DataType* const table = Parameters::FindByName<Parameters::DataType>(params,
        Parameters::ZXTune::Core::AYM::TABLE))
      {
        // as dump
        if (table->size() != FreqTable.size() * sizeof(FreqTable.front()))
        {
          throw MakeFormattedError(THIS_LINE, Module::ERROR_INVALID_PARAMETERS,
            Text::MODULE_ERROR_INVALID_FREQ_TABLE_SIZE, table->size());
        }
        std::memcpy(&FreqTable.front(), &table->front(), table->size());
      }
      // duty cycle value parameter
      if (Parameters::FindByName(params, Parameters::ZXTune::Core::AYM::DUTY_CYCLE, intParam))
      {
        //duty cycle in percents should be in range 1..99 inc
        if (intParam < 1 || intParam > 99)
        {
          throw MakeFormattedError(THIS_LINE, Module::ERROR_INVALID_PARAMETERS,
            Text::MODULE_ERROR_INVALID_DUTY_CYCLE, intParam);
        }
        Chunk.Data[DataChunk::PARAM_DUTY_CYCLE] = static_cast<uint8_t>(intParam);
      }
      // duty cycle mask parameter
      if (Parameters::FindByName(params, Parameters::ZXTune::Core::AYM::DUTY_CYCLE_MASK, strParam))
      {
        // as string mask
        Chunk.Data[DataChunk::PARAM_DUTY_CYCLE_MASK] = std::accumulate(strParam.begin(), strParam.end(), 0, LetterToMask);
      }
      else if (Parameters::FindByName(params, Parameters::ZXTune::Core::AYM::DUTY_CYCLE_MASK, intParam))
      {
        // as integer mask
        Chunk.Data[DataChunk::PARAM_DUTY_CYCLE_MASK] = static_cast<uint8_t>(intParam);
      }
      // layout parameter
      if (Parameters::FindByName(params, Parameters::ZXTune::Core::AYM::LAYOUT, strParam))
      {
        // as string mask
        Chunk.Data[DataChunk::PARAM_LAYOUT] = String2Layout(strParam);
      }
      else if (Parameters::FindByName(params, Parameters::ZXTune::Core::AYM::LAYOUT, intParam))
      {
        if (intParam < static_cast<int_t>(LAYOUT_ABC) || 
            intParam >= static_cast<int_t>(LAYOUT_LAST))
        {
          throw MakeFormattedError(THIS_LINE, Module::ERROR_INVALID_PARAMETERS,
            Text::MODULE_ERROR_INVALID_LAYOUT, intParam);
        }
      }
    }

    virtual const Module::FrequencyTable& GetFreqTable() const
    {
      return FreqTable;
    }

    virtual void GetDataChunk(DataChunk& dst) const
    {
      dst = Chunk;
    }

  private:
    Module::FrequencyTable FreqTable;
    DataChunk Chunk;
  };
}

namespace ZXTune
{
  namespace AYM
  {
    ParametersHelper::Ptr ParametersHelper::Create(const String& defaultFreqTable)
    {
      return ParametersHelper::Ptr(new ParametersHelperImpl(defaultFreqTable));
    }
  }
}