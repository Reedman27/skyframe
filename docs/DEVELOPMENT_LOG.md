# Skyframe development log

This log records substantive engineering work and its verification boundary. It is intentionally kept in the repository so a future contributor can understand what changed without relying on chat history.

## 2026-11-25 — Random world selection and independent map feature controls

### User-facing behavior

- Added a **Random** terrain selection and made it the default new-session mode. It deterministically resolves to one of the ten supported terrain themes from the current map seed, so the same seed reproduces the same terrain theme and generated layout.
- Added a **New Random Seed** control. It creates a new nonzero seed; users can retain the displayed seed to reproduce a world.
- Split the previous combined towers-and-power-lines switch into independent **Towers** and **Power Lines** controls.
- Added a separate **Natural Scenery** switch for terrain props. Airport generation and airport class remain independent controls.
- The Map panel now shows the concrete generated terrain while Random is selected, alongside size, airport, seed and scenery controls.
- Power-line poles are now connected by visible low-poly wire segments in the renderer.

### Simulation and persistence changes

- Extended `MapConfig` equality and the generated-map fingerprint to include each independent feature switch and the resolved terrain theme.
- Random terrain resolution uses a separate deterministic PRNG stream derived from the configured seed, so it does not perturb feature placement or airport selection.
- Disabled scenery skips natural terrain props without disabling terrain height generation, towers, power lines or the airport.
- Towers and power lines are generated independently and remain aligned to the sampled terrain height.
- Bumped `.sfr` replay writes to version 2 so all independent map switches are preserved. The loader remains backward-compatible with version 1; its former combined infrastructure flag maps to both towers and power lines, with natural scenery enabled by default.

### Regression coverage

- Same-seed Random terrain resolves to the same concrete biome and identical world fingerprint.
- Different seeds can resolve to different biomes.
- Towers can be generated without power lines, and power lines without towers.
- Natural scenery can be disabled without affecting the rest of the map generator.
- Version 2 replay round trips preserve independent map switches; a synthesized version 1 fixture remains loadable.

### Verification

- GCC 14.2 Release core build with warnings-as-errors: pass; CTest 1/1.
- Clang 17 Release core build with warnings-as-errors: pass; CTest 1/1.
- GCC AddressSanitizer + UndefinedBehaviorSanitizer build: pass; CTest 1/1.
- The current environment does not have raylib headers/libraries, so the changed world renderer and Map HUD still require a real raylib desktop build and visual check. This log does not claim that the updated graphical UI was launched here.

### Files changed

- `src/skyframe/sim/environment.hpp`, `environment.cpp`
- `src/skyframe/sim/simulation.hpp`, `simulation.cpp`
- `src/skyframe/sim/replay_archive.cpp`
- `src/skyframe/render/world.cpp`
- `src/skyframe/ui/hud.cpp`
- `tests/sim_tests.cpp`
- `README.md`, `CHANGELOG.md`, `docs/ARCHITECTURE.md`, `docs/PROJECT_STATUS.md`, `docs/ROADMAP.md`, `docs/DEVELOPMENT_LOG.md`

## 2026-11-25 — Repository recovery, seeded worlds, atmosphere and replay persistence

### Starting point and repository recovery

- Started from the user's uploaded `Skyframe-GitHub-Ready.zip`, not an older source snapshot.
- The archive had a Git `HEAD` pointing at `refs/heads/main`, but the branch reference directory was absent. The surviving index described 38 staged paths, while the working tree also contained newer modifications and additional untracked source files.
- The object database did not contain a recoverable original commit/tree graph. The surviving index was preserved as a reconstructed baseline commit on `main`; this is explicitly **not** represented as recovery of the original history.
- Preserved the newer working-tree source changes for a follow-up implementation commit. No destructive reset/clean operation was used.
- Updated `.gitignore` to exclude build output, CMake/Ninja artifacts, logs, local secrets and editor state. Added the exact root rule `/SKYFRAME-QUICK-REVIEW.md` so the private review handoff does not enter the public repository.

### Simulation and world generation

- Expanded environment configuration to ten terrain themes, three map sizes, deterministic seeds, optional airport generation and airport class selection, plus optional towers/power lines.
- Implemented a deterministic terrain-height function using seeded value/gradient noise and terrain-specific elevation profiles.
- Added a cached 33-by-33 height field (32-by-32 cells) to each generated map. The renderer reads the cached samples and submits two triangles per cell (2,048 triangles); it does not run the terrain noise function on every frame.
- Flattened the generated runway corridor so the airport pad is aligned with the runway rather than cutting across uneven terrain.
- Terrain themes have distinct profiles for training field, coast, highlands, desert, hills, plains, city, ocean, tundra and mountains. Feature placement and map fingerprints are deterministic for an identical `MapConfig` and seed.
- Updated the world renderer to draw low-poly terrain triangles, terrain props, airport/runway/apron/hangar elements, optional infrastructure, clouds, rain/snow particles, fog and time-of-day sky colors. Generated maps are cached until the map configuration changes; nearby features are distance-culled.

### Weather and time of day

- Added Auto, Clear, Cloudy, Rain, Storm, Fog, Snow and Windy presets, with deterministic time-based Auto transitions.
- Weather state includes wind vector/direction, gust strength, turbulence, visibility, cloud base, precipitation, fog and lightning intensity.
- Added Morning, Day, Evening, Night and Cycle time modes.
- Kept the environment state in the simulation layer, independent of raylib, and included the complete environment context in replay frames.
- Animated weather visuals use replay/simulation time so the same recorded session can reproduce the same atmosphere timeline.

### Replay persistence

- Added a versioned, human-readable `.sfr` replay archive with local list/load/rename/delete operations.
- Saved frames retain the aircraft snapshot, aircraft preset index, map configuration and seed, weather state, time-of-day state and timeline timestamp.
- Added file/frame/count limits, monotonic timestamp checks, enum and numeric validation, path-component validation, symlink rejection and temporary-file replacement behavior. During final review, fixed a validation-order bug so the passenger capacity check runs after the aircraft snapshot is parsed; a regression fixture now changes a valid recording to an over-capacity aircraft/load pair and verifies the loader rejects it.
- The archive is kept outside the source tree by default: XDG application data on Linux and APPDATA on Windows.
- Expanded replay interpolation so continuous weather/time values transition smoothly while discrete settings remain tied to the recorded frame context.
- Expanded the Replay HUD with saving the current buffer, loading saved recordings, rename/delete actions and pagination so the full 30-entry archive can be reached. Rename input captures keyboard input so aircraft controls do not fire while typing.

### Documentation and project direction

- Updated the README with project context, build/test instructions, feature inventory, storage paths and architecture/world SVG diagrams. These SVGs are explicitly labelled as diagrams, not in-game screenshots.
- Added a screenshots directory policy: actual gameplay captures should only be embedded after the current desktop build is launched and visually verified.
- Kept all content free and unlocked. No DLC or game-pass system was introduced. (Weapons policy superseded 2026-11-26: see the ordnance entry below.)
- Recorded the future Windows port as a deferred milestone that reuses the C++20/raylib codebase only after the Linux offline sandbox is complete, stable and tested on target hardware.

### Source files changed or added

- `src/skyframe/sim/environment.hpp` and `environment.cpp`
- `src/skyframe/sim/replay.hpp` and `replay.cpp`
- `src/skyframe/sim/replay_archive.hpp` and `replay_archive.cpp` (new)
- `src/skyframe/sim/simulation.hpp` and `simulation.cpp`
- `src/skyframe/sim/aircraft.hpp`
- `src/skyframe/render/world.hpp` and `world.cpp`
- `src/skyframe/ui/hud.hpp` and `hud.cpp`
- `src/main.cpp`
- `tests/sim_tests.cpp`
- `.gitignore`, `README.md`, `CLAUDE.md`, `CLAUDE_PROMPT.md`, `CHANGELOG.md`
- `docs/ARCHITECTURE.md`, `docs/CLAUDE_HANDOFF.md`, `docs/PROJECT_STATUS.md`, `docs/ROADMAP.md`, `docs/PERFORMANCE.md`, `docs/GITHUB_SETUP.md`
- `docs/images/skyframe-architecture.svg`, `docs/images/procedural-world-themes.svg`
- `docs/screenshots/README.md`

### Verification performed

- GCC Release core configuration with warnings-as-errors: build and CTest pass (1/1).
- Clang Release core configuration with warnings-as-errors: build and CTest pass (1/1).
- GCC AddressSanitizer + UndefinedBehaviorSanitizer configuration: build and CTest pass (1/1).
- Desktop CMake preset: core/tests build and CTest pass (1/1), but the configure step reports raylib unavailable and does not create the application executable.
- Desktop CMake preset: configured and built the headless fallback; raylib was not installed, so this did not build the graphical executable.
- Current renderer/HUD/main translation units passed a C++20 syntax-only check against a temporary local raylib declaration shim. This is a syntax check only; it is not a substitute for compiling against the real raylib headers/library.
- The source tree does not contain verified current-version gameplay screenshots. The previous Claude report described screenshots for an earlier desktop revision; they are not reused as proof of the current renderer.

### Remaining verification and work

- Compile and launch the current desktop target against the Fedora-provided raylib version.
- Check the renderer visually, including terrain seams/airport pad alignment, each atmosphere preset, replay playback, HUD pagination/rename and window resizing.
- Capture genuine in-game screenshots from the current revision and add them to `docs/screenshots/`, then embed them in README.
- Profile CPU/GPU frame time, draw calls and memory on the target Radeon 610M; no real-hardware performance claim has been made.
- Improve authored terrain/airport detail, aircraft cockpits/handling personalities, sound, and damage presentation as listed in the roadmap.
- Defer Windows porting until the Linux offline sandbox and target-hardware checks are complete.

### Reproducible verification commands

The passing checks above were produced with these commands from the repository root:

```sh
cmake --preset core-release
cmake --build --preset core-release
ctest --preset core-release

cmake -S . -B out/core-clang -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSKYFRAME_BUILD_GAME=OFF -DSKYFRAME_BUILD_TESTS=ON \
  -DSKYFRAME_WARNINGS_AS_ERRORS=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build out/core-clang --parallel 2
ctest --test-dir out/core-clang --output-on-failure

cmake --preset core-sanitize
cmake --build --preset core-sanitize
ctest --preset core-sanitize

cmake --preset desktop-release
cmake --build --preset desktop-release
ctest --test-dir out/desktop-release --output-on-failure
```

The desktop configure step reports `raylib was not found`; the resulting desktop preset is explicitly only the headless core/test fallback. The separate syntax-only renderer check used a temporary raylib declaration shim at `/tmp/skyframe-raylib-stubs`:

```sh
g++ -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  -I/tmp/skyframe-raylib-stubs -Isrc -fsyntax-only \
  src/main.cpp src/skyframe/render/world.cpp \
  src/skyframe/render/aircraft_model.cpp src/skyframe/ui/hud.cpp
```

That command passed, but the temporary shim is not part of the project and the result does not verify compatibility with a real raylib installation.

The local Git repository was also checked with `git fsck --full`, a clean `main` status, a fresh `git clone` followed by a clean core build/CTest run, and a `git bundle` verification/clone. The owner has not configured a remote and no content has been pushed to GitHub.

## 2026-11-26 - first real desktop build, verification pass, ordnance sandbox

Verified on Ubuntu 24.04, GCC 13.3, CMake 4.4, raylib 5.5 built from source, Xvfb + Mesa llvmpipe (software rendering; frame rates are not target-hardware numbers).

- Desktop target compiles and links with zero warnings; launched, rendered and resized (1024x720 to 1920x1080) without GL errors. Core tests pass in Release and ASan+UBSan.
- Replay archive verified end to end in the real app: save, restart, list, load, play, rename (typed input) and delete; files live in `~/.local/share/skyframe/replays/`, never in the repo.
- Fixed: runway/apron z-fighting (raised the near clip plane; `rlSetClipPlanes` in `world.cpp`). Fixed: HUD panel collisions below ~725 px height (minimum window now 1024x720; layout checked at 1024x720, enforcement not checkable without a window manager). FPS counter moved off the aircraft panel.
- Added (owner request): F-22 Raptor and B-52 Stratofortress; ordnance sandbox (`sim/ordnance.*`), static target range with crash-test dummies, bomb/explosion/arcade-nuclear rendering, Ordnance HUD page, `X`/`Z`/hold-`L` controls. Renderer engine slots raised from 4 to 8 for the B-52. New tests cover tiers, determinism, blasts, cluster split, bounds and simulation gating.
- Known issues (not fixed): flying off the map edge leaves no ground/horizon; night darkens only the sky; Windy has no visual cue; replay playback bar overlaps the Flight Controls panel; "Coastal approach" label overflows its button; ordnance is not recorded in replays; B-52 shows 1/1 occupants.
