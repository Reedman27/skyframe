# Changelog

All notable Skyframe changes are documented here.

## Unreleased — 2026-11-26

### Added
- F-22 Raptor and B-52 Stratofortress (20-aircraft fleet); renderer supports up to 8 engines.
- Arcade ordnance sandbox: six bomb types, static target range with crash-test dummies, explosion and nuclear-blast visuals, Ordnance panel, `X`/`Z` controls and hold-`L` look-back camera.
- First verified in-game screenshots in `docs/screenshots/`.

### Fixed
- Runway and apron z-fighting (near clip plane). HUD panel overlap at small window heights (minimum window 1024x720). FPS counter no longer overlaps the aircraft panel.

## 2026-11-25

### Added
- Random terrain mode that deterministically selects one of ten biomes from the world seed, with a New Random Seed control.
- Independent map toggles for towers, power lines and natural scenery; airport remains separately configurable.
- Replay archive format version 2 for the independent map toggles, retaining version 1 read compatibility.
- Ten deterministic terrain themes plus seed-driven Random mode, Small/Medium/Large map sizes, configurable seeds and a cached 33×33 height field (2,048 low-poly terrain triangles).
- Airport generation with By Map, Random, International, Regional, Military and Airfield options.
- Independently optional towers, power-line features and natural scenery.
- Auto, Clear, Cloudy, Rain, Storm, Fog, Snow and Windy weather presets with gust, visibility, precipitation, fog and lightning state.
- Morning, Day, Evening, Night and Cycle time-of-day modes.
- Versioned local `.sfr` replay archive with save, list, load, rename and delete support.
- Replay serialization of aircraft snapshots, aircraft preset, map seed/settings, weather and time-of-day context.
- README architecture and terrain-theme SVG diagrams.
- Dated development log, screenshot capture policy and later Windows-port milestone.

### Changed
- World renderer caches deterministic generated maps and draws generated terrain features, airport surfaces/buildings, weather particles and time-of-day sky states.
- HUD exposes the map generator, weather/time controls and saved replay archive.
- README and Claude handoff docs now distinguish diagrams from real gameplay screenshots and require genuine app captures after graphical verification.

### Fixed / Hardened
- Replay archive rejects oversized/corrupt files, invalid enum values, invalid/non-monotonic timestamps, non-finite state, over-capacity passenger loads and unsafe path components.
- Replay save files are written through a temporary file before replacement.
- `.gitignore` explicitly excludes the private `SKYFRAME-QUICK-REVIEW.md` note.
- Reconstructed a usable local `main` branch from the imported staged index because the uploaded archive lacked valid branch refs and commit history.

### Verification boundary
- GCC Release core tests: pass.
- Clang Release core tests: pass (see development log for exact run).
- GCC ASan/UBSan core tests: pass.
- Current raylib desktop renderer/HUD was not compiled or launched in this environment because raylib is unavailable. No gameplay screenshots are claimed for the current revision.
