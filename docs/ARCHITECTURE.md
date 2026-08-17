# Architecture

## Runtime flow

```text
React Native screens
        |
        v
TypeScript moonlightService
        |
        v
C++ MoonlightVegaCore TurboModule
   |             |              |
   v             v              v
GameStreamClient HostDiscovery  StreamSession
   |                            |
   v                            v
Sunshine/Wolf HTTP(S)     moonlight-common-c
                                |
                    +-----------+-----------+
                    |           |           |
                    v           v           v
                 H.264         Opus       input
                    |           |           |
                    +----- fMP4-+           |
                          |                 |
                          v                 v
                    Vega W3C Media   GameStream input
```

The UI depends only on typed service methods. Protocol sockets, pairing
cryptography, media packet handling, and session state remain native. Encoded
video and audio are emitted directly to the W3C Media service and never stored
in React component state.

## React Native application

- `screens/` owns host selection, application selection, and stream lifecycle.
- `components/` provides focusable controls for a ten-foot Fire TV UI.
- `services/moonlight.ts` is the only application boundary to the TurboModule.
- `services/vegaStreamPlayer.ts` queues fragmented MP4 initialization/media
  segments into W3C Media Source Extensions and owns the video surface.
- `hooks/useMoonlightGamepad.ts` maps Vega gamepad events to Moonlight's
  Xbox-compatible controller state.

## Native core

- `ServerInfoClient` implements bounded HTTP transport and GameStream XML
  parsing.
- `HostDiscovery` broadcasts the GameStream discovery probe and resolves
  responding hosts.
- `GameStreamClient` implements HTTPS requests, modern certificate pairing,
  certificate/key persistence, server-certificate pinning, application listing,
  launch/resume, and cancel.
- `StreamSession` owns `moonlight-common-c`, its callbacks, streaming threads,
  and controller submission.
- `FragmentedMp4` converts H.264 access units and Opus packets into independent
  ISO BMFF initialization and media fragments accepted by Vega W3C Media.

Mbed TLS is statically linked for TLS and pairing cryptography. The client
certificate, private key, and pinned server certificate are stored below the
application-private data directory and reused across launches.

## Media path

`moonlight-common-c` receives RTP media and invokes direct-submit callbacks.
The native muxer preserves the encoded H.264 and Opus payloads, adds timestamps
and the required MP4 boxes, and invokes a registered TurboModule callback. The
TypeScript media service appends those buffers to separate video and audio
`SourceBuffer` instances. Vega then performs H.264 hardware decode, Opus decode,
audio output, and presentation.

The manifest requests the Vega media buffer, transform, player session, and
audio services required by the W3C Media pipeline. Missing those service
declarations prevents the decoder from attaching even when the MIME type is
supported.

## Input path

Vega gamepad events update an in-memory controller snapshot. Each change calls
the native `sendControllerState()` method, which submits a controller-0 packet
through `LiSendMultiControllerEvent()`. Wolf/Sunshine therefore sees an Xbox
compatible controller. React Native focus and `Pressable` controls handle the
same controller's D-pad/confirm actions while browsing the local UI.

## Concurrency and lifecycle

Pairing and HTTP control operations run away from the JavaScript UI thread.
`StreamSession` serializes start/stop state, while `moonlight-common-c` owns its
network worker threads. Starting initializes media callbacks before launching
the remote app. Back requests a native disconnect, optionally sends GameStream
cancel to quit the app, tears down W3C Media, and returns to the app list.

## Next codec and quality work

The current negotiated profile is H.264 at 1920x1080, 60 fps, and 20 Mbps.
HEVC and AV1 require codec-specific sample entries, parameter-set handling, and
device capability selection. 4K60 should be enabled only after measuring
decoder support, thermal behavior, memory use, A/V sync, network resilience,
and end-to-end input latency on each supported Fire TV class.
