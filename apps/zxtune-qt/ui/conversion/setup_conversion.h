/*
Abstract:
  Conversion setup dialog

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

#pragma once
#ifndef ZXTUNE_QT_UI_SETUP_CONVERSION_H_DEFINED
#define ZXTUNE_QT_UI_SETUP_CONVERSION_H_DEFINED

//common includes
#include <parameters.h>
//qt includes
#include <QtWidgets/QDialog>

namespace UI
{
  class SetupConversionDialog : public QDialog
  {
    Q_OBJECT
  protected:
    explicit SetupConversionDialog(QWidget& parent);
  public:
    typedef boost::shared_ptr<SetupConversionDialog> Ptr;

    static Ptr Create(QWidget& parent);

    virtual Parameters::Accessor::Ptr Execute(String& type) = 0;
  private slots:
    virtual void UpdateDescriptions() = 0;
  };

  Parameters::Accessor::Ptr GetConversionParameters(QWidget& parent, String& type);
}

#endif //ZXTUNE_QT_UI_SETUP_CONVERSION_H_DEFINED
