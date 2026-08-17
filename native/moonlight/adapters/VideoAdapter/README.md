# Video adapter

H.264 access units currently pass through `StreamSession` and `FragmentedMp4`
into a Vega W3C Media video `SourceBuffer`, which selects the platform hardware
decoder and presents to `KeplerVideoSurfaceView`. This directory remains the
seam for future native decoder integration and HEVC/AV1 support.
