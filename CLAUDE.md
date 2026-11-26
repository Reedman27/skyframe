# Skyframe — persistent Claude project instructions

This file is the canonical, always-on instruction set for Claude Code and other coding agents working in this repository. Read it before making changes. Then read `docs/CLAUDE_HANDOFF.md`, `docs/PROJECT_STATUS.md`, `docs/ARCHITECTURE.md`, `docs/PERFORMANCE.md`, and `docs/ROADMAP.md` before beginning substantial work.

## Project identity

Skyframe is an independent, lightweight native 3D aircraft sandbox. Its gameplay inspiration is the user's supplied Roblox reference: choose aircraft, configure a session, fly or run hands-off, trigger fictional system failures, observe stylized component changes, and review a report/replay.

This repository is the source of truth. Inspect and extend the existing implementation. Do not replace it with a fresh scaffold or silently change the project identity.

## Non-negotiable architecture

- Primary language: **C++20**.
- Graphics and input: **raylib**.
- Build system: **CMake** with the checked-in presets.
- The simulation core must remain independent of raylib and testable headlessly.
- Keep the local single-player sandbox useful without a network connection.
- Do not add a second engine language (Zig, Rust, Go, C#, etc.), a heavyweight engine, ECS, job system, or external physics library without profiling evidence and explicit owner approval.
- Design for the documented Radeon 610M / low-memory Fedora laptop target. Prefer low-poly assets, conservative draw calls, bounded replay memory, and two build jobs.
- Aircraft handling and failures are fictional arcade gameplay. Do not add real-world crash reconstruction, realistic weapon engineering/specifications, operational attack procedures, or aviation-training claims. Arcade ordnance (bombs, explosions, an arcade nuclear blast) is allowed against static props and crash-test dummies only (owner decision, 2026-11-26): no human characters, no real people or places as targets, no moving or firing enemies unless the owner asks.

## Repository documents that must be preserved

These files are part of the deliverable, not disposable notes. **Never delete, rename, move, overwrite with placeholders, or omit them from a ZIP/repository handoff unless the owner explicitly asks.** Update their existing content in place when the project changes:

- `README.md` — public project overview, feature status, build instructions, controls, limitations.
- `LICENSE` — project license; do not replace or remove.
- `CLAUDE.md` — these persistent agent instructions.
- `CLAUDE_PROMPT.md` — concise reusable continuation prompt.
- `CONTRIBUTING.md` — contribution and verification guidance.
- `SECURITY.md` — private vulnerability-reporting guidance.
- `docs/ARCHITECTURE.md` — subsystem boundaries and design decisions.
- `docs/CLAUDE_HANDOFF.md` — detailed engineering handoff and next tasks.
- `docs/PERFORMANCE.md` — hardware budget and profiling constraints.
- `docs/PROJECT_STATUS.md` — verified versus unverified implementation status.
- `docs/ROADMAP.md` — public, GitHub-ready phased roadmap.
- `docs/DEVELOPMENT_LOG.md` — dated engineering log of implementation, verification and limitations.
- `docs/images/` — original README diagrams.
- `docs/screenshots/` — capture policy and future verified in-game screenshots.
- `.github/workflows/ci.yml` and `.github/ISSUE_TEMPLATE/` — repository automation and issue templates.

Do not add any of the above to `.gitignore`. A packaging or cleanup task must preserve them in the source tree and every source ZIP. If a document becomes outdated, revise it; do not delete it. The roadmap is intentionally retained as a tracked project document and should be updated using honest checkboxes, not silently discarded.

## Development rules

1. Inspect the relevant source and current tests before editing.
2. Make focused changes that preserve existing working behavior.
3. Keep simulation logic in `src/skyframe/sim/` and value/math primitives in `src/skyframe/core/`. Do not leak raylib headers into the core library.
4. Add a deterministic regression test for every fixed bug. Validate numeric input and guard against NaN/Inf propagation.
5. Keep memory bounded, avoid per-frame allocations where practical, and do not introduce unneeded dependencies.
6. Update `README.md`, `docs/PROJECT_STATUS.md`, and `docs/ROADMAP.md` when implementation status or user-facing behavior changes. Update the handoff/prompt when the next task or architecture changes.
7. Never claim the desktop app works until it has actually been compiled, launched, and checked. Clearly separate core test results from graphical runtime verification.
8. Keep the README visual: include real screenshots captured from the current executable, with captions and resolution notes. Do not present mockups, SVG concept diagrams, the Roblox inspiration image, or software-renderer placeholders as actual gameplay screenshots. If no current in-game screenshot can be captured, say so and keep the screenshot gallery clearly marked as pending.
9. Do not commit build output, generated binaries, caches, local settings, credentials, or machine-specific files.
10. Record every meaningful coding session in `docs/DEVELOPMENT_LOG.md`, including changed files, commands, test results, and anything not verified. Update the log in the same change as the implementation.
11. Keep the code and build system portable: prefer standard C++20 and `std::filesystem`, avoid Linux-only APIs in shared modules, and use platform-specific branches only behind small helpers.
12. A Windows desktop version is a planned later milestone. Do not start a rushed Windows port or change the engine now. Finish and verify the Linux offline sandbox first; then port the same C++20/raylib project to Windows, add MSVC/Windows CI and packaging, and test save paths and controls on Windows. Do not create a separate engine or duplicate codebase.

## Required checks

Use the checked-in presets and keep build parallelism conservative:

```sh
cmake --preset core-release
cmake --build --preset core-release
ctest --preset core-release
```

For changes affecting state, memory, or serialization, also run:

```sh
cmake --preset core-sanitize
cmake --build --preset core-sanitize
ctest --preset core-sanitize
```

For the graphical application on Fedora after installing raylib development files:

```sh
cmake --preset desktop-release
cmake --build --preset desktop-release
ctest --test-dir out/desktop-release --output-on-failure
./out/desktop-release/skyframe
```

If raylib is unavailable, report that desktop compilation/launch is unverified; do not pass off the headless fallback as a graphical build.

## End-of-task checklist

Before finishing any coding session:

- Run the relevant tests and state exact results.
- List the actual files changed.
- Update the roadmap/status documents honestly.
- Confirm all required repository documents listed above still exist and are non-empty.
- Confirm generated build directories and local artifacts remain ignored and are not in the handoff ZIP.
- State anything that could not be built, launched, or verified.
