/**
 *
 * @file
 *
 * @brief Local implementation of PlaybackService
 *
 * @author vitamin.caig@gmail.com
 *
 */

package app.zxtune.playback;

import java.io.IOException;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;

import android.content.Context;
import android.content.SharedPreferences;
import android.net.Uri;
import android.preference.PreferenceManager;
import android.util.Log;
import app.zxtune.Releaseable;
import app.zxtune.TimeStamp;
import app.zxtune.ZXTune;
import app.zxtune.sound.AsyncPlayer;
import app.zxtune.sound.Player;
import app.zxtune.sound.PlayerEventsListener;
import app.zxtune.sound.SamplesSource;
import app.zxtune.sound.StubPlayer;

public class PlaybackServiceLocal implements PlaybackService, Releaseable {
  
  private static final String TAG = PlaybackServiceLocal.class.getName();
  
  private final Context context;
  private final ExecutorService executor;
  private final CompositeCallback callbacks;
  private final PlaylistControl playlist;
  private final PlaybackControl playback;
  private final SeekControl seek;
  private final Visualizer visualizer;
  private Holder holder;

  public PlaybackServiceLocal(Context context) {
    this.context = context;
    this.executor = Executors.newSingleThreadExecutor();
    this.callbacks = new CompositeCallback();
    this.playlist = new PlaylistControlLocal(context);
    this.playback = new DispatchedPlaybackControl();
    this.seek = new DispatchedSeekControl();
    this.visualizer = new DispatchedVisualizer();
    this.holder = new Holder();
  }

  @Override
  public synchronized Item getNowPlaying() {
    return holder.item;
  }
  
  @Override
  public void setNowPlaying(Uri[] uris) {
    executeCommand(new SetNowPlayingCommand(uris));
  }
  
  private static interface Command {
    void execute() throws IOException;
  }
  
  private class SetNowPlayingCommand implements Command {
    
    private final Uri[] uris;
    
    SetNowPlayingCommand(Uri[] uris) {
      this.uris = uris;
    }

    @Override
    public void execute() throws IOException {
      final Iterator iter = IteratorFactory.createIterator(context, uris);
      play(iter);
    }
  }
  
  private void play(Iterator iter) throws IOException {
    final PlayerEventsListener events = new PlaybackEvents(callbacks, playback, seek);
    setNewHolder(new Holder(iter, events));
  }
  
  @Override
  public PlaylistControl getPlaylistControl() {
    return playlist;
  }

  @Override
  public PlaybackControl getPlaybackControl() {
    return playback;
  }

  @Override
  public SeekControl getSeekControl() {
    return seek;
  }

  @Override
  public Visualizer getVisualizer() {
    return visualizer;
  }

  @Override
  public void subscribe(Callback callback) {
    callbacks.add(callback);
  }
  
  @Override
  public void unsubscribe(Callback callback) {
    callbacks.remove(callback);
  }

  @Override
  public void release() {
    shutdownExecutor();
    synchronized (this) {
      try {
        holder.player.stopPlayback();
        holder.release();
      } finally {
        holder = new Holder();
      }
    }
  }
  
  private void shutdownExecutor() {
    try {
      do {
        executor.shutdownNow();
        Log.d(TAG, "Waiting for executor shutdown...");
      } while (!executor.awaitTermination(10, TimeUnit.SECONDS));
      Log.d(TAG, "Executor shut down");
    } catch (InterruptedException e) {
      Log.w(TAG, "Failed to shutdown executor", e);
    }
  }
  
  private void executeCommand(final Command cmd) {
    executor.execute(new Runnable() {

      @Override
      public void run() {
        try {
          callbacks.onIOStatusChanged(true);
          cmd.execute();
        } catch (Exception e) {//IOException|InterruptedException
          Log.w(TAG, cmd.getClass().getName(), e);
        } finally {
          callbacks.onIOStatusChanged(false);
        }
      }
    });
  }
  
  private void setNewHolder(Holder holder) {
    final Holder oldHolder = this.holder;
    oldHolder.player.stopPlayback();
    synchronized (this) {
      this.holder = holder;
    }
    try {
      callbacks.onItemChanged(holder.item);
      holder.player.startPlayback();
    } finally {
      oldHolder.release();
    }
  }
  
  private void saveProperty(String name, long value) {
    final SharedPreferences prefs = PreferenceManager.getDefaultSharedPreferences(context);
    prefs.edit().putLong(name, value).apply();
  }
  
  private static class Holder implements Releaseable {

    public final Iterator iterator;
    public final PlayableItem item;
    public final Player player;
    public final SeekControl seek;
    public final Visualizer visualizer;
    
    Holder() {
      this.iterator = IteratorStub.instance();
      this.item = PlayableItemStub.instance();
      this.player = StubPlayer.instance();
      this.seek = SeekControlStub.instance();
      this.visualizer = VisualizerStub.instance();
    }
    
    Holder(Iterator iterator, PlayerEventsListener events) throws IOException {
      this.iterator = iterator;
      this.item = iterator.getItem();
      final ZXTune.Player lowPlayer = item.getModule().createPlayer();
      final SeekableSamplesSource source = new SeekableSamplesSource(lowPlayer, item.getDuration());
      this.player = AsyncPlayer.create(source, events);
      this.seek = source;
      this.visualizer = new PlaybackVisualizer(lowPlayer);
    }
    
    @Override
    public void release() {
      player.release();
      item.release();
    }
  }
  
  private final class DispatchedPlaybackControl implements PlaybackControl {
    
    private final IteratorFactory.NavigationMode navigation;
    
    DispatchedPlaybackControl() {
      this.navigation = new IteratorFactory.NavigationMode(context);
    }
    
    @Override
    public void play() {
      synchronized (PlaybackServiceLocal.this) {
        holder.player.startPlayback();
      }
    }

    @Override
    public synchronized void stop() {
      synchronized (PlaybackServiceLocal.this) {
        holder.player.stopPlayback();
      }
    }

    @Override
    public synchronized boolean isPlaying() {
      synchronized (PlaybackServiceLocal.this) {
        return holder.player.isStarted();
      }
    }

    @Override
    public synchronized void next() {
      synchronized (PlaybackServiceLocal.this) {
        executeCommand(new PlayNextCommand(holder.iterator));
      }
    }

    @Override
    public synchronized void prev() {
      synchronized (PlaybackServiceLocal.this) {
        executeCommand(new PlayPrevCommand(holder.iterator));
      }
    }

    @Override
    public TrackMode getTrackMode() {
      final long val = ZXTune.GlobalOptions.instance().getProperty(ZXTune.Properties.Sound.LOOPED, 0);
      return val != 0 ? TrackMode.LOOPED : TrackMode.REGULAR;
    }
    
    @Override
    public void setTrackMode(TrackMode mode) {
      final long val = mode == TrackMode.LOOPED ? 1 : 0;
      ZXTune.GlobalOptions.instance().setProperty(ZXTune.Properties.Sound.LOOPED, val);
      saveProperty(ZXTune.Properties.Sound.LOOPED, val);
    }
    
    @Override
    public SequenceMode getSequenceMode() {
      return navigation.get();
    }
    
    @Override
    public void setSequenceMode(SequenceMode mode) {
      navigation.set(mode);
    }
  }
  
  private class PlayNextCommand implements Command {
    
    private final Iterator iter;
    
    PlayNextCommand(Iterator iter) {
      this.iter = iter;
    }

    @Override
    public void execute() throws IOException {
      if (iter.next()) {
        play(iter);
      }
    }
  }

  private class PlayPrevCommand implements Command {
    
    private final Iterator iter;
    
    PlayPrevCommand(Iterator iter) {
      this.iter = iter;
    }

    @Override
    public void execute() throws IOException {
      if (iter.prev()) {
        play(iter);
      }
    }
  }
  
  private final class DispatchedSeekControl implements SeekControl {

    @Override
    public synchronized TimeStamp getDuration() {
      synchronized (PlaybackServiceLocal.this) {
        return holder.seek.getDuration();
      }
    }

    @Override
    public synchronized TimeStamp getPosition() {
      synchronized (PlaybackServiceLocal.this) {
        return holder.seek.getPosition();
      }
    }

    @Override
    public synchronized void setPosition(TimeStamp position) {
      synchronized (PlaybackServiceLocal.this) {
        holder.seek.setPosition(position);
      }
    }
  }
  
  private final class DispatchedVisualizer implements Visualizer {
    
    @Override
    public synchronized int getSpectrum(int[] bands, int[] levels) {
      synchronized (PlaybackServiceLocal.this) {
        return holder.visualizer.getSpectrum(bands, levels);
      }
    }
  }

  private static final class PlaybackEvents implements PlayerEventsListener {

    private final Callback callback;
    private final PlaybackControl ctrl;
    private final SeekControl seek;
    
    public PlaybackEvents(Callback callback, PlaybackControl ctrl, SeekControl seek) {
      this.callback = callback;
      this.ctrl = ctrl;
      this.seek = seek;
    }
    
    @Override
    public void onStart() {
      callback.onStatusChanged(true);
    }

    @Override
    public void onFinish() {
      seek.setPosition(TimeStamp.EMPTY);
      ctrl.next();
    }

    @Override
    public void onStop() {
      callback.onStatusChanged(false);
    }

    @Override
    public void onError(Error e) {
      Log.d(TAG, "Error occurred: " + e);
    }
  }
  
  private static final class SeekableSamplesSource implements SamplesSource, SeekControl {

    private ZXTune.Player player;
    private final TimeStamp totalDuration;
    private final TimeStamp frameDuration;
    private volatile TimeStamp seekRequest;
    
    public SeekableSamplesSource(ZXTune.Player player, TimeStamp totalDuration) {
      this.player = player;
      this.totalDuration = totalDuration;
      final long frameDurationUs = player.getProperty(ZXTune.Properties.Sound.FRAMEDURATION, ZXTune.Properties.Sound.FRAMEDURATION_DEFAULT); 
      this.frameDuration = TimeStamp.createFrom(frameDurationUs, TimeUnit.MICROSECONDS);
      player.setPosition(0);
    }
    
    @Override
    public void initialize(int sampleRate) {
      player.setProperty(ZXTune.Properties.Sound.FREQUENCY, sampleRate);
    }

    @Override
    public boolean getSamples(short[] buf) {
      if (seekRequest != null) {
        final int frame = (int) seekRequest.divides(frameDuration); 
        player.setPosition(frame);
        seekRequest = null;
      }
      return player.render(buf);
    }

    @Override
    public void release() {
      player.release();
      player = null;
    }
    
    @Override
    public TimeStamp getDuration() {
      return totalDuration; 
    }

    @Override
    public TimeStamp getPosition() {
      TimeStamp res = seekRequest;
      if (res == null) {
        final int frame = player.getPosition();
        res = frameDuration.multiplies(frame); 
      }
      return res;
    }

    @Override
    public void setPosition(TimeStamp pos) {
      seekRequest = pos;
    }
  }
  
  private static final class PlaybackVisualizer implements Visualizer {
    
    private final ZXTune.Player player;
    
    public PlaybackVisualizer(ZXTune.Player player) {
      this.player = player;
    }

    @Override
    public int getSpectrum(int[] bands, int[] levels) {
      return player.analyze(bands, levels);
    }
  }
}
