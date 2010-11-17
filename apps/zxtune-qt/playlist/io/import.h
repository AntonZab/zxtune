/*
Abstract:
  Playlist import interfaces

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

#ifndef ZXTUNE_QT_PLAYLIST_IMPORT_H_DEFINED
#define ZXTUNE_QT_PLAYLIST_IMPORT_H_DEFINED

//local includes
#include "container.h"
#include "supp/playitems_provider.h"

namespace Playlist
{
  const Char ATTRIBUTE_NAME[] = {'N', 'a', 'm', 'e', 0};
  const Char ATTRIBUTE_SIZE[] = {'S', 'i', 'z', 'e', 0};
}

PlaylistIOContainer::Ptr OpenPlaylist(PlayitemsProvider::Ptr provider, const class QString& filename);
PlaylistIOContainer::Ptr OpenAYLPlaylist(PlayitemsProvider::Ptr provider, const class QString& filename);
PlaylistIOContainer::Ptr OpenXSPFPlaylist(PlayitemsProvider::Ptr provider, const class QString& filename);

#endif //ZXTUNE_QT_PLAYLIST_IMPORT_H_DEFINED