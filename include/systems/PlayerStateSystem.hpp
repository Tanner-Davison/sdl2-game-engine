#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <entt/entt.hpp>

// PLAYER_DUCK_ROFF_X / PLAYER_STAND_ROFF_X / collider dims come from GameConfig
// via Components.hpp → GameConfig.hpp include chain.

inline void PlayerStateSystem(entt::registry& reg) {
    auto view = reg.view<PlayerTag,
                         Velocity,
                         GravityState,
                         Transform,
                         Collider,
                         Renderable,
                         AnimationState,
                         AnimationSet,
                         InvincibilityTimer>();

    view.each([&reg](entt::entity              entity,
                     const Velocity&           v,
                     const GravityState&       g,
                     Transform&                t,
                     Collider&                 col,
                     Renderable&               r,
                     AnimationState&           anim,
                     const AnimationSet&       set,
                     const InvincibilityTimer& inv) {
        // Resolve per-character collider baseline — falls back to frost-knight
        // constants when the component is absent (sandbox / no-profile mode).
        const PlayerBaseCollider* base = reg.try_get<PlayerBaseCollider>(entity);
        const int standW    = base ? base->standW    : PLAYER_STAND_WIDTH;
        const int standH    = base ? base->standH    : PLAYER_STAND_HEIGHT;
        const int standRoffX = base ? base->standRoffX : PLAYER_STAND_ROFF_X;
        const int standRoffY = base ? base->standRoffY : PLAYER_STAND_ROFF_Y;
        const int duckW     = base ? base->duckW     : PLAYER_DUCK_WIDTH;
        const int duckH     = base ? base->duckH     : PLAYER_DUCK_HEIGHT;
        const int duckRoffX  = base ? base->duckRoffX  : PLAYER_DUCK_ROFF_X;
        const int duckRoffY  = base ? base->duckRoffY  : PLAYER_DUCK_ROFF_Y;
        // ── Attack state: start or continue slash ──────────────────────────
        // Attack ALWAYS takes priority over hurt/invincibility visuals.
        // Rules:
        //   - attackPressed while idle/hurt/anything: start a fresh slash.
        //   - attackPressed while already slashing (spam): restart from frame 0
        //     so rapid taps each play a full swing.
        //   - isAttacking + currentAnim==SLASH: hold the slash to completion,
        //     then drop back to hurt if still invincible, else to NONE.
        //   - isAttacking + currentAnim!=SLASH: something external (hurt) stomped
        //     the anim — re-assert slash immediately so it visually takes over.
        if (auto* atk = reg.try_get<AttackState>(entity)) {
            if (atk->attackPressed) {
                // Start a new swing (or restart mid-swing for rapid-fire F spam).
                atk->attackPressed = false;
                atk->isAttacking   = true;
                atk->hitEntities.clear();
                r.sheet            = set.slashSheet;
                r.frames           = set.slash;
                anim.currentFrame  = 0;
                anim.timer         = 0.0f;
                anim.fps           = (set.slashFps > 0.0f) ? set.slashFps : 18.0f;
                anim.looping       = false;
                anim.totalFrames   = (int)set.slash.size();
                anim.currentAnim   = AnimationID::SLASH;
                return;
            }
            if (atk->isAttacking) {
                if (anim.currentAnim != AnimationID::SLASH) {
                    // Hurt (or anything else) stomped our anim — re-assert slash.
                    // This is the key fix: instead of surrendering, we take it back.
                    r.sheet           = set.slashSheet;
                    r.frames          = set.slash;
                    anim.currentFrame = 0;
                    anim.timer        = 0.0f;
                    anim.fps          = (set.slashFps > 0.0f) ? set.slashFps : 18.0f;
                    anim.looping      = false;
                    anim.totalFrames  = (int)set.slash.size();
                    anim.currentAnim  = AnimationID::SLASH;
                    return;
                }
                if (anim.currentFrame >= anim.totalFrames - 1) {
                    // Slash finished — return to hurt if still invincible, else idle.
                    atk->isAttacking = false;
                    if (inv.isInvincible && !set.hurt.empty()) {
                        r.sheet           = set.hurtSheet;
                        r.frames          = set.hurt;
                        anim.currentFrame = 0;
                        anim.timer        = 0.0f;
                        anim.fps          = (set.hurtFps > 0.0f) ? set.hurtFps : 12.0f;
                        anim.looping      = false;
                        anim.totalFrames  = (int)set.hurt.size();
                        anim.currentAnim  = AnimationID::HURT;
                        return;
                    }
                    anim.currentAnim = AnimationID::NONE;
                } else {
                    return; // mid-slash — hold until last frame
                }
            }
        }

        // ── Determine target animation ────────────────────────────────────────
        const std::vector<SDL_Rect>* frames  = nullptr;
        float                        fps     = 12.0f;
        bool                         looping = true;
        AnimationID                  id      = AnimationID::NONE;

        bool moving    = std::abs(v.dx) > 1.0f || std::abs(v.dy) > 1.0f;
        bool openWorld = reg.all_of<OpenWorldTag>(entity);

        // Helper: use profile FPS when authored (> 0), else engine default
        auto resolveFps = [](float profileFps, float engineDefault) {
            return profileFps > 0.0f ? profileFps : engineDefault;
        };

        // Slot capability helpers — an empty frames vector means the character
        // profile has no animation for that action, which disables it entirely.
        // No jump frames = can't jump. No slash frames = can't attack. Etc.
        const bool canJump  = !set.jump.empty();
        const bool canDuck  = !set.duck.empty();
        const bool canSlash = !set.slash.empty();
        const bool canHurt  = !set.hurt.empty();
        const bool canWalk  = !set.walk.empty();

        // Disable jump at the physics level if no jump animation exists.
        // Clear jumpHeld so InputSystem can't queue a jump either.
        if (!canJump) {
            const_cast<GravityState&>(g).jumpHeld = false;
        }
        // Disable slash at the input level if no slash animation exists.
        if (!canSlash) {
            if (auto* atk = reg.try_get<AttackState>(entity)) {
                atk->attackPressed = false;
                atk->isAttacking   = false;
            }
        }

        // While on a ladder, force idle — skip all airborne checks.
        const ClimbState* climb = reg.try_get<ClimbState>(entity);
        if (climb && (climb->climbing || climb->atTop)) {
            frames  = &set.idle;
            fps     = resolveFps(set.idleFps, 12.0f);
            id      = AnimationID::IDLE;
        } else if (canHurt && inv.isInvincible
                   && !(reg.try_get<AttackState>(entity) && reg.get<AttackState>(entity).isAttacking)
                   && !(anim.currentAnim == AnimationID::HURT
                        && !anim.looping
                        && anim.currentFrame >= anim.totalFrames - 1)) {
            // Show hurt anim during invincibility, but only if:
            //   - not mid-slash (attack takes priority), AND
            //   - the hurt anim hasn't already finished playing.
            // Once the hurt anim reaches its last frame it is done — let normal
            // animation (idle/walk/jump) resume even while still invincible.
            // This prevents the player from being frozen on the last hurt frame
            // for the entire 1.5s invincibility window between hits.
            frames  = &set.hurt;
            fps     = resolveFps(set.hurtFps, 12.0f);
            looping = false;
            id      = AnimationID::HURT;
        } else if (!openWorld && !g.active) {
            // Gravity is suspended — check if it's due to an anti-gravity power-up.
            // If so, play the fall (FRONT) animation so the player looks like they're
            // floating, not standing still.  Fall back to idle if no front frames exist.
            const ActivePowerUps* aps = reg.try_get<ActivePowerUps>(entity);
            if (aps && aps->has(PowerUpType::AntiGravity)) {
                if (!set.front.empty()) {
                    frames = &set.front;
                    fps    = resolveFps(set.frontFps, 6.0f);
                    id     = AnimationID::FRONT;
                } else {
                    frames = &set.idle;
                    fps    = resolveFps(set.idleFps, 12.0f);
                    id     = AnimationID::IDLE;
                }
            } else {
                // Gravity disabled for another reason — default to idle
                frames = &set.idle;
                fps    = resolveFps(set.idleFps, 12.0f);
                id     = AnimationID::IDLE;
            }
        } else if (canJump && !openWorld && g.active && !g.isGrounded) {
            frames = &set.jump;
            fps    = resolveFps(set.jumpFps, 4.0f);
            id     = AnimationID::JUMP;
        } else if (canDuck && !openWorld && g.isCrouching) {
            frames = &set.duck;
            fps    = resolveFps(set.duckFps, 12.0f);
            id     = AnimationID::DUCK;
        } else if (canWalk && moving) {
            frames = &set.walk;
            fps    = resolveFps(set.walkFps, 24.0f);
            id     = AnimationID::WALK;
        } else {
            // Always fall back to idle — every character must have idle frames.
            frames = &set.idle;
            fps    = resolveFps(set.idleFps, 12.0f);
            id     = AnimationID::IDLE;
        }

        // ── Collider enforcement — runs every frame, before any early-out ─────
        // Must be here so wall transitions (which reset col to standing dims)
        // get corrected even when the animation ID hasn't changed.
        bool ducking = (id == AnimationID::DUCK);
        {
            int wantW = ducking ? duckW : standW;
            int wantH = ducking ? duckH : standH;

            if (col.w != wantW || col.h != wantH) {
                switch (g.direction) {
                    case GravityDir::DOWN:
                        t.y = (t.y + col.h) - wantH;
                        break;
                    case GravityDir::RIGHT:
                        t.x = (t.x + col.h) - wantH;
                        break;
                    case GravityDir::UP:
                    case GravityDir::LEFT:
                        break;
                }
                col.w = wantW;
                col.h = wantH;

                if (g.direction == GravityDir::DOWN) {
                    if (auto* roff = reg.try_get<RenderOffset>(entity)) {
                        roff->x = ducking ? duckRoffX : standRoffX;
                        roff->y = ducking ? duckRoffY : standRoffY;
                    }
                }
            }
        }

        // ── Animation swap — only when animation actually changes ─────────────
        // Special case: if we're re-entering HURT (new hit just landed, resetting
        // invincibility) but the anim is already on the last hurt frame, restart
        // from frame 0 so the full hurt animation plays again.
        if (id == AnimationID::HURT && anim.currentAnim == AnimationID::HURT
            && anim.currentFrame >= anim.totalFrames - 1) {
            anim.currentFrame = 0;
            anim.timer        = 0.0f;
            anim.looping      = false;
            return;
        }
        if (!frames || anim.currentAnim == id)
            return;

        SDL_Texture* sheet = nullptr;
        switch (id) {
            case AnimationID::IDLE:  sheet = set.idleSheet;  break;
            case AnimationID::WALK:  sheet = set.walkSheet;  break;
            case AnimationID::JUMP:  sheet = set.jumpSheet;  break;
            case AnimationID::HURT:  sheet = set.hurtSheet;  break;
            case AnimationID::DUCK:  sheet = set.duckSheet;  break;
            case AnimationID::FRONT: sheet = set.frontSheet; break;
            case AnimationID::SLASH: sheet = set.slashSheet; break;
            default: break;
        }
        // Always set sheet and frames together atomically — a mismatched
        // sheet/frames pair is what causes two-character flickering.
        if (sheet) r.sheet = sheet;
        r.frames          = *frames;
        anim.currentFrame = 0;
        anim.timer        = 0.0f;
        anim.fps          = fps;
        anim.looping      = looping;
        anim.totalFrames  = (int)frames->size();
        anim.currentAnim  = id;
    });
}
