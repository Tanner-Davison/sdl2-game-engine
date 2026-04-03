#pragma once
#include <algorithm>
#include <entt/entt.hpp>
#include <vector>
#include <systems/BloodParticleSystem.hpp>

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

    // Blood particle events collected during collision resolution.
    // GameScene iterates these after CollisionSystem returns and feeds them
    // into its BloodParticleSystem.  Adding a new hit type = push a new entry.
    std::vector<BloodEmitParams> bloodEvents;
};
