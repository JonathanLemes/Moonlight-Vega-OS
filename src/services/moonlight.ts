import {MoonlightVegaCore} from '@moonlight-vega/core';
import type {
  CoreInfo,
  MoonlightApp,
  ServerInfo,
  StreamConfig,
} from '../types/moonlight';

const DEFAULT_GAMESTREAM_HTTP_PORT = 47989;

const normalizeHost = (value: string) => {
  const host = value.trim();
  if (!host) {
    throw new Error('A Sunshine host name or IP address is required.');
  }
  if (host.includes('://') || host.includes('/')) {
    throw new Error('Enter only a host name or IP address, without a URL path.');
  }
  return host;
};

export const moonlightService = {
  getCoreInfo(): CoreInfo {
    return MoonlightVegaCore.getCoreInfo() as CoreInfo;
  },

  async getServerInfo(host: string): Promise<ServerInfo> {
    return (await MoonlightVegaCore.getServerInfo(
      normalizeHost(host),
      DEFAULT_GAMESTREAM_HTTP_PORT,
    )) as ServerInfo;
  },

  async pair(host: string, pin: string): Promise<void> {
    await MoonlightVegaCore.pair(
      normalizeHost(host),
      DEFAULT_GAMESTREAM_HTTP_PORT,
      pin,
    );
  },

  async getApps(host: string): Promise<MoonlightApp[]> {
    const result = (await MoonlightVegaCore.getApps(
      normalizeHost(host),
      DEFAULT_GAMESTREAM_HTTP_PORT,
    )) as {apps: MoonlightApp[]};
    return result.apps;
  },

  setStreamEventHandler(
    handler: (event: string, data: ArrayBuffer) => void,
  ): void {
    MoonlightVegaCore.setStreamEventHandler(handler);
  },

  async startStream(
    host: string,
    appId: number,
    config: StreamConfig,
  ): Promise<void> {
    await MoonlightVegaCore.startStream(
      normalizeHost(host),
      DEFAULT_GAMESTREAM_HTTP_PORT,
      appId,
      config.width,
      config.height,
      config.fps,
      config.bitrateKbps,
      config.codec,
    );
  },

  async stopStream(host: string, quitApp: boolean): Promise<void> {
    await MoonlightVegaCore.stopStream(
      normalizeHost(host),
      DEFAULT_GAMESTREAM_HTTP_PORT,
      quitApp,
    );
  },

  sendControllerState(
    buttons: number,
    leftTrigger: number,
    rightTrigger: number,
    leftStickX: number,
    leftStickY: number,
    rightStickX: number,
    rightStickY: number,
  ): void {
    MoonlightVegaCore.sendControllerState(
      buttons,
      leftTrigger,
      rightTrigger,
      leftStickX,
      leftStickY,
      rightStickX,
      rightStickY,
    );
  },
};
