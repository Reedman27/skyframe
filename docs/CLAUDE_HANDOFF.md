# Skyframe engineering handoff

## Project goal

Build a lightweight, native, offline-first aircraft sandbox inspired by the user's supplied Roblox reference. The owner wants a broad, polished sandbox with a free fleet, configurable worlds/weather, hands-on and unattended flight, fictional faults, cockpit warnings, reports and persistent replays. All content is free; no DLC/game-pass gates. Military-style models are unarmed. Do not recreate real crash forces, attacks or operational weapons.

## Fixed technology choices

- C++20, CMake/Ninja and raylib.
- Keep `skyframe_core` renderer-independent and headlessly testable.
- Do not create a second Zig/Rust/Go/C# engine, add an ECS/job system, or replace raylib without profiling evidence and owner approval.
- Target Fedora Linux, Ryzen 5-class APU, Radeon 610M iGPU, about 5.6 GiB usable RAM. Use modest geometry, bounded state and two build jobs.
- Future Windows port is desired, but only after the Linux offline sandbox is complete/stable and verified. Reuse this codebase with MSVC/raylib; do not start a parallel implementation.

## Current source structure

- `src/skyframe/core/`: vector/quaternion math.
- `src/skyframe/sim/aircraft.*`: arcade aircraft behavior and fictional failures.
- `src/skyframe/sim/fleet.*`: free 18-aircraft catalog/design dimensions.
- `src/skyframe/sim/environment.*`: weather/time-of-day state plus deterministic map/airport generator.
- `src/skyframe/sim/replay.*`: bounded in-memory recorder/interpolator.
- `src/skyframe/sim/replay_archive.*`: versioned persistent replay file manager.
- `src/skyframe/sim/simulation.*`: fixed-step orchestration.
- `src/skyframe/render/aircraft_model.*`: procedural aircraft visual builder.
- `src/skyframe/render/world.*`: terrain/airport/weather/time-of-day renderer.
- `src/skyframe/ui/hud.*`: panels including saved replay actions.
- `tests/sim_tests.cpp`: core regression coverage.

## Current implementation state

Implemented in source:

1. The free 18-aircraft fleet and procedural aircraft silhouettes.
2. Flight controls, arcade flight model, warning cues, fictional failure events, passengers and reports.
3. Rolling replay buffer with interpolation and environment snapshots.
4. Ten seeded map themes, Small/Medium/Large sizes, airport generation/class selection, optional towers/power lines.
5. Auto/Clear/Cloudy/Rain/Storm/Fog/Snow/Windy weather with wind/gust/visibility/precipitation/fog/lightning values.
6. Morning/Day/Evening/Night/Cycle time-of-day state.
7. Versioned `.sfr` replay persistence with list/load/rename/delete and defensive validation.
8. HUD controls for terrain, map size, seed adjustment, airport, infrastructure, weather, time and replay archive.
9. README architecture/world SVG diagrams and a documented screenshot gallery policy; real in-game captures remain pending current desktop verification.

The current terrain renderer reads a cached 33×33 height field (32×32 cells, 2,048 low-poly triangles), with a flattened airport corridor, generated props, weather particles/fog and time-of-day colors. The renderer/HUD changes are source-level only in this environment. raylib is unavailable and external package downloads fail, so do not claim the current desktop target compiled or launched.

## Required local checks

```sh
cmake --preset core-release
cmake --build --preset core-release
ctest --preset core-release

cmake --preset core-sanitize
cmake --build --preset core-sanitize
ctest --preset core-sanitize
```

Also use Clang if available. When raylib is available:

```sh
cmake --preset desktop-release
cmake --build --preset desktop-release
ctest --test-dir out/desktop-release --output-on-failure
./out/desktop-release/skyframe
```

## Important next steps

1. Build the desktop target with raylib and fix any compile errors in `world.cpp`, `hud.cpp` or `main.cpp`.
2. Run the current executable at 1280×720, 1600×900 and 1920×1080; inspect height-field seams, airport pad/runway alignment, terrain clutter, map scale, rain/snow/fog, night lighting and HUD clipping.
3. Test the replay panel end-to-end: record, save, restart, list, load, play, rename, delete. Verify saved paths are outside the Git repository.
4. Profile generated feature counts/draw calls and frame times on Radeon 610M; keep a lower-detail option if needed.
5. Once app launch is verified, capture genuine in-game screenshots and embed them in README with captions/resolution/backend. Follow `docs/screenshots/README.md`; diagrams are not gameplay screenshots and must never be labelled as such.
6. Continue improving airport geometry, terrain composition and aircraft cockpit/handling differences.
7. Keep README, status, roadmap, changelog and development log current after every substantive change.
8. Only after the offline Linux feature set and target hardware verification are complete, plan the Windows port with MSVC CI, packaging and replay-path verification. The owner explicitly wants a Windows version later; do not start it before the Linux milestone.

## Git integrity and preservation

The uploaded archive's original commit history was missing. The surviving staged index was preserved as a synthetic reconstructed baseline commit on `main`; it is not the recovered original history. Preserve the working changes layered on top of that commit. Do not perform a destructive reset/clean to get a tidy status.

Never delete or replace: `README.md`, `LICENSE`, `CLAUDE.md`, `CLAUDE_PROMPT.md`, `CONTRIBUTING.md`, `SECURITY.md`, `CHANGELOG.md`, `docs/ARCHITECTURE.md`, `docs/CLAUDE_HANDOFF.md`, `docs/DEVELOPMENT_LOG.md`, `docs/GITHUB_SETUP.md`, `docs/PERFORMANCE.md`, `docs/PROJECT_STATUS.md`, `docs/ROADMAP.md`, or `.github/` project metadata. Keep `docs/images/` and future verified `docs/screenshots/` tracked.

`SKYFRAME-QUICK-REVIEW.md` is a private one-off handoff note and must stay ignored. Do not ignore the project's permanent Markdown documentation.
