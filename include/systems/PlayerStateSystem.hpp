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
        // ── Attack state: start or continue slash ──────────────────────────
        if (auto* atk = reg.try_get<AttackState>(entity)) {
            if (atk->attackPressed && !inv.isInvincible) {
                atk->attackPressed = false;
                atk->isAttacking   = true;
                r.sheet            = set.slashSheet;
                r.frames           = set.slash;
                anim.currentFrame  = 0;
                anim.timer         = 0.0f;
                anim.fps           = 18.0f;
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
                // Safety: if something else (hazard hurt, etc.) stomped the anim
                // out from under us, the slash-finish check will never fire and
                // isAttacking gets stuck true forever. Clear it immediately.
                if (anim.currentAnim != AnimationID::SLASH) {
                    atk->isAttacking   = false;
                    atk->attackPressed = false;
                } else if (anim.currentFrame >= anim.totalFrames - 1) {
                    atk->isAttacking = false;
                    anim.currentAnim = AnimationID::NONE;
                } else if (!inv.isInvincible) {
                    return; // hold slash until last frame
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

        if (inv.isInvincible) {
            frames  = &set.hurt;
            fps     = 12.0f;
            looping = false;
            id      = AnimationID::HURT;
        } else if (!openWorld && g.active && !g.isGrounded) {
            // OpenWorld has no gravity, so we never show the jump animation
            frames = &set.jump;
            fps    = 4.0f;
            id     = AnimationID::JUMP;
        } else if (!openWorld && g.isCrouching) {
            frames = &set.duck;
            fps    = 12.0f;
            id     = AnimationID::DUCK;
        } else if (moving) {
            frames = &set.walk;
            fps    = 24.0f;
            id     = AnimationID::WALK;
        } else {
            frames = &set.idle;
            fps    = 12.0f;
            id     = AnimationID::IDLE;
        }

        // ── Collider enforcement — runs every frame, before any early-out ─────
        // Must be here so wall transitions (which reset col to standing dims)
        // get corrected even when the animation ID hasn't changed.
        bool ducking = (id == AnimationID::DUCK);
        {
            int wantW = ducking ? PLAYER_DUCK_WIDTH : PLAYER_STAND_WIDTH;
            int wantH = ducking ? PLAYER_DUCK_HEIGHT : PLAYER_STAND_HEIGHT;

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
                        roff->x = ducking ? PLAYER_DUCK_ROFF_X  : PLAYER_STAND_ROFF_X;
                        roff->y = ducking ? PLAYER_DUCK_ROFF_Y  : PLAYER_STAND_ROFF_Y;
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
