/*
Abstract:
  Playlist scanner view implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "scanner_view.h"
#include "scanner_view.ui.h"
#include "playlist/supp/scanner.h"
//common includes
#include <logging.h>

namespace
{
  const std::string THIS_MODULE("Playlist::Scanner::View");

  class PlaylistScannerViewImpl : public PlaylistScannerView
                                , private Ui::PlaylistScannerView
  {
  public:
    PlaylistScannerViewImpl(QWidget& parent, PlaylistScanner& scanner)
      : ::PlaylistScannerView(parent)
      , Scanner(scanner)
    {
      //setup self
      setupUi(this);
      hide();
      //make connections with scanner
      this->connect(&Scanner, SIGNAL(OnScanStart()), this, SLOT(ScanStart()));
      this->connect(&Scanner, SIGNAL(OnScanStop()), this, SLOT(ScanStop()));
      this->connect(&Scanner, SIGNAL(OnProgressStatus(unsigned, unsigned, unsigned)), SLOT(ShowProgress(unsigned, unsigned, unsigned)));
      this->connect(&Scanner, SIGNAL(OnProgressMessage(const QString&, const QString&)), SLOT(ShowProgressMessage(const QString&)));
      this->connect(&Scanner, SIGNAL(OnResolvingStart()), this, SLOT(ShowResolving()));
      this->connect(&Scanner, SIGNAL(OnResolvingStop()), this, SLOT(HideResolving()));
      this->connect(scanCancel, SIGNAL(clicked()), SLOT(ScanCancel()));

      Log::Debug(THIS_MODULE, "Created at %1%", this);
    }

    virtual ~PlaylistScannerViewImpl()
    {
      Log::Debug(THIS_MODULE, "Destroyed at %1%", this);
    }

    virtual void ScanStart()
    {
      show();
    }

    virtual void ScanCancel()
    {
      scanCancel->setEnabled(false);
      Scanner.Cancel();
    }

    virtual void ScanStop()
    {
      hide();
      scanCancel->setEnabled(true);
    }

    virtual void ShowProgress(unsigned progress, unsigned itemsDone, unsigned totalItems)
    {
      const QString itemsProgressText = QString("%1/%2").arg(itemsDone).arg(totalItems);
      itemsProgress->setText(itemsProgressText);
      scanProgress->setValue(progress);
      CheckedShow();
    }

    virtual void ShowProgressMessage(const QString& message)
    {
      scanProgress->setToolTip(message);
    }

    virtual void ShowResolving()
    {
      itemsProgress->setText(tr("Searching..."));
      CheckedShow();
    }

    virtual void HideResolving()
    {
      itemsProgress->setText(QString());
    }
  private:
    void CheckedShow()
    {
      if (!isVisible())
      {
        show();
      }
    }
  private:
    PlaylistScanner& Scanner;
  };
}

PlaylistScannerView::PlaylistScannerView(QWidget& parent) : QWidget(&parent)
{
}

PlaylistScannerView* PlaylistScannerView::Create(QWidget& parent, PlaylistScanner& scanner)
{
  return new PlaylistScannerViewImpl(parent, scanner);
}