// Part of ECS — engine-layer component definitions only.
// Game constants (health values, speeds, counts) live in GameConfig.hpp.
#pragma once
#include "GameConfig.hpp"
#include <SDL3/SDL.h>
#include <vector>

// ── Slope type ────────────────────────────────────────────────────────────────────
// Defined here (rather than Level.hpp) so CollisionSystem and other engine
// headers can reference it without pulling in Level.hpp.
enum class SlopeType { None, DiagUpRight, DiagUpLeft };

// ── Core transform / physics ──────────────────────────────────────────────────

// Position in world space (top-left of collider)
struct Transform {
    float x = 0.0f;
    float y = 0.0f;
};

// Movement direction and speed
struct Velocity {
    float dx    = 0.0f;
    float dy    = 0.0f;
    float speed = PLAYER_SPEED;
};

// ── Animation ─────────────────────────────────────────────────────────────────

enum class AnimationID { IDLE, WALK, JUMP, HURT, DUCK, FRONT, SLASH, NONE };

struct AnimationState {
    int         currentFrame = 0;
    int         totalFrames  = 0;
    float       timer        = 0.0f;
    float       fps          = 12.0f;
    bool        looping      = true;
    AnimationID currentAnim  = AnimationID::NONE;
};

// Holds all animation frame sets and their source sheets for an entity.
// sheet pointers are non-owning — the SpriteSheet objects must outlive this.
struct AnimationSet {
    std::vector<SDL_Rect> idle;
    SDL_Surface*          idleSheet = nullptr;
    std::vector<SDL_Rect> walk;
    SDL_Surface*          walkSheet = nullptr;
    std::vector<SDL_Rect> jump;
    SDL_Surface*          jumpSheet = nullptr;
    std::vector<SDL_Rect> hurt;
    SDL_Surface*          hurtSheet = nullptr;
    std::vector<SDL_Rect> duck;
    SDL_Surface*          duckSheet = nullptr;
    std::vector<SDL_Rect> front;
    SDL_Surface*          frontSheet = nullptr;
    std::vector<SDL_Rect> slash;
    SDL_Surface*          slashSheet = nullptr;
};

// ── Rendering ─────────────────────────────────────────────────────────────────

// What to draw
struct Renderable {
    SDL_Surface*          sheet = nullptr;
    std::vector<SDL_Rect> frames;
    bool                  flipH = false;
};

// Draws the sprite offset from Transform position.
// Used to center large sprites over their collision box.
struct RenderOffset {
    int x = 0;
    int y = 0;
};

// Per-frame flip cache for RenderSystem.
// Stores one pre-flipped SDL_Surface* per animation frame, built lazily on
// first use and reused every subsequent frame. Invalidated when the animation
// set changes (detected by frame count mismatch).
struct FlipCache {
    std::vector<SDL_Surface*> frames;

    FlipCache() = default;

    // Non-copyable
    FlipCache(const FlipCache&)            = delete;
    FlipCache& operator=(const FlipCache&) = delete;

    // Movable
    FlipCache(FlipCache&& o) noexcept : frames(std::move(o.frames)) {}
    FlipCache& operator=(FlipCache&& o) noexcept {
        if (this != &o) {
            for (auto* s : frames)
                if (s)
                    SDL_DestroySurface(s);
            frames = std::move(o.frames);
        }
        return *this;
    }

    ~FlipCache() {
        for (auto* s : frames)
            if (s)
                SDL_DestroySurface(s);
    }
};

// ── Collision ─────────────────────────────────────────────────────────────────

struct Collider {
    int w = 0;
    int h = 0;
};

// Optional offset for tiles whose hitbox doesn't start at their top-left corner.
// CollisionSystem adds this to the tile's Transform position before testing.
struct ColliderOffset {
    int x = 0;
    int y = 0;
};

// ── Gameplay state ────────────────────────────────────────────────────────────

struct Health {
    float current = PLAYER_MAX_HEALTH;
    float max     = PLAYER_MAX_HEALTH;
};

struct InvincibilityTimer {
    float remaining    = 0.0f;
    float duration     = PLAYER_INVINCIBILITY;
    bool  isInvincible = false;
};

enum class GravityDir { DOWN, UP, LEFT, RIGHT };

struct GravityState {
    bool       active          = true;
    float      timer           = 0.0f;
    bool       isGrounded      = true;
    float      velocity        = 0.0f;
    bool       jumpHeld        = false;
    bool       isCrouching     = false;
    GravityDir direction       = GravityDir::DOWN;
    float      punishmentTimer = 0.0f; // counts down after a hit; gravity locked off until 0
};

// ── Hazard state ──────────────────────────────────────────────────────────────
// Attached to the player. active=true while overlapping any HazardTag tile.
// RenderSystem reads flashTimer to pulse the sprite red independently of
// InvincibilityTimer (which must stay unaffected so the hit-flash still works).
struct HazardState {
    bool  active     = false;  // true while player overlaps any hazard tile this frame
    float flashTimer = 0.0f;   // counts up at ~8 Hz; drives the red flash pulse
};

// ── Attack state ─────────────────────────────────────────────────────────────
// Attached to the player. attackPressed fires the slash; isAttacking blocks
// any other animation swap until the slash plays to completion.
struct AttackState {
    bool attackPressed = false; // set by InputSystem on F key-down
    bool isAttacking   = false; // true while slash anim is playing
};

// ── Tags (marker components — no data) ───────────────────────────────────────

struct PlayerTag {}; // marks the player entity
struct EnemyTag {};  // marks a live enemy entity
struct CoinTag {};   // marks a collectible coin
struct DeadTag {};   // marks a stomped enemy — no longer harmful, acts as a platform
struct TileTag {};   // marks a solid tile — blocks movement
struct LadderTag {};    // marks a ladder tile — passthrough, player can climb with W/S
struct PropTag {};      // marks a prop tile — rendered only, no collision, no interaction
struct HazardTag {};    // marks a hazard tile — solid + drains player HP while overlapping

// Marks a tile as slash-destructible.
// breakSurface is a non-owning pointer into GameScene::tileScaledSurfaces.
// Stored as a pointer (not passed through EnTT view.each) — always accessed
// via reg.try_get<DestructibleTag> to avoid EnTT's copy/move restrictions.
// Anti-gravity tag — attached to enemies and tiles that should float.
// FloatState tracks the bob oscillation, drift velocity, and spin angle.
struct FloatTag {};

struct FloatState {
    float bobTimer   = 0.0f;   // accumulates time for sin-wave bob
    float bobAmp     = 6.0f;   // pixels of vertical oscillation
    float bobSpeed   = 2.0f;   // radians/sec  (each entity gets a random phase offset)
    float bobPhase   = 0.0f;   // random phase so not all entities bob in sync
    float baseY      = 0.0f;   // Y the entity was spawned at (bob centre)
    float driftVx    = 0.0f;   // horizontal push velocity (decays with drag)
    float driftVy    = 0.0f;   // vertical push velocity   (decays with drag)
    float spinAngle  = 0.0f;   // current visual rotation in degrees (render-only)
    float spinSpeed  = 0.0f;   // degrees/sec, decays to 0
    static constexpr float DRAG = 1.8f; // drag coefficient applied each second
};

struct DestructibleTag {
    SDL_Surface* breakSurface = nullptr;

    // Non-copyable: EnTT storage uses move-only path; prevents accidental copies.
    DestructibleTag() = default;
    explicit DestructibleTag(SDL_Surface* s) : breakSurface(s) {}
    DestructibleTag(const DestructibleTag&)            = delete;
    DestructibleTag& operator=(const DestructibleTag&) = delete;
    DestructibleTag(DestructibleTag&&)                 = default;
    DestructibleTag& operator=(DestructibleTag&&)      = default;
};
struct OpenWorldTag {}; // marks the player as running in open-world (top-down) mode
struct ActionTag {   // marks an action tile — rendered + collidable until the player
                     // makes contact, then Renderable and Collider are removed so it
                     // disappears and stops blocking (e.g. a door that opens on touch)
    int group = 0;   // 0 = standalone; matching non-zero groups trigger together
};

// ── Slope collision data ──────────────────────────────────────────────────────
// Attached to slope tiles.  CollisionSystem uses slopeType to compute the
// floor Y at the player's horizontal centre instead of using a flat AABB.
struct SlopeCollider {
    SlopeType slopeType = SlopeType::None;
};

// ── Ladder / climbing state ───────────────────────────────────────────────────
struct ClimbState {
    bool onLadder  = false; // true while player overlaps a ladder tile this frame
    bool climbing  = false; // true while actively climbing (gravity suspended)
    bool atTop     = false; // true when player reached the top and is hanging there
    bool wPressed  = false; // event-driven: true while W is held on the ladder
    bool sPressed  = false; // event-driven: true while S is held on the ladder
};
