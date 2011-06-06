/*
Abstract:
  Streamed modules support implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001
*/

//local includes
#include "streaming.h"
//boost includes
#include <boost/make_shared.hpp>

namespace
{
  using namespace ZXTune;
  using namespace ZXTune::Module;

  const uint_t STREAM_LOGICAL_CHANNELS = 1;

  class StreamStateCursor : public TrackState
  {
  public:
    typedef boost::shared_ptr<StreamStateCursor> Ptr;

    explicit StreamStateCursor(Information::Ptr info)
      : Info(info)
      , CurFrame()
    {
      Reset();
    }

    //status functions
    virtual uint_t Position() const
    {
      return 0;
    }

    virtual uint_t Pattern() const
    {
      return 0;
    }

    virtual uint_t PatternSize() const
    {
      return 0;
    }

    virtual uint_t Line() const
    {
      return 0;
    }

    virtual uint_t Tempo() const
    {
      return 1;
    }

    virtual uint_t Quirk() const
    {
      return 0;
    }

    virtual uint_t Frame() const
    {
      return CurFrame;
    }

    virtual uint_t Channels() const
    {
      return STREAM_LOGICAL_CHANNELS;
    }

    //navigation
    void Reset()
    {
      CurFrame = 0;
    }

    void ResetToLoop()
    {
      CurFrame = Info->LoopFrame();
    }

    bool NextFrame()
    {
      return ++CurFrame < Info->FramesCount();
    }
  private:
    const Information::Ptr Info;
    uint_t CurFrame;
  };

  class StreamInfo : public Information
  {
  public:
    explicit StreamInfo(uint_t frames, uint_t physChannels)
      : TotalFrames(frames)
      , PhysChans(physChannels)
    {
    }
    virtual uint_t PositionsCount() const
    {
      return 1;
    }
    virtual uint_t LoopPosition() const
    {
      return 0;
    }
    virtual uint_t PatternsCount() const
    {
      return 0;
    }
    virtual uint_t FramesCount() const
    {
      return TotalFrames;
    }
    virtual uint_t LoopFrame() const
    {
      return 0;
    }
    virtual uint_t LogicalChannels() const
    {
      return STREAM_LOGICAL_CHANNELS;
    }
    virtual uint_t PhysicalChannels() const
    {
      return PhysChans;
    }
    virtual uint_t Tempo() const
    {
      return 1;
    }
  private:
    const uint_t TotalFrames;
    const uint_t PhysChans;
  };

  class StreamStateIterator : public StateIterator
  {
  public:
    explicit StreamStateIterator(Information::Ptr info)
      : Cursor(boost::make_shared<StreamStateCursor>(info))
    {
    }

    //iterator functions
    virtual void Reset()
    {
      Cursor->Reset();
    }

    virtual bool NextFrame(bool looped)
    {
      if (Cursor->NextFrame())
      {
        return true;
      }
      if (looped)
      {
        Cursor->ResetToLoop();
        return true;
      }
      else
      {
        Cursor->Reset();
        return false;
      }
    }

    virtual TrackState::Ptr GetStateObserver() const
    {
      return Cursor;
    }
  private:
    const StreamStateCursor::Ptr Cursor;
  };
}

namespace ZXTune
{
  namespace Module
  {
    Information::Ptr CreateStreamInfo(uint_t frames, uint_t physChannels)
    {
      return boost::make_shared<StreamInfo>(frames, physChannels);
    }

    StateIterator::Ptr CreateStreamStateIterator(Information::Ptr info)
    {
      return boost::make_shared<StreamStateIterator>(info);
    }
  }
}
