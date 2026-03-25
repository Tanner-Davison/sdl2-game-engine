#pragma once
#include "Components.hpp" // for SlopeType
#include <optional>
#include <string>
#include <vector>

// ── Plain data — one entry per entity type in the level ──────────────────────

struct EnemySpawn {
    float x, y;
    float speed;
    bool        antiGravity = false;
    bool        startLeft   = false; // true = starts moving left, false = starts moving right
    std::string enemyType;           // enemy profile name (empty = legacy generic slime)
};

struct PlayerSpawn {
    float x, y;
};

// ── TileSpawn sub-structs ────────────────────────────────────────────────────
// Each optional group holds the data for a specific tile feature.
// When the optional is std::nullopt, that feature is inactive.

struct ActionData {
    int         group          = 0;   // 0 = standalone; non-zero groups trigger together
    int         hitsRequired   = 1;   // total slashes to destroy
    std::string destroyAnimPath;      // animated tile JSON for death anim (empty = none)
    bool        cameraShake    = false; // trigger camera shake when destroy anim plays
};

struct SlopeData {
    SlopeType type       = SlopeType::DiagUpRight;
    float     heightFrac = 1.0f; // 0..1: fraction of tile height the slope rises
};

struct HitboxData {
    int offX = 0; // offset from tile top-left
    int offY = 0;
    int w    = 0; // 0 = use tile w
    int h    = 0; // 0 = use tile h
};

struct MovingPlatformData {
    bool  horiz   = true;    // true = horizontal, false = vertical
    float range   = 96.0f;   // half-travel in pixels
    float speed   = 60.0f;   // pixels per second
    int   groupId = 0;       // 0 = solo; non-zero groups tiles into one platform
    bool  loop    = false;   // true = ping-pong, false = sine oscillate
    bool  trigger = false;   // true = waits for player landing before moving
    float phase   = 0.0f;   // starting position: 0.0 = origin, 1.0 = far end
    int   loopDir = 1;      // starting direction: +1 = right/down, -1 = left/up
};

struct PowerUpData {
    std::string type;             // "antigravity", etc.
    float       duration = 15.0f; // seconds the effect lasts
};

// ── TileSpawn ────────────────────────────────────────────────────────────────

struct TileSpawn {
    // Core fields — every tile has these
    float       x = 0.0f, y = 0.0f;
    int         w = 0, h = 0;
    std::string imagePath;
    int         rotation    = 0;     // clockwise degrees: 0, 90, 180, 270

    // Simple boolean flags (mutually exclusive rules enforced by editor tools)
    bool prop        = false; // rendered, no collision — background decoration
    bool ladder      = false; // rendered, no solid collision — player can climb
    bool hazard      = false; // solid tile that drains HP while player overlaps
    bool antiGravity = false; // floats — bobs in place, no gravity, pushable
    bool goal        = false; // collectible goal — player must collect all to complete level

    // Optional feature groups — present only when the feature is active
    std::optional<ActionData>          action;
    std::optional<SlopeData>           slope;
    std::optional<HitboxData>          hitbox;
    std::optional<MovingPlatformData>  moving;
    std::optional<PowerUpData>         powerUp;

    // ── Convenience queries ──────────────────────────────────────────────────
    bool HasAction()   const { return action.has_value(); }
    bool HasSlope()    const { return slope.has_value(); }
    bool HasHitbox()   const { return hitbox.has_value(); }
    bool HasMoving()   const { return moving.has_value(); }
    bool HasPowerUp()  const { return powerUp.has_value(); }
    bool HasGoal()    const { return goal; }

    SlopeType GetSlopeType() const {
        return slope ? slope->type : SlopeType::None;
    }
};

// ── Parallax background layer ────────────────────────────────────────────────

struct ParallaxLayer {
    std::string imagePath;
    float       scrollFactor = 0.5f;  // 0 = static, 1 = moves 1:1 with camera
    float       yOffset      = 0.0f;  // vertical pixel offset (positive = down)
};

// ── Level-wide settings ──────────────────────────────────────────────────────

enum class GravityMode { Platformer, WallRun, OpenWorld };

struct Level {
    std::string             name        = "Untitled";
    std::string             background  = "game_assets/backgrounds/deepspace_scene.png";
    std::string             bgFitMode   = "cover";
    bool                    bgRepeat    = false;
    GravityMode             gravityMode = GravityMode::Platformer;
    PlayerSpawn             player      = {0.0f, 0.0f};
    std::vector<EnemySpawn> enemies;
    std::vector<TileSpawn>  tiles;
    std::vector<ParallaxLayer> parallaxLayers;

    // ── Audio ─────────────────────────────────────────────────────────────
    std::string musicPath;              // relative path to OGG/MP3 (empty = no music)
    float       musicVolume = 1.0f;     // 0..1 level-specific music volume
};
