import {
  AudioPlayer,
  MediaSource,
  type SourceBuffer,
  VideoPlayer,
} from '@amazon-devices/react-native-w3cmedia';
import {moonlightService} from './moonlight';

class AppendQueue {
  private sourceBuffer: SourceBuffer;
  private pending: ArrayBuffer[] = [];
  private failed = false;
  private onError: (message: string) => void;

  constructor(sourceBuffer: SourceBuffer, onError: (message: string) => void) {
    this.sourceBuffer = sourceBuffer;
    this.onError = onError;
    this.sourceBuffer.onupdateend = () => this.flush();
    this.sourceBuffer.onerror = () => {
      this.failed = true;
      this.pending = [];
      this.onError('SourceBuffer rejected the stream format');
    };
  }

  push(data: ArrayBuffer): void {
    if (this.failed) {
      return;
    }
    this.pending.push(data);
    this.flush();
  }

  fail(): void {
    this.failed = true;
    this.pending = [];
  }

  private flush(): void {
    if (this.sourceBuffer.updating || this.pending.length === 0) {
      return;
    }
    const batch = this.pending.splice(0, 12);
    const size = batch.reduce((total, item) => total + item.byteLength, 0);
    const combined = new Uint8Array(size);
    let offset = 0;
    for (const item of batch) {
      combined.set(new Uint8Array(item), offset);
      offset += item.byteLength;
    }
    try {
      this.sourceBuffer.appendBuffer(combined);
    } catch (error) {
      this.failed = true;
      this.pending = [];
      this.onError(
        error instanceof Error ? error.message : String(error),
      );
    }
  }
}

// Vega's shared W3C Media pipeline takes its video renderer down whenever an
// Opus audio SourceBuffer is added alongside a video one on the same
// MediaSource/player: both start failing together the instant playback
// starts (see docs/2026-08-17 history). Giving audio its own AudioPlayer +
// MediaSource keeps it on an entirely separate native pipeline, so a problem
// on one track can't take the other down with it.
export class VegaStreamPlayer {
  readonly player = new VideoPlayer();
  readonly audioPlayer = new AudioPlayer();

  private mediaSource = new MediaSource();
  private audioMediaSource = new MediaSource();
  private sourceOpen = false;
  private audioSourceOpen = false;
  private pendingVideoEvents: Array<{event: string; data: ArrayBuffer}> = [];
  private pendingAudioEvents: Array<{event: string; data: ArrayBuffer}> = [];
  private videoQueue: AppendQueue | null = null;
  private audioQueue: AppendQueue | null = null;
  private surfaceReady = false;
  private videoInitReceived = false;
  private audioInitReceived = false;
  private started = false;
  private audioGraceTimer: ReturnType<typeof setTimeout> | null = null;
  private onStatus: (status: string, isError: boolean) => void;

  constructor(onStatus: (status: string, isError: boolean) => void) {
    this.onStatus = onStatus;
  }

  async initialize(): Promise<void> {
    await Promise.all([this.player.initialize(), this.audioPlayer.initialize()]);

    this.player.addEventListener('error', () => {
      this.videoQueue?.fail();
      const mediaError = this.player.error;
      this.onStatus(
        `Vega playback error ${mediaError?.code ?? 'unknown'}: ${
          mediaError?.message || 'unsupported or invalid media'
        }`,
        true,
      );
    });
    this.audioPlayer.addEventListener('error', () => {
      this.audioQueue?.fail();
      const mediaError = this.audioPlayer.error;
      // Audio is a bonus on top of the video stream, not the main event:
      // don't flag it as a hard error that keeps the overlay from fading.
      this.onStatus(
        `Vega audio error ${mediaError?.code ?? 'unknown'}: ${
          mediaError?.message || 'unsupported or invalid audio'
        }`,
        false,
      );
    });

    this.mediaSource.onsourceopen = () => {
      this.sourceOpen = true;
      this.mediaSource.duration = Number.POSITIVE_INFINITY;
      const events = this.pendingVideoEvents.splice(0);
      for (const item of events) {
        this.routeVideoEvent(item.event, item.data);
      }
    };
    this.audioMediaSource.onsourceopen = () => {
      this.audioSourceOpen = true;
      this.audioMediaSource.duration = Number.POSITIVE_INFINITY;
      const events = this.pendingAudioEvents.splice(0);
      for (const item of events) {
        this.routeAudioEvent(item.event, item.data);
      }
    };

    this.player.src = URL.createObjectURL(this.mediaSource);
    this.audioPlayer.src = URL.createObjectURL(this.audioMediaSource);

    moonlightService.setStreamEventHandler((event, data) => {
      this.handleNativeEvent(event, data);
    });
  }

  setSurface(surfaceHandle: string): void {
    this.player.setSurfaceHandle(surfaceHandle);
    this.surfaceReady = true;
    this.maybeStart();
  }

  clearSurface(surfaceHandle: string): void {
    this.surfaceReady = false;
    this.player.clearSurfaceHandle(surfaceHandle);
  }

  async deinitialize(): Promise<void> {
    if (this.audioGraceTimer) {
      clearTimeout(this.audioGraceTimer);
      this.audioGraceTimer = null;
    }
    this.player.pause();
    this.audioPlayer.pause();
    await Promise.all([
      this.player.deinitialize(),
      this.audioPlayer.deinitialize(),
    ]);
  }

  private handleNativeEvent(event: string, data: ArrayBuffer): void {
    if (event.startsWith('status:')) {
      this.onStatus(event.slice(7), false);
      return;
    }
    if (event.startsWith('error:')) {
      this.onStatus(event.slice(6), true);
      return;
    }
    if (event.startsWith('log:')) {
      return;
    }
    const isAudio = event.startsWith('audio-init:') || event === 'audio';
    if (isAudio) {
      if (!this.audioSourceOpen) {
        this.pendingAudioEvents.push({event, data});
        return;
      }
      this.routeAudioEvent(event, data);
    } else {
      if (!this.sourceOpen) {
        this.pendingVideoEvents.push({event, data});
        return;
      }
      this.routeVideoEvent(event, data);
    }
  }

  private routeVideoEvent(event: string, data: ArrayBuffer): void {
    try {
      if (event.startsWith('video-init:')) {
        this.videoQueue = new AppendQueue(
          this.mediaSource.addSourceBuffer(event.slice(11)),
          message => this.onStatus(`Video buffer error: ${message}`, true),
        );
        this.videoQueue.push(data);
        this.videoInitReceived = true;
        this.maybeStart();
      } else if (event === 'video') {
        this.videoQueue?.push(data);
      }
    } catch (reason) {
      this.onStatus(
        `Vega video pipeline error: ${
          reason instanceof Error ? reason.message : String(reason)
        }`,
        true,
      );
    }
  }

  private routeAudioEvent(event: string, data: ArrayBuffer): void {
    try {
      if (event.startsWith('audio-init:')) {
        this.audioQueue = new AppendQueue(
          this.audioMediaSource.addSourceBuffer(event.slice(11)),
          message => this.onStatus(`Audio buffer error: ${message}`, false),
        );
        this.audioQueue.push(data);
        this.audioInitReceived = true;
        if (this.started) {
          // Video already started without waiting (grace period elapsed, or
          // audio simply arrived after playback began) — join in now rather
          // than staying silent for the rest of the session.
          this.playAudio();
        } else {
          this.maybeStart();
        }
      } else if (event === 'audio') {
        this.audioQueue?.push(data);
      }
    } catch (reason) {
      // Audio pipeline problems stay non-fatal: video keeps playing either way.
      this.onStatus(
        `Vega audio pipeline error: ${
          reason instanceof Error ? reason.message : String(reason)
        }`,
        false,
      );
    }
  }

  // Video and audio are two independent players/pipelines now (see class
  // comment), so nothing keeps their clocks aligned the way a single shared
  // MediaSource would. The best we can do at this layer is start both as
  // close to the same instant as possible: once video is ready to play, wait
  // up to AUDIO_GRACE_MS for audio to also be ready and start them together;
  // if audio isn't ready by then, start video alone rather than delay it
  // indefinitely, and let audio join in as soon as it does arrive.
  private static readonly AUDIO_GRACE_MS = 500;

  private maybeStart(): void {
    if (this.started || !this.surfaceReady || !this.videoInitReceived) {
      return;
    }
    if (this.audioInitReceived) {
      this.startBoth();
      return;
    }
    if (this.audioGraceTimer) {
      return;
    }
    this.audioGraceTimer = setTimeout(() => {
      this.audioGraceTimer = null;
      this.startBoth();
    }, VegaStreamPlayer.AUDIO_GRACE_MS);
  }

  private startBoth(): void {
    if (this.started) {
      return;
    }
    this.started = true;
    if (this.audioGraceTimer) {
      clearTimeout(this.audioGraceTimer);
      this.audioGraceTimer = null;
    }
    this.player.play().catch(reason => {
      this.onStatus(
        `Playback failed: ${
          reason instanceof Error ? reason.message : String(reason)
        }`,
        true,
      );
    });
    if (this.audioInitReceived) {
      this.playAudio();
    }
  }

  private playAudio(): void {
    this.audioPlayer.play().catch(reason => {
      this.onStatus(
        `Audio playback failed: ${
          reason instanceof Error ? reason.message : String(reason)
        }`,
        false,
      );
    });
  }
}
