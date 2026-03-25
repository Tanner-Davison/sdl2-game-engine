#pragma once
#include <algorithm>
#include <entt/entt.hpp>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// GameEvents.hpp
//
// Lightweight value types that systems return or emit instead of mutating
// out-parameters owned by a Scene.  Keeps systems self-contained and testable.
// ─────────────────────────────────────────────────────────────────────────────

// Returned by FloatingSystem every frame.
struct FloatingResult {
    // Action tiles whose hitsRemaining reached 0 from a floating-object bounce.
    // GameScene merges these into the same destroy-animation pipeline as
    // collision.actionTilesTriggered so the death anim plays correctly.
    std::vector<entt::entity> actionTilesTriggered;

    // Enemies killed by a fast-moving floating object this frame.
    // GameScene processes these through the same kill pipeline as stomps/slashes.
    std::vector<entt::entity> enemiesKilledByFloat;
};

// Returned by CollisionSystem every frame.
// The calling Scene accumulates these into its own state.
struct CollisionResult {
    bool                      playerDied           = false;
    bool                      playerHit            = false; // true if player took enemy contact damage this frame
    int                       goalsCollected       = 0;
    int                       enemiesStomped       = 0;
    bool                      onHazard             = false; // true if player overlaps a HazardTag tile this frame
    // Action tiles triggered by a slash this frame.
    // CollisionSystem populates this; the Scene strips Renderable/TileTag/Collider after iteration.
    std::vector<entt::entity> actionTilesTriggered;
    int                       enemiesSlashed   = 0;    // enemies killed by slash this frame
    int                       slashHits        = 0;    // total sword-to-enemy connections (lethal + non-lethal)
    float                     lowestHitHpFrac  = 1.0f; // lowest enemy HP fraction after any hit (0 = killed)
};
