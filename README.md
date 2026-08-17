# Moonlight Vega

Moonlight Vega is a native Moonlight client for Amazon Vega OS and Fire TV. It
connects to Sunshine and Wolf hosts, pairs securely, lists the server's
applications/integrations, and streams H.264 video with Opus audio and gamepad
input.

The current implementation targets 1080p60. HEVC, AV1, 4K60, rumble, and
multiple simultaneous controllers are not implemented yet.

## Implemented

- LAN host discovery plus manual host entry
- Modern Sunshine/Wolf certificate and PIN pairing
- Persistent client certificate, private key, and pinned server certificate
- Authenticated `/serverinfo` and application-list requests
- Application launch, resume, disconnect, and quit
- `moonlight-common-c` streaming and input protocols
- H.264 access units packaged as fragmented MP4 for Vega hardware playback
- Opus packets packaged as fragmented MP4 for Vega audio playback
- Fire TV remote navigation throughout the React Native UI
- Bluetooth gamepad forwarding with Xbox-compatible button, trigger, and stick
  mapping during a stream
- Vega `armv7`, `aarch64`, and `x86_64` builds

Media payloads stay in the native-to-media callback path. React Native owns the
screens and session controls; it does not parse GameStream packets or decode
media.

## Repository layout

```text
src/
  screens/                 Host, app, and streaming screens
  components/              Focusable Fire TV UI components
  services/                Typed TurboModule and W3C Media integration
  hooks/                   Server and gamepad event handling
  types/                   Shared application types
native/moonlight/
  MoonlightVegaCore/       C++ TurboModule and GameStream implementation
  adapters/                Platform-boundary documentation
third_party/
  moonlight-common-c/      Streaming and input protocol implementation
  mbedtls/                 TLS and pairing cryptography
  libgamestream/           Reference/evaluation checkout
docs/                      Architecture and milestone status
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the runtime data flow.

## Requirements

- Node.js 20 or newer and npm
- Amazon Vega SDK with the `vega` CLI configured in the shell
- A connected Vega OS Fire TV device
- A Sunshine or Wolf server reachable from the Fire TV
- The GameStream ports allowed between the device and server

This repository was built with Vega SDK `0.24.9914`, Vega CLI `1.3.4`, React
Native Kepler `4.x`, and tested on a 32-bit Fire TV running Vega OS 1.2.

For Wolf, expose the documented GameStream ports. The main ones used by this
client are TCP `47984`, `47989`, `48010`, and UDP `47998`, `47999`, `48000`, and
`48010`. Host discovery uses UDP `47998`.

## Install and validate

```bash
git submodule update --init --recursive
npm install
npm run typecheck
npm run lint
npm test -- --runInBand
npm run doctor
```

The native server and discovery smoke tests can be run against a reachable
host:

```bash
npm run smoke:serverinfo -- 192.168.1.20
npm run smoke:discovery
```

Regenerate the TurboModule bindings after changing its TypeScript specification:

```bash
npm --prefix native/moonlight/MoonlightVegaCore run kepler-codegen
```

## Build and run

Build all configured target architectures:

```bash
npm run build:debug
```

For a 32-bit Fire TV, install and launch the generated package with:

```bash
vega run-app \
  'build/private/kepler/@moonlight-vega/app/undefined/vega/armv7/Debug/@moonlight-vega/app_armv7.vpkg' \
  com.jonathanlemes.moonlightvega.main \
  --deviceId DEVICE_ID
```

Use `aarch64` in both path segments and filename for a 64-bit Fire TV. To build
optimized packages:

```bash
npm run build:release
```

## Using the app

1. Start Sunshine or Wolf and ensure the Fire TV can reach it.
2. Open Moonlight Vega. Select a discovered host, or enter its host name/IP.
3. If the host is new, the app displays a four-digit PIN. Enter that PIN in the
   Sunshine pairing dialog. For Wolf, open the pairing URL printed by Wolf and
   submit the same PIN.
4. Select an application or Wolf integration to launch it at 1080p60 H.264.
5. Use the paired Bluetooth gamepad in the stream. Press Fire TV Back to stop
   the stream and quit the remote application.

Pairing material is stored in the application's private data directory. A
normal app restart keeps it; uninstalling/reinstalling the debug package may
require pairing again.

## TypeScript-facing native API

```ts
discoverHosts(): Promise<ServerInfo[]>;
getServerInfo(host: string): Promise<ServerInfo>;
pair(host: string, pin: string): Promise<void>;
getApps(host: string): Promise<MoonlightApp[]>;
startStream(host: string, appId: number, config: StreamConfig): Promise<void>;
stopStream(host: string, quitApp: boolean): Promise<void>;
sendControllerState(/* Xbox-compatible controller state */): void;
```

## Current limitations

- H.264 is the only enabled video codec. HEVC and AV1 require additional
  fragmented-MP4 sample entry and capability work.
- The preset is fixed at 1920x1080, 60 fps, and 20 Mbps.
- One controller is exposed to the server; hot-plug state, rumble, motion,
  touchpad, and controller-specific remapping are not implemented.
- The app assumes the Bluetooth controller has already been paired in Fire TV
  settings.
- The stream UI provides a minimal status overlay rather than production
  quality settings, statistics, and recovery controls.

`moonlight-common-c` is GPL-3.0 licensed. Mbed TLS is available under its
documented Apache-2.0 or GPL-2.0-or-later choice. Review all dependency licenses
before distributing packages.
