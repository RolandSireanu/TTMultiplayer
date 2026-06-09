# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build System

This project uses **Bazel with Bzlmod** (MODULE.bazel). C++20 is enforced globally via `.bazelrc`.

```bash
# Build and run
bazel run //:server   # server binary (defines SERVER_SIDE)
bazel run //:client   # client binary

# Build a specific library
bazel build //:slide
bazel build //:communication
```

There are no tests yet. When adding tests, use `cc_test` targets in `BUILD.bazel`.

## Dependencies

- **raylib 5.5** — fetched from source via `http_archive` in `MODULE.bazel`; compiled via `third_party/raylib.BUILD` using the GLFW desktop backend with OpenGL 3.3. Not in the Bazel Central Registry, so it is built from scratch.
- **boost.asio 1.90** — from BCR; used for UDP networking.

On Linux, raylib links against `-lGL -lX11 -lm -lpthread -ldl -lrt`. Make sure X11 and OpenGL dev packages are installed. UDP port 5000 must be open on the server machine's firewall.

## Architecture

This is an early-stage multiplayer breakout-style game. There are two Bazel libraries:

- **`//:slide`** (`src/slide.cpp`, `include/slide.hpp`) — `Slide` is a paddle that knows its window bounds, velocity, and whether it is a player paddle (bottom) or villain paddle (top). It handles movement clamping, drawing via raylib, and window-resize recalculation.

- **`//:communication`** (`src/communication.cpp`, `include/communication.hpp`) — UDP networking via Boost.Asio. `Server` binds on port 5000; `ReadFrame()` blocks until a datagram arrives and returns `std::pair<const std::array<std::byte, 1200>&, std::size_t>` — the buffer and actual bytes received. `Client` resolves `192.168.56.102:5000` (hardcoded) at construction; `SendFrame` accepts `std::span<std::byte>` and sends only those bytes. `Client::ReadFrame()` is declared but not implemented.

- **`include/messages.hpp`** — `Messages::SlidePosition` is a packed struct (`mOrderId`, `mX`, `mY` as `int32_t`). Structs here are the wire format; callers convert to bytes with `std::as_bytes(std::span{&msg, 1})` before passing to `SendFrame`. Note: `__attribute__((__packed__))` is currently placed before the struct keyword instead of after it — the correct form is `struct __attribute__((__packed__)) SlidePosition`.

- **`//:server` / `//:client`** (`src/main.cpp`) — same source file compiled two ways via `SERVER_SIDE` define in BUILD. Server path: receive one frame, print each byte as hex. Client path: infinite `SendFrame` loop sending a `SlidePosition` before `InitWindow` (so the raylib window never opens while sending). Both create `Slide` instances for the game window.

`-Wswitch` is enabled on all targets so unhandled `Direction` enum values produce a compiler warning.

## Known In-Progress Issues

- `Slide::Resize()` always places the paddle at `aYWindowSize - mSlideHeight`, so the villain paddle's top-edge position is not restored after a resize.
- `communication.hpp` contains stale commented-out code from early prototyping.
- `Server::SendFrame()` is not implemented.
- `Client::ReadFrame()` is declared but not implemented.
- The client send loop in `main.cpp` blocks before `InitWindow`, so the client never shows a window.
- `__attribute__((__packed__))` placement in `messages.hpp` is before the struct name instead of after it.

## Key Conventions

- Member variables are prefixed with `m` (e.g., `mX`, `mVelocity`), parameters with `a` (e.g., `aDirection`, `aBuffer`), and local variables with `l` (e.g., `lSlide`, `lClient`).
- Headers live in `include/`, sources in `src/`. The `includes = ["include"]` attribute in BUILD propagates `-Iinclude` so headers are included without a path prefix.
- Wire messages are defined as packed structs in `include/messages.hpp` inside the `Messages` namespace. Convert to bytes at the call site with `std::as_bytes(std::span{&msg, 1})`.
