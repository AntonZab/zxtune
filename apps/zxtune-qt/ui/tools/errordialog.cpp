/*
Abstract:
  Error display widget implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "errordialog.h"
#include "ui/utils.h"
//qt includes
#include <QtWidgets/QMessageBox>

void ShowErrorMessage(const QString& title, const Error& err)
{
  QMessageBox msgBox;
  msgBox.setWindowTitle(QMessageBox::tr("Error"));
  const QString& errorText = ToQString(err.GetText());
  if (title.size() != 0)
  {
    msgBox.setText(title);
    msgBox.setInformativeText(errorText);
  }
  else
  {
    msgBox.setText(errorText);
  }
  msgBox.setDetailedText(ToQString(err.ToString()));
  msgBox.setIcon(QMessageBox::Critical);
  msgBox.setStandardButtons(QMessageBox::Ok);
  msgBox.exec();
}
