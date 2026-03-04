#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <algorithm>
#include <entt/entt.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// LadderSystem
//
// Uses event-driven climb.wPressed / climb.sPressed flags (set/cleared by
// InputSystem on KEY_DOWN/KEY_UP) rather than polling SDL_GetKeyboardState.
// This means a tap moves for exactly the frames the key is physically held —
// releasing W immediately stops the player mid-ladder.
//
// Column model: all LadderTag tiles horizontally aligned with the player are
// treated as one column. columnTop/columnBot define the full climbable range.
// The player can never move above columnTop - pc.h (topRestY).
//
// States:
//   idle    — normal gravity, no ladder interaction
//   climbing — gravity off, wPressed moves up (clamped), sPressed moves down
//   atTop   — gravity off, player snapped to topRestY, only sPressed descends
//
// Grab rules:
//   - Only W can initiate climbing from idle (S never grabs the ladder).
//   - W only grabs when the player is grounded — prevents the ladder from
//     stealing a jump or mid-air movement.
//   - Releasing both W and S while climbing restores gravity so the player
//     falls naturally instead of floating mid-ladder.
// ─────────────────────────────────────────────────────────────────────────────
inline void LadderSystem(entt::registry& reg, float dt) {
    // SPACE still polled — used for jump-off which is hold-sensitive
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
        constexpr float inset     = 8.0f;
        float           columnTop = 1e9f;
        float           columnBot = -1e9f;
        bool            inColumn  = false;
        bool            touching  = false;

        ladderView.each([&](const Transform& lt, const Collider& lc) {
            bool alignX = (pt.x + inset) < (lt.x + lc.w) && (pt.x + pc.w - inset) > lt.x;
            if (!alignX)
                return;
            inColumn      = true;
            columnTop     = std::min(columnTop, lt.y);
            columnBot     = std::max(columnBot, lt.y + lc.h);
            bool overlapY = pt.y < (lt.y + lc.h) && (pt.y + pc.h) > lt.y;
            if (overlapY)
                touching = true;
        });

        if (!inColumn) {
            columnTop = 0.0f;
            columnBot = 0.0f;
        }

        // Player's feet sit exactly here when at the top of the column
        float topRestY = columnTop - pc.h;

        climb.onLadder = touching;

        // Use event-driven flags, not keyboard poll
        bool wHeld = climb.wPressed;
        bool sHeld = climb.sPressed;

        // ─────────────────────────────────────────────────────────────────────
        // atTop
        // ─────────────────────────────────────────────────────────────────────
        if (climb.atTop) {
            g.active   = false;
            g.velocity = 0.0f;
            v.dy       = 0.0f;
            // Hard-lock position every frame
            if (inColumn)
                pt.y = topRestY;

            if (spaceHeld) {
                climb.atTop  = false;
                g.active     = true;
                g.velocity   = -JUMP_FORCE;
                g.isGrounded = false;
                return;
            }
            if (!inColumn) {
                climb.atTop  = false;
                g.active     = true;
                g.velocity   = 0.0f;
                g.isGrounded = false;
                return;
            }
            if (sHeld) {
                climb.atTop    = false;
                climb.climbing = true;
                g.active       = false;
                g.velocity     = 0.0f;
                pt.y           = columnTop + 1.0f; // nudge into column
            }
            // wHeld or no input → stay frozen
            return;
        }

        // ─────────────────────────────────────────────────────────────────────
        // climbing
        // ─────────────────────────────────────────────────────────────────────
        if (climb.climbing) {
            g.velocity = 0.0f;
            v.dy       = 0.0f;

            if (spaceHeld) {
                climb.climbing = false;
                g.active       = true;
                g.velocity     = -JUMP_FORCE;
                g.isGrounded   = false;
                return;
            }
            if (!inColumn) {
                // Left the column entirely — restore gravity
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
            } else {
                // Neither W nor S held — release the ladder and restore gravity
                // so the player falls naturally instead of floating mid-ladder.
                climb.climbing = false;
                g.active       = true;
                g.velocity     = 0.0f;
                g.isGrounded   = false;
            }
            return;
        }

        // ─────────────────────────────────────────────────────────────────────
        // idle — grab ladder on W press while touching the ladder column.
        // Allowed both grounded AND mid-air so the player can grab during a jump.
        // S is excluded from initiating: pressing S from idle would pull the
        // player into the ground and CollisionSystem would push them back up.
        // ─────────────────────────────────────────────────────────────────────
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
