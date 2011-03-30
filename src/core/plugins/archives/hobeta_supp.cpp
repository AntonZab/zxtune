/*
Abstract:
  Hobeta convertors support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include <core/plugins/registrator.h>
//common includes
#include <byteorder.h>
#include <tools.h>
//library includes
#include <core/plugin_attrs.h>
#include <io/container.h>
//std includes
#include <numeric>
//boost includes
#include <boost/make_shared.hpp>
//text includes
#include <core/text/plugins.h>

#define FILE_TAG 1CF1A62A

namespace
{
  using namespace ZXTune;

  const Char HOBETA_PLUGIN_ID[] = {'H', 'O', 'B', 'E', 'T', 'A', '\0'};
  const String HOBETA_PLUGIN_VERSION(FromStdString("$Rev$"));

#ifdef USE_PRAGMA_PACK
#pragma pack(push,1)
#endif
  PACK_PRE struct Header
  {
    uint8_t Filename[9];
    uint16_t Start;
    uint16_t Length;
    uint16_t FullLength;
    uint16_t CRC;
  } PACK_POST;
#ifdef USE_PRAGMA_PACK
#pragma pack(pop)
#endif

  BOOST_STATIC_ASSERT(sizeof(Header) == 17);
  const std::size_t HOBETA_MIN_SIZE = 0x100;
  const std::size_t HOBETA_MAX_SIZE = 0xff00;

  bool CheckHobeta(const IO::DataContainer& inputData)
  {
    const std::size_t limit = inputData.Size();
    if (limit < sizeof(Header))
    {
      return false;
    }
    const uint8_t* const data = static_cast<const uint8_t*>(inputData.Data());
    const Header* const header = safe_ptr_cast<const Header*>(data);
    const std::size_t dataSize = fromLE(header->Length);
    const std::size_t fullSize = fromLE(header->FullLength);
    if (dataSize < HOBETA_MIN_SIZE ||
        dataSize > HOBETA_MAX_SIZE ||
        dataSize + sizeof(*header) > limit ||
        fullSize != align<std::size_t>(dataSize, 256) ||
        //check for valid name
        ArrayEnd(header->Filename) != std::find_if(header->Filename, ArrayEnd(header->Filename),
          std::bind2nd(std::less<uint8_t>(), uint8_t(' ')))
        )
    {
      return false;
    }
    //check for crc
    if (fromLE(header->CRC) == ((105 + 257 * std::accumulate(data, data + 15, 0u)) & 0xffff))
    {
      return true;
    }
    return false;
  }

  class HobetaExtractionResult : public ArchiveExtractionResult
  {
  public:
    explicit HobetaExtractionResult(IO::DataContainer::Ptr data)
      : RawData(data)
      , PackedSize(0)
    {
    }

    virtual std::size_t GetMatchedDataSize() const
    {
      TryToExtract();
      return PackedSize;
    }

    virtual std::size_t GetLookaheadOffset() const
    {
      const uint_t size = RawData->Size();
      if (size < sizeof(Header))
      {
        return size;
      }
      const uint8_t* const begin = static_cast<const uint8_t*>(RawData->Data());
      const uint8_t* const end = begin + size;
      return std::search_n(begin, end, 9, uint8_t(' '), std::greater<uint8_t>()) - begin;
    }

    virtual IO::DataContainer::Ptr GetExtractedData() const
    {
      TryToExtract();
      return ExtractedData;
    }
  private:
    void TryToExtract() const
    {
      if (PackedSize || !CheckHobeta(*RawData))
      {
        return;
      }
      const Header* const header = safe_ptr_cast<const Header*>(RawData->Data());
      const std::size_t dataSize = fromLE(header->Length);
      const std::size_t fullSize = fromLE(header->FullLength);
      PackedSize = fullSize + sizeof(*header);
      ExtractedData = RawData->GetSubcontainer(sizeof(*header), dataSize);
    }
  private:
    const IO::DataContainer::Ptr RawData;
    mutable std::size_t PackedSize;
    mutable IO::DataContainer::Ptr ExtractedData;
  };

  //////////////////////////////////////////////////////////////////////////
  class HobetaPlugin : public ArchivePlugin
  {
  public:
    virtual String Id() const
    {
      return HOBETA_PLUGIN_ID;
    }

    virtual String Description() const
    {
      return Text::HOBETA_PLUGIN_INFO;
    }

    virtual String Version() const
    {
      return HOBETA_PLUGIN_VERSION;
    }
    
    virtual uint_t Capabilities() const
    {
      return CAP_STOR_CONTAINER | CAP_STOR_PLAIN;
    }

    virtual bool Check(const IO::DataContainer& inputData) const
    {
      return CheckHobeta(inputData);
    }

    virtual ArchiveExtractionResult::Ptr ExtractSubdata(const Parameters::Accessor& /*parameters*/,
      IO::DataContainer::Ptr input) const
    {
      return boost::make_shared<HobetaExtractionResult>(input);
    }
  };
}

namespace ZXTune
{
  void RegisterHobetaConvertor(PluginsRegistrator& registrator)
  {
    const ArchivePlugin::Ptr plugin(new HobetaPlugin());
    registrator.RegisterPlugin(plugin);
  }
}
