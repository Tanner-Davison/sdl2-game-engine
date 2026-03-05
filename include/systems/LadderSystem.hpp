#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <algorithm>
#include <entt/entt.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// LadderSystem
//
// Ladder top acts like a solid floor — the player walks over it freely and
// only descends when S is pressed. W grabs the ladder mid-column during a
// jump or from the ground. Space always jumps off.
//
// States:
//   idle    — normal gravity; ladder top acts as a floor clamp
//   climbing — gravity off, W moves up, S moves down, no input = frozen
//   atTop   — gravity off, locked to topRestY, S descends, Space jumps off
// ─────────────────────────────────────────────────────────────────────────────
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
        // ── Build column extents ──────────────────────────────────────────────
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

        float topRestY = columnTop - pc.h;  // feet-level when standing at top

        climb.onLadder = touching;

        bool wHeld = climb.wPressed;
        bool sHeld = climb.sPressed;

        // ─────────────────────────────────────────────────────────────────────
        // atTop — locked to top, S descends, Space jumps, walk off freely
        // ─────────────────────────────────────────────────────────────────────
        if (climb.atTop) {
            g.active   = false;
            g.velocity = 0.0f;
            v.dy       = 0.0f;

            if (!inColumn) {
                // Walked off the side — restore gravity
                climb.atTop  = false;
                g.active     = true;
                g.velocity   = 0.0f;
                g.isGrounded = false;
                return;
            }

            pt.y = topRestY; // hard-lock Y, allow dx freely

            if (spaceHeld) {
                climb.atTop  = false;
                g.active     = true;
                g.velocity   = -JUMP_FORCE;
                g.isGrounded = false;
                return;
            }
            if (sHeld) {
                climb.atTop    = false;
                climb.climbing = true;
                g.active       = false;
                g.velocity     = 0.0f;
                pt.y           = columnTop + 1.0f;
            }
            // W or no input — stay at top, walk freely horizontally
            return;
        }

        // ─────────────────────────────────────────────────────────────────────
        // climbing — gravity off, W up, S down, no input = frozen
        // ─────────────────────────────────────────────────────────────────────
        if (climb.climbing) {
            g.velocity   = 0.0f;
            g.isGrounded = true; // prevent fall/jump animation while on ladder
            v.dy         = 0.0f;
            v.dx        *= 0.6f; // slow horizontal movement while climbing

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
                // Reached the bottom — release ladder and restore gravity
                if (pt.y + pc.h >= columnBot) {
                    pt.y           = columnBot - pc.h;
                    climb.climbing = false;
                    g.active       = true;
                    g.velocity     = 0.0f;
                    g.isGrounded   = false;
                }
            }
            // No input — frozen mid-ladder
            return;
        }

        // ─────────────────────────────────────────────────────────────────────
        // idle — treat ladder top as a solid floor.
        // The player walks over the ladder freely. When they step off the
        // platform edge so their feet are above the column top AND S is pressed,
        // drop into climbing. If their feet land on the column top from above
        // (falling), snap them onto it like a floor.
        // W mid-column grabs the ladder to climb up.
        // ─────────────────────────────────────────────────────────────────────
        float feetY = pt.y + pc.h;

        // Floor clamp: player's feet are at or just below columnTop while
        // horizontally aligned — treat it as solid ground.
        bool overColumnTop = inColumn
                             && feetY >= columnTop - 2.0f
                             && feetY <= columnTop + STEP_UP_HEIGHT;
        if (overColumnTop) {
            if (sHeld) {
                // S pressed — drop into ladder
                climb.climbing = true;
                climb.atTop    = false;
                g.active       = false;
                g.velocity     = 0.0f;
                v.dy           = 0.0f;
                pt.y           = columnTop + 1.0f;
            } else {
                // No S — treat ladder top as solid floor AND mark atTop so
                // PlayerStateSystem always sees a valid ladder state.
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
