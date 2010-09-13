/*
Abstract:
  Format detector implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "detector.h"
//std includes
#include <cassert>
#include <cctype>

namespace
{
  inline uint8_t ToHex(char c)
  {
    assert(std::isxdigit(c));
    return std::isdigit(c) ? c - '0' : std::toupper(c) - 'A' + 10;
  }
}

bool DetectFormat(const uint8_t* data, std::size_t size, const std::string& pattern)
{
  for (std::string::const_iterator it = pattern.begin(), lim(pattern.end());
    0 != size && it != lim; ++data, ++it, --size)
  {
    const char sym(*it);
    if ('?' == sym)
    {
      continue;//skip
    }
    else if ('+' == sym)//skip
    {
      std::size_t skip(0);
      while ('+' != *++it)
      {
        assert((std::isdigit(*it) && it != lim) || !"Invalid pattern format");
        skip = skip * 10 + (*it - '0');
      }
      if (skip >= size)
      {
        return false;
      }
      --skip;
      size -= skip;
      data += skip;
    }
    else
    {
      ++it;
      assert(it != lim || !"Invalid pattern format");
      const char sym1(*it);
      const uint8_t byte(ToHex(sym) * 16 + ToHex(sym1));
      if (byte != *data)
      {
        return false;
      }
    }
  }
  return 0 != size;
}