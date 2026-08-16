import type {KeplerTurboModule} from '@amazon-devices/keplerscript-turbomodule-api';
import {TurboModuleRegistry} from '@amazon-devices/keplerscript-turbomodule-api';

export interface MoonlightVegaCore extends KeplerTurboModule {
  getCoreInfo: () => Object;
  getServerInfo: (host: string, port: number) => Promise<Object>;
}

export default TurboModuleRegistry.getEnforcing<MoonlightVegaCore>(
  'MoonlightVegaCore',
);

