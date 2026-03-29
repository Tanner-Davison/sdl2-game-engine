# Forge2D

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![SDL3](https://img.shields.io/badge/SDL-3-orange.svg)](https://wiki.libsdl.org/SDL3)
[![License](https://img.shields.io/badge/license-proprietary-red.svg)](#license)

2D platformer engine with integrated authoring tools. Built on SDL3, EnTT, and modern C++.

Forge2D includes a runtime engine, level editor, character creator, enemy designer, and tile animation tool. All game content is data-driven through JSON -- no hardcoded assets, no external editors required.

## Features

**Runtime**
- Entity Component System (EnTT) with data-oriented, cache-friendly design
- Fixed 120 Hz physics with accumulator-based frame decoupling and interpolated rendering
- Multi-pass AABB collision with slope correction, step-up, and moving platform carry
- Combat system with sword hitbox sweeps, enemy stomps, dash, and invincibility frames
- Camera with deadzone follow, exponential smoothing, screen shake, and directional punch
- Multi-directional gravity (down/up/left/right) for wall-run levels
- Parallax scrolling backgrounds with per-layer scroll factors
- Dual-track audio engine with per-animation SFX, time-stretch, and background music

**Authoring Tools**
- Level editor with tile palette, multi-tool workflow, grid snap, undo, pan/zoom, and play-test
- Character creator with 8 animation slots, per-slot SFX, hitbox handles, and drag-and-drop sprites
- Enemy designer with configurable speed, health, animation states, and per-slot SFX
- Tile animation creator for building animated sequences from PNG folders
- Parallax background editor with adjustable scroll factors and infinite repeat

## Quick Start

**Prerequisites:** [vcpkg](https://github.com/microsoft/vcpkg)

```bash
# Install dependencies
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf sdl3-mixer entt nlohmann-json --triplet arm64-osx

# Build
cmake --preset mac-arm && cmake --build --preset mac-arm

# Run (from project root)
./build/forge2d
```

Available presets: `linux`, `mac-arm`, `mac-intel`, `windows`

<details>
<summary>Manual CMake</summary>

```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=~/tools/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=<your-triplet>
cmake --build build
```
</details>

## Architecture

Forge2D uses an ECS architecture. Components are plain structs in `Components.hpp`. Systems are stateless free functions under `include/systems/`, one per file. No inheritance in the hot path.

The game loop uses a [fixed timestep](https://gafferongames.com/post/fix_your_timestep/) -- physics ticks at 120 Hz, rendering interpolates between frames via `PrevTransform` and an alpha factor.

**Systems pipeline** (per physics tick):
```
Input → Movement → Ladder → Bounds → PlatformTick → Collision
→ PlatformCarry → Floating → PlayerState → Animation → SFX → Camera
```

Scenes inherit from `Scene` and implement the standard lifecycle (`Load`, `Update`, `Render`, `Unload`). `SceneManager` owns the active scene and handles transitions.

The audio engine composes `AudioDevice`, `SoundBank`, and `MusicPlayer` behind an `AudioEngine` facade. Two independent playback tracks keep looping SFX (footsteps) from being interrupted by one-shot sounds (slash, hurt).

The level editor is decomposed into focused subsystems (`EditorToolbar`, `EditorCanvasRenderer`, `EditorUIRenderer`, `EditorPalette`, `EditorPopups`, `EditorFileOps`, `EditorSurfaceCache`). Tools inherit from `EditorTool` and operate through a decoupled `EditorToolContext`.

Source in `src/`, headers in `include/`. Engine-layer headers live in `include/engine/` and have no knowledge of game-specific types. Game-layer headers in `include/game/` depend on the engine, never the reverse.

## Controls

**Gameplay**

| Key | Action |
|-----|--------|
| A / D | Move |
| Space | Jump (hold for higher) |
| F | Slash |
| Double-tap A/D | Dash |
| Ctrl | Crouch |
| W / S | Climb (on ladder) |
| ESC | Pause |
| F1 | Debug hitboxes |

Gamepad supported (Xbox layout). Hot-plug enabled.

**Level Editor**

| Key | Action |
|-----|--------|
| Arrow keys / Middle-drag | Pan |
| Scroll | Zoom |
| 1-9 | Switch tools |
| Delete | Remove selection |
| Ctrl+S | Save |
| Ctrl+Z | Undo |

## License

Copyright (c) 2025 Tanner Davison. All Rights Reserved.
