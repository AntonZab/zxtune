/*
Abstract:
  Common container plugins support implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "container_supp_common.h"
#include "core/src/callback.h"
#include "core/src/core.h"
#include "core/plugins/utils.h"
//common includes
#include <format.h>
#include <logging.h>
//boost includes
#include <boost/make_shared.hpp>
//text includes
#include <core/text/plugins.h>

namespace
{
  using namespace ZXTune;

  class LoggerHelper
  {
  public:
    LoggerHelper(uint_t total, const Module::DetectCallback& delegate, const Plugin& plugin, const String& path)
      : Total(total)
      , Delegate(delegate)
      , Progress(CreateProgressCallback(delegate, Total))
      , Id(plugin.Id())
      , Path(path)
      , Current()
    {
    }

    void operator()(const Container::File& cur)
    {
      if (Progress.get())
      {
        const String text = Path.empty()
          ? Strings::Format(Text::CONTAINER_PLUGIN_PROGRESS_NOPATH, Id, cur.GetName())
          : Strings::Format(Text::CONTAINER_PLUGIN_PROGRESS, Id, cur.GetName(), Path);
        Progress->OnProgress(Current, text);
      }
    }

    std::auto_ptr<Module::DetectCallback> CreateNestedCallback() const
    {
      Log::ProgressCallback* const parentProgress = Delegate.GetProgress();
      if (parentProgress && Total < 50)
      {
        Log::ProgressCallback::Ptr nestedProgress = CreateNestedPercentProgressCallback(Total, Current, *parentProgress);
        return std::auto_ptr<Module::DetectCallback>(new Module::CustomProgressDetectCallbackAdapter(Delegate, nestedProgress));
      }
      else
      {
        return std::auto_ptr<Module::DetectCallback>(new Module::CustomProgressDetectCallbackAdapter(Delegate));
      }
    }

    void Next()
    {
      ++Current;
    }
  private:
    const uint_t Total;
    const Module::DetectCallback& Delegate;
    const Log::ProgressCallback::Ptr Progress;
    const String Id;
    const String Path;
    uint_t Current;
  };

  class ContainerDetectCallback : public Container::Catalogue::Callback
  {
  public:
    ContainerDetectCallback(Plugin::Ptr descr, DataLocation::Ptr location, uint_t count, const Module::DetectCallback& callback)
      : BaseLocation(location)
      , Description(descr)
      , Logger(count, callback, *Description, BaseLocation->GetPath()->AsString())
    {
    }

    virtual void OnFile(const Container::File& file) const
    {
      Logger(file);
      if (const Binary::Container::Ptr subData = file.GetData())
      {
        const String subPath = file.GetName();
        const ZXTune::DataLocation::Ptr subLocation = CreateNestedLocation(BaseLocation, subData, Description, subPath);
        const std::auto_ptr<Module::DetectCallback> nestedProgressCallback = Logger.CreateNestedCallback();
        ZXTune::Module::Detect(subLocation, *nestedProgressCallback);
      }
    }
  private:
    const DataLocation::Ptr BaseLocation;
    const Plugin::Ptr Description;
    mutable LoggerHelper Logger;
  };

  class CommonContainerPlugin : public ArchivePlugin
  {
  public:
    CommonContainerPlugin(Plugin::Ptr descr, ContainerFactory::Ptr factory)
      : Description(descr)
      , Factory(factory)
    {
    }

    virtual Plugin::Ptr GetDescription() const
    {
      return Description;
    }

    virtual DetectionResult::Ptr Detect(DataLocation::Ptr input, const Module::DetectCallback& callback) const
    {
      const Binary::Container::Ptr rawData = input->GetData();
      if (const Container::Catalogue::Ptr files = Factory->CreateContainer(*callback.GetPluginsParameters(), rawData))
      {
        if (const uint_t count = files->GetFilesCount())
        {
          ContainerDetectCallback detect(Description, input, count, callback);
          files->ForEachFile(detect);
        }
        return DetectionResult::CreateMatched(files->GetSize());
      }
      return DetectionResult::CreateUnmatched(Factory->GetFormat(), rawData);
    }

    virtual DataLocation::Ptr Open(const Parameters::Accessor& commonParams, DataLocation::Ptr location, const DataPath& inPath) const
    {
      const Binary::Container::Ptr inData = location->GetData();
      if (const Container::Catalogue::Ptr files = Factory->CreateContainer(commonParams, inData))
      {
        if (const Container::File::Ptr fileToOpen = files->FindFile(inPath))
        {
          if (const Binary::Container::Ptr subData = fileToOpen->GetData())
          {
            return CreateNestedLocation(location, subData, Description, fileToOpen->GetName());
          }
        }
      }
      return DataLocation::Ptr();
    }
  private:
    const Plugin::Ptr Description;
    const ContainerFactory::Ptr Factory;
  };
}

namespace ZXTune
{
  ArchivePlugin::Ptr CreateContainerPlugin(const String& id, const String& info, const String& version, uint_t caps,
    ContainerFactory::Ptr factory)
  {
    const Plugin::Ptr description = CreatePluginDescription(id, info, version, caps);
    return ArchivePlugin::Ptr(new CommonContainerPlugin(description, factory));
  }
}
