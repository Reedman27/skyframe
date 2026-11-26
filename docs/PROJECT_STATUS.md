# Project status — 2026-11-25

## Implemented in the current source tree

### Simulation and fleet

- C++20 fixed-step simulation with bounded catch-up.
- Renderer-independent aircraft state, flight telemetry, passenger load, reports and fictional failure events.
- 20 free, unlocked aircraft across Small, Medium, Large and Military categories, including the F-22 Raptor (ordnance tier 1) and B-52 Stratofortress (8 engines, ordnance tier 2).
- Arcade ordnance sandbox: six bomb types, ballistic drop, blasts, cluster bomblets, arcade nuclear blast visuals, seeded static target range with crash-test dummies (`sim/ordnance.*`), Ordnance HUD page, `X`/`Z` keys and hold-`L` look-back camera.
- Procedural low-poly aircraft silhouettes with varied fuselage, wing, tail, engine and landing-gear layouts.
- Five flap detents; independent spoilers and parking brake; flight and low-altitude warning cues.
- Bounded rolling replay with interpolation and preservation of environment context.

### Procedural worlds

- Ten generated terrain themes: Training Field, Coast, Highlands, Desert, Hills, Plains, City, Ocean, Tundra and Mountains; plus a Random selector that deterministically chooses a biome from the seed.
- Small, Medium and Large size presets.
- Deterministic seeded map generation using a local PRNG; the same random-terrain selection and seed reproduce the same biome, height field, scenery and airport layout. A New Random Seed control creates a fresh world seed.
- Airport enabled/disabled and By Map/Random/International/Regional/Military/Airfield selection.
- Independent toggles for towers, power lines and natural scenery; airport generation is independently toggleable.
- Each generated map includes a deterministic 33×33 height field over 32×32 cells; the renderer draws 2,048 terrain triangles from cached samples. The runway corridor is flattened for airport placement.
- Renderer caches generated maps and redraws the feature set only when its configuration changes; nearby props are distance-culled.

### Weather and time

- Auto, Clear, Cloudy, Rain, Storm, Fog, Snow and Windy presets.
- Wind vector/direction, gust strength, turbulence, visibility, cloud base, rain, snow, fog and lightning values.
- Auto weather transitions in a fixed deterministic sequence on simulation time.
- Morning, Day, Evening, Night and Cycle time-of-day state.
- Renderer atmosphere support for sky color, cloud cover, rain/snow particles, fog overlay and storm flash.
- Environment values are captured in replay frames.

### Persistent replay archive

- Version 2 text `.sfr` format with an explicit magic/header and quoted display name; version 1 recordings remain readable.
- Saves every replay frame's aircraft snapshot, aircraft preset, map options/seed, weather state and time-of-day state.
- List, load, rename and delete API; Replay HUD exposes save/load/rename/delete controls and paginates the 30-entry list.
- Default storage is outside the source repository: XDG data directory on Linux, APPDATA on Windows (path helper included; Windows runtime is not yet verified).
- Bounds: 1200 frames per replay, 16 MiB per file, 30 visible/saved archive entries.
- Validates headers/version, frame count, monotonic timestamps, enum ranges, numeric bounds, passenger capacity against the selected aircraft and path components before accepting a file.
- Writes through a temporary file before replacing the destination, with a backup/restore fallback for platforms where replacing an existing file by rename is not supported.

## Verification in this development environment

The simulation core was built and tested with:

- GCC 14.2, Release, warnings-as-errors: pass.
- Clang 17, Release, warnings-as-errors: pass.
- GCC AddressSanitizer + UndefinedBehaviorSanitizer: pass.
- CTest regression suite: pass in GCC, Clang and sanitizer configurations.

The tests cover aircraft/failure/flap behavior, deterministic random-biome selection, seed variation, independent tower/power-line/scenery toggles, map height fields, terrain profiles, airport settings, Auto weather transitions, storm/fog values, time-of-day cycle, replay interpolation, version 2 archive round trips, version 1 compatibility, list/rename/delete, path traversal rejection, corrupt archive handling, over-capacity passenger rejection, archive limits and environment restoration.

## Graphical verification boundary

The current development container does not have raylib headers/libraries and cannot resolve external package hosts, so the current renderer/HUD source could not be linked against the real raylib SDK or launched here. CMake's desktop preset falls back to building the core/tests and does not create a graphical executable. The current renderer/HUD/main files also passed a C++20 syntax-only check against a temporary local raylib declaration shim; this verifies C++ syntax and internal declarations only, not the actual raylib ABI/API.

The pasted prior Claude progress report states that an earlier revision of the application was compiled against raylib 5.5 and launched under Xvfb/Mesa software rendering at 1280×720, 1600×900 and 1920×1080. That report predates the current environment/renderer/HUD additions. **It does not verify the current graphical code.**

Still unverified:

- Current desktop renderer/HUD compilation against raylib.
- Actual visual quality of generated terrains, airport layouts, separately toggled towers/power lines, weather and time of day.
- Replay archive interaction via the HUD.
- Fedora 45 / Window Maker / Radeon 610M launch and real GPU frame rate.
- Windows runtime/packaging.

The README contains architecture/world design diagrams, and `docs/screenshots/README.md` records the capture policy. Neither design diagrams nor screenshots reported for an older renderer revision are presented as current gameplay captures. Add current gameplay screenshots only after the latest executable is launched and visually checked.

## Documentation and future platform work

The README includes original architecture and terrain-theme SVG diagrams, with a separate pending gameplay screenshot gallery. The future Windows version is explicitly deferred until the Linux offline sandbox is complete, stable and verified on target hardware; it will reuse this codebase.

## Git repository recovery

The uploaded ZIP contained an index with 38 staged paths and objects, but no commits or valid branch reference; original commit/tree history was not recoverable. The staged index was preserved as reconstructed baseline commit `88a0a114ca9cf77ca026c9faaaa00b34d4027370` on `main`. The implementation and documentation changes are committed on top of that baseline in the final package. This reconstructs a usable repository; it is not a recovery of the original historical commit graph.

`SKYFRAME-QUICK-REVIEW.md` is explicitly ignored. Build directories under `out/` remain ignored and are not intended for the source archive. The local repository was verified with `git fsck --full`, a fresh clone/build/test, and a portable Git bundle clone. The working tree is clean on `main`; no remote is configured and nothing was pushed.
