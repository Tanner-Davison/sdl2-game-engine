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
- **Audio engine** — SDL3_mixer-based audio subsystem with per-animation SFX, dual-track playback (managed looping track + dedicated one-shot track), optional time-stretch matching, per-slot volume control, background music with fade-in/out, and automatic PCM16 WAV conversion for non-standard audio files.
- **Player death sequence** — Dedicated death animation slot with timed hold, camera shake, dimmed overlay, death sound effect, and graceful transition to the game-over screen. Hazard deaths play the full death animation and SFX identically to enemy-caused deaths.
- **Health bars** — Enemy HP rendered as color-graded bars above each sprite, fading from green to red as health drops.
- **Dash mechanic** — Double-tap directional dash with invulnerability window.

### Audio Engine

The audio subsystem is built on SDL3_mixer and injected into scenes through `SceneManager`. It is **not** a singleton — the `AudioEngine` is owned by `SceneManager` and passed to scenes via `Scene::SetAudio()`.

**Architecture:**

| Component | Role |
|-----------|------|
| `AudioDevice` | Opens the SDL3_mixer device and creates the mixer. RAII — closes on destruction. |
| `SoundBank` | Loads WAV/OGG/MP3 files (pre-decoded to RAM), keyed by string ID. Three playback modes: fire-and-forget (`Play`), one-shot with gain (`mOneShotTrack`), and looping/time-stretched (`mSfxTrack`). |
| `MusicPlayer` | Streams background music with fade-in/out and track switching. |
| `AudioEngine` | Facade that composes all three and provides convenience methods (`PlayAnimSFX`, `StopAnimSFX`, `StartLevelMusic`). |
| `AudioEvents` | Pure data header — canonical SFX IDs (`player_idle`, `player_slash`, etc.) and `AnimationID` → SFX ID mapping functions. |

**Per-animation SFX workflow:**

1. In the **Character Creator** or **Enemy Creator**, drag a WAV/OGG file onto any animation slot's SFX drop zone.
2. Adjust per-slot **volume** with the +/- buttons.
3. Toggle **TS** (time-stretch) to match audio playback speed to the animation cycle duration, or leave it off to play the sound at its natural speed.
4. The profile saves `sfxPath`, `sfxVolume`, and `sfxTimeStretch` per slot in JSON.
5. At runtime, `GameScene` loads each slot's SFX into the `SoundBank` and triggers playback on animation transitions.
6. Enemy SFX are keyed as `<enemyTypeName>_<slot>` (e.g. `slime_hurt`, `bat_dead`), so multiple enemy types can have unique sounds.

**Playback behavior:**

| Scenario | Track used | Behavior |
|----------|-----------|----------|
| One-shot, TS off (slash, hurt, jump, death) | `mOneShotTrack` | Plays at natural speed with per-slot gain. Sound finishes even after the animation transitions away. |
| Looping, TS off (idle, walk) | `mSfxTrack` | Loops at natural speed. Stopped when animation changes. |
| Any animation, TS on | `mSfxTrack` | Frequency ratio adjusted so one audio cycle matches the animation cycle. Looping anims loop the audio; one-shots play once at the adjusted speed. |

**SFX trigger logic** — runs once per frame after all animation-changing systems:

*Player:*
- Fires on **animation transitions** (AnimationID changed from previous frame).
- Fires on **cycle restarts** (same non-looping animation, frame jumped back to 0) — ensures hurt SFX replays on each hazard damage cycle.
- Calls `StopAnimSFX()` (stops the looping track only) before starting the new sound.

*Enemies:*
- Snapshots each enemy's current sprite sheet before animation recovery and collision processing.
- After all transitions resolve, detects **sheet changes** (move → hurt, move → attack, any → dead).
- Maps the new sheet to the EnemyAnimSlot index, looks up the per-type SFX ID, and fires `PlayTimed` with the slot's volume and time-stretch settings.

**Auto-conversion** — If `MIX_LoadAudio` fails on a WAV file, `SoundBank` automatically attempts to convert it to standard 16-bit PCM WAV using `afconvert` (macOS) or `ffmpeg`, caching the result next to the original with a `_pcm16` suffix.

### Editor Tools

- **Level Editor** — Full-featured tile-based editor with palette browser, multi-tool workflow, grid snapping, undo, camera pan/zoom, play-test, and save/load.
- **Character Creator** — 8 animation slots (Idle, Walk, Crouch, Jump, Fall, Slash, Hurt, Death) with drag-and-drop sprite folders, per-slot FPS, interactive hitbox handles, and per-slot SFX assignment with volume control and a time-stretch toggle that matches audio playback to animation cycle duration.
- **Enemy Designer** — 5 animation slots (Idle, Move, Attack, Hurt, Dead) with sprite size configuration, custom hitboxes, speed, health, live animation preview, and per-slot SFX assignment with volume control and time-stretch toggle.
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
| `PlayerStateSystem` | Animation priority state machine (death > attack > hazard hurt > invincibility hurt > crouch > airborne > walk > idle) with per-character collider enforcement and hazard-aware hurt cycling |
| `AnimationSystem` | Delta-time frame advancement with looping and non-looping support |
| `CollisionSystem` | Multi-pass AABB (slopes, gravity snap, lateral push-out, step-up), enemy stomp/slash, goal collection, hazard overlap, action tile triggers |
| `BoundsSystem` | Level-edge clamping, wall-run gravity flip, open-world bounds |
| `MovingPlatformSystem` | Platform tick (sine/ping-pong/trigger), grouped sync, player carry |
| `FloatingSystem` | Bob oscillation, drift/spin decay, player push, slash push, object collisions, tile bounce, enemy gravity |
| `LadderSystem` | Ladder column detection, climb/descend/top-lock, jump-off, side walk-off |
| `RenderSystem` | GPU-rendered sprites with camera offset, flip, rotation, invincibility flash, hazard flash, hit flash, health bars, and pre-sorted tile draw order |
| `HUDSystem` | Health bar, gravity indicator, goal count, stomp count |
| Audio SFX trigger | Detects player animation transitions / cycle restarts and enemy sprite-sheet swaps, fires per-slot SFX with volume and optional time-stretch |

### Scene System

Scenes inherit from `Scene` (load, unload, handle event, update, render, next scene). `SceneManager` owns the active scene and handles transitions.

| Scene | Purpose |
|-------|---------|
| `TitleScene` | Title screen with Play, Editor, Character Creator, and Enemy Creator buttons |
| `GameScene` | Runtime gameplay — owns the ECS registry, all assets, and the fixed-step game loop |
| `LevelEditorScene` | Level editor with modular subsystems (FileOps, Palette, Toolbar, Popups, Camera, Canvas, UI, SurfaceCache) |
| `PlayerCreatorScene` | Character creator with drag-and-drop sprites, hitbox editor, and JSON save/load |
| `EnemyCreatorScene` | Enemy type designer with animation slots, hitbox handles, speed/health fields, per-slot SFX (drop, volume, TS), and a saved roster |
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
| `PlayerProfile` | JSON save/load for character profiles (name, dimensions, per-slot sprites, hitboxes, FPS, per-slot SFX path/volume/time-stretch) |
| `EnemyProfile` | JSON save/load for enemy types (name, dimensions, speed, health, per-slot sprites, hitboxes, per-slot SFX path/volume/time-stretch) |
| `AnimatedTile` | JSON manifest loader for tile animation sequences |
| `GameConfig` | Compile-time gameplay constants (player stats, physics, enemy stats, camera) |
| `GameEvents` | Lightweight result structs returned by systems (`CollisionResult`, `FloatingResult`) |
| `AudioEngine` | High-level audio facade composing AudioDevice, SoundBank, and MusicPlayer — injected into scenes via `Scene::SetAudio()` |
| `AudioDevice` | RAII SDL3_mixer device and mixer initialization |
| `SoundBank` | Named SFX resource manager with fire-and-forget playback, dual managed tracks (looping + one-shot), per-sound gain, and frequency-ratio time-stretch |
| `MusicPlayer` | Background music streaming with fade-in/out and track switching |
| `AudioEvents` | Canonical SFX ID constants, player AnimationID → SFX ID mapping, and per-enemy-type SFX ID builder (`EnemySfxId`) |

---

## Building

### Prerequisites

Dependencies are managed via [vcpkg](https://github.com/microsoft/vcpkg):

| Dependency | Purpose |
|------------|---------|
| SDL3 | Windowing, rendering, input |
| SDL3_image | PNG sprite loading |
| SDL3_ttf | Font rendering |
| SDL3_mixer | Audio mixing, SFX playback, music streaming |
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
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf sdl3-mixer entt nlohmann-json

# macOS Apple Silicon
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf sdl3-mixer entt nlohmann-json --triplet arm64-osx

# macOS Intel
vcpkg install sdl3 "sdl3-image[png]" sdl3-ttf sdl3-mixer entt nlohmann-json --triplet x64-osx
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
│   ├── audio/                     # Audio subsystem implementations
│   │   ├── AudioDevice.cpp        # SDL3_mixer device init
│   │   ├── AudioEngine.cpp        # High-level facade (music start/stop)
│   │   ├── MusicPlayer.cpp        # Background music streaming
│   │   └── SoundBank.cpp          # SFX loading, dual-track playback
│   └── tools/                     # Editor tool implementations
├── include/
│   ├── audio/                     # Audio subsystem headers
│   │   ├── AudioDevice.hpp        # RAII mixer device
│   │   ├── AudioEngine.hpp        # Facade composing device + SFX + music
│   │   ├── AudioEvents.hpp        # SFX ID constants and AnimationID mapping
│   │   ├── MusicPlayer.hpp        # Music streaming with fade
│   │   └── SoundBank.hpp          # Named SFX manager (looping + one-shot tracks)
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
├── game_assets/                   # Sprites, backgrounds, tilesets, audio
│   ├── audio/                     # Sound effects and music
│   │   ├── music/                 # Background music tracks (OGG)
│   │   └── sfx/                   # Per-entity sound effects (WAV)
│   │       ├── player/            # Player animation SFX (footsteps, slash, hurt, etc.)
│   │       └── enemies/           # Enemy animation SFX (attack, hurt, dead, etc.)
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
3. **Design enemies** — In the Enemy Creator, configure sprite dimensions, speed, health, animation slots (Idle, Move, Attack, Hurt, Dead), and per-slot SFX with volume and time-stretch, then save to `enemies/`.
4. **Build levels** — In the Level Editor, paint tiles from the palette, set properties (solid, prop, hazard, ladder, slope, action, goal, moving platform, power-up), place enemies and coins, configure the player spawn, and set up layered parallax backgrounds.
5. **Animate tiles** — In the Tile Animation Creator, select a folder of PNGs, preview the sequence, and export a manifest JSON that the editor can reference.
6. **Play-test** — Press Play in the editor to drop into the level with your selected character. Press ESC to return to the editor and iterate.

---

## License

Copyright (c) 2025 Tanner Davison. All Rights Reserved.
