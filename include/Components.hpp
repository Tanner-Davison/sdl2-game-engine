// Part of ECS — engine-layer component definitions only.
// Game constants (health values, speeds, counts) live in GameConfig.hpp.
#pragma once
#include "GameConfig.hpp"
#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <array>
#include <string>
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

// Previous-frame position, snapshotted at the start of every physics tick.
// RenderSystem lerps between PrevTransform and Transform using the sub-step
// alpha so motion appears smooth at any render frame rate, regardless of the
// fixed physics tick rate. Attached to every moving entity (player, enemies,
// moving platforms). Static tiles do not need this component.
struct PrevTransform {
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

enum class AnimationID { IDLE, WALK, JUMP, HURT, DUCK, FRONT, SLASH, DEATH, NONE };

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
    std::vector<SDL_Rect> death;
    SDL_Texture*          deathSheet = nullptr;
    float                 deathFps   = 0.0f;
};

// ── Rendering ─────────────────────────────────────────────────────────────────

// What to draw
struct Renderable {
    SDL_Texture*          sheet = nullptr;
    std::vector<SDL_Rect> frames;
    bool                  flipH = false;
    int                   renderW = 0; // intended render width  (0 = use frame src.w)
    int                   renderH = 0; // intended render height (0 = use frame src.h)
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
    bool       sprinting       = false; // true while Shift is held
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

// ── Dash state (double-tap dodge) ─────────────────────────────────────────────
struct DashState {
    bool  active         = false;
    float remaining      = 0.0f;  // time left in current dash
    float cooldown       = 0.0f;  // time until next dash allowed
    float direction      = 0.0f;  // +1 right, -1 left
    float tapTimerLeft   = 0.0f;  // countdown window for second left tap
    float tapTimerRight  = 0.0f;  // countdown window for second right tap
    bool  releasedLeft   = true;  // key must be released between taps
    bool  releasedRight  = true;
};

// ── Tags (marker components — no data) ───────────────────────────────────────

struct PlayerTag {};      // marks the player entity
struct EnemyTag {};       // marks a live enemy entity
struct GoalTag {};        // marks a goal tile — player collects all to complete the level
struct DeadTag {};        // marks a stomped enemy
struct FaceRightTag {};   // sprite art faces right by default (flip when moving left)

// Tracks whether an enemy is currently playing its attack animation.
// Prevents re-triggering the attack every frame during overlap.
struct EnemyAttackState {
    bool  attacking = false;
    float cooldown  = 0.0f;   // seconds remaining before can attack again
};

// Enemy ladder climbing state. Enemies autonomously use ladders when they
// encounter one during patrol or when chasing the player.
struct EnemyClimbState {
    bool  climbing    = false;
    bool  goingUp     = true;
    bool  steppingOff = false;  // walking horizontally off ladder onto platform
    float columnTop   = 0.0f;  // top Y of the current ladder column
    float columnBot   = 0.0f;  // bottom Y of the current ladder column
    float ladderCX    = 0.0f;  // ladder center X (enemy is centered here while climbing)
    float ladderW     = 0.0f;  // ladder column width
};

// ── Enemy animation data (custom enemies only) ──────────────────────────────
// Holds non-owning pointers to Hurt/Dead sprite sheets and frames so the
// collision system can swap the enemy's Renderable when hit or killed.
// The actual SpriteSheet objects are owned by GameScene::mEnemySpriteSheets.
// Per-animation hitbox for enemies: collider dimensions + offsets from sprite top-left.
// If w == 0, the system falls back to spriteW x spriteH with no offset.
struct EnemyHitbox {
    int w = 0, h = 0;   // collider size (0 = use spriteW/spriteH)
    int offX = 0, offY = 0; // ColliderOffset (from sprite top-left)
    int roffX = 0, roffY = 0; // RenderOffset (sprite drawn offset from collider)
    bool IsDefault() const { return w == 0 && h == 0; }
};

struct EnemyAnimData {
    // Attack animation (played when enemy hits the player)
    SDL_Texture*          attackSheet = nullptr;
    std::vector<SDL_Rect> attackFrames;
    float                 attackFps   = 10.0f;
    EnemyHitbox           attackHitbox;
    // Hurt animation (played when taking damage)
    SDL_Texture*          hurtSheet   = nullptr;
    std::vector<SDL_Rect> hurtFrames;
    float                 hurtFps     = 8.0f;
    EnemyHitbox           hurtHitbox;
    // Dead animation (played when killed)
    SDL_Texture*          deadSheet   = nullptr;
    std::vector<SDL_Rect> deadFrames;
    float                 deadFps     = 6.0f;
    EnemyHitbox           deadHitbox;
    // Move animation (to restore after hurt/attack finishes)
    SDL_Texture*          moveSheet   = nullptr;
    std::vector<SDL_Rect> moveFrames;
    float                 moveFps     = 7.0f;
    EnemyHitbox           moveHitbox;
    // Idle animation hitbox (used as default when no move hitbox set)
    EnemyHitbox           idleHitbox;
    // Sprite dimensions
    int spriteW = 40, spriteH = 40;
    // Enemy type name (for building per-type SFX IDs at runtime)
    std::string typeName;
    // Per-slot SFX metadata (indexed by EnemyAnimSlot)
    struct SfxFile { float volume = 1.0f; bool timeStretch = false; float trimStart = 0.0f; float trimEnd = 1.0f; };
    struct SlotSfx {
        std::vector<SfxFile> files;
        int nextIdx = 0;
    };
    std::array<SlotSfx, 5> slotSfx{};

    // Apply the hitbox for a given animation to the entity's Collider
    // and RenderOffset. Falls back to spriteW x spriteH.
    //
    // For enemies, Transform is the COLLIDER position (top-left of hitbox).
    // RenderOffset shifts the sprite relative to the collider so the art
    // lines up. CollisionSystem does NOT use ColliderOffset for enemies,
    // so we only set RenderOffset here.
    void ApplyHitbox(const EnemyHitbox& hb, entt::registry& reg, entt::entity e) const {
        int cw = hb.IsDefault() ? spriteW : hb.w;
        int ch = hb.IsDefault() ? spriteH : hb.h;

        // Adjust Transform so the collider BOTTOM stays at the same Y.
        // This keeps the enemy's feet on the ground when the hitbox
        // changes size between animations (e.g. move -> dead).
        if (reg.all_of<Collider>(e) && reg.all_of<Transform>(e)) {
            auto& col = reg.get<Collider>(e);
            auto& tr  = reg.get<Transform>(e);
            float oldBottom = tr.y + col.h;
            col.w = cw;
            col.h = ch;
            tr.y = oldBottom - ch; // keep feet pinned
        } else if (reg.all_of<Collider>(e)) {
            auto& col = reg.get<Collider>(e);
            col.w = cw; col.h = ch;
        }

        if (!hb.IsDefault()) {
            // RenderOffset = negative of the hitbox origin so the sprite
            // draws with its top-left at (transform - offset), placing the
            // hitbox region at exactly the Transform position.
            int rox = -hb.offX;
            int roy = -hb.offY;
            if (reg.all_of<RenderOffset>(e))
                reg.get<RenderOffset>(e) = {rox, roy};
            else
                reg.emplace<RenderOffset>(e, rox, roy);
        } else {
            // Default: no offset
            if (reg.all_of<RenderOffset>(e))
                reg.get<RenderOffset>(e) = {0, 0};
        }
    }
};
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
    bool        cameraShake     = false; // trigger camera shake on destruction
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

// ── Enemy reaction delay ────────────────────────────────────────────────────
// Prevents enemies from instantly flipping direction. While turnCooldown > 0
// the enemy decelerates instead of re-acquiring the player.
struct EnemyReaction {
    float turnCooldown = 0.0f;
    float lastDirSign  = 0.0f; // +1 or -1, last committed chase direction
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
// Per-animation collider dimensions. If w == 0, the system falls back to
// the standing collider for that animation — so only slots with a custom
// hitbox set in the character creator will override.
struct AnimCollider {
    int w     = 0; // 0 = use stand dims
    int h     = 0;
    int roffX = 0;
    int roffY = 0;
    bool IsDefault() const { return w == 0 && h == 0; }
};

struct PlayerBaseCollider {
    int standW     = PLAYER_STAND_WIDTH;
    int standH     = PLAYER_STAND_HEIGHT;
    int standRoffX = PLAYER_STAND_ROFF_X;
    int standRoffY = PLAYER_STAND_ROFF_Y;
    int duckW      = PLAYER_DUCK_WIDTH;
    int duckH      = PLAYER_DUCK_HEIGHT;
    int duckRoffX  = PLAYER_DUCK_ROFF_X;
    int duckRoffY  = PLAYER_DUCK_ROFF_Y;

    // Per-animation overrides (zeros = fall back to stand dims)
    AnimCollider walk;
    AnimCollider jump;
    AnimCollider fall;
    AnimCollider slash;
    AnimCollider hurt;
    AnimCollider death;

    // Resolve the correct collider for a given animation ID.
    // Returns stand dims if the animation has no custom override.
    void Resolve(AnimationID id, int& outW, int& outH, int& outRoffX, int& outRoffY) const {
        const AnimCollider* ac = nullptr;
        switch (id) {
            case AnimationID::DUCK:  outW = duckW; outH = duckH; outRoffX = duckRoffX; outRoffY = duckRoffY; return;
            case AnimationID::WALK:  ac = &walk;  break;
            case AnimationID::JUMP:  ac = &jump;  break;
            case AnimationID::FRONT: ac = &fall;  break;
            case AnimationID::SLASH: ac = &slash; break;
            case AnimationID::HURT:  ac = &hurt;  break;
            case AnimationID::DEATH: ac = &death; break;
            default: break;
        }
        if (ac && !ac->IsDefault()) {
            outW = ac->w; outH = ac->h; outRoffX = ac->roffX; outRoffY = ac->roffY;
        } else {
            outW = standW; outH = standH; outRoffX = standRoffX; outRoffY = standRoffY;
        }
    }
};
