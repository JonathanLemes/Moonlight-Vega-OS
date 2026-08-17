# Input adapter

Vega Bluetooth gamepad events are normalized in `useMoonlightGamepad` and sent
through the TurboModule to `LiSendMultiControllerEvent`. This directory remains
the seam for future native input batching, hot-plug, rumble, and multi-controller
support.
