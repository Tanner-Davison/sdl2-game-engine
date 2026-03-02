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
