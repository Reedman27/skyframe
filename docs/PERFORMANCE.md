# Performance targets

The initial target is the user's Ryzen 5-class low-power laptop with Radeon 610M integrated graphics, about 5.6 GiB usable RAM, and dual 1920x1080 60 Hz displays.

## Defaults

- 1600×900 resizable window, 60 FPS cap, minimum window size 1024×720 (fixed-pixel HUD needs about 725 px of height).
- No real-time shadows, deferred renderer, post-processing chain, or high-resolution texture sets.
- Procedural low-poly aircraft and a cached 33×33 terrain height field (32×32 cells / 2,048 terrain triangles per map).
- No external physics engine; simulation uses a small vector/quaternion math layer.
- Fixed 120 Hz simulation with bounded catch-up count.
- Replay buffer capped at 1,200 snapshots (about two minutes at a 0.1-second capture interval).
- Build with two jobs to avoid unnecessary build-time memory pressure.
- Keep the core headless and free of graphics dependencies.

## Profiling before adding complexity

Do not add an ECS, job system, custom allocator, or second language based on guesswork. First profile a representative scene on the target laptop. If performance becomes a problem, inspect frame time and draw-call count, then reduce world detail and replay retention. Keep simulation state deterministic where practical so regression tests stay reliable.

## Current profiling status

The geometry budget above is a source-level design constraint, not a measured frame-time result. Current renderer performance, draw-call count, memory use and frame pacing have **not** been measured on the Radeon 610M. The current desktop renderer also still needs a build against the target raylib package and a visual pass before screenshot or performance claims are made.
