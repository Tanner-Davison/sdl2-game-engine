#pragma once
#include <Components.hpp>
#include <GameEvents.hpp>
#include <cmath>
#include <entt/entt.hpp>
#include <unordered_map>
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
//   * onSlopeThisFrame suppresses ALL floor, ceiling, and lateral corrections
//     in both Pass 1 and Pass 2.  The slope snap already placed the player at
//     the correct surface height; any floor snap from an underlying fill tile
//     (or lateral push from a tile the slope lifted the player above) would
//     overwrite the slope result and re-introduce stair-stepping.
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

        // ── Open-world mode: simple 4-sided AABB push-out, no gravity ──────────
        // Push out on the smallest overlap axis from all 4 sides equally.
        // This runs instead of all the gravity-axis passes below.
        if (reg.all_of<OpenWorldTag>(playerEnt)) {
            auto owTileView = reg.view<TileTag, Transform, Collider>(entt::exclude<SlopeCollider, ActionTag>);
            owTileView.each([&](const Transform& tt, const Collider& tc) {
                if (pt.x + pw <= tt.x || pt.x >= tt.x + tc.w) return;
                if (pt.y + ph <= tt.y || pt.y >= tt.y + tc.h) return;

                float oTop    = (pt.y + ph) - tt.y;
                float oBottom = (tt.y + tc.h) - pt.y;
                float oLeft   = (pt.x + pw)  - tt.x;
                float oRight  = (tt.x + tc.w) - pt.x;

                // Push out on the axis with the smallest penetration
                float minH = oLeft < oRight ? oLeft : oRight;
                float minV = oTop  < oBottom ? oTop  : oBottom;
                if (minV <= minH) {
                    // Vertical push
                    if (oTop < oBottom)  pt.y = tt.y - ph;
                    else                 pt.y = tt.y + tc.h;
                } else {
                    // Horizontal push
                    if (oLeft < oRight)  pt.x = tt.x - pw;
                    else                 pt.x = tt.x + tc.w;
                }
            });

            // Coin collection
            coinView.each([&](entt::entity coin, const Transform& ct, const Collider& cc) {
                if (aabb(ct, cc)) {
                    toDestroy.push_back(coin);
                    result.coinsCollected++;
                }
            });
            return; // skip all gravity-based collision logic
        }

        // -- Slope pass (FIRST) -----------------------------------------------
        // Surface formula: anchored at each tile's own top-left (DiagUpLeft) or
        // top-right (DiagUpRight) corner, using world-space X.
        //
        // This produces a CONTINUOUS value at tile seams because adjacent tiles
        // in the standard staircase layout each start where the previous ended:
        //
        //   DiagUpLeft  (high-left, descends right):
        //     surfaceY = tt.y + (playerX - tt.x) * (tc.h / tc.w)
        //     Seam proof with 48x48 tiles at x=912,y=768 and x=960,y=816:
        //       Tile1 at wx=960: 768 + (960-912)*1 = 816
        //       Tile2 at wx=960: 816 + (960-960)*1 = 816  ✓
        //
        //   DiagUpRight (high-right, descends left):
        //     surfaceY = tt.y + (tt.x + tc.w - playerX) * (tc.h / tc.w)
        bool onSlopeThisFrame = false;

        if (g.direction == GravityDir::DOWN) {
            float pFeet      = pt.y + pc.h;
            float pLeft      = pt.x;
            float pRight     = pt.x + pc.w;
            float bestSurface = pFeet + SLOPE_SNAP_LOOKAHEAD; // sentinel (far below)
            bool  onSlope    = false;

            auto slopeView = reg.view<SlopeCollider, TileTag, Transform, Collider>();
            slopeView.each([&](const SlopeCollider& sc, const Transform& tt, const Collider& tc) {
                if (pRight <= tt.x || pLeft >= tt.x + tc.w) return;

                // heightFrac controls how much of the tile height the slope
                // actually spans.  1.0 = fully diagonal; 0.5 = gentle half-slope.
                // The HIGH corner is always anchored at tt.y (tile top).
                // The low corner sits at tt.y + (tc.h * heightFrac).
                float riseH = tc.h * sc.heightFrac;  // pixels of vertical drop from high to low
                float ratio = riseH / (float)tc.w;   // rise-over-run
                float highY = (float)tt.y;            // high corner always at tile top

                // Use player centre X for the surface formula — this keeps
                // seam continuity across tiles while sitting at a visually
                // correct height (neither floating nor sunken).
                float playerX = pLeft + pc.w * 0.5f;

                float surface;
                if (sc.slopeType == SlopeType::DiagUpLeft) {
                    // High on right, descends left-to-right
                    // At playerX=tt.x (left edge): surface = highY + riseH (low)
                    // At playerX=tt.x+tc.w (right edge): surface = highY (high)
                    surface = highY + (tt.x + tc.w - playerX) * ratio;
                } else {
                    // DiagUpRight: high on left, descends right
                    // At playerX=tt.x: surface = highY (high)
                    // At playerX=tt.x+tc.w: surface = highY + riseH (low)
                    surface = highY + (playerX - tt.x) * ratio;
                }

                // Reject extrapolation outside the tile's vertical bounds
                if (surface < tt.y || surface > tt.y + tc.h) return;

                if (pFeet < surface - SLOPE_SNAP_LOOKAHEAD) return;
                if (pt.y  > tt.y + tc.h + SLOPE_SNAP_LOOKAHEAD) return;
                // Reject slopes that are ABOVE the player (walking underneath a
                // sloped roof/overhang).  The surface is only reachable from
                // below if the surface Y is at or below the player's feet — if
                // it is above pt.y (player's head) the slope is an overhead
                // surface and should not snap the player upward.
                if (surface < pt.y) return;

                if (surface < bestSurface) {
                    bestSurface = surface;
                    onSlope     = true;
                }
            });

            if (onSlope) {
                // Only snap & ground the player when they are falling onto (or
                // resting on) the slope.  If velocity is meaningfully upward
                // (player just jumped or is still rising), leave them airborne so
                // the jump isn't immediately cancelled by the surface snap.
                //
                // SLOPE_JUMP_THRESHOLD gives a small tolerance: gentle upward
                // corrections from SLOPE_STICK_VELOCITY (walking uphill) are
                // still treated as grounded, but a real jump (-JUMP_FORCE) is not.
                constexpr float SLOPE_JUMP_THRESHOLD = -80.0f; // px/s — below this = real jump
                bool risingFast = (g.velocity < SLOPE_JUMP_THRESHOLD);

                if (!risingFast) {
                    // Descending, stationary, or barely rising (walk-uphill correction):
                    // snap to slope and mark grounded.
                    pt.y         = bestSurface - pc.h;
                    g.velocity   = 0.0f;
                    g.isGrounded = true;
                }
                // Always set onSlopeThisFrame so Pass 1/2 flat-tile corrections
                // are suppressed — we don’t want flat tiles fighting the slope
                // geometry even while the player is jumping off it.
                onSlopeThisFrame = true;
            }
        }

        // -- Flat tile Pass 1: gravity-axis snap -------------------------------
        // Handles floor, ceiling, and (for LEFT/RIGHT gravity) wall grounding.
        // When onSlopeThisFrame is true, lateral corrections (the else-if
        // branches) are skipped entirely -- the slope already positioned the
        // player and any lateral push here would fight it and cause sticking.
        auto tileView = reg.view<TileTag, Transform, Collider>(entt::exclude<SlopeCollider, ActionTag>);

        tileView.each([&](entt::entity te, const Transform& tt, const Collider& tc) {
            // Apply ColliderOffset if present (custom hitbox position)
            float tax = tt.x, tay = tt.y;
            if (const auto* co = reg.try_get<ColliderOffset>(te)) {
                tax += co->x;
                tay += co->y;
            }

            if (pt.x + pw <= tax || pt.x >= tax + tc.w) return;
            if (pt.y + ph <= tay || pt.y >= tay + tc.h) return;

            float oTop    = (pt.y + ph) - tay;
            float oBottom = (tay + tc.h) - pt.y;
            float oLeft   = (pt.x + pw)  - tax;
            float oRight  = (tax + tc.w) - pt.x;

            switch (g.direction) {
                case GravityDir::DOWN:
                    if (!onSlopeThisFrame
                        && oTop < oBottom && oTop <= oLeft && oTop <= oRight) {
                        // Floor snap: suppressed while on slope so the slope pass
                        // result isn't overwritten by underlying fill tiles.
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.y       = tay - ph;   // snap to hitbox top, not visual top
                        g.velocity = 0.0f;
                    } else if (!onSlopeThisFrame
                               && oBottom < oTop && oBottom <= oLeft && oBottom <= oRight) {
                        pt.y       = tay + tc.h; // snap to hitbox bottom
                        g.velocity = 0.0f;
                    }
                    break;
                case GravityDir::UP:
                    if (oBottom < oTop && oBottom <= oLeft && oBottom <= oRight) {
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.y       = tay + tc.h;
                        g.velocity = 0.0f;
                    } else if (!onSlopeThisFrame
                               && oTop < oBottom && oTop <= oLeft && oTop <= oRight) {
                        pt.y       = tay - ph;
                        g.velocity = 0.0f;
                    }
                    break;
                case GravityDir::LEFT:
                    if (oRight < oLeft && oRight <= oTop && oRight <= oBottom) {
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.x       = tax + tc.w;
                        g.velocity = 0.0f;
                    } else if (!onSlopeThisFrame
                               && oLeft < oRight && oLeft <= oTop && oLeft <= oBottom) {
                        pt.x       = tax - pw;
                        g.velocity = 0.0f;
                    }
                    break;
                case GravityDir::RIGHT:
                    if (oLeft < oRight && oLeft <= oTop && oLeft <= oBottom) {
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.x       = tax - pw;
                        g.velocity = 0.0f;
                    } else if (!onSlopeThisFrame
                               && oRight < oLeft && oRight <= oTop && oRight <= oBottom) {
                        pt.x       = tax + tc.w;
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

        tileView.each([&](entt::entity te, const Transform& tt, const Collider& tc) {
            float tax = tt.x, tay = tt.y;
            if (const auto* co = reg.try_get<ColliderOffset>(te)) {
                tax += co->x;
                tay += co->y;
            }
            if (pt.x + pw <= tax || pt.x >= tax + tc.w) return;
            if (pt.y + ph <= tay || pt.y >= tay + tc.h) return;

            // Moving platforms own their own lateral carry — skip lateral
            // push-out for them in Pass 2 to avoid fighting MovingPlatformSystem.
            const bool isMovingPlat = reg.all_of<MovingPlatformTag>(te);

            switch (g.direction) {
                case GravityDir::DOWN:
                case GravityDir::UP: {
                    float oTop    = (pt.y + ph) - tay;
                    float oBottom = (tay + tc.h) - pt.y;
                    float oLeft   = (pt.x + pw)  - tax;
                    float oRight  = (tax + tc.w) - pt.x;

                    if (onSlopeThisFrame) {
                        if (g.direction == GravityDir::DOWN
                            && oBottom < oTop && oBottom <= oLeft && oBottom <= oRight) {
                            pt.y       = tay + tc.h;
                            g.velocity = 0.0f;
                        }
                        break;
                    }

                    // Floor contact from above: step up.
                    if (g.direction == GravityDir::DOWN
                        && oTop >= 0.0f
                        && oTop <= STEP_UP_HEIGHT
                        && oTop <= oLeft && oTop <= oRight) {
                        pt.y         = tay - ph;
                        g.velocity   = 0.0f;
                        g.isGrounded = true;
                        break;
                    }
                    // Ceiling contact from below (UP gravity).
                    if (g.direction == GravityDir::UP
                        && oBottom > 0.0f
                        && oBottom <= oLeft && oBottom <= oRight) {
                        pt.y       = tay + tc.h;
                        g.velocity = 0.0f;
                        break;
                    }
                    // Lateral wall: skip ejection for moving platforms ONLY when
                    // contact is from above (player standing on top) — ejecting
                    // would fight MovingPlatformCarry. For side contacts, always
                    // eject so the player can't walk through platform edges.
                    bool isTopContact = (oTop < oLeft && oTop < oRight);
                    if (!isMovingPlat || !isTopContact)
                        pt.x = oLeft < oRight ? tax - pw : tax + tc.w;
                    break;
                }
                case GravityDir::LEFT:
                case GravityDir::RIGHT: {
                    float oTop    = (pt.y + ph) - tay;
                    float oBottom = (tay + tc.h) - pt.y;
                    pt.y = oTop < oBottom ? tay - ph : tay + tc.h;
                    break;
                }
            }
        });

        // -- Hazard overlap: handled in the dedicated pass below playerView ----
        // (Removed the duplicate early check that ignored ColliderOffset.)

        // -- Sword slash-hit detection --------------------------------------
        // Builds a directional sword hitbox and tests it against:
        //   - ActionTag tiles  (unified slash-trigger; strips Renderable+TileTag+Collider)
        //   - Live enemies     (applies SLASH_DAMAGE; kills when HP reaches 0)
        //
        // Facing is determined by:
        //   - Horizontal gravity (DOWN/UP): Renderable::flipH  false=right, true=left
        //   - Left-wall gravity:            player faces UP the wall
        //   - Right-wall gravity:           player faces UP the wall (opposite side)
        if (auto* atk = reg.try_get<AttackState>(playerEnt); atk && atk->isAttacking) {
            // SWORD_REACH and SWORD_HEIGHT are defined in GameConfig.hpp

            // Determine facing direction as a unit vector (fx, fy)
            const auto* rend = reg.try_get<Renderable>(playerEnt);
            float fx = 0.0f, fy = 0.0f;
            switch (g.direction) {
                case GravityDir::DOWN:
                case GravityDir::UP:
                    // flipH=false → facing right (+x), flipH=true → facing left (-x)
                    fx = (rend && rend->flipH) ? -1.0f : 1.0f;
                    fy = 0.0f;
                    break;
                case GravityDir::LEFT:  fx =  0.0f; fy = -1.0f; break; // faces up-left wall
                case GravityDir::RIGHT: fx =  0.0f; fy = -1.0f; break; // faces up-right wall
            }

            // Build sword rect: starts at the leading edge of the player collider
            // and extends SWORD_REACH px forward. Perpendicular coverage matches
            // the player's height with a small inset so short enemies still register.
            float swordX, swordY, swordW, swordH;
            if (fx != 0.0f) {
                // Horizontal swing
                float insetY = pc.h * (1.0f - SWORD_HEIGHT) * 0.5f;
                swordW = SWORD_REACH;
                swordH = pc.h * SWORD_HEIGHT;
                swordY = pt.y + insetY;
                swordX = (fx > 0.0f) ? pt.x + pc.w          // right-facing: start at right edge
                                     : pt.x - SWORD_REACH;   // left-facing:  extend left
            } else {
                // Vertical swing (wall gravity)
                float insetX = pc.w * (1.0f - SWORD_HEIGHT) * 0.5f;
                swordW = pc.w * SWORD_HEIGHT;
                swordH = SWORD_REACH;
                swordX = pt.x + insetX;
                swordY = (fy > 0.0f) ? pt.y + pc.h          // downward: start at bottom
                                     : pt.y - SWORD_REACH;   // upward:   extend up
            }

            // Sword AABB helper
            auto swordHits = [&](float tx, float ty, float tw, float th) -> bool {
                return swordX          < tx + tw &&
                       swordX + swordW > tx      &&
                       swordY          < ty + th  &&
                       swordY + swordH > ty;
            };

            // Test every ActionTag tile against the sword rect.
            // hitEntities guards against multi-frame decrements — each swing
            // only registers one hit per tile regardless of overlap duration.
            {
                auto actionView = reg.view<ActionTag, Transform, Collider>();
                actionView.each([&](entt::entity at, ActionTag& tag,
                                    const Transform& tt, const Collider& tc) {
                    if (!swordHits(tt.x, tt.y, (float)tc.w, (float)tc.h)) return;
                    // Only gate multi-hit tiles — single-hit tiles destroy immediately
                    if (tag.hitsRequired > 1) {
                        if (atk->hitEntities.count(at)) return; // already hit this swing
                        atk->hitEntities.insert(at);
                    }
                    tag.hitsRemaining--;
                    if (tag.hitsRemaining <= 0) {
                        result.actionTilesTriggered.push_back(at);
                    } else {
                        // Flash red to show the hit registered
                        if (reg.all_of<HitFlash>(at))
                            reg.get<HitFlash>(at).timer = HitFlash{}.duration; // reset
                        else
                            reg.emplace<HitFlash>(at);
                    }
                });
            }

            // Test every live enemy against the sword rect
            liveEnemyView.each([&](entt::entity enemy,
                                   const Transform& et, const Collider& ec) {
                if (!swordHits(et.x, et.y, (float)ec.w, (float)ec.h)) return;
                if (atk->hitEntities.count(enemy)) return; // already hit this swing
                atk->hitEntities.insert(enemy);
                // Apply slash damage to the enemy's health
                auto* eh = reg.try_get<Health>(enemy);
                if (!eh) {
                    // No health component — one-shot kill (legacy behaviour)
                    toKill.push_back(enemy);
                    result.enemiesSlashed++;
                    return;
                }
                eh->current -= SLASH_DAMAGE;
                if (eh->current <= 0.0f) {
                    eh->current = 0.0f;
                    toKill.push_back(enemy);
                    result.enemiesSlashed++;
                }
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
    // Deduplicate toKill — an entity can be pushed by both stomp and slash
    // in the same frame; emplace<DeadTag> on an already-tagged entity would assert.
    {
        std::sort(toKill.begin(), toKill.end());
        toKill.erase(std::unique(toKill.begin(), toKill.end()), toKill.end());
    }
    for (auto e : toKill) {
        if (reg.all_of<Velocity>(e)) {
            auto& v = reg.get<Velocity>(e);
            v.dx = v.dy = 0.0f;
        }
        // Transition to dead visual state using the named "slimeDead" frame
        // from the enemy's current spritesheet instead of hard-coded pixel coords.
        // Falls back to the legacy rect if the frame lookup fails (e.g. custom
        // enemy spritesheets that don't have a slimeDead entry).
        if (reg.all_of<Renderable, AnimationState>(e)) {
            auto& r    = reg.get<Renderable>(e);
            auto& anim = reg.get<AnimationState>(e);
            // Try to find a "slimeDead" or "Dead" rect at offset (0,112) 59x12.
            // We still use the same source coords since the enemy spritesheet
            // layout hasn't changed, but now it's a named constant rather than
            // a magic literal so it's easy to update if the sheet changes.
            static constexpr SDL_Rect SLIME_DEAD_RECT = {0, 112, 59, 12};
            r.frames          = {SLIME_DEAD_RECT};
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

    // -- Collect group members for triggered action tiles ----------------------
    // CollisionSystem no longer strips tiles directly. It expands groups here
    // so GameScene::Update() receives the full set (primary + all group members)
    // and can decide per-entity whether to run a destroy animation or strip cold.
    {
        auto& v = result.actionTilesTriggered;
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }

    {
        // Expand groups: for every triggered tile with a non-zero group, add all
        // other ActionTag entities in that group to actionTilesTriggered.
        //
        // Old approach iterated all ActionTag entities once per triggered tile
        // (O(N * total_action_tiles)). New approach builds a group->entities map
        // once, then looks up each triggered group in O(1). Total: O(total_action_tiles).
        std::unordered_map<int, std::vector<entt::entity>> groupMap;
        {
            auto allActions = reg.view<ActionTag>();
            allActions.each([&](entt::entity e, const ActionTag& at) {
                if (at.group != 0) groupMap[at.group].push_back(e);
            });
        }

        std::vector<entt::entity> extras;
        for (auto e : result.actionTilesTriggered) {
            if (!reg.valid(e) || !reg.all_of<ActionTag>(e)) continue;
            int grp = reg.get<ActionTag>(e).group;
            if (grp == 0) continue;
            auto it = groupMap.find(grp);
            if (it == groupMap.end()) continue;
            for (auto other : it->second)
                if (other != e) extras.push_back(other);
        }
        for (auto ex : extras)
            result.actionTilesTriggered.push_back(ex);
        // Re-deduplicate after expansion
        std::sort(result.actionTilesTriggered.begin(), result.actionTilesTriggered.end());
        result.actionTilesTriggered.erase(
            std::unique(result.actionTilesTriggered.begin(), result.actionTilesTriggered.end()),
            result.actionTilesTriggered.end());
    }

    // -- Hazard tile overlap -----------------------------------------------
    // Hazard tiles have no TileTag so the player is never pushed out of them.
    // We use a small TOUCH expansion so standing flush on the surface of a
    // hazard tile (e.g. spikes on the ground) also registers.
    // ColliderOffset is respected so custom hitboxes set in the editor apply.
    {
        // TOUCH: 1px so standing flush on a hazard surface registers,
        // but the player must actually overlap/touch the hitbox — not just be near it.
        constexpr float TOUCH = 1.0f;
        auto hazardView = reg.view<HazardTag, Transform, Collider>();
        auto pView      = reg.view<PlayerTag, Transform, Collider>();
        pView.each([&](const Transform& pt, const Collider& pc) {
            if (result.onHazard) return;
            hazardView.each([&](entt::entity he, const Transform& ht, const Collider& hc) {
                if (result.onHazard) return;
                // Apply ColliderOffset if present (custom hitbox position from editor)
                float hx = ht.x;
                float hy = ht.y;
                if (const auto* off = reg.try_get<ColliderOffset>(he)) {
                    hx += off->x;
                    hy += off->y;
                }
                if (pt.x        < hx + hc.w + TOUCH &&
                    pt.x + pc.w > hx          - TOUCH &&
                    pt.y        < hy + hc.h + TOUCH &&
                    pt.y + pc.h > hy          - TOUCH)
                    result.onHazard = true;
            });
        });
    }

    return result;
}
