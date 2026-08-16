import {MoonlightVegaCore} from '@moonlight-vega/core';
import type {CoreInfo, ServerInfo} from '../types/moonlight';

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
};

