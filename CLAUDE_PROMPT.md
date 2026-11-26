# Skyframe continuation prompt

Read `CLAUDE.md` first. Then inspect `docs/PROJECT_STATUS.md`, `docs/ROADMAP.md`, `docs/ARCHITECTURE.md`, `docs/DEVELOPMENT_LOG.md` and `docs/CLAUDE_HANDOFF.md`.

Continue the existing C++20/raylib aircraft sandbox. Do not restart it, change engines/languages, gate content behind DLC, add weapons, or discard the current implementation. Preserve all permanent docs and add a dated entry to `docs/DEVELOPMENT_LOG.md` for every substantive change.

Current implemented systems include the free 18-aircraft catalog, flight/fault simulation, deterministic seeded terrain and airport generation, weather and time-of-day state, and versioned persistent replay files with HUD list/load/rename/delete controls. Read the source before modifying any of these systems.

First priority: compile the *current* desktop renderer/HUD with raylib, then launch and inspect it at 1280x720, 1600x900 and 1920x1080. The current development environment had no raylib, so the newly changed graphical code is not yet verified. Do not claim otherwise. Test map options, visual weather, time-of-day, replay persistence, and resize/input behavior. Core GCC/Clang/sanitizer tests should remain green.

The README must embed genuine screenshots from the current executable after visual verification. Do not pass off architecture diagrams, concept art or the Roblox reference as game screenshots. The Windows port is a planned later phase, after the Linux offline sandbox is complete and tested on the Radeon 610M target; reuse this C++20/raylib codebase.
