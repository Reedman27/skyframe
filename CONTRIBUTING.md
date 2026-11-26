# Contributing to Skyframe

Thanks for helping improve Skyframe. The project is an early C++20/raylib aircraft sandbox, so correctness, a small dependency footprint, and honest feature status matter more than adding systems quickly.

## Before opening an issue or pull request

- Search existing issues and pull requests to avoid duplicates.
- For bugs, include the operating system, compiler, raylib version (for desktop issues), exact reproduction steps, and relevant logs. Remove usernames, tokens, and other private information from logs.
- For feature proposals, explain the player-facing problem and why the change fits the lightweight single-player-first direction.

## Development setup

On Fedora, install the toolchain and raylib development package:

```sh
sudo dnf install gcc-c++ cmake ninja-build raylib-devel
```

Build and test the renderer-independent simulation core:

```sh
cmake --preset core-release
cmake --build --preset core-release
ctest --preset core-release
```

Build the desktop app when raylib is installed:

```sh
cmake --preset desktop-release
cmake --build --preset desktop-release
ctest --test-dir out/desktop-release --output-on-failure
./out/desktop-release/skyframe
```

Run the sanitizer configuration for changes to state management, replay, or persistence:

```sh
cmake --preset core-sanitize
cmake --build --preset core-sanitize
ctest --preset core-sanitize
```

The checked-in presets use at most two parallel build jobs by default for the low-memory target hardware.

## Engineering guidelines

- Keep C++20 as the single engine language, raylib as the graphics/input layer, and CMake as the build system.
- Keep `skyframe_core` independent of raylib so tests can run headlessly.
- Add tests for simulation behavior and regression tests for bug fixes.
- Prefer deterministic behavior and explicit bounds for replay/session memory.
- Avoid heavyweight dependencies and premature architecture such as an ECS or job system. Explain the concrete problem a dependency solves.
- Keep failures and damage fictional and non-instructional. This project is not an aviation training tool or real-world crash simulator.
- Do not add weapon or anti-aircraft systems, real-world crash reconstruction, or operational attack procedures.
- Do not commit generated build output, IDE state, secrets, or machine-specific settings.
- Update `README.md`, `docs/PROJECT_STATUS.md`, and `docs/ROADMAP.md` when user-visible functionality or implementation status changes.
- Preserve the repository's permanent documentation and agent instructions listed in `CLAUDE.md`.

## Pull requests

A good pull request should:

1. Explain the user-facing change and why it is needed.
2. Describe the important implementation decisions.
3. List exact configure/build/test commands and their results.
4. Identify graphical behavior that still needs testing on real hardware.
5. Update documentation and roadmap checkboxes where appropriate.
6. Stay focused; split unrelated changes into separate pull requests.
