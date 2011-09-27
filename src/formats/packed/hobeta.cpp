/*
Abstract:
  Hobeta support

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "container.h"
//common includes
#include <byteorder.h>
#include <tools.h>
//library includes
#include <formats/packed.h>
//std includes
#include <numeric>

namespace Hobeta
{
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

  bool Check(const void* rawData, std::size_t limit)
  {
    if (limit < sizeof(Header))
    {
      return false;
    }
    const uint8_t* const data = static_cast<const uint8_t*>(rawData);
    const Header* const header = static_cast<const Header*>(rawData);
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

  const std::string FORMAT(
    //Filename
    "20-7a 20-7a 20-7a 20-7a 20-7a 20-7a 20-7a 20-7a 20-7a"
    //Start
    "??"
    //Length
    "?01-ff"
    //FullLength
    "0001-ff"
  );
}

namespace Formats
{
  namespace Packed
  {
    class HobetaDecoder : public Decoder
    {
    public:
      HobetaDecoder()
        : Format(Binary::Format::Create(Hobeta::FORMAT))
      {
      }

      virtual Binary::Format::Ptr GetFormat() const
      {
        return Format;
      }

      virtual bool Check(const Binary::Container& rawData) const
      {
        const void* const data = rawData.Data();
        const std::size_t availSize = rawData.Size();
        return Format->Match(data, availSize) && Hobeta::Check(data, availSize);
      }

      virtual Formats::Packed::Container::Ptr Decode(const Binary::Container& rawData) const
      {
        const uint8_t* const data = static_cast<const uint8_t*>(rawData.Data());
        const std::size_t availSize = rawData.Size();
        if (!Hobeta::Check(data, availSize))
        {
          return Formats::Packed::Container::Ptr();
        }
        const Hobeta::Header* const header = safe_ptr_cast<const Hobeta::Header*>(data);
        const std::size_t dataSize = fromLE(header->Length);
        const std::size_t fullSize = fromLE(header->FullLength);
        const Binary::Container::Ptr subdata = rawData.GetSubcontainer(sizeof(*header), dataSize);
        return CreatePackedContainer(subdata, fullSize + sizeof(*header));
      }
    private:
      const Binary::Format::Ptr Format;
    };

    Decoder::Ptr CreateHobetaDecoder()
    {
      return boost::make_shared<HobetaDecoder>();
    }
  }
}
