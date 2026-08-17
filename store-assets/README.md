# Store submission assets

Assets here are **not** part of the app package/manifest — they're uploaded
manually through the Amazon Appstore Developer Console when submitting
("Publish > Appstore Details > Image assets"). Vega OS keeps two completely
separate icon concepts and it's easy to conflate them:

- `assets/image/icon.png` (512×512, referenced by `manifest.toml`'s
  `[package.icon]`) — shown in **Settings > Applications > Managed Installed
  Applications**. Bundled in the vpkg, applies to sideloaded/dev installs too.
- `store-assets/firetv-app-icon-1280x720.png` (1280×720, no transparency) —
  Amazon's Fire TV "App Icon" asset for the **home screen carousel/grid**
  tile and Appstore listing. Content must stay inside the centered 882×448
  safe area. This is *only* wired up once uploaded to the Console during
  submission — a sideloaded build has no way to carry it, so local dev
  installs fall back to a system default landscape tile with the 512×512
  icon centered/letterboxed inside it (see the icon-size note in project
  history for 2026-08-17).

Also still required for submission per Amazon's Fire TV image-asset
guidelines (not yet produced here): 3–10 screenshots at 1920×1080, and a
1920×1080 background image (safe area 1214×830).
