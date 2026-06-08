# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This project uses **Bazel with Bzlmod** (MODULE.bazel). C++17 is enforced globally via `.bazelrc`.

```bash
# Build the game binary
bazel build //:game

# Run the game
bazel run //:game

# Build a specific library
bazel build //:slide
bazel build //:communication
```

There are no tests yet. When adding tests, use `cc_test` targets in `BUILD.bazel`.

## Dependencies

- **raylib 5.5** — fetched from source via `http_archive` in `MODULE.bazel`; compiled via `third_party/raylib.BUILD` using the GLFW desktop backend with OpenGL 3.3. Not in the Bazel Central Registry, so it is built from scratch.
- **boost.asio 1.90** — from BCR; used for UDP networking.

On Linux, raylib links against `-lGL -lX11 -lm -lpthread -ldl -lrt`. Make sure X11 and OpenGL dev packages are installed.

## Architecture

This is an early-stage multiplayer breakout-style game. There are two Bazel libraries:

- **`//:slide`** (`src/slide.cpp`, `include/slide.hpp`) — `Slide` is a paddle/bar that knows its window bounds, velocity, and whether it is a player paddle (bottom) or villain paddle (top). It handles movement clamping, drawing via raylib, and window-resize recalculation.

- **`//:communication`** (`src/communication.cpp`, `include/communication.hpp`) — UDP networking via Boost.Asio. `Server` binds on port 5000 and receives frames. `Client` resolves `192.168.56.102:5000` (hardcoded) and sends frames. Frame size is fixed at 1200 bytes. `Client::ReadFrame()` is declared but not yet implemented.

- **`//:game`** (`src/main.cpp`) — entry point; creates player and villain `Slide` instances, a `Client`, and drives the raylib 60 FPS render loop with keyboard input (LEFT/RIGHT arrows). Server-mode vs client-mode is gated by `SERVER_SIDE` / `CLIENT_SIDE` preprocessor macros (currently in-progress — the client send loop runs before `InitWindow`, blocking the game from starting).

`-Wswitch` is enabled on `slide` and `game` targets so unhandled `Direction` enum values produce a compiler warning.

## Known In-Progress Issues

- `Server::ReadFrame()` is missing a `return` statement (UB — returns a dangling reference).
- `Slide::Resize()` always places the paddle at `aYWindowSize - mSlideHeight`, so the villain paddle's top-edge position is not restored after a resize.
- `communication.hpp` contains stale commented-out code from early prototyping.

## Key Conventions

- Member variables are prefixed with `m` (e.g., `mX`, `mVelocity`), parameters with `a` (e.g., `aDirection`, `aBuffer`), and local variables with `l` (e.g., `lSlide`, `lClient`).
- Headers live in `include/`, sources in `src/`. The `includes = ["include"]` attribute in BUILD propagates `-Iinclude` so headers are included without a path prefix.
