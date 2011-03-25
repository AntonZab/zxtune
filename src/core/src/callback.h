/*
Abstract:
  Module detection callback functions

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

#pragma once
#ifndef __CORE_PLUGINS_CALLBACK_H_DEFINED__
#define __CORE_PLUGINS_CALLBACK_H_DEFINED__

//local includes
#include "core/plugins/enumerator.h"
//common includes
#include <error.h>

namespace Log
{
  class ProgressCallback;
}

namespace ZXTune
{
  namespace Module
  {
    class DetectCallback
    {
    public:
      virtual ~DetectCallback() {}

      //! @brief Returns list of plugins enabled to be processed
      virtual PluginsEnumerator::Ptr GetUsedPlugins() const = 0;
      //! @brief Returns plugins parameters
      virtual Parameters::Accessor::Ptr GetPluginsParameters() const = 0;
      //! @brief Returns parameters for future module
      virtual Parameters::Accessor::Ptr CreateModuleParameters(const DataLocation& location) const = 0;
      //! @brief Process module
      virtual Error ProcessModule(const DataLocation& location, Module::Holder::Ptr holder) const = 0;
      //! @brief Logging callback
      virtual Log::ProgressCallback* GetProgress() const = 0;
    };

    // Helper classes
    class DetectCallbackDelegate : public DetectCallback
    {
    public:
      explicit DetectCallbackDelegate(const DetectCallback& delegate)
        : Delegate(delegate)
      {
      }

      virtual PluginsEnumerator::Ptr GetUsedPlugins() const
      {
        return Delegate.GetUsedPlugins();
      }
      
      virtual Parameters::Accessor::Ptr GetPluginsParameters() const
      {
        return Delegate.GetPluginsParameters();
      }

      virtual Parameters::Accessor::Ptr CreateModuleParameters(const DataLocation& location) const
      {
        return Delegate.CreateModuleParameters(location);
      }

      virtual Error ProcessModule(const DataLocation& location, Module::Holder::Ptr holder) const
      {
        return Delegate.ProcessModule(location, holder);
      }

      virtual Log::ProgressCallback* GetProgress() const
      {
        return Delegate.GetProgress();
      }
    protected:
      const DetectCallback& Delegate;
    };

    class NoProgressDetectCallbackAdapter : public DetectCallbackDelegate
    {
    public:
      explicit NoProgressDetectCallbackAdapter(const DetectCallback& delegate)
        : DetectCallbackDelegate(delegate)
      {
      }

      virtual Log::ProgressCallback* GetProgress() const
      {
        return 0;
      }
    };
  }
}

#endif //__CORE_PLUGINS_CALLBACK_H_DEFINED__