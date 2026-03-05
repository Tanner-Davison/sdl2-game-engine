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
};

// Returned by CollisionSystem every frame.
// The calling Scene accumulates these into its own state.
struct CollisionResult {
    bool                      playerDied           = false;
    int                       coinsCollected       = 0;
    int                       enemiesStomped       = 0;
    bool                      onHazard             = false; // true if player overlaps a HazardTag tile this frame
    // Action tiles triggered by a slash this frame.
    // CollisionSystem populates this; the Scene strips Renderable/TileTag/Collider after iteration.
    std::vector<entt::entity> actionTilesTriggered;
    int                       enemiesSlashed = 0; // enemies killed by slash this frame
};
