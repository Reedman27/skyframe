# Verified gameplay screenshots

This directory is reserved for screenshots captured from the **current Skyframe executable**.

Do not put concept art, mockups, the Roblox inspiration image, editor previews, or screenshots from an older build here and label them as current gameplay. The renderer must first compile and launch successfully against the actual raylib version being tested.

For each capture, record:

- Skyframe revision or Git commit
- Window resolution
- GPU and graphics backend (for example, Radeon 610M / Mesa RADV)
- Scene settings (aircraft, map seed/theme, weather and time of day)
- Whether the capture was made on target hardware or in a virtual/software-rendered environment

Use descriptive file names, for example `fleet-1920x1080-radeon610m.png` or `mountains-storm-1600x900.png`. Embed verified images in the root README with concise captions. Do not commit excessively large duplicate captures; optimize PNGs losslessly where possible.

## Current captures (2026-11-26)

All captured from the current executable in a virtual, **software-rendered** environment, not target hardware: Ubuntu 24.04, GCC 13.3, raylib 5.5, Xvfb 24-bit, Mesa llvmpipe (OpenGL 4.5). Frame rates in the images are meaningless for performance. Git revision: working tree on top of the initial commit (uncommitted).

| File | Resolution | Scene |
|---|---|---|
| `01-default-1280x720.png`, `02-default-1600x900.png`, `03-default-1920x1080.png` | as named | Default start: Training Field, Clear, Day, Skyline Jumbo #3 |
| `04-fleet-military-f22-b52.png` | 1600x900 | Fleet > Military tab |
| `05-ordnance-f22-locked-bombs.png` | 1600x900 | F-22 Raptor, Ordnance panel, heavier bombs locked |
| `06-ordnance-b52-all-bombs.png` | 1600x900 | B-52, Nuclear selected |
| `07`/`08`/`09-nuclear-blast-*-lookback.png` | 1600x900 | Arcade nuclear blast, paused at three moments, look-back camera |
| `10-minimum-size-1024x720.png` | 1024x720 | HUD layout at the minimum window size |
