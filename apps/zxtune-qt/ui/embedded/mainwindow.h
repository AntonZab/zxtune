/**
* 
* @file
*
* @brief Main window interface
*
* @author vitamin.caig@gmail.com
*
**/

#pragma once

//library includes
#include <parameters/container.h>
#include <strings/array.h>
//qt includes
#include <QtCore/QPointer>
#include <QtWidgets/QMainWindow>
//common includes
#include <parameters.h>
//library includes
#include <strings/array.h>

class EmbeddedMainWindow : public QMainWindow
{
  Q_OBJECT
public:
  static QPointer<EmbeddedMainWindow> Create(Parameters::Container::Ptr options, const Strings::Array& cmdline);
};
