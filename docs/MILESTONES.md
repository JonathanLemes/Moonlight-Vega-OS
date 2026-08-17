# Milestones

## 1. Native bridge and server information

Status: complete.

- Vega React Native application and generated C++ TurboModule
- `moonlight-common-c`, Mbed TLS, ENet, and nanors target builds
- Sunshine/Wolf discovery and `/serverinfo`

## 2. Host management and pairing

Status: complete for the first usable client.

- LAN discovery and manual host entry
- Modern PIN/certificate pairing
- Persistent client identity and pinned server certificate
- Paired/offline/error UI states

Persisted named host management and richer certificate recovery remain future
polish.

## 3. Applications and session control

Status: complete for 1080p60 H.264.

- Authenticated application/integration list
- Launch, resume, disconnect, and quit
- Explicit native session ownership

## 4. Media and input

Status: usable baseline complete.

- H.264 hardware playback through Vega W3C Media
- Opus playback through Vega W3C Media
- Fire TV remote UI navigation and Back-to-stop behavior
- Single Bluetooth gamepad forwarded as an Xbox-compatible controller

HEVC, AV1, rumble, controller hot-plug polish, and multiple controllers remain.

## 5. Quality and 4K60

Status: planned.

- Network recovery and session reconnection UX
- Latency and frame-drop telemetry
- Suspend/resume and controller reconnect testing
- Settings for resolution, bitrate, codec, and frame rate
- Device-specific HEVC/AV1 and 4K60 qualification
