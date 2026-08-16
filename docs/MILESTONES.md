# Milestones

## 1. Native bridge and server information

Status: implemented.

- Build and launch the Vega React Native application.
- Call native C++ from TypeScript through a generated TurboModule.
- Build and link `moonlight-common-c` and its native dependencies.
- Reach a Sunshine/Wolf host through the GameStream HTTP port.
- Retrieve and display basic `/serverinfo` fields.

## 2. Host management and pairing

- Add persisted manual hosts and reachability refresh.
- Add mDNS host discovery suitable for Vega OS.
- Generate and persist a client certificate and key securely.
- Implement the Sunshine PIN pairing exchange over HTTPS.
- Model paired, offline, incompatible, and certificate-error states.

## 3. Applications and session control

- Retrieve and cache the application list.
- Launch and quit applications.
- Negotiate a 1080p60 session configuration.
- Add explicit native session state and cancellation.

## 4. Native media and input

- Implement hardware-backed H.264 decoding first.
- Add HEVC and AV1 after device capability detection.
- Decode Opus and implement low-latency audio output.
- Map Fire TV remote and Bluetooth gamepads in native code.
- Keep packet, frame, audio, and input paths off the JavaScript bridge.

## 5. Quality and 4K60 investigation

- Add network resilience, telemetry, and end-to-end latency measurement.
- Exercise suspend/resume, controller reconnect, and stream recovery.
- Validate thermal and memory behavior on supported Fire TV devices.
- Qualify 4K60 only on devices with adequate decode and presentation support.

