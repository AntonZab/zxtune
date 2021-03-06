/**
* 
* @file
*
* @brief  Song length database implementation
*
* @author vitamin.caig@gmail.com
*
**/

//local includes
#include "songlengths.h"
//common includes
#include <contract.h>
#include <crc.h>
#include <pointers.h>
//boost includes
#include <boost/range/end.hpp>

namespace Module
{
namespace Sid
{
  struct SongEntry
  {
    const uint32_t HashCrc32;
    const unsigned Seconds;
    
    bool operator < (uint32_t hashCrc32) const
    {
      return HashCrc32 < hashCrc32;
    }
  };
  
  const SongEntry SONGS[] =
  {
#include "songlengths_db.inc"
  };

  typedef Time::Stamp<uint_t, 1> Seconds;

  const Seconds DEFAULT_SONG_LENGTH(120);

  TimeType GetSongLength(const char* md5digest, uint_t idx)
  {
    const uint32_t hashCrc32 = Crc32(safe_ptr_cast<const uint8_t*>(md5digest), 32);
    const SongEntry* const end = boost::end(SONGS);
    const SongEntry* const lower = std::lower_bound(SONGS, end, hashCrc32);
    if (lower + idx < end && lower->HashCrc32 == hashCrc32)
    {
      const SongEntry* const entry = lower + idx;
      if (lower->HashCrc32 == entry->HashCrc32)
      {
        return Seconds(entry->Seconds);
      }
    }
    return DEFAULT_SONG_LENGTH;
  }
}//namespace Sid
}//namespace Module
