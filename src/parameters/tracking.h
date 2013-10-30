/**
*
* @file
*
* @brief  Parameters tracking functionality
*
* @author vitamin.caig@gmail.com
*
**/

#pragma once

//library includes
#include <parameters/container.h>

namespace Parameters
{
  Container::Ptr CreatePreChangePropertyTrackedContainer(Container::Ptr delegate, Modifier& callback);
  Container::Ptr CreatePostChangePropertyTrackedContainer(Container::Ptr delegate, Modifier& callback);
}
