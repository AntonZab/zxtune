/*
Abstract:
  Conversion setup dialog

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "setup_conversion.h"
#include "setup_conversion.ui.h"
#include "filename_template.h"
#include "supported_formats.h"
#include "mp3_settings.h"
#include "ogg_settings.h"
#include "flac_settings.h"
#include "supp/options.h"
#include "ui/state.h"
#include "ui/utils.h"
#include "ui/tools/parameters_helpers.h"
//library includes
#include <io/providers_parameters.h>
#include <sound/backends_parameters.h>
//boost includes
#include <boost/bind.hpp>
//qt includes
#include <QtCore/QThread>
#include <QtGui/QCloseEvent>
#include <QtGui/QPushButton>

namespace
{
  enum
  {
    TEMPLATE_PAGE = 0,
    FORMAT_PAGE = 1,
    SETTINGS_PAGE = 2
  };

  const uint_t MULTITHREAD_BUFFERS_COUNT = 1000;//20 sec usually

  bool HasMultithreadEnvironment()
  {
    return QThread::idealThreadCount() > 1;
  }

  class SetupConversionDialogImpl : public UI::SetupConversionDialog
                                  , private UI::Ui_SetupConversionDialog
  {
  public:
    explicit SetupConversionDialogImpl(QWidget& parent)
      : UI::SetupConversionDialog(parent)
      , Options(GlobalOptions::Instance().Get())
      , TargetTemplate(UI::FilenameTemplateWidget::Create(*this))
      , TargetFormat(UI::SupportedFormatsWidget::Create(*this))
    {
      //setup self
      setupUi(this);
      State = UI::State::Create(*this);
      toolBox->insertItem(TEMPLATE_PAGE, TargetTemplate, QString());
      toolBox->insertItem(FORMAT_PAGE, TargetFormat, QString());

      AddBackendSettingsWidget(&UI::CreateMP3SettingsWidget);
      AddBackendSettingsWidget(&UI::CreateOGGSettingsWidget);
      AddBackendSettingsWidget(&UI::CreateFLACSettingsWidget);

      connect(TargetTemplate, SIGNAL(SettingsChanged()), SLOT(UpdateDescriptions()));
      connect(TargetFormat, SIGNAL(SettingsChanged()), SLOT(UpdateDescriptions()));

      connect(buttonBox, SIGNAL(accepted()), SLOT(accept()));
      connect(buttonBox, SIGNAL(rejected()), SLOT(reject()));

      toolBox->setCurrentIndex(TEMPLATE_PAGE);
      useMultithreading->setEnabled(HasMultithreadEnvironment());
      using namespace Parameters;
      BooleanValue::Bind(*useMultithreading, *Options, ZXTune::Sound::Backends::File::BUFFERS, false, MULTITHREAD_BUFFERS_COUNT);
      BooleanValue::Bind(*overwriteExisting, *Options, ZXTune::IO::Providers::File::OVERWRITE_EXISTING, ZXTune::IO::Providers::File::OVERWRITE_EXISTING_DEFAULT);
      BooleanValue::Bind(*createDirectories, *Options, ZXTune::IO::Providers::File::CREATE_DIRECTORIES, ZXTune::IO::Providers::File::CREATE_DIRECTORIES_DEFAULT);

      UpdateDescriptions();
      State->Load();
    }

    virtual Parameters::Accessor::Ptr Execute(String& type)
    {
      if (exec())
      {
        type = TargetFormat->GetSelectedId();
        using namespace Parameters;
        const Container::Ptr options = GetBackendSettings(type);
        const QString filename = TargetTemplate->GetFilenameTemplate();
        options->SetValue(ZXTune::Sound::Backends::File::FILENAME, FromQString(filename));
        CopyExistingValue<IntType>(*Options, *options, ZXTune::Sound::Backends::File::BUFFERS);
        CopyExistingValue<IntType>(*Options, *options, ZXTune::IO::Providers::File::OVERWRITE_EXISTING);
        CopyExistingValue<IntType>(*Options, *options, ZXTune::IO::Providers::File::CREATE_DIRECTORIES);
        return options;
      }
      else
      {
        return Parameters::Accessor::Ptr();
      }
    }

    virtual void UpdateDescriptions()
    {
      UpdateTargetDescription();
      UpdateFormatDescription();
      UpdateSettingsDescription();
    }

    //QWidgets virtuals
    virtual void closeEvent(QCloseEvent* event)
    {
      State->Save();
      event->accept();
    }
  private:
    void AddBackendSettingsWidget(UI::BackendSettingsWidget* factory(QWidget&))
    {
      QWidget* const settingsWidget = toolBox->widget(SETTINGS_PAGE);
      UI::BackendSettingsWidget* const result = factory(*settingsWidget);
      formatSettingsLayout->addWidget(result);
      connect(result, SIGNAL(SettingsChanged()), SLOT(UpdateDescriptions()));
      BackendSettings[result->GetBackendId()] = result;
    }

    Parameters::Container::Ptr GetBackendSettings(const String& type) const
    {
      const BackendIdToSettings::const_iterator it = BackendSettings.find(type);
      if (it != BackendSettings.end())
      {
        return it->second->GetSettings();
      }
      return Parameters::Container::Create();
    }

    void UpdateTargetDescription()
    {
      const QString templ = TargetTemplate->GetFilenameTemplate();
      toolBox->setItemText(TEMPLATE_PAGE, templ);
      if (QPushButton* okButton = buttonBox->button(QDialogButtonBox::Ok))
      {
        okButton->setEnabled(!templ.isEmpty());
      }
    }

    void UpdateFormatDescription()
    {
      toolBox->setItemText(FORMAT_PAGE, TargetFormat->GetDescription());
    }

    void UpdateSettingsDescription()
    {
      const String type = TargetFormat->GetSelectedId();
      std::for_each(BackendSettings.begin(), BackendSettings.end(),
        boost::bind(&QWidget::setVisible, boost::bind(&BackendIdToSettings::value_type::second, _1), false));
      const BackendIdToSettings::const_iterator it = BackendSettings.find(type);
      if (it != BackendSettings.end())
      {
        it->second->setVisible(true);
        toolBox->setItemText(SETTINGS_PAGE, it->second->GetDescription());
        toolBox->setItemEnabled(SETTINGS_PAGE, true);
      }
      else
      {
        toolBox->setItemText(SETTINGS_PAGE, UI::SetupConversionDialog::tr("No options"));
        toolBox->setItemEnabled(SETTINGS_PAGE, false);
      }
    }
  private:
    const Parameters::Container::Ptr Options;
    UI::State::Ptr State;
    UI::FilenameTemplateWidget* const TargetTemplate;
    UI::SupportedFormatsWidget* const TargetFormat;
    typedef std::map<String, UI::BackendSettingsWidget*> BackendIdToSettings;
    BackendIdToSettings BackendSettings;
  };
}

namespace UI
{
  SetupConversionDialog::SetupConversionDialog(QWidget& parent)
    : QDialog(&parent)
  {
  }

  SetupConversionDialog::Ptr SetupConversionDialog::Create(QWidget& parent)
  {
    return SetupConversionDialog::Ptr(new SetupConversionDialogImpl(parent));
  }

  Parameters::Accessor::Ptr GetConversionParameters(QWidget& parent, String& type)
  {
    const SetupConversionDialog::Ptr dialog = SetupConversionDialog::Create(parent);
    return dialog->Execute(type);
  }
}
