/*
Abstract:
  Qt plugins registering function for X-Server based linux

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "plugins.h"
//qt includes
#include <QtCore/QtPlugin>

void LoadStaticQtPlugins()
{
  Q_IMPORT_PLUGIN(QXcbIntegrationPlugin);
}
