/*
Abstract:
  Playback support implementation

Last changed:
  $Id: playback_thread.cpp 513 2010-05-19 12:50:41Z vitamin.caig $

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "playback_supp.h"
#include "playback_supp_moc.h"
#include "supp/playitems_provider.h"
#include "ui/utils.h"
//common includes
#include <error.h>
#include <tools.h>
//library includes
#include <sound/backend.h>
//qt includes
#include <QtCore/QMutex>
#include <QtCore/QThread>
#include <QtGui/QWidget>

namespace
{
  class PlaybackSupportImpl : virtual public PlaybackSupport
                            , virtual private QThread
  {
  public:
    explicit PlaybackSupportImpl(QWidget* owner)
    {
      PlaybackSupport::setParent(owner);

      using namespace ZXTune;
      //create backend
      {
        Sound::BackendInformationArray backends;
        Sound::EnumerateBackends(backends);
        Parameters::Map params;
        for (Sound::BackendInformationArray::const_iterator it = backends.begin(), lim = backends.end(); it != lim; ++it)
        {
          if (!Sound::CreateBackend(it->Id, params, Backend))
          {
            break;
          }
        }
        assert(Backend.get());
        static const Sound::MultiGain MIXER3[] =
        {
          { {1.0, 0.0} },
          { {0.5, 0.5} },
          { {0.0, 1.0} }
        };
        static const Sound::MultiGain MIXER4[] =
        {
          { {1.0, 0.0} },
          { {0.7, 0.3} },
          { {0.3, 0.7} },
          { {0.0, 1.0} }
        };
        Backend->SetMixer(std::vector<Sound::MultiGain>(MIXER3, ArrayEnd(MIXER3)));
        Backend->SetMixer(std::vector<Sound::MultiGain>(MIXER4, ArrayEnd(MIXER4)));
      }
    }

    virtual ~PlaybackSupportImpl()
    {
      this->wait();
    }

    virtual void SelectItem(const Playitem& item)
    {
      //if nothing set, just select
      if (0 == Player.use_count())
      {
        Backend->SetModule(item.GetModule());
        Info = item.GetModuleInfo();
        Player = Backend->GetPlayer();
      }
    }
    
    virtual void SetItem(const Playitem& item)
    {
      Backend->SetModule(item.GetModule());
      this->wait();
      Info = item.GetModuleInfo();
      Player = Backend->GetPlayer();
      Backend->Play();
      this->start();
    }

    virtual void Play()
    {
      //play only if any module selected or set
      if (0 != Player.use_count())
      {
        Backend->Play();
        this->start();
      }
    }

    virtual void Stop()
    {
      Backend->Stop();
      this->wait();
    }

    virtual void Pause()
    {
      const ZXTune::Sound::Backend::State curState = Backend->GetCurrentState();
      if (ZXTune::Sound::Backend::STARTED == curState)
      {
        Backend->Pause();
      }
      else if (ZXTune::Sound::Backend::PAUSED == curState)
      {
        Backend->Play();
      }
    }

    virtual void Seek(int frame)
    {
      Backend->SetPosition(frame);
    }

    virtual void run()
    {
      using namespace ZXTune;
      //notify about start
      OnStartModule(Info);
      
      SignalsCollector::Ptr signaller = Backend->CreateSignalsCollector(
        Sound::Backend::MODULE_RESUME | Sound::Backend::MODULE_PAUSE |
        Sound::Backend::MODULE_STOP | Sound::Backend::MODULE_FINISH);
      //playback state, just for optimization
      uint_t time = 0;
      Module::Tracking tracking;
      Module::Analyze::ChannelsState analyze;
      for (;;)
      {
        uint_t sigmask = 0;
        if (signaller->WaitForSignals(sigmask, 100/*10fps*/))
        {
          if (sigmask & (Sound::Backend::MODULE_STOP | Sound::Backend::MODULE_FINISH))
          {
            if (sigmask & Sound::Backend::MODULE_FINISH)
            {
              OnFinishModule(Info);
            }
            break;
          }
          else if (sigmask & Sound::Backend::MODULE_RESUME)
          {
            OnResumeModule(Info);
          }
          else if (sigmask & Sound::Backend::MODULE_PAUSE)
          {
            OnPauseModule(Info);
          }
          else
          {
            assert(!"Invalid signal");
          }
        }
        const Sound::Backend::State curState = Backend->GetCurrentState();
        if (Sound::Backend::STARTED != curState)
        {
          continue;
        }
        if (Module::Player::ConstPtr realPlayer = Player.lock())
        {
          realPlayer->GetPlaybackState(time, tracking, analyze);
          OnUpdateState(static_cast<uint>(time), tracking, analyze);
        }
      }
      //notify about stop
      OnStopModule(Info);
    }
  private:
    ZXTune::Sound::Backend::Ptr Backend;
    ZXTune::Module::Information Info;
    ZXTune::Module::Player::ConstWeakPtr Player;
  };
}

PlaybackSupport* PlaybackSupport::Create(QWidget* owner)
{
  REGISTER_METATYPE(ZXTune::Module::Information);
  REGISTER_METATYPE(ZXTune::Module::Tracking);
  REGISTER_METATYPE(ZXTune::Module::Analyze::ChannelsState);
  assert(owner);
  return new PlaybackSupportImpl(owner);
}