/*
Abstract:
  Player plugins list

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#ifndef __CORE_PLUGINS_PLAYERS_LIST_H_DEFINED__
#define __CORE_PLUGINS_PLAYERS_LIST_H_DEFINED__

namespace ZXTune
{
  //forward declaration
  class PluginsEnumerator;
  
  void RegisterPSGSupport(PluginsEnumerator& enumerator);
  void RegisterSTCSupport(PluginsEnumerator& enumerator);
  void RegisterPT2Support(PluginsEnumerator& enumerator);
  void RegisterPT3Support(PluginsEnumerator& enumerator);
  void RegisterASCSupport(PluginsEnumerator& enumerator);
  void RegisterPDTSupport(PluginsEnumerator& enumerator);
  
  void RegisterPlayerPlugins(PluginsEnumerator& enumerator)
  {
    RegisterPT3Support(enumerator);
    RegisterPT2Support(enumerator);
    RegisterSTCSupport(enumerator);
    RegisterASCSupport(enumerator);
    RegisterPSGSupport(enumerator);
    RegisterPDTSupport(enumerator);
  }
}

#endif //__CORE_PLUGINS_PLAYERS_LIST_H_DEFINED__
