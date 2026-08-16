# Moonlight Vega

Moonlight Vega is an early native Moonlight client for Amazon Vega OS and Fire
TV. The repository currently implements the first vertical slice only: a React
Native screen calls a C++ TurboModule, the TurboModule links
`moonlight-common-c`, and native code requests and displays a Sunshine/Wolf
`/serverinfo` response.

Video decoding, audio playback, input forwarding, pairing, application listing,
host discovery, and stream lifecycle management are intentionally out of scope
for this scaffold.

## Current milestone

- React Native 0.83 and TypeScript television UI
- Fire TV remote-friendly host form and server information screen
- Node-API C++ TurboModule with generated TypeScript bindings
- Native `moonlight-common-c`, ENet, nanors, and Mbed TLS builds through CMake
- Plain HTTP GameStream `/serverinfo` transport on port `47989`
- Builds for Vega `armv7`, `aarch64`, and `x86_64` targets
- Verified installation and launch on a connected Fire TV running Vega OS 1.2
- Verified `/serverinfo` parsing against a Wolf server

The currently callable TypeScript API is deliberately small:

```ts
MoonlightVegaCore.getCoreInfo(): CoreInfo;
MoonlightVegaCore.getServerInfo(host, port): Promise<ServerInfo>;
```

The intended future API (`discoverHosts`, `pair`, `getApps`, `startStream`, and
`stopStream`) is represented by `MoonlightClientApi` in
`src/types/moonlight.ts`, but those operations are not exposed by the native
module yet.

## Repository layout

```text
src/
  screens/                 TV screens and presentation state
  components/              Reusable focusable UI
  services/                TypeScript/native boundary
  hooks/                   Screen-facing state orchestration
  types/                   Public application types and future API shape
native/moonlight/
  MoonlightVegaCore/       C++ TurboModule and minimal GameStream transport
  adapters/
    VideoAdapter/          Reserved native video boundary
    AudioAdapter/          Reserved native audio boundary
    InputAdapter/          Reserved native input boundary
third_party/
  moonlight-common-c/      GameStream streaming protocol submodule
  mbedtls/                 Target-compatible crypto/TLS submodule
  libgamestream/           Evaluation notes; not compiled yet
docs/                      Architecture and roadmap
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for layer responsibilities and
[docs/MILESTONES.md](docs/MILESTONES.md) for the implementation sequence.

## Prerequisites

- Node.js 20 or newer
- npm
- Amazon Vega SDK and `vega` CLI configured in the current shell
- A Vega OS device or virtual device registered with the SDK
- A Sunshine or Wolf host reachable from the Vega device

This scaffold was inspected and tested with Vega SDK `0.24.9914`, Vega CLI
`1.3.4`, React Native Kepler `4.x`, and a Fire TV running Vega OS `1.2`. The
project follows the SDK's installed React Native 0.83 and basic TurboModule
templates rather than assuming Android React Native conventions.

## Install

Clone all nested native dependencies and install JavaScript packages:

```bash
git submodule update --init --recursive
npm install
```

The generated TurboModule JavaScript is built automatically by the application
build. Regenerate the native specification after changing
`native/moonlight/MoonlightVegaCore/src/turbo-modules/MoonlightVegaCore.ts`:

```bash
npm --prefix native/moonlight/MoonlightVegaCore run kepler-codegen
```

## Validate

Run the fast checks:

```bash
npm run typecheck
npm run lint
npm test -- --runInBand
npm run doctor
```

To exercise the exact native HTTP client and XML parser against a reachable
Sunshine/Wolf host from the development machine:

```bash
npm run smoke:serverinfo -- 192.168.1.20
```

An optional port can follow the host. The default is the GameStream HTTP port,
`47989`.

## Build and run

Build a debug package:

```bash
npm run build:debug
```

The prebuild step compiles the TurboModule and all bundled native dependencies
for each Vega architecture required by the application packager. Packages are
written beneath `build/`; for a physical 32-bit Fire TV the package is normally:

```text
build/armv7-debug/app_armv7.vpkg
```

Install and launch it on the already-connected device:

```bash
vega run-app build/armv7-debug/app_armv7.vpkg \
  com.jonathanlemes.moonlightvega.main
```

For a release package:

```bash
npm run build:release
```

After launch, enter only the Sunshine/Wolf hostname or IP address. The app sends
an unpaired request to `http://HOST:47989/serverinfo` and renders the basic
GameStream fields returned by the server.

For UI iteration, start Metro separately with `npm start` and use the normal
Vega SDK development workflow supported by the connected target.

## Native dependency notes

The inspected Vega target sysroot does not provide a linkable target OpenSSL
package. `moonlight-common-c` is therefore configured with bundled Mbed TLS
`3.6.7`, while dependency programs, tests, and shared libraries are disabled.

Moonlight Embedded's `libgamestream` was evaluated but is not built in this
milestone because its legacy desktop-oriented dependency set includes Avahi,
libcurl, Expat, libuuid, and OpenSSL. Pairing and application-list work should
either port the useful protocol pieces behind the native service boundary or
replace the minimal transport without leaking those details into the UI.

`moonlight-common-c` is licensed under GPL-3.0. Mbed TLS is available under its
documented Apache-2.0 or GPL-2.0-or-later choice. Review all applicable licenses
before distributing application packages.

