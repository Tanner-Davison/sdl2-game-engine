#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <algorithm>
#include <entt/entt.hpp>

// States: idle (gravity on, ladder top = floor), climbing (gravity off,
// W/S moves, no input = frozen), atTop (locked to topRestY, S descends,
// Space jumps off). Ladder top acts as a solid floor in idle.
inline void LadderSystem(entt::registry& reg, float dt) {
    const bool* keys      = SDL_GetKeyboardState(nullptr);
    bool        spaceHeld = keys[SDL_SCANCODE_SPACE];

    auto ladderView = reg.view<LadderTag, Transform, Collider>();
    auto playerView =
        reg.view<PlayerTag, Transform, Collider, GravityState, Velocity, ClimbState>();

    playerView.each([&](Transform&      pt,
                        const Collider& pc,
                        GravityState&   g,
                        Velocity&       v,
                        ClimbState&     climb) {
        constexpr float inset = 8.0f;
        float columnTop = 1e9f;
        float columnBot = -1e9f;
        bool  inColumn  = false;
        bool  touching  = false;

        ladderView.each([&](const Transform& lt, const Collider& lc) {
            bool alignX = (pt.x + inset) < (lt.x + lc.w) && (pt.x + pc.w - inset) > lt.x;
            if (!alignX) return;
            inColumn  = true;
            columnTop = std::min(columnTop, lt.y);
            columnBot = std::max(columnBot, lt.y + lc.h);
            bool overlapY = pt.y < (lt.y + lc.h) && (pt.y + pc.h) > lt.y;
            if (overlapY) touching = true;
        });

        if (!inColumn) { columnTop = 0.0f; columnBot = 0.0f; }

        float topRestY = columnTop - pc.h;

        climb.onLadder = touching;

        bool wHeld = climb.wPressed;
        bool sHeld = climb.sPressed;

        // --- atTop ---
        if (climb.atTop) {
            g.active   = false;
            g.velocity = 0.0f;
            v.dy       = 0.0f;

            if (!inColumn) {
                climb.atTop  = false;
                g.active     = true;
                g.velocity   = 0.0f;
                g.isGrounded = false;
                return;
            }

            pt.y = topRestY;

            if (spaceHeld) {
                climb.atTop  = false;
                g.active     = true;
                g.velocity   = -JUMP_FORCE;
                g.isGrounded = false;
                return;
            }
            if (sHeld) {
                // Release top-lock and let climbing branch move down smoothly.
                climb.atTop    = false;
                climb.climbing = true;
                g.active       = false;
                g.velocity     = 0.0f;
            }
            return;
        }

        // --- climbing ---
        if (climb.climbing) {
            g.velocity   = 0.0f;
            g.isGrounded = true; // suppress fall/jump animation while on ladder
            v.dy         = 0.0f;
            v.dx        *= 0.6f;

            if (spaceHeld) {
                climb.climbing = false;
                g.active       = true;
                g.velocity     = -JUMP_FORCE;
                g.isGrounded   = false;
                return;
            }
            if (!inColumn) {
                climb.climbing = false;
                g.active       = true;
                g.velocity     = 0.0f;
                g.isGrounded   = false;
                return;
            }
            if (wHeld) {
                pt.y -= CLIMB_SPEED * dt;
                if (pt.y <= topRestY) {
                    pt.y           = topRestY;
                    climb.climbing = false;
                    climb.atTop    = true;
                    g.active       = false;
                    g.velocity     = 0.0f;
                }
            } else if (sHeld) {
                pt.y += CLIMB_SPEED * dt;
                if (pt.y + pc.h >= columnBot) {
                    pt.y           = columnBot - pc.h;
                    climb.climbing = false;
                    g.active       = true;
                    g.velocity     = 0.0f;
                    g.isGrounded   = false;
                }
            }
            return;
        }

        // --- idle: ladder top acts as solid floor ---
        float feetY = pt.y + pc.h;

        bool overColumnTop = inColumn
                             && feetY >= columnTop - 2.0f
                             && feetY <= columnTop + STEP_UP_HEIGHT;
        if (overColumnTop) {
            if (sHeld) {
                climb.climbing = true;
                climb.atTop    = false;
                g.active       = false;
                g.velocity     = 0.0f;
                v.dy           = 0.0f;
                pt.y           = columnTop + 1.0f;
            } else {
                // Mark atTop so PlayerStateSystem always sees a valid ladder state.
                pt.y           = topRestY;
                climb.atTop    = true;
                climb.climbing = false;
                g.active       = false;
                g.isGrounded   = true;
                g.velocity     = 0.0f;
                v.dy           = 0.0f;
            }
            return;
        }

        // W mid-column — grab ladder to climb up
        if (touching && wHeld) {
            climb.climbing = true;
            g.active       = false;
            g.velocity     = 0.0f;
            v.dy           = 0.0f;

            pt.y -= CLIMB_SPEED * dt;
            if (pt.y <= topRestY) {
                pt.y           = topRestY;
                climb.climbing = false;
                climb.atTop    = true;
            }
        }
    });
}
