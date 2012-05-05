/*
Abstract:
  OGG settings widget

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "ogg_settings.h"
#include "ogg_settings.ui.h"
#include "supp/options.h"
#include "ui/utils.h"
#include "ui/tools/parameters_helpers.h"
//common includes
#include <tools.h>
//library includes
#include <sound/backends_parameters.h>

namespace
{
  class OGGSettingsWidget : public UI::BackendSettingsWidget
                          , private Ui::OggSettings
  {
  public:
    explicit OGGSettingsWidget(QWidget& parent)
      : UI::BackendSettingsWidget(parent)
      , Options(GlobalOptions::Instance().Get())
    {
      //setup self
      setupUi(this);

      connect(selectQuality, SIGNAL(toggled(bool)), SIGNAL(SettingsChanged()));
      connect(qualityValue, SIGNAL(valueChanged(int)), SIGNAL(SettingsChanged()));
      connect(selectBitrate, SIGNAL(toggled(bool)), SIGNAL(SettingsChanged()));
      connect(bitrateValue, SIGNAL(valueChanged(int)), SIGNAL(SettingsChanged()));

      using namespace Parameters;
      ExclusiveValue::Bind(*selectQuality, *Options, ZXTune::Sound::Backends::Ogg::MODE,
        ZXTune::Sound::Backends::Ogg::MODE_QUALITY);
      IntegerValue::Bind(*qualityValue, *Options, ZXTune::Sound::Backends::Ogg::QUALITY,
        ZXTune::Sound::Backends::Ogg::QUALITY_DEFAULT);
      ExclusiveValue::Bind(*selectBitrate, *Options, ZXTune::Sound::Backends::Ogg::MODE,
        ZXTune::Sound::Backends::Ogg::MODE_ABR);
      IntegerValue::Bind(*bitrateValue, *Options, ZXTune::Sound::Backends::Ogg::BITRATE,
        ZXTune::Sound::Backends::Ogg::BITRATE_DEFAULT);
      //fixup
      if (!selectQuality->isChecked() && !selectBitrate->isChecked())
      {
        selectQuality->setChecked(true);
      }
    }

    virtual Parameters::Container::Ptr GetSettings() const
    {
      using namespace Parameters;
      const Container::Ptr result = Container::Create();
      CopyExistingValue<StringType>(*Options, *result, ZXTune::Sound::Backends::Ogg::MODE);
      CopyExistingValue<IntType>(*Options, *result, ZXTune::Sound::Backends::Ogg::QUALITY);
      CopyExistingValue<IntType>(*Options, *result, ZXTune::Sound::Backends::Ogg::BITRATE);
      return result;
    }

    virtual String GetBackendId() const
    {
      static const Char ID[] = {'o', 'g', 'g', '\0'};
      return ID;
    }

    virtual QString GetDescription() const
    {
      if (selectQuality->isChecked())
      {
        return QString("Quality %1").arg(qualityValue->value());
      }
      else if (selectBitrate->isChecked())
      {
        return QString("ABR %1 kbps").arg(bitrateValue->value());
      }
      else
      {
        return QString();
      }
    }
  private:
    const Parameters::Container::Ptr Options;
  };
}

namespace UI
{
  BackendSettingsWidget* CreateOGGSettingsWidget(QWidget& parent)
  {
    return new OGGSettingsWidget(parent);
  }
}