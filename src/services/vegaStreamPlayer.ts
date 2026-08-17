import {
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

export class VegaStreamPlayer {
  readonly player = new VideoPlayer();

  private mediaSource = new MediaSource();
  private sourceOpen = false;
  private pendingEvents: Array<{event: string; data: ArrayBuffer}> = [];
  private videoQueue: AppendQueue | null = null;
  private audioQueue: AppendQueue | null = null;
  private surfaceReady = false;
  private playRequested = false;
  private onStatus: (status: string, isError: boolean) => void;

  constructor(onStatus: (status: string, isError: boolean) => void) {
    this.onStatus = onStatus;
  }

  async initialize(): Promise<void> {
    await this.player.initialize();
    this.player.addEventListener('error', () => {
      const mediaError = this.player.error;
      this.onStatus(
        `Vega playback error ${mediaError?.code ?? 'unknown'}: ${
          mediaError?.message || 'unsupported or invalid media'
        }`,
        true,
      );
    });
    this.mediaSource.onsourceopen = () => {
      this.sourceOpen = true;
      this.mediaSource.duration = Number.POSITIVE_INFINITY;
      const events = this.pendingEvents.splice(0);
      for (const item of events) {
        this.handleNativeEvent(item.event, item.data);
      }
    };
    this.player.src = URL.createObjectURL(this.mediaSource);
    moonlightService.setStreamEventHandler((event, data) => {
      this.handleNativeEvent(event, data);
    });
  }

  setSurface(surfaceHandle: string): void {
    this.player.setSurfaceHandle(surfaceHandle);
    this.surfaceReady = true;
    this.maybePlay();
  }

  clearSurface(surfaceHandle: string): void {
    this.surfaceReady = false;
    this.player.clearSurfaceHandle(surfaceHandle);
  }

  async deinitialize(): Promise<void> {
    this.player.pause();
    await this.player.deinitialize();
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
    if (!this.sourceOpen) {
      this.pendingEvents.push({event, data});
      return;
    }
    try {
      if (event.startsWith('video-init:')) {
        this.videoQueue = new AppendQueue(
          this.mediaSource.addSourceBuffer(event.slice(11)),
          message => this.onStatus(`Video buffer error: ${message}`, true),
        );
        this.videoQueue.push(data);
        this.playRequested = true;
        this.maybePlay();
      } else if (event === 'video') {
        this.videoQueue?.push(data);
      } else if (event.startsWith('audio-init:')) {
        // Vega's Media Source implementation does not accept Opus in ISO BMFF.
        // Audio is intentionally dropped until it is packaged as WebM below.
        this.audioQueue = null;
      } else if (event === 'audio') {
        this.audioQueue?.push(data);
      }
    } catch (reason) {
      this.onStatus(
        `Vega media pipeline error: ${
          reason instanceof Error ? reason.message : String(reason)
        }`,
        true,
      );
    }
  }

  private maybePlay(): void {
    if (!this.surfaceReady || !this.playRequested) {
      return;
    }
    this.player.play().catch(reason => {
      this.onStatus(
        `Playback failed: ${
          reason instanceof Error ? reason.message : String(reason)
        }`,
        true,
      );
    });
    this.playRequested = false;
  }
}
