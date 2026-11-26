# Skyframe roadmap

This is the public, tracked roadmap for the repository. A checkbox means implementation and verification, not merely the existence of a source file or mockup. All features remain free and unlocked.

## P0 — Build and verify the desktop foundation

- [x] Keep the simulation independent from raylib.
- [x] Implement a 120 Hz fixed-step simulation with bounded catch-up work.
- [x] Add renderer-independent regression tests.
- [x] Add GCC, Clang and sanitizer core CI.
- [x] Add a free 18-aircraft unarmed fleet with procedural low-poly models.
- [x] Add bounded rolling replay and interpolation.
- [x] Repair imported Git metadata and commit the reconstructed source baseline.
- [x] Add a versioned local replay archive with list/load/rename/delete and validation.
- [ ] Compile, launch and interact with the actual raylib desktop app on Fedora 45.
- [ ] Verify keyboard/mouse controls, window resizing and dual-1080p usability on the target laptop.
- [ ] Profile a representative generated world on Radeon 610M and record frame time/draw-call observations.

## P1 — Make the sandbox feel like a real game

- [x] Add 10 seeded procedural terrain themes, a seed-driven Random biome selector, and Small/Medium/Large map sizes, including a cached 33×33 height field and flattened runway corridor.
- [x] Add airport generation (By Map, Random, International, Regional, Military, Airfield) and independent airport, towers, power-lines and natural-scenery toggles.
- [x] Add Auto, Clear, Cloudy, Rain, Storm, Fog, Snow and Windy weather values.
- [x] Add Morning, Day, Evening, Night and Cycle time-of-day state.
- [x] Include map seed, weather and time-of-day in recorded/saved replay frames.
- [x] Add renderer support for a cached low-poly terrain mesh, terrain props, runway/apron/hangars, precipitation, fog overlays and time-of-day sky colors (source complete; graphical build still needs verification).
- [ ] Improve the generated environment with more coherent terrain silhouettes, airport layouts and recognizable taxiway/apron markings.
- [ ] Give each aircraft an authored cockpit shape and a distinct handling personality.
- [ ] Add event-driven audio for engines, alerts, weather, landing and fictional system faults.
- [ ] Improve cockpit presentation with an attitude indicator, airspeed/altitude tapes and engine annunciators.
- [ ] Add scenario setup, keyboard help and accessibility controls.
- [ ] Capture real in-game screenshots and embed them in the README after the graphical app is visually verified.

## P2 — Expand aircraft and scenario systems

- [ ] Move aircraft/scenario definitions into validated, versioned data files.
- [ ] Add named visual attach points and component state for aircraft presentation.
- [ ] Add stylized, non-realistic break-away panels and bounded debris/particle effects.
- [ ] Add scenario events for timed failures, passenger manifests and weather transitions.
- [ ] Add versioned local save/load for aircraft presets, scenarios and session settings.
- [ ] Add accessibility settings: key remapping, UI scale, color-safe warnings and reduced visual effects.
- [ ] Add low/medium/high graphics presets with conservative defaults.

## P3 — Sharing and online features (only after local stability)

- [ ] Design an optional server-authoritative shared counter after local persistence is stable.
- [ ] Add opt-in feedback submission with clear privacy expectations.
- [ ] Consider anonymous identities/accounts only if shared features require them.
- [ ] Consider multiplayer only after documenting the network model and measuring a realistic performance budget.

## P4 — Windows desktop version (deferred)

This phase starts **only after P0 and the offline portions of P1/P2 are complete, the Linux desktop app has been tested on the target hardware, and local replay/save behavior is stable**.

- [ ] Build the existing C++20/raylib codebase with MSVC and CMake on Windows.
- [ ] Add Windows CI and reproducible desktop packaging.
- [ ] Verify Windows keyboard/mouse input, resizing, graphics and replay playback.
- [ ] Verify `%APPDATA%/Skyframe/replays` save/load/rename/delete behavior.
- [ ] Document Windows installation and troubleshooting alongside the Linux instructions.

Do not start a parallel Windows rewrite or change the engine to achieve this port.

## Repository maintenance

- [x] Keep the public roadmap in version control.
- [x] Add persistent root-level Claude instructions that require project guidance files to be preserved.
- [x] Add `.gitignore` for build output, caches, logs, editor state, local secrets and the private `SKYFRAME-QUICK-REVIEW.md` note.
- [x] Add GitHub Actions CI, issue forms and a pull request template.
- [x] Add a dated engineering log, screenshot capture policy and README architecture/world diagrams.
- [ ] Add verified application screenshots to the README gallery.
- [ ] Add release notes and tagged releases after desktop builds are reproducible on target hardware.

## Explicit non-goals

- No paid DLC, game passes or locked content.
- No realistic weapon modelling or real-world targeting; arcade ordnance against static targets only.
- No real-world crash reconstruction or aviation-training claims.
- No required online service for the local sandbox.
- No split C++/Zig/Rust/Go/C# engine and no heavyweight engine replacement without measured evidence and owner approval.

## Ordnance follow-ups

- [ ] Record bombs/blasts in replays.
- [ ] More bomb types (the reference game has 15), bay doors and release animation.
- [ ] Crater decals and persistent rubble across blasts.
- [ ] Optional targets on other maps/airports; score or challenge mode.
