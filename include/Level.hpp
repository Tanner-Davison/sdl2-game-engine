#pragma once
#include "Components.hpp" // for SlopeType
#include <string>
#include <vector>

// Plain data — one entry per entity type in the level
struct CoinSpawn {
    float x, y;
};
struct EnemySpawn {
    float x, y;
    float speed;
};
struct PlayerSpawn {
    float x, y;
};
struct TileSpawn {
    float       x, y;
    int         w, h;
    std::string imagePath;
    bool prop   = false; // rendered, no collision — background decoration
    bool ladder = false; // rendered, no solid collision — player can climb with W
    bool      action      = false;           // rendered and collidable until the player touches it,
                                             // then stops rendering and stops blocking (e.g. a door)
    int       actionGroup = 0;              // 0 = standalone; non-zero = group ID.
                                             // All action tiles sharing the same non-zero group
                                             // are triggered simultaneously when any one is touched.
    SlopeType slope       = SlopeType::None; // diagonal slope — collision rides the hypotenuse
    int       rotation    = 0;               // clockwise degrees: 0, 90, 180, 270
    // Custom hitbox — all zero means "use full tile rect" (default).
    // When non-zero, the collider is this sub-rect relative to (x,y).
    int hitboxOffX = 0;
    int hitboxOffY = 0;
    int hitboxW    = 0;  // 0 = use tile w
    int hitboxH    = 0;  // 0 = use tile h
};

enum class GravityMode { Platformer, WallRun, OpenWorld };

struct Level {
    std::string             name        = "Untitled";
    std::string             background  = "game_assets/backgrounds/deepspace_scene.png";
    GravityMode             gravityMode = GravityMode::Platformer;
    PlayerSpawn             player      = {0.0f, 0.0f};
    std::vector<CoinSpawn>  coins;
    std::vector<EnemySpawn> enemies;
    std::vector<TileSpawn>  tiles;
};
