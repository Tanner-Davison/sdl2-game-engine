// Part of ECS — engine-layer component definitions only.
// Game constants (health values, speeds, counts) live in GameConfig.hpp.
#pragma once
#include "GameConfig.hpp"
#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <unordered_map>
#include <unordered_set>
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

// Holds all animation frame sets and their source textures for an entity.
// texture pointers are non-owning — the SpriteSheet objects must outlive this.
struct AnimationSet {
    std::vector<SDL_Rect> idle;
    SDL_Texture*          idleSheet  = nullptr;
    float                 idleFps    = 0.0f; // 0 = use engine default
    std::vector<SDL_Rect> walk;
    SDL_Texture*          walkSheet  = nullptr;
    float                 walkFps    = 0.0f;
    std::vector<SDL_Rect> jump;
    SDL_Texture*          jumpSheet  = nullptr;
    float                 jumpFps    = 0.0f;
    std::vector<SDL_Rect> hurt;
    SDL_Texture*          hurtSheet  = nullptr;
    float                 hurtFps    = 0.0f;
    std::vector<SDL_Rect> duck;
    SDL_Texture*          duckSheet  = nullptr;
    float                 duckFps    = 0.0f;
    std::vector<SDL_Rect> front;
    SDL_Texture*          frontSheet = nullptr;
    float                 frontFps   = 0.0f;
    std::vector<SDL_Rect> slash;
    SDL_Texture*          slashSheet = nullptr;
    float                 slashFps   = 0.0f;
};

// ── Rendering ─────────────────────────────────────────────────────────────────

// What to draw
struct Renderable {
    SDL_Texture*          sheet = nullptr;
    std::vector<SDL_Rect> frames;
    bool                  flipH = false;
};

// Draws the sprite offset from Transform position.
// Used to center large sprites over their collision box.
struct RenderOffset {
    int x = 0;
    int y = 0;
};

// FlipCache is no longer needed — SDL_RenderTextureRotated handles flipping natively.
// Kept as an empty placeholder so code that references it still compiles;
// GameScene should stop emplace<FlipCache> and RenderSystem no longer uses it.
struct FlipCache {};

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
    bool attackPressed = false;
    bool isAttacking   = false;
    // Entities already struck this swing — cleared on each new swing so each
    // attack only registers one hit per target, regardless of how many frames
    // the sword rect overlaps the same entity.
    std::unordered_set<entt::entity> hitEntities;
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
    bool  wasInContact = false; // true if player was in contact last frame (impulse edge-trigger)
    float dyThisFrame  = 0.0f;  // Y movement applied this frame (used to carry player)
    static constexpr float DRAG = 1.8f; // drag coefficient applied each second
};

struct DestructibleTag {
    SDL_Texture* breakSurface = nullptr;

    // Non-copyable: EnTT storage uses move-only path; prevents accidental copies.
    DestructibleTag() = default;
    explicit DestructibleTag(SDL_Texture* s) : breakSurface(s) {}
    DestructibleTag(const DestructibleTag&)            = delete;
    DestructibleTag& operator=(const DestructibleTag&) = delete;
    DestructibleTag(DestructibleTag&&)                 = default;
    DestructibleTag& operator=(DestructibleTag&&)      = default;
};
struct OpenWorldTag {}; // marks the player as running in open-world (top-down) mode

// ── Power-up system ───────────────────────────────────────────────────────────
// Extensible: add new enum values + handling in GameScene and MovementSystem.
// Each PowerUpType maps to a specific gameplay effect applied to the player
// for the duration defined by the power-up tile (default 15 seconds).
enum class PowerUpType {
    None,
    AntiGravity,   // player has zero gravity for `duration` seconds
    // Future: SpeedBoost, Invincibility, DoubleJump, ...
};

// Attached to a tile entity that functions as a power-up pickup.
// When the player overlaps this tile it is consumed (entity destroyed),
// and ActivePowerUp is emplaced on the player.
struct PowerUpTag {
    PowerUpType type     = PowerUpType::None;
    float       duration = 15.0f; // seconds the effect lasts (configurable per tile)
};

// One active power-up slot — tracks remaining time for a single effect.
struct ActivePowerUp {
    PowerUpType type      = PowerUpType::None;
    float       remaining = 0.0f; // seconds left
    float       duration  = 0.0f; // total duration (for progress bar)
};

// Attached to the player while ANY power-ups are active.
// Each type gets its own independent timer so multiple power-ups
// can run simultaneously without overwriting each other.
struct ActivePowerUps {
    // Maps PowerUpType -> {remaining, duration}.
    // Insert/update with add(), query with has(), remove on expiry in tick().
    struct Slot { float remaining = 0.f; float duration = 0.f; };
    std::unordered_map<int, Slot> slots; // keyed by (int)PowerUpType

    void add(PowerUpType t, float dur) {
        int k = (int)t;
        auto it = slots.find(k);
        if (it != slots.end()) {
            // Accumulate: add the new duration on top of whatever is left
            it->second.remaining += dur;
            // Track the new total as the duration so the HUD bar scales correctly
            it->second.duration = it->second.remaining;
        } else {
            slots[k] = {dur, dur};
        }
    }
    bool has(PowerUpType t) const { return slots.count((int)t) > 0; }
    float remaining(PowerUpType t) const {
        auto it = slots.find((int)t);
        return it != slots.end() ? it->second.remaining : 0.f;
    }
    float duration(PowerUpType t) const {
        auto it = slots.find((int)t);
        return it != slots.end() ? it->second.duration : 0.f;
    }
};
struct ActionTag {   // marks an action tile — rendered + collidable until the player
                     // slashes it enough times, then Renderable and Collider are removed.
    int         group           = 0; // 0 = standalone; matching non-zero groups trigger together
    int         hitsRequired    = 1; // total slashes needed to destroy (set from editor)
    int         hitsRemaining   = 1; // current hits left — decremented each slash
    std::string destroyAnimPath;     // optional animated tile JSON to play on destruction
};

// Attached to an action tile the frame it is destroyed (hitsRemaining hits 0).
// Signals that the tile is mid-death-animation: no longer solid/visible as a tile,
// but the anim frames are being played. GameScene::Update() owns the frame counter
// via tileAnimFrameMap (same mechanism as live animated tiles).
// When the animation finishes the entity is fully destroyed (reg.destroy).
struct DestroyAnimTag {
    int   totalFrames  = 0;     // total frame count for the anim
    float fps          = 8.0f;
    bool  reachedEnd   = false; // true once currentFrame has hit totalFrames-1 for the first time
    // Destruction is deferred one full tick after reachedEnd becomes true so
    // the last frame is actually rendered at least once before the entity dies.
};

// ── Slope collision data ──────────────────────────────────────────────────────
// Attached to slope tiles.  CollisionSystem uses slopeType to compute the
// floor Y at the player's horizontal centre instead of using a flat AABB.
//
// heightFrac: fraction of the tile height the slope actually rises over.
//   1.0 = fully diagonal (default, high-corner is at tile top)
//   0.5 = gentle slope (high-corner is at tile mid-height)
// The low corner is always at the tile bottom on the appropriate side.
struct SlopeCollider {
    SlopeType slopeType  = SlopeType::None;
    float     heightFrac = 1.0f; // 0.0 < heightFrac <= 1.0
};

// ── Ladder / climbing state ───────────────────────────────────────────────────
// ── Moving platform ──────────────────────────────────────────────────────────
struct MovingPlatformTag {};

struct MovingPlatformState {
    bool  horiz       = true;   // true = horizontal, false = vertical
    float range       = 96.0f; // half-travel in pixels
    float speed       = 60.0f; // pixels per second
    int   groupId     = 0;     // 0 = solo; matching IDs move as one rigid unit
    float originX     = 0.0f;  // spawn X
    float originY     = 0.0f;  // spawn Y
    float phase       = 0.0f;  // shared oscillator phase [0, 2*pi)
    float vx          = 0.0f;  // X delta this frame
    float vy          = 0.0f;  // Y delta this frame
    bool  playerOnTop = false; // set in MovingPlatformTick, read in Carry
    bool  loop        = false; // ping-pong: travel right to originX+range, then back
    int   loopDir     = 1;     // +1 = moving right, -1 = moving left (ping-pong)
    bool  trigger     = false; // waits for first player landing before moving
    bool  triggered   = false; // becomes true once player has landed on it
};

// Marks a tile as an animated tile driven by tileAnimFrameMap in GameScene.
// AnimationSystem skips entities with this tag so only GameScene::Update
// advances the frame counter and swaps the sheet pointer.
struct TileAnimTag {};

// ── Hit flash ───────────────────────────────────────────────────────────────
// Attached to action tiles when struck. RenderSystem overlays a transparent
// red tint for the flash duration, then the component is removed.
struct HitFlash {
    float timer    = 0.18f; // counts down to 0 — initialised to duration on emplace
    float duration = 0.18f; // seconds the flash lasts
};

struct ClimbState {
    bool onLadder  = false; // true while player overlaps a ladder tile this frame
    bool climbing  = false; // true while actively climbing (gravity suspended)
    bool atTop     = false; // true when player reached the top and is hanging there
    bool wPressed  = false; // event-driven: true while W is held on the ladder
    bool sPressed  = false; // event-driven: true while S is held on the ladder
};

// ── Per-character collider baseline ──────────────────────────────────────────
// Stores the resolved standing collider dims and render offsets for this
// specific character (from PlayerProfile or frost-knight defaults).
// PlayerStateSystem reads these instead of the hardcoded PLAYER_STAND_* /
// PLAYER_DUCK_* constants so custom characters keep their correct hitbox.
struct PlayerBaseCollider {
    int standW    = PLAYER_STAND_WIDTH;
    int standH    = PLAYER_STAND_HEIGHT;
    int standRoffX = PLAYER_STAND_ROFF_X;
    int standRoffY = PLAYER_STAND_ROFF_Y;
    int duckW     = PLAYER_DUCK_WIDTH;
    int duckH     = PLAYER_DUCK_HEIGHT;
    int duckRoffX  = PLAYER_DUCK_ROFF_X;
    int duckRoffY  = PLAYER_DUCK_ROFF_Y;
};
