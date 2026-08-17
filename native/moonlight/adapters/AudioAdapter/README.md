# Audio adapter

Opus packets currently pass through `StreamSession` and `FragmentedMp4` into a
Vega W3C Media audio `SourceBuffer`. This directory remains the seam for a
future lower-latency native audio backend if W3C Media cannot meet target
latency on all supported devices.
