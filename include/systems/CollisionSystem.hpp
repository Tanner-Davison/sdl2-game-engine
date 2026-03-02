#pragma once
#include <Components.hpp>
#include <GameEvents.hpp>
#include <cmath>
#include <entt/entt.hpp>
#include <vector>

// -----------------------------------------------------------------------------
// CollisionSystem
//
// Pass order:
//   1. Slope pass   -- snap to diagonal surface, sets onSlopeThisFrame
//   2. Flat tile Pass 1  -- gravity axis snap (vertical + ceiling/floor)
//   3. Flat tile Pass 2  -- lateral push-out with step-up
//
// The slope pass MUST run first so onSlopeThisFrame is known before Pass 1.
// Without this, Pass 1's lateral corrections fire against tiles the slope would
// have lifted the player above, causing slope-to-slope seam sticking.
//
// Key design decisions:
//
//   * onSlopeThisFrame suppresses ALL lateral corrections in both Pass 1 and
//     Pass 2.  The slope snap already placed the player at the correct surface
//     height; any lateral push from a tile whose top the slope put them above
//     is wrong and causes the seam-sticking and corner-catching bugs.
//
//   * STEP_UP_HEIGHT == tile height (64 px): in Pass 2, the player can walk
//     from the base of a slope (a full tile below the adjacent flat top) onto
//     that platform without being laterally blocked.
//
//   * Pass 2 step-up uses overlap-axis comparison (oTop <= oLeft && oTop <=
//     oRight) to distinguish floor contacts from wall contacts, preventing
//     the "walking up vertical walls" bug that a sinkDepth-only check caused.
//
//   * The slope proximity guard uses OR (either foot-edge within lookahead)
//     so valley joins, peak joins, and slope<->flat transitions from either
//     direction are all handled uniformly.
// -----------------------------------------------------------------------------
inline CollisionResult CollisionSystem(entt::registry& reg, float dt, int windowW, int windowH) {
    CollisionResult result;

    auto timerView = reg.view<InvincibilityTimer>();
    timerView.each([dt](InvincibilityTimer& inv) {
        if (inv.isInvincible) {
            inv.remaining -= dt;
            if (inv.remaining <= 0.0f) {
                inv.remaining    = 0.0f;
                inv.isInvincible = false;
            }
        }
    });

    auto liveEnemyView = reg.view<EnemyTag, Transform, Collider>(entt::exclude<DeadTag>);
    auto deadEnemyView = reg.view<DeadTag, Transform, Collider>();
    auto coinView      = reg.view<CoinTag, Transform, Collider>();
    auto playerView    = reg.view<PlayerTag, GravityState, Transform, Collider, Health, InvincibilityTimer>();

    std::vector<entt::entity> toKill;
    std::vector<entt::entity> toDestroy;
    toKill.reserve(4);
    toDestroy.reserve(8);

    playerView.each([&](entt::entity playerEnt, GravityState& g, Transform& pt,
                        const Collider& pc, Health& health, InvincibilityTimer& inv) {
        bool  sidewall = g.direction == GravityDir::LEFT || g.direction == GravityDir::RIGHT;
        float pw       = sidewall ? (float)pc.h : (float)pc.w;
        float ph       = sidewall ? (float)pc.w : (float)pc.h;

        auto aabb = [&](const Transform& et, const Collider& ec) -> bool {
            return pt.x < et.x + ec.w && pt.x + pw > et.x &&
                   pt.y < et.y + ec.h && pt.y + ph > et.y;
        };

        auto isStomp = [&](const Transform& et, const Collider& ec) -> bool {
            if (!aabb(et, ec) || g.velocity <= 0.0f) return false;
            switch (g.direction) {
                case GravityDir::DOWN:  return (pt.y + pc.h) <= et.y + ec.h;
                case GravityDir::UP:    return pt.y >= et.y;
                case GravityDir::LEFT:  return pt.x >= et.x;
                case GravityDir::RIGHT: return (pt.x + pc.h) <= et.x + ec.w;
            }
            return false;
        };

        // -- Live enemy collisions ---------------------------------------------
        liveEnemyView.each([&](entt::entity enemy, const Transform& et, const Collider& ec) {
            if (isStomp(et, ec)) {
                toKill.push_back(enemy);
                result.enemiesStomped++;
                g.velocity   = -JUMP_FORCE * 0.5f;
                g.isGrounded = false;
            } else if (!inv.isInvincible && aabb(et, ec)) {
                health.current -= PLAYER_HIT_DAMAGE;
                if (health.current <= 0.0f) {
                    health.current    = 0.0f;
                    result.playerDied = true;
                }
                inv.isInvincible  = true;
                inv.remaining     = inv.duration;
                g.active          = false;
                g.velocity        = 0.0f;
                g.isGrounded      = false;
                g.punishmentTimer = GRAVITY_DURATION;
            }
        });

        // -- Dead enemy platforms ---------------------------------------------
        bool onDeadEnemy = false;
        deadEnemyView.each([&](const Transform& et, const Collider& ec) {
            if (g.velocity < 0.0f) return;
            switch (g.direction) {
                case GravityDir::DOWN: {
                    float bottom = pt.y + pc.h;
                    if (pt.x < et.x + ec.w && pt.x + pc.w > et.x &&
                        bottom >= et.y && bottom <= et.y + ec.h) {
                        pt.y         = et.y - pc.h;
                        g.velocity   = 0.0f;
                        g.isGrounded = onDeadEnemy = true;
                    }
                    break;
                }
                case GravityDir::UP: {
                    if (pt.x < et.x + ec.w && pt.x + pc.w > et.x &&
                        pt.y <= et.y + ec.h && pt.y >= et.y) {
                        pt.y         = et.y + ec.h;
                        g.velocity   = 0.0f;
                        g.isGrounded = onDeadEnemy = true;
                    }
                    break;
                }
                case GravityDir::LEFT: {
                    if (pt.y < et.y + ec.h && pt.y + pc.h > et.y &&
                        pt.x <= et.x + ec.w && pt.x >= et.x) {
                        pt.x         = et.x + ec.w;
                        g.velocity   = 0.0f;
                        g.isGrounded = onDeadEnemy = true;
                    }
                    break;
                }
                case GravityDir::RIGHT: {
                    float right = pt.x + pc.h;
                    if (pt.y < et.y + ec.h && pt.y + pc.w > et.y &&
                        right >= et.x && right <= et.x + ec.w) {
                        pt.x         = et.x - pc.h;
                        g.velocity   = 0.0f;
                        g.isGrounded = onDeadEnemy = true;
                    }
                    break;
                }
            }
        });

        if (!onDeadEnemy && g.isGrounded) {
            bool onWindow = false;
            switch (g.direction) {
                case GravityDir::DOWN:  onWindow = pt.y + pc.h >= windowH; break;
                case GravityDir::UP:    onWindow = pt.y <= 0.0f;            break;
                case GravityDir::LEFT:  onWindow = pt.x <= 0.0f;            break;
                case GravityDir::RIGHT: onWindow = pt.x + pc.h >= windowW;  break;
            }
            if (!onWindow) g.isGrounded = false;
        }

        // -- Slope pass (FIRST) -----------------------------------------------
        // Determines onSlopeThisFrame before any flat-tile passes so that
        // Pass 1 and Pass 2 can suppress lateral corrections while on a slope.
        bool onSlopeThisFrame = false;

        if (g.direction == GravityDir::DOWN) {
            float pFeet    = pt.y + pc.h;
            float bestSnap = pFeet + SLOPE_SNAP_LOOKAHEAD;
            bool  onSlope  = false;

            auto slopeView = reg.view<SlopeCollider, TileTag, Transform, Collider>();
            slopeView.each([&](const SlopeCollider& sc, const Transform& tt, const Collider& tc) {
                float pLeft  = pt.x;
                float pRight = pt.x + pc.w;

                if (pRight <= tt.x || pLeft >= tt.x + tc.w) return;
                if (pFeet  <  tt.y - SLOPE_SNAP_LOOKAHEAD)  return;
                if (pt.y   >  tt.y + tc.h)                   return;

                auto surfaceAtX = [&](float wx) -> float {
                    float t = (wx - tt.x) / (float)tc.w;
                    t = std::max(0.0f, std::min(1.0f, t));
                    return (sc.slopeType == SlopeType::DiagUpRight)
                        ? (tt.y + tc.h) - t * tc.h  // high-left -> low-right
                        : tt.y + t * tc.h;           // low-left  -> high-right
                };

                float snapSurface = surfaceAtX(pt.x + pc.w * 0.5f);

                // Proximity guard: either foot-edge must be within lookahead of
                // the surface at that edge.  OR catches all approach directions
                // including valley/peak slope joins.
                float sL          = surfaceAtX(pLeft  + 1.0f);
                float sR          = surfaceAtX(pRight - 1.0f);
                bool  closeEnough = (pFeet >= sL - SLOPE_SNAP_LOOKAHEAD) ||
                                    (pFeet >= sR - SLOPE_SNAP_LOOKAHEAD);

                // Pick the highest (smallest Y) candidate -- correct for both
                // ascending and the converging surface at slope joins.
                if (closeEnough && snapSurface < bestSnap) {
                    bestSnap = snapSurface;
                    onSlope  = true;
                }
            });

            if (onSlope) {
                pt.y             = bestSnap - pc.h;
                g.velocity       = 0.0f;
                g.isGrounded     = true;
                onSlopeThisFrame = true;
            }
        }

        // -- Flat tile Pass 1: gravity-axis snap -------------------------------
        // Handles floor, ceiling, and (for LEFT/RIGHT gravity) wall grounding.
        // When onSlopeThisFrame is true, lateral corrections (the else-if
        // branches) are skipped entirely -- the slope already positioned the
        // player and any lateral push here would fight it and cause sticking.
        auto tileView = reg.view<TileTag, Transform, Collider>(entt::exclude<SlopeCollider, ActionTag>);

        tileView.each([&](const Transform& tt, const Collider& tc) {
            if (pt.x + pw <= tt.x || pt.x >= tt.x + tc.w) return;
            if (pt.y + ph <= tt.y || pt.y >= tt.y + tc.h) return;

            float oTop    = (pt.y + ph) - tt.y;
            float oBottom = (tt.y + tc.h) - pt.y;
            float oLeft   = (pt.x + pw)  - tt.x;
            float oRight  = (tt.x + tc.w) - pt.x;

            switch (g.direction) {
                case GravityDir::DOWN:
                    if (oTop < oBottom && oTop <= oLeft && oTop <= oRight) {
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.y       = tt.y - ph;
                        g.velocity = 0.0f;
                    } else if (!onSlopeThisFrame
                               && oBottom < oTop && oBottom <= oLeft && oBottom <= oRight) {
                        // ceiling hit -- only when not on slope
                        pt.y       = tt.y + tc.h;
                        g.velocity = 0.0f;
                    }
                    break;
                case GravityDir::UP:
                    if (oBottom < oTop && oBottom <= oLeft && oBottom <= oRight) {
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.y       = tt.y + tc.h;
                        g.velocity = 0.0f;
                    } else if (!onSlopeThisFrame
                               && oTop < oBottom && oTop <= oLeft && oTop <= oRight) {
                        pt.y       = tt.y - ph;
                        g.velocity = 0.0f;
                    }
                    break;
                case GravityDir::LEFT:
                    if (oRight < oLeft && oRight <= oTop && oRight <= oBottom) {
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.x       = tt.x + tc.w;
                        g.velocity = 0.0f;
                    } else if (!onSlopeThisFrame
                               && oLeft < oRight && oLeft <= oTop && oLeft <= oBottom) {
                        pt.x       = tt.x - pw;
                        g.velocity = 0.0f;
                    }
                    break;
                case GravityDir::RIGHT:
                    if (oLeft < oRight && oLeft <= oTop && oLeft <= oBottom) {
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.x       = tt.x - pw;
                        g.velocity = 0.0f;
                    } else if (!onSlopeThisFrame
                               && oRight < oLeft && oRight <= oTop && oRight <= oBottom) {
                        pt.x       = tt.x + tc.w;
                        g.velocity = 0.0f;
                    }
                    break;
            }
        });

        // -- Flat tile Pass 2: lateral push-out with step-up ------------------
        // When onSlopeThisFrame: only ceiling correction, no lateral push.
        // When not on slope: overlap-axis comparison decides step-up vs wall.
        //   oTop <= oLeft && oTop <= oRight  -> contact is from above -> step-up
        //   otherwise                         -> lateral wall -> push out
        // Step-up also requires oTop in [0, STEP_UP_HEIGHT] to prevent
        // stepping up full walls.

        tileView.each([&](const Transform& tt, const Collider& tc) {
            if (pt.x + pw <= tt.x || pt.x >= tt.x + tc.w) return;
            if (pt.y + ph <= tt.y || pt.y >= tt.y + tc.h) return;

            switch (g.direction) {
                case GravityDir::DOWN:
                case GravityDir::UP: {
                    float oTop    = (pt.y + ph) - tt.y;
                    float oBottom = (tt.y + tc.h) - pt.y;
                    float oLeft   = (pt.x + pw)  - tt.x;
                    float oRight  = (tt.x + tc.w) - pt.x;

                    if (onSlopeThisFrame) {
                        // On slope: ceiling only, never lateral.
                        if (g.direction == GravityDir::DOWN
                            && oBottom < oTop && oBottom <= oLeft && oBottom <= oRight) {
                            pt.y       = tt.y + tc.h;
                            g.velocity = 0.0f;
                        }
                        break;
                    }

                    // Floor contact from above: step up.
                    if (g.direction == GravityDir::DOWN
                        && oTop >= 0.0f
                        && oTop <= STEP_UP_HEIGHT
                        && oTop <= oLeft && oTop <= oRight) {
                        pt.y         = tt.y - ph;
                        g.velocity   = 0.0f;
                        g.isGrounded = true;
                        break;
                    }
                    // Ceiling contact from below (UP gravity).
                    if (g.direction == GravityDir::UP
                        && oBottom > 0.0f
                        && oBottom <= oLeft && oBottom <= oRight) {
                        pt.y       = tt.y + tc.h;
                        g.velocity = 0.0f;
                        break;
                    }
                    // Lateral wall: push out on shallower horizontal axis.
                    pt.x = oLeft < oRight ? tt.x - pw : tt.x + tc.w;
                    break;
                }
                case GravityDir::LEFT:
                case GravityDir::RIGHT: {
                    float oTop    = (pt.y + ph) - tt.y;
                    float oBottom = (tt.y + tc.h) - pt.y;
                    pt.y = oTop < oBottom ? tt.y - ph : tt.y + tc.h;
                    break;
                }
            }
        });

        // -- Action tile trigger ----------------------------------------------
        {
            auto actionView = reg.view<ActionTag, Transform, Collider>();
            actionView.each([&](entt::entity at, const ActionTag& /*tag*/,
                                const Transform& tt, const Collider& tc) {
                if (aabb(tt, tc))
                    result.actionTilesTriggered.push_back(at);
            });
        }

        // -- Coin collection --------------------------------------------------
        if (g.active) {
            coinView.each([&](entt::entity coin, const Transform& ct, const Collider& cc) {
                if (aabb(ct, cc)) {
                    toDestroy.push_back(coin);
                    result.coinsCollected++;
                }
            });
        }
    });

    // -- Commit deferred mutations --------------------------------------------
    for (auto e : toKill) {
        if (reg.all_of<Velocity>(e)) {
            auto& v = reg.get<Velocity>(e);
            v.dx = v.dy = 0.0f;
        }
        if (reg.all_of<Renderable, AnimationState>(e)) {
            auto& r    = reg.get<Renderable>(e);
            auto& anim = reg.get<AnimationState>(e);
            r.frames          = {{0, 112, 59, 12}};
            anim.currentFrame = 0;
            anim.totalFrames  = 1;
            anim.looping      = false;
        }
        if (reg.all_of<Collider>(e)) {
            auto& col = reg.get<Collider>(e);
            col.w = 59;
            col.h = 12;
        }
        reg.emplace<DeadTag>(e);
    }

    for (auto e : toDestroy)
        reg.destroy(e);

    // -- Strip triggered action tiles (and their groups) ----------------------
    auto stripTile = [&](entt::entity e) {
        if (!reg.valid(e)) return;
        if (reg.all_of<Renderable>(e)) reg.remove<Renderable>(e);
        if (reg.all_of<TileTag>(e))    reg.remove<TileTag>(e);
        if (reg.all_of<Collider>(e))   reg.remove<Collider>(e);
    };

    {
        auto& v = result.actionTilesTriggered;
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }

    for (auto e : result.actionTilesTriggered) {
        if (!reg.valid(e)) continue;
        if (!reg.all_of<ActionTag>(e)) continue;
        int grp = reg.get<ActionTag>(e).group;
        stripTile(e);
        if (grp != 0) {
            std::vector<entt::entity> groupMembers;
            auto groupView = reg.view<ActionTag>();
            groupView.each([&](entt::entity other, const ActionTag& at) {
                if (other != e && at.group == grp)
                    groupMembers.push_back(other);
            });
            for (auto gm : groupMembers)
                stripTile(gm);
        }
    }

    return result;
}
