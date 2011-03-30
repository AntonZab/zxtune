/*
Abstract:
  Module detect helper implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "core/src/location.h"
#include "detect_helper.h"
#include "enumerator.h"
//common includes
#include <detector.h>

namespace
{
  using namespace ZXTune;

  class FastDataContainer : public IO::DataContainer
  {
  public:
    FastDataContainer(const IO::DataContainer& delegate, std::size_t offset)
      : Delegate(delegate)
      , Offset(offset)
    {
    }

    virtual std::size_t Size() const
    {
      return Delegate.Size() - Offset;
    }

    virtual const void* Data() const
    {
      return static_cast<const uint8_t*>(Delegate.Data()) + Offset;
    }

    virtual Ptr GetSubcontainer(std::size_t offset, std::size_t size) const
    {
      return Delegate.GetSubcontainer(offset + Offset, size);
    }
  private:
    const IO::DataContainer& Delegate;
    const std::size_t Offset;
  };
}

namespace ZXTune
{
  bool CheckDataFormat(const DataDetector& detector, const IO::DataContainer& inputData)
  {
    const uint8_t* const data = static_cast<const uint8_t*>(inputData.Data());
    const std::size_t limit = inputData.Size();

    //detect without
    if (detector.CheckData(data, limit))
    {
      return true;
    }
    for (DataPrefixIterator chain = detector.GetPrefixes(); chain; ++chain)
    {
      const std::size_t dataOffset = chain->PrefixSize;
      if (limit > chain->PrefixSize &&
          chain->Prefix->Match(data, dataOffset) &&
          detector.CheckData(data + dataOffset, limit - dataOffset))
      {
        return true;
      }
    }
    return false;
  }

  Module::Holder::Ptr CreateModuleFromData(const ModuleDetector& detector,
    Parameters::Accessor::Ptr parameters, DataLocation::Ptr location, std::size_t& usedSize)
  {
    const IO::DataContainer::Ptr data = location->GetData();
    const std::size_t limit(data->Size());
    const uint8_t* const rawData(static_cast<const uint8_t*>(data->Data()));

    //try to detect without player
    if (detector.CheckData(rawData, limit))
    {
      if (Module::Holder::Ptr holder = detector.TryToCreateModule(parameters, location, usedSize))
      {
        return holder;
      }
    }
    for (DataPrefixIterator chain = detector.GetPrefixes(); chain; ++chain)
    {
      const std::size_t dataOffset = chain->PrefixSize;
      if (limit < dataOffset ||
          !chain->Prefix->Match(rawData, dataOffset) ||
          !detector.CheckData(rawData + dataOffset, limit - dataOffset))
      {
        continue;
      }
      const IO::DataContainer::Ptr subData = data->GetSubcontainer(dataOffset, limit - dataOffset);
      const DataLocation::Ptr subLocation = CreateNestedLocation(location, subData);
      std::size_t moduleSize = 0;
      if (Module::Holder::Ptr holder = detector.TryToCreateModule(parameters, subLocation, moduleSize))
      {
        usedSize = dataOffset + moduleSize;
        return holder;
      }
    }
    return Module::Holder::Ptr();
  }
}
