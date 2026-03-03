#pragma once
#include <Components.hpp>
#include <entt/entt.hpp>

// PLAYER_STAND_WIDTH / PLAYER_STAND_HEIGHT come from GameConfig via Components.hpp.

// levelW/levelH: total world size in pixels. Pass 0 to use windowW/windowH (single-screen).
// In a scrolling level, pass the actual world dimensions so the player can walk
// past the initial viewport without being clamped to the screen edge.
inline void BoundsSystem(entt::registry& reg, float dt, int windowW, int windowH,
                         bool wallRunEnabled = false,
                         float levelW = 0.0f, float levelH = 0.0f) {
    // Fall back to screen size when no explicit level bounds are given
    if (levelW <= 0.0f) levelW = static_cast<float>(windowW);
    if (levelH <= 0.0f) levelH = static_cast<float>(windowH);

    auto view = reg.view<Transform, Collider, GravityState, Velocity, AnimationState, PlayerTag>();
    view.each(
        [&](entt::entity ent, Transform& t, Collider& c, GravityState& g,
            Velocity& v, AnimationState& anim) {
            // ── Open-world: clamp to level bounds, no gravity logic ───────────
            if (reg.all_of<OpenWorldTag>(ent)) {
                if (t.x < 0.0f)             t.x = 0.0f;
                if (t.x + c.w > levelW)     t.x = levelW - c.w;
                if (t.y < 0.0f)             t.y = 0.0f;
                if (t.y + c.h > levelH)     t.y = levelH - c.h;
                return;
            }

            // ── Punishment timer tick ─────────────────────────────────────────
            if (g.punishmentTimer > 0.0f) {
                g.punishmentTimer -= dt;
                if (g.punishmentTimer <= 0.0f) {
                    g.punishmentTimer = 0.0f;
                    g.active          = true;
                }
            }

            // Activate gravity toward a wall, resetting motion and crouch state.
            auto activate = [&](GravityDir dir) {
                if (g.punishmentTimer > 0.0f) return;
                if (g.active && g.isGrounded && g.direction == dir) return;

                g.isCrouching    = false;
                c.w              = PLAYER_STAND_WIDTH;
                c.h              = PLAYER_STAND_HEIGHT;
                anim.currentAnim = AnimationID::NONE;

                g.timer      = 0.0f;
                g.active     = true;
                g.isGrounded = false;
                g.velocity   = 0.0f;
                g.direction  = dir;
                v.dx         = 0.0f;
                v.dy         = 0.0f;
            };

            // ── Wall-run: trigger gravity flip at level edges ─────────────────
            if (wallRunEnabled) {
                if (t.x < 0.0f) {
                    t.x = 0.0f;
                    activate(GravityDir::LEFT);
                }
                {
                    bool  sw        = g.active && (g.direction == GravityDir::LEFT ||
                                                   g.direction == GravityDir::RIGHT);
                    float rightEdge = t.x + (sw ? c.h : c.w);
                    if (rightEdge > levelW) {
                        t.x = levelW - (sw ? c.h : c.w);
                        activate(GravityDir::RIGHT);
                    }
                }
                if (t.y < 0.0f) {
                    t.y = 0.0f;
                    activate(GravityDir::UP);
                }
                {
                    bool  sw         = g.active && (g.direction == GravityDir::LEFT ||
                                                    g.direction == GravityDir::RIGHT);
                    float bottomEdge = t.y + (sw ? c.w : c.h);
                    if (bottomEdge > levelH) {
                        t.y = levelH - (sw ? c.w : c.h);
                        activate(GravityDir::DOWN);
                    }
                }
            } else {
                // Platformer — clamp to level bounds, no gravity flip
                if (t.x < 0.0f)             t.x = 0.0f;
                if (t.x + c.w > levelW)     t.x = levelW - c.w;
                if (t.y < 0.0f)             t.y = 0.0f;
                if (t.y + c.h > levelH) {
                    t.y          = levelH - c.h;
                    g.velocity   = 0.0f;
                    g.isGrounded = true;
                }
            }

            // ── Ground-clamp per gravity direction ────────────────────────────
            if (g.active) {
                switch (g.direction) {
                    case GravityDir::DOWN:
                        if (t.y + c.h >= levelH) {
                            t.y          = levelH - c.h;
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        break;
                    case GravityDir::UP:
                        if (t.y <= 0.0f) {
                            t.y          = 0.0f;
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        break;
                    case GravityDir::LEFT:
                        if (t.x <= 0.0f) {
                            t.x          = 0.0f;
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        break;
                    case GravityDir::RIGHT:
                        if (t.x + c.h >= levelW) {
                            t.x          = levelW - c.h;
                            g.velocity   = 0.0f;
                            g.isGrounded = true;
                        }
                        break;
                }
            }
        });
}
