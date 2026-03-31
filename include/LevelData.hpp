#pragma once
#include "Components.hpp" // for SlopeType
#include <optional>
#include <string>
#include <vector>

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
    std::string type;             // "antigravity", "turret", "healthboost", "teleport", etc.
    float       duration = 15.0f; // seconds the effect lasts
    float       fireRate = 3.0f;  // shots/sec (turret power-up only)
    float       healthPct = 25.0f; // % of base max HP to add (healthboost only)
    int         teleportGroup = 0; // shared ID linking a teleport entrance to its destination
    bool        teleportDest = false; // true = this tile is the destination, not the entrance
    std::string sfxPath;          // optional fire SFX (turret power-up)
};

enum class ShooterSide { Top, Right, Bottom, Left };

struct ShooterData {
    ShooterSide side        = ShooterSide::Right;
    float       range       = 1000.0f;  // detection range in pixels
    float       fireRate    = 2.5f;     // shots per second
    float       bulletSpeed = 700.0f;   // pixels per second
    float       damage      = 25.0f;    // HP per bullet hit
    std::string sfxPath;                // optional fire sound effect
};

struct ShieldData {
    float duration = 20.0f; // seconds the shield orbits the player
};

struct TileSpawn {
    float       x = 0.0f, y = 0.0f;
    int         w = 0, h = 0;
    std::string imagePath;
    int         rotation    = 0;     // clockwise degrees: 0, 90, 180, 270

    // Mutually exclusive rules enforced by editor tools
    bool prop        = false; // rendered, no collision — background decoration
    bool propBehind  = false; // when prop=true: false=front (over player), true=behind player
    bool ladder      = false; // rendered, no solid collision — player can climb
    bool hazard      = false; // solid tile that drains HP while player overlaps
    bool antiGravity = false; // floats — bobs in place, no gravity, pushable
    bool goal        = false; // collectible goal — player must collect all to complete level

    std::optional<ActionData>          action;
    std::optional<SlopeData>           slope;
    std::optional<HitboxData>          hitbox;
    std::optional<MovingPlatformData>  moving;
    std::optional<PowerUpData>         powerUp;
    std::optional<ShooterData>         shooter;
    std::optional<ShieldData>          shield;

    bool HasAction()   const { return action.has_value(); }
    bool HasSlope()    const { return slope.has_value(); }
    bool HasHitbox()   const { return hitbox.has_value(); }
    bool HasMoving()   const { return moving.has_value(); }
    bool HasPowerUp()  const { return powerUp.has_value(); }
    bool HasShooter()  const { return shooter.has_value(); }
    bool HasShield()   const { return shield.has_value(); }
    bool HasGoal()    const { return goal; }

    SlopeType GetSlopeType() const {
        return slope ? slope->type : SlopeType::None;
    }
};

struct ParallaxLayer {
    std::string imagePath;
    float       scrollFactor = 0.5f;  // 0 = static, 1 = moves 1:1 with camera
    float       yOffset      = 0.0f;  // vertical pixel offset (positive = down)
};

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

    std::string musicPath;              // relative path to OGG/MP3 (empty = no music)
    float       musicVolume = 1.0f;     // 0..1 level-specific music volume
};
