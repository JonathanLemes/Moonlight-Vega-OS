export interface CoreInfo {
  moonlightCommonLinked: boolean;
  defaultWidth: number;
  defaultHeight: number;
  defaultFps: number;
  launchQueryParameters: string;
}

export interface ServerInfo {
  address: string;
  port: number;
  hostname: string;
  appVersion: string;
  gsVersion: string;
  uniqueId: string;
  state: string;
  paired: boolean;
  currentGame: number;
  serverCodecModeSupport: number;
}

export interface StreamConfig {
  width: number;
  height: number;
  fps: number;
  bitrateKbps: number;
  codec: 'h264' | 'hevc' | 'av1';
}

export interface MoonlightClientApi {
  discoverHosts(): Promise<ServerInfo[]>;
  getServerInfo(host: string): Promise<ServerInfo>;
  pair(host: string, pin: string): Promise<void>;
  getApps(host: string): Promise<unknown[]>;
  startStream(host: string, appId: number, config: StreamConfig): Promise<void>;
  stopStream(): Promise<void>;
}

