import type {KeplerTurboModule} from '@amazon-devices/keplerscript-turbomodule-api';
import {TurboModuleRegistry} from '@amazon-devices/keplerscript-turbomodule-api';

export interface MoonlightVegaCore extends KeplerTurboModule {
  getCoreInfo: () => Object;
  discoverHosts: () => Promise<Object>;
  getServerInfo: (host: string, port: number) => Promise<Object>;
  pair: (host: string, port: number, pin: string) => Promise<Object>;
  getApps: (host: string, port: number) => Promise<Object>;
  setStreamEventHandler: (
    handler: (event: string, data: ArrayBuffer) => void,
  ) => void;
  startStream: (
    host: string,
    port: number,
    appId: number,
    width: number,
    height: number,
    fps: number,
    bitrateKbps: number,
    codec: string,
  ) => Promise<Object>;
  stopStream: (host: string, port: number, quitApp: boolean) => Promise<Object>;
  sendControllerState: (
    buttons: number,
    leftTrigger: number,
    rightTrigger: number,
    leftStickX: number,
    leftStickY: number,
    rightStickX: number,
    rightStickY: number,
  ) => void;
}

export default TurboModuleRegistry.getEnforcing<MoonlightVegaCore>(
  'MoonlightVegaCore',
);
