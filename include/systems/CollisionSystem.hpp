#pragma once
#include <Components.hpp>
#include <GameEvents.hpp>
#include <cmath>
#include <entt/entt.hpp>
#include <unordered_map>
#include <vector>

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
    auto goalView      = reg.view<GoalTag, Transform, Collider>();
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

        // --- Live enemy collisions ---
        bool dashing = false;
        if (auto* ds = reg.try_get<DashState>(playerEnt))
            dashing = ds->active;

        // --- Shield vs enemy knockback ---
        if (auto* as = reg.try_get<ActiveShield>(playerEnt)) {
            constexpr float PI2 = 2.f * 3.14159265f;
            constexpr float SHIELD_KB = 64.f;
            float pcx = pt.x + pw * 0.5f;
            float pcy = pt.y + ph * 0.5f;
            int n = (int)as->shields.size();

            liveEnemyView.each([&](entt::entity enemy, const Transform& et, const Collider& ec) {
                SDL_FRect enemyR = {et.x, et.y, (float)ec.w, (float)ec.h};
                for (int i = 0; i < n; ++i) {
                    float a = as->angle + (float)i / n * PI2;
                    const auto& se = as->shields[i];
                    float sx = pcx + std::cos(a) * as->orbitRadius - se.renderW * 0.5f;
                    float sy = pcy + std::sin(a) * as->orbitRadius - se.renderH * 0.5f;
                    SDL_FRect shieldR = {sx, sy, (float)se.renderW, (float)se.renderH};
                    if (SDL_HasRectIntersectionFloat(&shieldR, &enemyR)) {
                        float sCX = sx + se.renderW * 0.5f;
                        float sCY = sy + se.renderH * 0.5f;
                        float eCX = et.x + ec.w * 0.5f;
                        float eCY = et.y + ec.h * 0.5f;
                        float dx = eCX - sCX;
                        float dy = eCY - sCY;
                        float len = std::sqrt(dx * dx + dy * dy);
                        if (len < 0.01f) { dx = 1.f; dy = 0.f; len = 1.f; }
                        auto& eMut = reg.get<Transform>(enemy);
                        eMut.x += dx / len * SHIELD_KB;
                        eMut.y += dy / len * SHIELD_KB;
                        result.shieldBounce = true;
                        break;
                    }
                }
            });
        }

        liveEnemyView.each([&](entt::entity enemy, const Transform& et, const Collider& ec) {
            if (dashing) return;

            if (isStomp(et, ec)) {
                auto* eh = reg.try_get<Health>(enemy);
                if (!eh) {
                    toKill.push_back(enemy);
                    result.lowestHitHpFrac = 0.0f;
                } else {
                    float dmg = eh->max * STOMP_DAMAGE_FRAC;
                    eh->current -= dmg;
                    {
                        float frac = std::max(eh->current, 0.0f) / eh->max;
                        if (frac < result.lowestHitHpFrac)
                            result.lowestHitHpFrac = frac;
                    }

                    constexpr float STOMP_STUN = 0.55f;
                    if (!reg.all_of<HitFlash>(enemy))
                        reg.emplace<HitFlash>(enemy, STOMP_STUN, STOMP_STUN);
                    else
                        reg.get<HitFlash>(enemy).timer = STOMP_STUN;

                    if (auto* react = reg.try_get<EnemyReaction>(enemy))
                        react->turnCooldown = STOMP_STUN + 0.25f;

                    if (auto* ead = reg.try_get<EnemyAnimData>(enemy);
                        ead && ead->hurtSheet && !ead->hurtFrames.empty()) {
                        auto& r    = reg.get<Renderable>(enemy);
                        auto& anim = reg.get<AnimationState>(enemy);
                        r.sheet         = ead->hurtSheet;
                        r.frames        = ead->hurtFrames;
                        r.renderW       = ead->spriteW;
                        r.renderH       = ead->spriteH;
                        anim.currentFrame = 0;
                        anim.totalFrames  = (int)ead->hurtFrames.size();
                        anim.fps          = ead->hurtFps;
                        anim.looping      = false;
                        ead->ApplyHitbox(ead->hurtHitbox, reg, enemy);
                    }

                    if (eh->current <= 0.0f) {
                        eh->current = 0.0f;
                        toKill.push_back(enemy);
                    }
                }
                result.enemiesStomped++;
                g.velocity   = -JUMP_FORCE * 0.5f;
                g.isGrounded = false;
                return;
            }

            if (!inv.isInvincible && aabb(et, ec)) {
                health.current -= PLAYER_HIT_DAMAGE;
                if (health.current <= 0.0f) {
                    health.current    = 0.0f;
                    result.playerDied = true;
                }
                result.playerHit  = true;
                inv.isInvincible  = true;
                inv.remaining     = 0.15f;

                {
                    float playerCX = pt.x + pw * 0.5f;
                    float enemyCX  = et.x + ec.w * 0.5f;
                    float kbDir    = (playerCX >= enemyCX) ? 1.0f : -1.0f;
                    pt.x += kbDir * 24.0f;
                }

                auto* eas = reg.try_get<EnemyAttackState>(enemy);
                auto* ead = reg.try_get<EnemyAnimData>(enemy);
                if (eas && ead && !eas->attacking &&
                    ead->attackSheet && !ead->attackFrames.empty()) {
                    auto& r    = reg.get<Renderable>(enemy);
                    auto& anim = reg.get<AnimationState>(enemy);
                    r.sheet         = ead->attackSheet;
                    r.frames        = ead->attackFrames;
                    r.renderW       = ead->spriteW;
                    r.renderH       = ead->spriteH;
                    anim.currentFrame = 0;
                    anim.totalFrames  = (int)ead->attackFrames.size();
                    anim.fps          = ead->attackFps;
                    anim.looping      = false;
                    eas->attacking    = true;
                    eas->cooldown     = 0.8f;
                    ead->ApplyHitbox(ead->attackHitbox, reg, enemy);
                }
            }

            if (aabb(et, ec)) {
                float oLeft  = (pt.x + pw) - et.x;
                float oRight = (et.x + ec.w) - pt.x;
                if (oLeft <= 0 || oRight <= 0) return;
                if (oLeft < oRight)
                    pt.x = et.x - pw;
                else
                    pt.x = et.x + ec.w;
            }
        });

        // --- Dead enemy platforms ---
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

        // --- Open-world mode: 4-sided AABB push-out, no gravity ---
        if (reg.all_of<OpenWorldTag>(playerEnt)) {
            auto owTileView = reg.view<TileTag, Transform, Collider>(entt::exclude<SlopeCollider, ActionTag>);
            owTileView.each([&](const Transform& tt, const Collider& tc) {
                if (pt.x + pw <= tt.x || pt.x >= tt.x + tc.w) return;
                if (pt.y + ph <= tt.y || pt.y >= tt.y + tc.h) return;

                float oTop    = (pt.y + ph) - tt.y;
                float oBottom = (tt.y + tc.h) - pt.y;
                float oLeft   = (pt.x + pw)  - tt.x;
                float oRight  = (tt.x + tc.w) - pt.x;

                float minH = oLeft < oRight ? oLeft : oRight;
                float minV = oTop  < oBottom ? oTop  : oBottom;
                if (minV <= minH) {
                    if (oTop < oBottom)  pt.y = tt.y - ph;
                    else                 pt.y = tt.y + tc.h;
                } else {
                    if (oLeft < oRight)  pt.x = tt.x - pw;
                    else                 pt.x = tt.x + tc.w;
                }
            });

            goalView.each([&](entt::entity goal, const Transform& ct, const Collider& cc) {
                if (reg.all_of<ActionTag>(goal)) return;
                if (aabb(ct, cc)) {
                    toDestroy.push_back(goal);
                    result.goalsCollected++;
                }
            });
            return; // skip all gravity-based collision logic
        }

        // --- Slope pass (FIRST) ---
        // Surface formula: surfaceY = tileTop + localX * (rise/run).
        // DiagUpLeft descends L->R, DiagUpRight descends R->L.
        // Continuous at seams because adjacent tiles start where the previous ended.
        bool onSlopeThisFrame = false;

        if (g.direction == GravityDir::DOWN) {
            float pFeet      = pt.y + pc.h;
            float pLeft      = pt.x;
            float pRight     = pt.x + pc.w;
            float bestSurface = pFeet + SLOPE_SNAP_LOOKAHEAD;
            bool  onSlope    = false;

            auto slopeView = reg.view<SlopeCollider, TileTag, Transform, Collider>();
            slopeView.each([&](const SlopeCollider& sc, const Transform& tt, const Collider& tc) {
                if (pRight <= tt.x || pLeft >= tt.x + tc.w) return;

                float riseH = tc.h * sc.heightFrac;
                float ratio = riseH / (float)tc.w;
                float highY = (float)tt.y;

                float playerX = pLeft + pc.w * 0.5f;

                float surface;
                if (sc.slopeType == SlopeType::DiagUpLeft) {
                    surface = highY + (tt.x + tc.w - playerX) * ratio;
                } else {
                    surface = highY + (playerX - tt.x) * ratio;
                }

                if (surface < tt.y || surface > tt.y + tc.h) return;

                if (pFeet < surface - SLOPE_SNAP_LOOKAHEAD) return;
                if (pt.y  > tt.y + tc.h + SLOPE_SNAP_LOOKAHEAD) return;
                // Skip overhead slopes (surface above player's head).
                if (surface < pt.y) return;

                if (surface < bestSurface) {
                    bestSurface = surface;
                    onSlope     = true;
                }
            });

            if (onSlope) {
                // [SLOPE FIX] Don't snap to slope while rising (jumping).
                constexpr float SLOPE_JUMP_THRESHOLD = 0.0f;
                bool risingFast = (g.velocity < SLOPE_JUMP_THRESHOLD);

                if (!risingFast) {
                    // Only snap when feet are near the surface.
                    float feetToSurface = bestSurface - pFeet;
                    if (feetToSurface >= -SLOPE_SNAP_LOOKAHEAD &&
                        feetToSurface <=  SLOPE_SNAP_LOOKAHEAD) {
                        pt.y         = bestSurface - pc.h;
                        g.velocity   = 0.0f;
                        g.isGrounded = true;
                        onSlopeThisFrame = true;
                    }
                }
                // onSlopeThisFrame only set when grounded — old unconditional
                // set broke flat-tile collisions while jumping.
            }
        }

        // --- Flat tile Pass 1: gravity-axis snap ---
        // Skips lateral corrections when onSlopeThisFrame to avoid fighting slope snap.
        auto tileView = reg.view<TileTag, Transform, Collider>(entt::exclude<SlopeCollider, ActionTag>);

        tileView.each([&](entt::entity te, const Transform& tt, const Collider& tc) {
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
                        if (g.velocity >= 0.0f) g.isGrounded = true;
                        pt.y       = tay - ph;   // snap to hitbox top, not visual top
                        g.velocity = 0.0f;
                    } else if (!onSlopeThisFrame
                               && oBottom < oTop && oBottom <= oLeft && oBottom <= oRight) {
                        pt.y       = tay + tc.h;
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

        // --- Flat tile Pass 2: lateral push-out with step-up ---
        // On slope: ceiling correction only. Off slope: overlap-axis comparison
        // decides step-up (oTop smallest, <= STEP_UP_HEIGHT) vs lateral push-out.

        tileView.each([&](entt::entity te, const Transform& tt, const Collider& tc) {
            float tax = tt.x, tay = tt.y;
            if (const auto* co = reg.try_get<ColliderOffset>(te)) {
                tax += co->x;
                tay += co->y;
            }
            if (pt.x + pw <= tax || pt.x >= tax + tc.w) return;
            if (pt.y + ph <= tay || pt.y >= tay + tc.h) return;

            // Skip push-out for moving platforms to avoid fighting MovingPlatformCarry.
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

                    if (g.direction == GravityDir::DOWN
                        && oTop >= 0.0f
                        && oTop <= STEP_UP_HEIGHT
                        && oTop <= oLeft && oTop <= oRight) {
                        pt.y         = tay - ph;
                        g.velocity   = 0.0f;
                        g.isGrounded = true;
                        break;
                    }
                    if (g.direction == GravityDir::UP
                        && oBottom > 0.0f
                        && oBottom <= oLeft && oBottom <= oRight) {
                        pt.y       = tay + tc.h;
                        g.velocity = 0.0f;
                        break;
                    }
                    // Skip lateral ejection for moving platforms when player is on top.
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

        // --- Sword slash-hit detection ---
        if (auto* atk = reg.try_get<AttackState>(playerEnt); atk && atk->isAttacking) {
            const auto* rend = reg.try_get<Renderable>(playerEnt);
            float fx = 0.0f, fy = 0.0f;
            switch (g.direction) {
                case GravityDir::DOWN:
                case GravityDir::UP:
                    fx = (rend && rend->flipH) ? -1.0f : 1.0f;
                    fy = 0.0f;
                    break;
                case GravityDir::LEFT:  fx =  0.0f; fy = -1.0f; break; // faces up-left wall
                case GravityDir::RIGHT: fx =  0.0f; fy = -1.0f; break; // faces up-right wall
            }

            float swordX, swordY, swordW, swordH;
            if (fx != 0.0f) {
                float insetY = pc.h * (1.0f - SWORD_HEIGHT) * 0.5f;
                swordW = SWORD_REACH;
                swordH = pc.h * SWORD_HEIGHT;
                swordY = pt.y + insetY;
                swordX = (fx > 0.0f) ? pt.x + pc.w
                                     : pt.x - SWORD_REACH;
            } else {
                float insetX = pc.w * (1.0f - SWORD_HEIGHT) * 0.5f;
                swordW = pc.w * SWORD_HEIGHT;
                swordH = SWORD_REACH;
                swordX = pt.x + insetX;
                swordY = (fy > 0.0f) ? pt.y + pc.h
                                     : pt.y - SWORD_REACH;
            }

            auto swordHits = [&](float tx, float ty, float tw, float th) -> bool {
                return swordX          < tx + tw &&
                       swordX + swordW > tx      &&
                       swordY          < ty + th  &&
                       swordY + swordH > ty;
            };

            // hitEntities prevents multi-frame damage stacking per swing.
            {
                auto actionView = reg.view<ActionTag, Transform, Collider>();
                actionView.each([&](entt::entity at, ActionTag& tag,
                                    const Transform& tt, const Collider& tc) {
                    if (!swordHits(tt.x, tt.y, (float)tc.w, (float)tc.h)) return;
                    // Only gate multi-hit tiles — single-hit tiles destroy immediately
                    if (tag.hitsRequired > 1) {
                        if (atk->hitEntities.count(at)) return;
                        atk->hitEntities.insert(at);
                    }
                    tag.hitsRemaining--;
                    if (tag.hitsRemaining <= 0) {
                        result.actionTilesTriggered.push_back(at);
                    } else {
                        if (reg.all_of<HitFlash>(at))
                            reg.get<HitFlash>(at).timer = HitFlash{}.duration;
                        else
                            reg.emplace<HitFlash>(at);
                    }
                });
            }

            liveEnemyView.each([&](entt::entity enemy,
                                   const Transform& et, const Collider& ec) {
                if (!swordHits(et.x, et.y, (float)ec.w, (float)ec.h)) return;
                if (atk->hitEntities.count(enemy)) return; // already hit this swing
                atk->hitEntities.insert(enemy);
                auto* eh = reg.try_get<Health>(enemy);
                if (!eh) {
                    toKill.push_back(enemy);
                    result.enemiesSlashed++;
                    result.slashHits++;
                    result.lowestHitHpFrac = 0.0f;
                    return;
                }
                eh->current -= SLASH_DAMAGE;
                result.slashHits++;
                {
                    float frac = std::max(eh->current, 0.0f) / eh->max;
                    if (frac < result.lowestHitHpFrac)
                        result.lowestHitHpFrac = frac;
                }

                // Positional knockback only — doesn't override velocity.
                {
                    float playerCX = pt.x + pw * 0.5f;
                    float enemyCX  = et.x + ec.w * 0.5f;
                    float kbDir    = (enemyCX >= playerCX) ? 1.0f : -1.0f;
                    if (auto* et2 = reg.try_get<Transform>(enemy))
                        et2->x += kbDir * 24.0f;
                }

                constexpr float SLASH_STUN = 0.35f;
                if (!reg.all_of<HitFlash>(enemy))
                    reg.emplace<HitFlash>(enemy, SLASH_STUN, SLASH_STUN);
                else
                    reg.get<HitFlash>(enemy).timer = SLASH_STUN;

                if (auto* react = reg.try_get<EnemyReaction>(enemy))
                    react->turnCooldown = SLASH_STUN + 0.15f;

                if (auto* ead = reg.try_get<EnemyAnimData>(enemy);
                    ead && ead->hurtSheet && !ead->hurtFrames.empty()) {
                    auto& r    = reg.get<Renderable>(enemy);
                    auto& anim = reg.get<AnimationState>(enemy);
                    r.sheet         = ead->hurtSheet;
                    r.frames        = ead->hurtFrames;
                    r.renderW       = ead->spriteW;
                    r.renderH       = ead->spriteH;
                    anim.currentFrame = 0;
                    anim.totalFrames  = (int)ead->hurtFrames.size();
                    anim.fps          = ead->hurtFps;
                    anim.looping      = false;
                    ead->ApplyHitbox(ead->hurtHitbox, reg, enemy);
                }
                if (eh->current <= 0.0f) {
                    eh->current = 0.0f;
                    toKill.push_back(enemy);
                    result.enemiesSlashed++;
                }
            });
        }

        // --- Goal collection ---
        goalView.each([&](entt::entity goal, const Transform& gt, const Collider& gc) {
            if (reg.all_of<ActionTag>(goal)) return;
            if (aabb(gt, gc)) {
                toDestroy.push_back(goal);
                result.goalsCollected++;
            }
        });
    });

    // --- Commit deferred mutations ---
    // Deduplicate — stomp + slash can target same entity; double emplace asserts.
    {
        std::sort(toKill.begin(), toKill.end());
        toKill.erase(std::unique(toKill.begin(), toKill.end()), toKill.end());
    }
    for (auto e : toKill) {
        if (reg.all_of<Velocity>(e)) {
            auto& v = reg.get<Velocity>(e);
            v.dx = v.dy = 0.0f;
        }
        // FaceRightTag enemies freeze; legacy slimes swap to dead sprite.
        if (reg.all_of<Renderable, AnimationState>(e)) {
            auto& r    = reg.get<Renderable>(e);
            auto& anim = reg.get<AnimationState>(e);
            if (auto* ead = reg.try_get<EnemyAnimData>(e);
                ead && ead->deadSheet && !ead->deadFrames.empty()) {
                r.sheet         = ead->deadSheet;
                r.frames        = ead->deadFrames;
                r.renderW       = ead->spriteW;
                r.renderH       = ead->spriteH;
                anim.currentFrame = 0;
                anim.totalFrames  = (int)ead->deadFrames.size();
                anim.fps          = ead->deadFps;
                anim.looping      = false;
                ead->ApplyHitbox(ead->deadHitbox, reg, e);
            } else if (reg.all_of<FaceRightTag>(e)) {
                // Custom enemy without dead frames: freeze
                anim.looping = false;
                anim.currentFrame = std::min(anim.currentFrame, anim.totalFrames - 1);
            } else {
                // Legacy slime: swap to the flat dead sprite
                static constexpr SDL_Rect SLIME_DEAD_RECT = {0, 112, 59, 12};
                r.frames          = {SLIME_DEAD_RECT};
                anim.currentFrame = 0;
                anim.totalFrames  = 1;
                anim.looping      = false;
            }
        }
        if (reg.all_of<Collider>(e)) {
            if (!reg.all_of<FaceRightTag>(e)) {
                // Only shrink collider for legacy slime
                auto& col = reg.get<Collider>(e);
                col.w = 59;
                col.h = 12;
            }
        }
        reg.emplace<DeadTag>(e);
    }

    for (auto e : toDestroy)
        reg.destroy(e);

    // --- Collect group members for triggered action tiles ---
    // Expands groups so GameScene receives the full set for destroy animations.
    {
        auto& v = result.actionTilesTriggered;
        std::sort(v.begin(), v.end());
        v.erase(std::unique(v.begin(), v.end()), v.end());
    }

    {
        // For each triggered tile in a group, add all other group members.
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

    // --- Hazard tile overlap ---
    // No TileTag so no push-out. TOUCH margin catches flush contact.
    {
        constexpr float TOUCH = 1.0f;
        auto hazardView = reg.view<HazardTag, Transform, Collider>();
        auto pView      = reg.view<PlayerTag, Transform, Collider>();
        pView.each([&](const Transform& pt, const Collider& pc) {
            if (result.onHazard) return;
            hazardView.each([&](entt::entity he, const Transform& ht, const Collider& hc) {
                if (result.onHazard) return;
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
