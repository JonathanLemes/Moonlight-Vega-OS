# Architecture

## First vertical slice

```text
HostInfoScreen / useServerInfo
              |
              v
       moonlightService
              |
              v
  MoonlightVegaCore TypeScript spec
              |
              v
    C++ Node-API TurboModule
          |           |
          v           v
 ServerInfoClient  moonlight-common-c
          |
          v
 Sunshine/Wolf /serverinfo
```

The application UI depends only on typed service methods. It does not import
protocol headers, manage sockets, or know how native media backends work. The
TurboModule owns the stable cross-language boundary. Native implementation code
can therefore evolve from the milestone transport into a full GameStream client
without coupling React components to native details.

## Layer responsibilities

### React Native application

- `screens/` composes television experiences and renders state.
- `components/` contains focusable, remote-friendly UI primitives.
- `hooks/` manages request state and screen lifecycle.
- `services/` validates UI input and is the only application layer that calls
  the TurboModule.
- `types/` defines values shared across application layers and records the
  planned client surface.

### MoonlightVegaCore TurboModule

- Defines the minimal, serializable TypeScript/C++ contract.
- Maps native errors to rejected JavaScript promises.
- Owns client/session state once pairing and streaming are implemented.
- Links `moonlight-common-c` and platform adapters into one Vega native module.

`getCoreInfo()` invokes `moonlight-common-c` symbols so the first milestone
proves that the dependency is present in the installed module. `getServerInfo()`
uses the small native `ServerInfoClient` because `moonlight-common-c` implements
the streaming protocol but not the higher-level HTTP pairing and application
management workflow.

### Native platform adapters

The empty adapter directories are intentional seams:

- `VideoAdapter` will receive encoded H.264, HEVC, or AV1 access units from
  `moonlight-common-c` and feed a Vega hardware decoder and presentation path.
- `AudioAdapter` will receive Opus packets, decode them, and submit PCM to the
  lowest-latency Vega audio path available.
- `InputAdapter` will map Fire TV remote and Bluetooth gamepad events to
  Moonlight input packets.

None of these adapters is implemented in the initial scaffold. In particular,
no encoded media is accepted or decoded yet.

### Native dependencies

- `moonlight-common-c` supplies the GameStream control, RTP, and input protocol
  implementation.
- ENet and nanors are its recursively checked-out dependencies.
- Mbed TLS supplies target-compatible crypto/TLS support and is statically
  linked into the TurboModule dependency graph.
- `libgamestream` is a documented evaluation placeholder, not a compiled
  dependency.

## Server information request

The milestone implementation resolves IPv4 or IPv6 hosts, opens a bounded TCP
connection, requests the unpaired GameStream `/serverinfo` endpoint, validates
the HTTP and GameStream status codes, supports fixed and chunked response bodies,
and maps selected XML fields into a `ServerInfo` object. Responses are capped at
1 MiB and socket operations time out after five seconds.

This is deliberately not a general HTTP or XML implementation. When pairing is
added, certificate persistence, HTTPS identity validation, cancellation, a
native worker queue, and a tested GameStream control client must replace or
extend this narrow transport.

## Planned stable API

```ts
discoverHosts(): Promise<ServerInfo[]>;
getServerInfo(host: string): Promise<ServerInfo>;
pair(host: string, pin: string): Promise<void>;
getApps(host: string): Promise<App[]>;
startStream(host: string, appId: number, config: StreamConfig): Promise<void>;
stopStream(): Promise<void>;
```

Large media buffers must not cross the JavaScript bridge. JavaScript should
configure sessions and observe coarse state; compressed video/audio and
high-frequency input must stay on native threads.

## Initial performance target

The first streaming target is 1920x1080 at 60 frames per second. A future 4K60
target should be enabled only after measuring decoder support, presentation
latency, memory bandwidth, thermal behavior, network behavior, and controller
latency on representative Fire TV hardware.

