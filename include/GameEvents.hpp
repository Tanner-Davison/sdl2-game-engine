#pragma once
#include <algorithm>
#include <entt/entt.hpp>
#include <vector>

struct FloatingResult {
    std::vector<entt::entity> actionTilesTriggered;
    std::vector<entt::entity> enemiesKilledByFloat;
};

struct CollisionResult {
    bool                      playerDied           = false;
    bool                      playerHit            = false;
    int                       goalsCollected       = 0;
    int                       enemiesStomped       = 0;
    bool                      onHazard             = false;
    std::vector<entt::entity> actionTilesTriggered;
    int                       enemiesSlashed   = 0;
    int                       slashHits        = 0;
    float                     lowestHitHpFrac  = 1.0f;
    bool                      shieldBounce     = false;
};
