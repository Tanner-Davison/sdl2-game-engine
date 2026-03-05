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
        // Slashing is always allowed, even during the invincibility/hurt window
        // after taking a hit. The hurt flash is visual-only and should never
        // prevent the player from attacking.
        if (auto* atk = reg.try_get<AttackState>(entity)) {
            if (atk->attackPressed) {
                atk->attackPressed = false;
                atk->isAttacking   = true;
                atk->hitEntities.clear(); // new swing — reset per-swing hit registry
                r.sheet            = set.slashSheet;
                r.frames           = set.slash;
                anim.currentFrame  = 0;
                anim.timer         = 0.0f;
                anim.fps           = (set.slashFps > 0.0f) ? set.slashFps : 18.0f;
                anim.looping       = false;
                anim.totalFrames   = (int)set.slash.size();
                anim.currentAnim   = AnimationID::SLASH;
                if (reg.all_of<FlipCache>(entity)) {
                    auto& fc = reg.get<FlipCache>(entity);
                    for (auto* s : fc.frames) if (s) SDL_DestroySurface(s);
                    fc.frames.clear();
                }
                return;
            }
            if (atk->isAttacking) {
                // Safety: if something else stomped the anim out from under us,
                // isAttacking would be stuck true forever — clear it.
                if (anim.currentAnim != AnimationID::SLASH) {
                    atk->isAttacking   = false;
                    atk->attackPressed = false;
                } else if (anim.currentFrame >= anim.totalFrames - 1) {
                    atk->isAttacking = false;
                    // If still in invincibility window, resume hurt immediately
                    // so it overlaps naturally rather than replaying from frame 0.
                    if (inv.isInvincible) {
                        r.sheet           = set.hurtSheet;
                        r.frames          = set.hurt;
                        anim.currentFrame = 0;
                        anim.timer        = 0.0f;
                        anim.fps          = (set.hurtFps > 0.0f) ? set.hurtFps : 12.0f;
                        anim.looping      = false;
                        anim.totalFrames  = (int)set.hurt.size();
                        anim.currentAnim  = AnimationID::HURT;
                        if (reg.all_of<FlipCache>(entity)) {
                            auto& fc = reg.get<FlipCache>(entity);
                            for (auto* s : fc.frames) if (s) SDL_DestroySurface(s);
                            fc.frames.clear();
                        }
                        return;
                    }
                    anim.currentAnim = AnimationID::NONE;
                } else {
                    return; // hold slash anim until last frame
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

        // While on a ladder, force idle — skip all airborne checks.
        const ClimbState* climb = reg.try_get<ClimbState>(entity);
        if (climb && (climb->climbing || climb->atTop)) {
            frames  = &set.idle;
            fps     = resolveFps(set.idleFps, 12.0f);
            id      = AnimationID::IDLE;
        } else if (inv.isInvincible && !(reg.try_get<AttackState>(entity) &&
                                          reg.get<AttackState>(entity).isAttacking)) {
            // Show hurt anim during invincibility, but only if not mid-slash.
            // The player can always attack even while flashing from a hit.
            frames  = &set.hurt;
            fps     = resolveFps(set.hurtFps, 12.0f);
            looping = false;
            id      = AnimationID::HURT;
        } else if (!openWorld && g.active && !g.isGrounded) {
            frames = &set.jump;
            fps    = resolveFps(set.jumpFps, 4.0f);
            id     = AnimationID::JUMP;
        } else if (!openWorld && g.isCrouching) {
            frames = &set.duck;
            fps    = resolveFps(set.duckFps, 12.0f);
            id     = AnimationID::DUCK;
        } else if (moving) {
            frames = &set.walk;
            fps    = resolveFps(set.walkFps, 24.0f);
            id     = AnimationID::WALK;
        } else {
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
        if (!frames || anim.currentAnim == id)
            return;

        SDL_Surface* sheet = nullptr;
        switch (id) {
            case AnimationID::IDLE:
                sheet = set.idleSheet;
                break;
            case AnimationID::WALK:
                sheet = set.walkSheet;
                break;
            case AnimationID::JUMP:
                sheet = set.jumpSheet;
                break;
            case AnimationID::HURT:
                sheet = set.hurtSheet;
                break;
            case AnimationID::DUCK:
                sheet = set.duckSheet;
                break;
            case AnimationID::FRONT:
                sheet = set.frontSheet;
                break;
            case AnimationID::SLASH:
                sheet = set.slashSheet;
                break;
            default:
                break;
        }
        if (sheet && sheet != r.sheet) {
            r.sheet = sheet;
            if (reg.all_of<FlipCache>(entity)) {
                auto& fc = reg.get<FlipCache>(entity);
                for (auto* s : fc.frames)
                    if (s)
                        SDL_DestroySurface(s);
                fc.frames.clear();
            }
        }

        r.frames          = *frames;
        anim.currentFrame = 0;
        anim.timer        = 0.0f;
        anim.fps          = fps;
        anim.looping      = looping;
        anim.totalFrames  = (int)frames->size();
        anim.currentAnim  = id;
    });
}
