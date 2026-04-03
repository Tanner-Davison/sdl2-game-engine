#pragma once
#include "GameConfig.hpp"
#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Defined here so CollisionSystem can reference it without pulling in Level.hpp.
enum class SlopeType { None, DiagUpRight, DiagUpLeft };

struct Transform {
    float x = 0.0f;
    float y = 0.0f;
};

// Snapshotted each physics tick; RenderSystem lerps with alpha for smooth motion.
struct PrevTransform {
    float x = 0.0f;
    float y = 0.0f;
};

struct Velocity {
    float dx    = 0.0f;
    float dy    = 0.0f;
    float speed = PLAYER_SPEED;
};

enum class AnimationID { IDLE, WALK, JUMP, HURT, DUCK, FRONT, SLASH, DEATH, NONE };

struct AnimationState {
    int         currentFrame = 0;
    int         totalFrames  = 0;
    float       timer        = 0.0f;
    float       fps          = 12.0f;
    bool        looping      = true;
    AnimationID currentAnim  = AnimationID::NONE;
};

// Texture pointers are non-owning; the SpriteSheet objects must outlive this.
struct AnimationSet {
    std::vector<SDL_Rect> idle;
    SDL_Texture*          idleSheet  = nullptr;
    float                 idleFps    = 0.0f;
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

struct Renderable {
    SDL_Texture*          sheet = nullptr;
    std::vector<SDL_Rect> frames;
    bool                  flipH = false;
    int                   renderW = 0;
    int                   renderH = 0;
};

// Offset from Transform to sprite draw position (centers large sprites over their collider).
struct RenderOffset {
    int x = 0;
    int y = 0;
};

// Deprecated -- SDL_RenderTextureRotated handles flipping natively now.
struct FlipCache {};

struct Collider {
    int w = 0;
    int h = 0;
};

// Tiles whose hitbox doesn't start at their top-left corner.
struct ColliderOffset {
    int x = 0;
    int y = 0;
};

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
    bool       sprinting       = false;
    GravityDir direction       = GravityDir::DOWN;
    float      punishmentTimer = 0.0f;
};

// RenderSystem reads flashTimer to pulse the sprite red independently of
// InvincibilityTimer so the hit-flash still works during hazard damage.
struct HazardState {
    bool  active     = false;
    float flashTimer = 0.0f;
};

struct AttackState {
    bool attackPressed = false;
    bool isAttacking   = false;
    // Cleared each new swing so each attack only registers one hit per target.
    std::unordered_set<entt::entity> hitEntities;
};

struct DashState {
    bool  active         = false;
    float remaining      = 0.0f;
    float cooldown       = 0.0f;
    float direction      = 0.0f;
    float tapTimerLeft   = 0.0f;
    float tapTimerRight  = 0.0f;
    bool  releasedLeft   = true;
    bool  releasedRight  = true;
};

struct PlayerTag {};
struct EnemyTag {};

// Which player slot this entity belongs to.
// index 0 = P1 (keyboard + gamepad 0)
// index 1 = P2 (gamepad 1 only, active when two controllers are connected)
struct PlayerIndex {
    int index = 0;
};

// Optional color tint applied by RenderSystem when present (for player distinction).
// Values are SDL color-mod multipliers [0,255].
struct PlayerTint {
    Uint8 r = 255, g = 255, b = 255;
};
struct GoalTag {};
struct DeadTag {};
struct FaceRightTag {};  // sprite art faces right by default (flip when moving left)

struct EnemyAttackState {
    bool  attacking   = false;
    bool  dealtDamage = false;
    float cooldown    = 0.0f;
};

struct EnemyClimbState {
    bool  climbing    = false;
    bool  goingUp     = true;
    bool  steppingOff = false;
    float columnTop   = 0.0f;
    float columnBot   = 0.0f;
    float ladderCX    = 0.0f;
    float ladderW     = 0.0f;
};

// w == 0 means fall back to spriteW x spriteH with no offset.
struct EnemyHitbox {
    int w = 0, h = 0;
    int offX = 0, offY = 0;
    int roffX = 0, roffY = 0;
    bool IsDefault() const { return w == 0 && h == 0; }
};

// Non-owning sheet pointers; actual SpriteSheet objects owned by GameScene::mEnemySpriteSheets.
struct EnemyAnimData {
    SDL_Texture*          attackSheet = nullptr;
    std::vector<SDL_Rect> attackFrames;
    float                 attackFps   = 10.0f;
    EnemyHitbox           attackHitbox;

    SDL_Texture*          hurtSheet   = nullptr;
    std::vector<SDL_Rect> hurtFrames;
    float                 hurtFps     = 8.0f;
    EnemyHitbox           hurtHitbox;

    SDL_Texture*          deadSheet   = nullptr;
    std::vector<SDL_Rect> deadFrames;
    float                 deadFps     = 6.0f;
    EnemyHitbox           deadHitbox;

    SDL_Texture*          moveSheet   = nullptr;
    std::vector<SDL_Rect> moveFrames;
    float                 moveFps     = 7.0f;
    EnemyHitbox           moveHitbox;

    SDL_Texture*          idleSheet   = nullptr;  // nullptr = same as moveSheet
    EnemyHitbox           idleHitbox;
    int spriteW = 40, spriteH = 40;
    std::string typeName;

    struct SfxFile { float volume = 1.0f; bool timeStretch = false; float trimStart = 0.0f; float trimEnd = 1.0f; };
    struct SlotSfx {
        std::vector<SfxFile> files;
        int nextIdx = 0;
    };
    std::array<SlotSfx, 5> slotSfx{};
    bool sfxRetrigger = false;

    // Applies hitbox dims to entity Collider/RenderOffset, pinning the collider bottom
    // so feet stay on the ground when hitbox changes between animations.
    void ApplyHitbox(const EnemyHitbox& hb, entt::registry& reg, entt::entity e) const {
        int cw = hb.IsDefault() ? spriteW : hb.w;
        int ch = hb.IsDefault() ? spriteH : hb.h;

        if (reg.all_of<Collider>(e) && reg.all_of<Transform>(e)) {
            auto& col = reg.get<Collider>(e);
            auto& tr  = reg.get<Transform>(e);
            float oldBottom = tr.y + col.h;
            col.w = cw;
            col.h = ch;
            tr.y = oldBottom - ch;
        } else if (reg.all_of<Collider>(e)) {
            auto& col = reg.get<Collider>(e);
            col.w = cw; col.h = ch;
        }

        if (!hb.IsDefault()) {
            int rox = -hb.offX;
            int roy = -hb.offY;
            if (reg.all_of<RenderOffset>(e))
                reg.get<RenderOffset>(e) = {rox, roy};
            else
                reg.emplace<RenderOffset>(e, rox, roy);
        } else {
            if (reg.all_of<RenderOffset>(e))
                reg.get<RenderOffset>(e) = {0, 0};
        }
    }
};

struct TileTag {};
struct LadderTag {};
struct PropTag {};       // prop rendered behind the player (Pass 1)
struct PropFrontTag {};  // prop rendered in front of the player (Pass 3)
struct HazardTag {};

struct FloatTag {};

struct FloatState {
    float bobTimer   = 0.0f;
    float bobAmp     = 6.0f;
    float bobSpeed   = 2.0f;
    float bobPhase   = 0.0f;
    float baseY      = 0.0f;
    float driftVx    = 0.0f;
    float driftVy    = 0.0f;
    float spinAngle  = 0.0f;
    float spinSpeed  = 0.0f;
    bool  wasInContact = false;
    float dyThisFrame  = 0.0f;
    static constexpr float DRAG = 1.8f;
};

struct DestructibleTag {
    SDL_Texture* breakSurface = nullptr;

    DestructibleTag() = default;
    explicit DestructibleTag(SDL_Texture* s) : breakSurface(s) {}
    DestructibleTag(const DestructibleTag&)            = delete;
    DestructibleTag& operator=(const DestructibleTag&) = delete;
    DestructibleTag(DestructibleTag&&)                 = default;
    DestructibleTag& operator=(DestructibleTag&&)      = default;
};

struct OpenWorldTag {};

enum class PowerUpType {
    None,
    AntiGravity,
    Turret,
    HealthBoost,
    Teleport,
};

struct PowerUpTag {
    PowerUpType type      = PowerUpType::None;
    float       duration  = 15.0f;
    float       fireRate  = 3.0f;
    float       healthPct = 25.0f;
    int         teleportGroup = 0;
    std::string sfxPath;
};

// Marks a tile as a teleport entrance. On player overlap, moves player to the
// matching TeleportDestTag with the same group ID.
struct TeleportEntranceTag {
    int group = 0;
};

// Marks a tile as a teleport destination (prop, no collision).
struct TeleportDestTag {
    int group = 0;
};

// Attached to the player after teleporting to prevent re-trigger.
struct TeleportCooldown {
    float remaining = 0.3f;
};

struct ActivePowerUp {
    PowerUpType type      = PowerUpType::None;
    float       remaining = 0.0f;
    float       duration  = 0.0f;
};

// Each PowerUpType gets its own independent timer so multiple can run simultaneously.
struct ActivePowerUps {
    struct Slot { float remaining = 0.f; float duration = 0.f; };
    std::unordered_map<int, Slot> slots;

    void add(PowerUpType t, float dur) {
        int k = (int)t;
        auto it = slots.find(k);
        if (it != slots.end()) {
            it->second.remaining += dur;
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

struct ActionTag {
    int         group           = 0;
    int         hitsRequired    = 1;
    int         hitsRemaining   = 1;
    std::string destroyAnimPath;
    bool        cameraShake     = false;
};

// Mid-death-animation: no longer solid, anim frames being played.
// Destruction deferred one tick after reachedEnd so the last frame renders.
struct DestroyAnimTag {
    int   totalFrames  = 0;
    float fps          = 8.0f;
    bool  reachedEnd   = false;
};

// heightFrac: fraction of tile height the slope rises over (1.0 = fully diagonal).
// Low corner is always at tile bottom on the appropriate side.
struct SlopeCollider {
    SlopeType slopeType  = SlopeType::None;
    float     heightFrac = 1.0f;
};

struct MovingPlatformTag {};

struct MovingPlatformState {
    bool  horiz       = true;
    float range       = 96.0f;
    float speed       = 60.0f;
    int   groupId     = 0;
    float originX     = 0.0f;
    float originY     = 0.0f;
    float phase       = 0.0f;
    float vx          = 0.0f;
    float vy          = 0.0f;
    bool  playerOnTop = false;
    bool  loop        = false;
    int   loopDir     = 1;
    bool  trigger     = false;
    bool  triggered   = false;
};

// AnimationSystem skips these; GameScene::Update advances the frame counter.
struct TileAnimTag {};

struct HitFlash {
    float timer    = 0.18f;
    float duration = 0.18f;
};

// Prevents enemies from instantly flipping direction on aggro change.
struct EnemyReaction {
    float turnCooldown = 0.0f;
    float lastDirSign  = 0.0f;
};

struct ClimbState {
    bool onLadder  = false;
    bool climbing  = false;
    bool atTop     = false;
    bool wPressed  = false;
    bool sPressed  = false;
};

// Resolved standing collider dims for this character (from PlayerProfile or defaults).
// Per-animation overrides: w == 0 falls back to standing collider.
struct AnimCollider {
    int w     = 0;
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

    AnimCollider walk;
    AnimCollider jump;
    AnimCollider fall;
    AnimCollider slash;
    AnimCollider hurt;
    AnimCollider death;

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

struct ShooterTag {
    int   side        = 0;     // 0=Top 1=Right 2=Bottom 3=Left
    float range       = 300.f;
    float fireRate    = 1.5f;
    float bulletSpeed = 200.f;
    float damage      = 10.f;
    std::string sfxPath;
};

struct ShooterState {
    float cooldownLeft = 0.f;
};

struct BulletTag {
    float dx = 0.f, dy = 0.f;
    float speed  = 200.f;
    float damage = 10.f;
    float originX = 0.f, originY = 0.f;
    float maxRange = 300.f;
    entt::entity sourceTurret = entt::null;
    bool playerOwned = false;
};

struct ShieldPickupTag {
    float duration = 20.f;
};

struct ShieldEntry {
    SDL_Texture* tex       = nullptr;
    int          renderW   = 0;
    int          renderH   = 0;
    float        remaining = 20.f;
};

struct ActiveShield {
    std::vector<ShieldEntry> shields;
    float angle       = 0.f;
    float orbitRadius = 30.f;
};

struct ActiveTurretPowerUp {
    float angle       = 0.f;
    float orbitRadius = 28.f;
    float remaining   = 15.f;
    float fireRate    = 3.0f;
    float cooldown    = 0.f;
    float bulletSpeed = 500.f;
    float damage      = 15.f;
    float range       = 1200.f;
    std::string sfxId;
};
