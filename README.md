# Forge2D

**A complete 2D game engine and toolkit built from scratch in C++23.**

Author: Tanner Davison

Forge2D is a self-contained 2D game engine powered by SDL3, EnTT ECS, and modern C++. It ships with an integrated level editor, character creator, enemy designer, tile animation tool, and a runtime engine with physics, combat, camera effects, and scene management. Every piece of game content — levels, characters, enemies, animated tiles, and backgrounds — is authored through the built-in editor tools. No external editors or hardcoded assets required.

---

## Engine Highlights

### Runtime

- **Entity Component System** — Clean data-oriented architecture using EnTT. Components are plain structs; systems are stateless functions. Hot-path views are cache-friendly and exclude-filtered.
- **Fixed-timestep simulation** — 120 Hz physics with accumulator-based frame decoupling and sub-step interpolated rendering for silky-smooth motion at any display refresh rate.
- **GPU-accelerated rendering** — SDL3 Renderer pipeline with texture atlas caching, sprite sheet batching, and texture persistence across respawns.
- **Camera system** — Smooth lerp follow with configurable dead zones, level-bounds clamping, screen shake (random + directional punch), and event-driven intensity scaling.
- **Physics** — Gravity integration, variable-height jumping, slope traversal, step-up collision, friction, and multi-directional gravity (down, up, left, right) for wall-run levels.
- **Collision** — Multi-pass AABB with slope correction, lateral push-out, stomp detection, sword hitbox sweeps, hazard overlap, ladder grabs, and moving platform carry.
- **Combat** — Sword slash with per-frame hitbox, enemy stomp (configurable % damage), invincibility frames, contact knockback, and progressive camera feedback that intensifies as enemies approach death.
- **Enemy AI** — Patrol-based movement with ledge detection, tile-edge reversal, custom move speed, slope awareness, and full death/hurt/attack animation state machines.
- **Moving platforms** — Horizontal/vertical travel, ping-pong, sine oscillation, grouped sync, and player-trigger activation.
- **Floating objects** — Anti-gravity bob, drift, spin, player push, sword-slash push, object-to-object collision, and tile bounce.
- **Parallax backgrounds** — Multi-layered scrolling backgrounds with per-layer scroll factors, infinite repeat, and full editor integration.
- **Power-up system** — Anti-gravity power-ups with extensible type registry and timed duration.
- **Player death sequence** — Dedicated death animation slot with timed hold, camera shake, dimmed overlay, and graceful transition to the game-over screen.
- **Health bars** — Enemy HP rendered as color-graded bars above each sprite, fading from green to red as health drops.
- **Dash mechanic** — Double-tap directional dash with invulnerability window.

### Editor Tools

- **Level Editor** — Full-featured tile-based editor with palette browser, multi-tool workflow, grid snapping, undo, camera pan/zoom, play-test, and save/load.
- **Character Creator** — 8 animation slots (Idle, Walk, Crouch, Jump, Fall, Slash, Hurt, Death) with drag-and-drop sprite folders, per-slot FPS, and interactive hitbox handles.
- **Enemy Designer** — 5 animation slots (Idle, Move, Attack, Hurt, Dead) with sprite size configuration, custom hitboxes, speed, health, and a live animation preview.
- **Tile Animation Creator** — Build animated tile sequences from PNG folders, preview playback, and export manifest JSON.
- **Backgrounds Panel** — Browse and assign base backgrounds, add parallax layers with adjustable scroll factors, and toggle infinite repeat.

---

## Architecture

### ECS (Entity Component System)

Components are plain data structs defined in `Components.hpp`. Systems are free functions in individual headers under `include/systems/`, aggregated by `Systems.hpp`.

**Core Components:** Transform, PrevTransform, Velocity, Collider, ColliderOffset, Renderable, RenderOffset, AnimationState, AnimationSet, Health, InvincibilityTimer, GravityState, ClimbState, HazardState, AttackState, DashState, PlayerBaseCollider, ActivePowerUps, FloatState, MovingPlatformState, HitFlash, DestroyAnimTag, SlopeCollider, EnemyAnimData, EnemyAttackState

**Tag Components:** PlayerTag, EnemyTag, CoinTag, DeadTag, TileTag, LadderTag, PropTag, HazardTag, GoalTag, FloatTag, MovingPlatformTag, TileAnimTag, OpenWorldTag, ActionTag, PowerUpTag, DestructibleTag

**Systems:**

| System | Responsibility |
|--------|----------------|
| `InputSystem` | Keyboard input — WASD/arrows, space, ctrl, F (slash), W/S on ladders, dash |
| `MovementSystem` | Velocity integration, friction, gravity, jump hold boost, enemy patrol with ledge reversal |
| `PlayerStateSystem` | Animation priority state machine (death > attack > hurt > crouch > airborne > walk > idle) with per-character collider enforcement |
| `AnimationSystem` | Delta-time frame advancement with looping and non-looping support |
| `CollisionSystem` | Multi-pass AABB (slopes, gravity snap, lateral push-out, step-up), enemy stomp/slash, goal collection, hazard overlap, action tile triggers |
| `BoundsSystem` | Level-edge clamping, wall-run gravity flip, open-world bounds |
| `MovingPlatformSystem` | Platform tick (sine/ping-pong/trigger), grouped sync, player carry |
| `FloatingSystem` | Bob oscillation, drift/spin decay, player push, slash push, object collisions, tile bounce, enemy gravity |
| `LadderSystem` | Ladder column detection, climb/descend/top-lock, jump-off, side walk-off |
| `RenderSystem` | GPU-rendered sprites with camera offset, flip, rotation, invincibility flash, hazard flash, hit flash, health bars, and pre-sorted tile draw order |
| `HUDSystem` | Health bar, gravity indicator, goal count, stomp count |

### Scene System

Scenes inherit from `Scene` (load, unload, handle event, update, render, next scene). `SceneManager` owns the active scene and handles transitions.

| Scene | Purpose |
|-------|---------|
| `TitleScene` | Title screen with Play, Editor, Character Creator, and Enemy Creator buttons |
| `GameScene` | Runtime gameplay — owns the ECS registry, all assets, and the fixed-step game loop |
| `LevelEditorScene` | Level editor with modular subsystems (FileOps, Palette, Toolbar, Popups, Camera, Canvas, UI, SurfaceCache) |
| `PlayerCreatorScene` | Character creator with drag-and-drop sprites, hitbox editor, and JSON save/load |
| `EnemyCreatorScene` | Enemy type designer with animation slots, hitbox handles, speed/health fields, and a saved roster |
| `TileAnimCreatorScene` | Animated tile builder from PNG sequences |
| `PauseMenuScene` | In-game pause overlay with resume, back-to-editor, and back-to-title |

### Level Editor Tools

The editor uses a tool abstraction (`EditorTool` base) with a shared `EditorToolContext`:

| Tool | Function |
|------|----------|
| `PlacementTools` | Single tile placement, line fill, rect fill |
| `SelectTool` | Click or marquee-select tiles and enemies, drag to move, delete, multi-select with Shift |
| `ResizeTool` | Drag tile edges/corners to resize |
| `HitboxTool` | Draw custom collider sub-rects per tile |
| `EnemyTool` | Place enemies with ghost preview, speed adjustment (Shift+scroll), and profile-based dimensions |
| `ModifierTools` | Toggle tile properties (prop, hazard, ladder, slope, action, goal, moving platform, power-up, anti-gravity) |

### Supporting Classes

| Class | Role |
|-------|------|
| `Window` | RAII SDL3 window with GPU renderer and fullscreen toggle |
| `Image` | Image loading and rendering with fit modes (Cover, Contain, Stretch, Scroll, ScrollWide) and tiling/repeat |
| `SpriteSheet` | Texture atlas parser (text, XML, or path list) with named animation lookup and GPU upload |
| `Text` | SDL3_ttf rendering with centering helpers and font cache |
| `LevelSerializer` | JSON save/load for levels (tiles, enemies, player spawn, backgrounds, parallax layers, gravity mode) |
| `PlayerProfile` | JSON save/load for character profiles (name, dimensions, per-slot sprites, hitboxes, FPS) |
| `EnemyProfile` | JSON save/load for enemy types (name, dimensions, speed, health, per-slot sprites, hitboxes) |
| `AnimatedTile` | JSON manifest loader for tile animation sequences |
| `GameConfig` | Compile-time gameplay constants (player stats, physics, enemy stats, camera) |
| `GameEvents` | Lightweight result structs returned by systems (`CollisionResult`, `FloatingResult`) |

---

## Building

### Prerequisites

Dependencies are managed via [vcpkg](https://github.com/microsoft/vcpkg):

| Dependency | Purpose |
|------------|---------|
| SDL3 | Windowing, rendering, input |
| SDL3_image | PNG sprite loading |
| SDL3_ttf | Font rendering |
| EnTT | Entity Component System |
| nlohmann_json | Level/profile serialization |

**Install vcpkg** (if not already installed):
```bash
git clone https://github.com/microsoft/vcpkg ~/tools/vcpkg
cd ~/tools/vcpkg && ./bootstrap-vcpkg.sh
```

**Install dependencies:**

```bash
# Linux
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf entt nlohmann-json

# macOS Apple Silicon
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf entt nlohmann-json --triplet arm64-osx

# macOS Intel
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf entt nlohmann-json --triplet x64-osx
```

### Build & Run

```bash
# Linux
cmake --preset linux && cmake --build --preset linux

# macOS Apple Silicon
cmake --preset mac-arm && cmake --build --preset mac-arm

# macOS Intel
cmake --preset mac-intel && cmake --build --preset mac-intel

# Run (from project root so asset paths resolve)
./build/forge2d
```

**Manual CMake (no presets):**
```bash
cmake -B build \
  -DCMAKE_TOOLCHAIN_FILE=~/tools/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=<your-triplet>
cmake --build build
./build/forge2d
```

---

## Project Structure

```
forge2d/
├── src/                           # Implementation files
│   ├── main.cpp                   # Entry point — fixed-step game loop
│   ├── GameScene.cpp              # Runtime gameplay scene
│   ├── LevelEditorScene.cpp       # Level editor core
│   ├── Editor*.cpp                # Editor subsystems (Canvas, UI, Popups, Palette, FileOps)
│   ├── PlayerCreatorScene.cpp     # Character creator
│   ├── EnemyCreatorScene.cpp      # Enemy type designer
│   ├── TileAnimCreatorScene.cpp   # Tile animation creator
│   ├── TitleScene.cpp             # Title screen
│   └── tools/                     # Editor tool implementations
├── include/
│   ├── systems/                   # ECS system headers (one per system)
│   ├── tools/                     # Editor tool class headers
│   ├── engine/                    # Scene, SceneManager base classes
│   ├── Components.hpp             # All ECS component definitions
│   ├── Systems.hpp                # System aggregator
│   ├── GameScene.hpp              # Game scene header
│   ├── LevelEditorScene.hpp       # Editor scene header
│   ├── LevelData.hpp              # Level data structures (tiles, enemies, parallax layers)
│   ├── LevelSerializer.hpp        # Level JSON I/O
│   ├── PlayerProfile.hpp          # Character profile data and JSON I/O
│   ├── EnemyProfile.hpp           # Enemy profile data and JSON I/O
│   ├── AnimatedTile.hpp           # Animated tile manifest loader
│   ├── GameConfig.hpp             # Gameplay constants
│   └── GameEvents.hpp             # System result structs
├── game_assets/                   # Sprites, backgrounds, tilesets
│   ├── backgrounds/               # Parallax and base background PNGs
│   ├── char_sprites/              # Player character sprite folders
│   ├── enemy_sprites/             # Enemy type sprite folders
│   └── base_pack/                 # Default tileset and enemy sprites
├── enemies/                       # Saved enemy profile JSON files
├── levels/                        # Saved level JSON files
├── players/                       # Saved character profile JSON files
├── fonts/                         # TTF font files
├── CMakeLists.txt                 # Build configuration (C++23, vcpkg)
└── CMakePresets.json              # Platform build presets
```

---

## Controls

### Gameplay — Keyboard

| Key | Action |
|-----|--------|
| A / D (or Left / Right) | Move |
| W / S on ladder | Climb up / down |
| Space | Jump (hold for higher) |
| Left Ctrl | Crouch |
| F | Slash attack |
| Double-tap A or D | Dash (with invulnerability) |
| ESC | Pause menu |
| R | Retry after game over |
| F1 | Debug hitbox overlay |
| F11 | Toggle fullscreen |

### Gameplay — Xbox Controller

Plug in any Xbox-compatible controller (XInput/SDL gamepad). Hot-plug is supported.

| Button | Action |
|--------|--------|
| Left Stick | Move (analog) |
| A (South) | Jump (hold for higher) |
| X (West) | Slash attack |
| B (East) / RB | Dash (in current movement direction) |
| Left Stick Click | Crouch |
| LB | Sprint |
| Start | Pause / Resume |
| Back (Select) | Return to title (while paused) |
| A (South) | Retry (on game over screen) |

### Level Editor

| Key | Action |
|-----|--------|
| Arrow keys / Middle-click drag | Pan camera |
| Scroll wheel | Zoom |
| 1–9 | Switch tools |
| G | Cycle gravity mode |
| Delete / Backspace | Delete selected tiles or enemies |
| Shift+click (Backgrounds tab) | Add parallax layer |
| Shift+scroll (Enemy tool) | Adjust enemy speed |
| Ctrl+S | Save level |
| Ctrl+Z | Undo |
| Play button | Test level in-game |

---

## Workflow

1. **Launch** — The title screen offers Play, Editor, Character Creator, and Enemy Creator.
2. **Design characters** — In the Character Creator, assign sprite folders to each of the 8 animation slots, tune hitboxes and FPS, and save to `players/`.
3. **Design enemies** — In the Enemy Creator, configure sprite dimensions, speed, health, and animation slots (Idle, Move, Attack, Hurt, Dead), then save to `enemies/`.
4. **Build levels** — In the Level Editor, paint tiles from the palette, set properties (solid, prop, hazard, ladder, slope, action, goal, moving platform, power-up), place enemies and coins, configure the player spawn, and set up layered parallax backgrounds.
5. **Animate tiles** — In the Tile Animation Creator, select a folder of PNGs, preview the sequence, and export a manifest JSON that the editor can reference.
6. **Play-test** — Press Play in the editor to drop into the level with your selected character. Press ESC to return to the editor and iterate.

---

## License

Copyright (c) 2025 Tanner Davison. All Rights Reserved.
