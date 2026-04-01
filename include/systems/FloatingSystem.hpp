#pragma once
#include <Components.hpp>
#include <GameConfig.hpp>
#include <GameEvents.hpp>
#include <cmath>
#include <set>
#include <utility>
#include <entt/entt.hpp>

inline FloatingResult FloatingSystem(entt::registry& reg, float dt) {
    FloatingResult result;

    // --- 1. Gravity for non-floating enemies ---
    {
        constexpr float ENEMY_GRAVITY  = 800.0f;
        constexpr float ENEMY_MAX_FALL = 900.0f;

        auto tileView  = reg.view<TileTag, Transform, Collider>(entt::exclude<HazardTag>);
        auto slopeView = reg.view<SlopeCollider, TileTag, Transform, Collider>();
        auto enemyView = reg.view<EnemyTag, Transform, Collider, Velocity>(
            entt::exclude<DeadTag, FloatTag>);

        enemyView.each([&](entt::entity ent, Transform& et, const Collider& ec, Velocity& ev) {
            if (auto* cs = reg.try_get<EnemyClimbState>(ent);
                cs && (cs->climbing || cs->steppingOff)) {
                ev.dy = 0.0f;
                return;
            }
            ev.dy = std::min(ev.dy + ENEMY_GRAVITY * dt, ENEMY_MAX_FALL);
            et.y += ev.dy * dt;

            // --- Slope pass ---
            bool onSlopeThisFrame = false;
            {
                float eFeet       = et.y + ec.h;
                float eLeft       = et.x;
                float eRight      = et.x + ec.w;
                float bestSurface = eFeet + SLOPE_SNAP_LOOKAHEAD;
                bool  foundSlope  = false;

                slopeView.each([&](const SlopeCollider& sc, const Transform& tt, const Collider& tc) {
                    if (eRight <= tt.x || eLeft >= tt.x + tc.w) return;

                    float riseH  = tc.h * sc.heightFrac;
                    float ratio  = riseH / (float)tc.w;
                    float highY  = (float)tt.y;
                    float centerX = eLeft + ec.w * 0.5f;

                    float surface;
                    if (sc.slopeType == SlopeType::DiagUpLeft)
                        surface = highY + (tt.x + tc.w - centerX) * ratio;
                    else
                        surface = highY + (centerX - tt.x) * ratio;

                    if (surface < tt.y || surface > tt.y + tc.h) return;
                    if (eFeet < surface - SLOPE_SNAP_LOOKAHEAD) return;
                    if (et.y  > tt.y + tc.h + SLOPE_SNAP_LOOKAHEAD) return;
                    if (surface < et.y) return;

                    if (surface < bestSurface) {
                        bestSurface = surface;
                        foundSlope  = true;
                    }
                });

                if (foundSlope && ev.dy >= 0.0f) {
                    float feetToSurface = bestSurface - eFeet;
                    if (feetToSurface >= -SLOPE_SNAP_LOOKAHEAD &&
                        feetToSurface <=  SLOPE_SNAP_LOOKAHEAD) {
                        et.y  = bestSurface - ec.h;
                        ev.dy = 0.0f;
                        onSlopeThisFrame = true;
                    }
                }
            }

            if (!onSlopeThisFrame) {
                constexpr float STEP_UP = 8.0f;
                tileView.each([&](const Transform& tt, const Collider& tc) {
                    if (et.x + ec.w <= tt.x || et.x >= tt.x + tc.w) return;
                    float penetration = (et.y + ec.h) - tt.y;
                    if (penetration >= 0.0f && penetration <= STEP_UP + std::max(0.0f, ev.dy * dt)) {
                        et.y  = tt.y - ec.h;
                        ev.dy = 0.0f;
                    }
                });
            }

            tileView.each([&](const Transform& tt, const Collider& tc) {
                if (et.x + ec.w <= tt.x || et.x >= tt.x + tc.w) return;
                if (et.y + ec.h <= tt.y || et.y >= tt.y + tc.h) return;

                float penetration = (et.y + ec.h) - tt.y;
                if (penetration <= 8.0f) return;

                float overlapL = (et.x + ec.w) - tt.x;
                float overlapR = (tt.x + tc.w) - et.x;
                if (overlapL < overlapR) {
                    et.x = tt.x - ec.w;
                    if (ev.dx > 0.f) ev.dx = -std::abs(ev.dx);
                } else {
                    et.x = tt.x + tc.w;
                    if (ev.dx < 0.f) ev.dx = std::abs(ev.dx);
                }
            });
        });
    }

    // --- 1b. Gravity for dead enemies (fall to ground after dying) ---
    {
        constexpr float DEAD_GRAVITY  = 800.0f;
        constexpr float DEAD_MAX_FALL = 900.0f;
        constexpr float STEP_UP       = 8.0f;

        auto tileView2 = reg.view<TileTag, Transform, Collider>(entt::exclude<HazardTag>);
        auto deadView  = reg.view<DeadTag, EnemyTag, Transform, Collider, Velocity>(
            entt::exclude<FloatTag>);

        deadView.each([&](Transform& et, const Collider& ec, Velocity& ev) {
            ev.dy = std::min(ev.dy + DEAD_GRAVITY * dt, DEAD_MAX_FALL);
            et.y += ev.dy * dt;

            tileView2.each([&](const Transform& tt, const Collider& tc) {
                if (et.x + ec.w <= tt.x || et.x >= tt.x + tc.w) return;
                float penetration = (et.y + ec.h) - tt.y;
                if (penetration >= 0.0f && penetration <= STEP_UP + std::max(0.0f, ev.dy * dt)) {
                    et.y  = tt.y - ec.h;
                    ev.dy = 0.0f;
                }
            });
        });
    }

    // --- 2. Read player state ---
    float playerX  = 0.0f, playerY  = 0.0f;
    float playerW  = 0.0f, playerH  = 0.0f;
    float playerVx = 0.0f, playerVy = 0.0f;
    float slashDir = 0.0f; // +1 right, -1 left
    bool  playerSlashing = false;
    float swordX = 0.0f, swordY = 0.0f, swordW = 0.0f, swordH = 0.0f;

    {
        auto pv = reg.view<PlayerTag, Transform, Collider, Velocity,
                           GravityState, Renderable, AttackState>();
        pv.each([&](const Transform& pt, const Collider& pc,
                    const Velocity& pvel, const GravityState& pg,
                    const Renderable& pr, const AttackState& atk) {
            playerX  = pt.x;
            playerY  = pt.y;
            playerW  = static_cast<float>(pc.w);
            playerH  = static_cast<float>(pc.h);
            playerVx = pvel.dx;
            playerVy = pg.velocity;

            playerSlashing = atk.isAttacking;
            if (playerSlashing) {
                float fx = pr.flipH ? -1.0f : 1.0f;
                slashDir = fx;
                swordW   = SWORD_REACH;
                swordH   = pc.h * SWORD_HEIGHT;
                swordY   = pt.y + pc.h * (1.0f - SWORD_HEIGHT) * 0.5f;
                swordX   = (fx > 0.0f) ? pt.x + pc.w : pt.x - SWORD_REACH;
            }
        });
    }

    // --- 3. Float update ---
    // FloatTag in the view filter avoids a libc++ std::apply deleted-function
    // error when unpacked in view.each().
    auto floatView = reg.view<FloatState, Transform, Collider, FloatTag>();
    for (auto entity : floatView) {

        auto& fs        = floatView.get<FloatState>(entity);
        auto& ft        = floatView.get<Transform>(entity);
        const auto& fc  = floatView.get<Collider>(entity);

        fs.bobTimer += dt;
        float bob = fs.bobAmp * std::sin(fs.bobTimer * fs.bobSpeed + fs.bobPhase);

        float drag = std::exp(-FloatState::DRAG * dt);
        fs.driftVx *= drag;
        fs.driftVy *= drag;
        if (std::abs(fs.driftVx) < 0.5f) fs.driftVx = 0.0f;
        if (std::abs(fs.driftVy) < 0.5f) fs.driftVy = 0.0f;

        fs.spinSpeed *= drag;
        if (std::abs(fs.spinSpeed) < 0.5f) fs.spinSpeed = 0.0f;
        // Clamp to (-89, +89) — RotateSurfaceDeg only handles 0/90/180/270,
        // so reaching 90 causes a one-frame flicker.
        fs.spinAngle += fs.spinSpeed * dt;
        if (fs.spinAngle >= 89.0f)  { fs.spinAngle =  89.0f; fs.spinSpeed = 0.0f; }
        if (fs.spinAngle <= -89.0f) { fs.spinAngle = -89.0f; fs.spinSpeed = 0.0f; }

        ft.x      += fs.driftVx * dt;
        float prevY = ft.y;
        fs.baseY  += fs.driftVy * dt;
        ft.y       = fs.baseY + bob;
        fs.dyThisFrame = ft.y - prevY;

        float preBounceSpeed = std::abs(fs.driftVx) + std::abs(fs.driftVy);

        // --- Bounce off solid surfaces ---
        constexpr float BOUNCE_DAMP = 0.55f;
        constexpr float MIN_BOUNCE  = 30.0f;
        {
            auto solidView  = reg.view<TileTag,   Transform, Collider>();
            auto hazardView = reg.view<HazardTag,  Transform, Collider>();

            auto bounce = [&](entt::entity te, const Transform& tt, const Collider& tc) {
                if (te == entity) return;
                float tx = tt.x, ty = tt.y;
                if (const auto* co = reg.try_get<ColliderOffset>(te)) { tx += co->x; ty += co->y; }
                if (ft.x + fc.w <= tx || ft.x >= tx + tc.w) return;
                if (ft.y + fc.h <= ty || ft.y >= ty + tc.h) return;

                // Register action-tile hit while still overlapping (before push-out).
                if (preBounceSpeed >= 80.0f) {
                    if (auto* tag = reg.try_get<ActionTag>(te)) {
                        bool recentlyHit = reg.all_of<HitFlash>(te) &&
                                           reg.get<HitFlash>(te).timer > HitFlash{}.duration * 0.5f;
                        if (!recentlyHit) {
                            tag->hitsRemaining--;
                            if (tag->hitsRemaining <= 0) {
                                result.actionTilesTriggered.push_back(te);
                            } else {
                                if (reg.all_of<HitFlash>(te))
                                    reg.get<HitFlash>(te).timer = HitFlash{}.duration;
                                else
                                    reg.emplace<HitFlash>(te);
                            }
                        }
                    }
                }

                float oLeft   = (ft.x + fc.w) - tx;
                float oRight  = (tx + tc.w)   - ft.x;
                float oTop    = (ft.y + fc.h) - ty;
                float oBottom = (ty + tc.h)   - ft.y;

                float minH = oLeft < oRight  ? oLeft  : oRight;
                float minV = oTop  < oBottom ? oTop   : oBottom;

                // Velocity-direction bias breaks overlap ties.
                float absVx = std::abs(fs.driftVx);
                float absVy = std::abs(fs.driftVy);
                bool horizBounce = (absVx >= absVy)
                    ? (minH <= minV * 1.5f)
                    : (minH < minV * 0.5f);

                if (horizBounce) {
                    if (absVx >= MIN_BOUNCE) {
                        fs.driftVx = -fs.driftVx * BOUNCE_DAMP;
                        fs.spinSpeed += fs.driftVx * 0.3f;
                    } else {
                        fs.driftVx = 0.0f;
                    }
                    if (oLeft < oRight) ft.x = tx - fc.w;
                    else                ft.x = tx + tc.w;
                } else {
                    bool hitFromBelow = (oBottom < oTop);
                    if (hitFromBelow) {
                        if (absVy >= MIN_BOUNCE)
                            fs.driftVy = -fs.driftVy * BOUNCE_DAMP;
                        else
                            fs.driftVy = 0.0f;
                        fs.baseY = ty + tc.h - bob;
                        ft.y = fs.baseY + bob;
                    } else {
                        fs.driftVy = 0.0f;
                        fs.baseY = ty - fc.h - bob;
                        ft.y = fs.baseY + bob;
                    }
                }
            };

            solidView.each( [&](entt::entity te, const Transform& tt, const Collider& tc) { bounce(te, tt, tc); });
            hazardView.each([&](entt::entity te, const Transform& tt, const Collider& tc) { bounce(te, tt, tc); });
        }

        // --- Player body push ---
        // Runs after CollisionSystem which pushes the player out of solid
        // floating entities; CONTACT_MARGIN compensates for that push-out.
        constexpr float BODY_PUSH_H    = 240.0f;
        constexpr float BODY_PUSH_V    = 180.0f;
        constexpr float BODY_SPIN      =  90.0f;
        constexpr float CONTACT_MARGIN =   8.0f;
        constexpr float BOTTOM_MARGIN  =   6.0f;

        bool bodyContact =
            playerX           < ft.x + fc.w  + CONTACT_MARGIN &&
            playerX + playerW > ft.x          - CONTACT_MARGIN &&
            playerY           < ft.y + fc.h  + CONTACT_MARGIN &&
            playerY + playerH > ft.y          - CONTACT_MARGIN;

        // Bottom-hit: player jumping up into the object's underside.
        bool bottomHit = false;
        {
            float headY   = playerY;
            float tileBot = ft.y + fc.h;
            bool hOverlap = (playerX + playerW > ft.x) && (playerX < ft.x + fc.w);
            bool nearBot  = (headY >= tileBot - BOTTOM_MARGIN) && (headY < tileBot + BOTTOM_MARGIN);
            bool jumping  = (playerVy < -10.0f);
            bottomHit = hOverlap && nearBot && jumping;
        }

        if (bodyContact || bottomHit) {
            float pCx = playerX + playerW * 0.5f;
            float fCx = ft.x    + fc.w   * 0.5f;
            float pCy = playerY + playerH * 0.5f;
            float fCy = ft.y    + fc.h   * 0.5f;

            float hDir = (fCx >= pCx) ? 1.0f : -1.0f;
            float vDir = (fCy >= pCy) ? 1.0f : -1.0f;

            float overlapH = (playerW * 0.5f + fc.w * 0.5f) - std::abs(pCx - fCx);
            float overlapV = (playerH * 0.5f + fc.h * 0.5f) - std::abs(pCy - fCy);
            bool topBottomHit = overlapV < overlapH || bottomHit;

            if (!fs.wasInContact) {
                if (topBottomHit && !bottomHit) {
                    float vMag = std::max(120.0f, std::abs(playerVy));
                    fs.driftVy  += vDir * vMag * (BODY_PUSH_V / MAX_FALL_SPEED);
                    if (std::abs(playerVx) > 10.0f) {
                        float hMag = std::abs(playerVx) * 0.6f;
                        fs.driftVx  += (playerVx > 0.0f ? 1.0f : -1.0f) * hMag * (BODY_PUSH_H / PLAYER_SPEED);
                        fs.spinSpeed += (playerVx > 0.0f ? 1.0f : -1.0f) * BODY_SPIN * 0.5f;
                    }
                    fs.spinSpeed += hDir * BODY_SPIN * 0.5f;
                } else if (!topBottomHit) {
                    float hMag = std::max(80.0f, std::abs(playerVx));
                    fs.driftVx  += hDir * hMag * (BODY_PUSH_H / PLAYER_SPEED);
                    fs.spinSpeed += hDir * BODY_SPIN;
                }
                fs.baseY = ft.y - bob;
            }

            // Continuous upward push while player presses against underside,
            // capped to prevent runaway acceleration.
            if (bottomHit) {
                constexpr float BOTTOM_HIT_PUSH  = 220.0f;
                constexpr float BOTTOM_HIT_CAP   = 400.0f;
                if (fs.driftVy > -BOTTOM_HIT_CAP) {
                    float jMag = std::max(BOTTOM_HIT_PUSH, std::abs(playerVy) * 0.8f);
                    fs.driftVy -= jMag * dt * 8.0f;
                    fs.driftVy  = std::max(fs.driftVy, -BOTTOM_HIT_CAP);
                }
                fs.spinSpeed += hDir * BODY_SPIN * dt * 6.0f;
                fs.baseY = ft.y - bob;
            }

            fs.wasInContact = true;
        } else {
            fs.wasInContact = false;
        }

        // --- Sword slash push ---
        constexpr float SLASH_PUSH_FORCE = 280.0f;
        constexpr float SLASH_SPIN       = 360.0f;

        // --- Float carry ---
        if (fs.wasInContact && std::abs(fs.dyThisFrame) > 0.001f) {
            auto pCarry = reg.view<PlayerTag, Transform, Collider, GravityState>();
            pCarry.each([&](Transform& pt, const Collider& pc, GravityState& pg) {
                float pFeet    = pt.y + pc.h;
                float objTop   = ft.y;
                constexpr float CARRY_MARGIN = 12.0f;
                bool onTop = std::abs(pFeet - objTop) < CARRY_MARGIN &&
                             pt.x + pc.w > ft.x && pt.x < ft.x + fc.w;
                if (onTop) {
                    pt.y += fs.dyThisFrame;
                    if (fs.dyThisFrame > 0.5f)
                        pg.isGrounded = false;
                }
            });
        }

        if (playerSlashing) {
            bool swordOverlap =
                swordX          < ft.x + fc.w &&
                swordX + swordW > ft.x         &&
                swordY          < ft.y + fc.h  &&
                swordY + swordH > ft.y;

            if (swordOverlap) {
                fs.driftVx   += slashDir * SLASH_PUSH_FORCE;
                fs.spinSpeed += slashDir * SLASH_SPIN;
            }
        }
    }

    // --- 4. Float vs float collisions ---
    // Separate pass so all entities have final post-bounce positions.
    // Pair deduplication ensures each (A,B) is handled once.
    {
        constexpr float FLOAT_PUSH_H   = 260.0f;
        constexpr float FLOAT_PUSH_V   = 160.0f;
        constexpr float FLOAT_SPIN_HIT =  90.0f;
        constexpr float MARGIN         =   4.0f;

        using Pair = std::pair<entt::entity, entt::entity>;
        std::set<Pair> processed;

        for (auto A : floatView) {
            auto& fsA = floatView.get<FloatState>(A);
            auto& ftA = floatView.get<Transform>(A);
            const auto& fcA = floatView.get<Collider>(A);

            for (auto B : floatView) {
                if (B == A) continue;
                Pair key = (A < B) ? Pair{A, B} : Pair{B, A};
                if (!processed.insert(key).second) continue;

                auto& fsB = floatView.get<FloatState>(B);
                auto& ftB = floatView.get<Transform>(B);
                const auto& fcB = floatView.get<Collider>(B);

                if (ftA.x + fcA.w + MARGIN <= ftB.x || ftA.x >= ftB.x + fcB.w + MARGIN) continue;
                if (ftA.y + fcA.h + MARGIN <= ftB.y || ftA.y >= ftB.y + fcB.h + MARGIN) continue;

                float dx = (ftA.x + fcA.w * 0.5f) - (ftB.x + fcB.w * 0.5f);
                float dy = (ftA.y + fcA.h * 0.5f) - (ftB.y + fcB.h * 0.5f);
                float hDir = (dx >= 0.0f) ? 1.0f : -1.0f;
                float vDir = (dy >= 0.0f) ? 1.0f : -1.0f;

                float relVx = fsA.driftVx - fsB.driftVx;
                float relVy = fsA.driftVy - fsB.driftVy;

                float approachH = -(hDir * relVx);
                float approachV = -(vDir * relVy);

                float oLeft   = (ftA.x + fcA.w) - ftB.x;
                float oRight  = (ftB.x + fcB.w) - ftA.x;
                float oTop    = (ftA.y + fcA.h) - ftB.y;
                float oBottom = (ftB.y + fcB.h) - ftA.y;
                float minH = std::min(oLeft,  oRight);
                float minV = std::min(oTop,   oBottom);
                bool  horizAxis = (minH < minV);

                if ( horizAxis && approachH < -10.0f) continue;
                if (!horizAxis && approachV < -10.0f) continue;

                if (minH > 0.0f && minV > 0.0f) {
                    if (horizAxis) {
                        float half = minH * 0.5f + 0.5f;
                        ftA.x += hDir * half;
                        ftB.x -= hDir * half;
                    } else {
                        float half = minV * 0.5f + 0.5f;
                        ftA.y += vDir * half;  fsA.baseY += vDir * half;
                        ftB.y -= vDir * half;  fsB.baseY -= vDir * half;
                    }
                }

                if (horizAxis) {
                    float imp = std::max(FLOAT_PUSH_H, std::abs(relVx) * 1.2f);
                    fsA.driftVx   +=  hDir * imp * 0.5f;
                    fsB.driftVx   -= hDir * imp * 0.5f;
                    fsA.spinSpeed +=  hDir * FLOAT_SPIN_HIT;
                    fsB.spinSpeed -= hDir * FLOAT_SPIN_HIT;
                    fsA.driftVy  += vDir * FLOAT_PUSH_V * 0.15f;
                    fsB.driftVy  -= vDir * FLOAT_PUSH_V * 0.15f;
                } else {
                    float imp = std::max(FLOAT_PUSH_V, std::abs(relVy) * 1.2f);
                    fsA.driftVy   +=  vDir * imp * 0.5f;
                    fsB.driftVy   -= vDir * imp * 0.5f;
                    fsA.driftVx  += hDir * FLOAT_PUSH_H * 0.15f;
                    fsB.driftVx  -= hDir * FLOAT_PUSH_H * 0.15f;
                    fsA.spinSpeed +=  hDir * FLOAT_SPIN_HIT * 0.5f;
                    fsB.spinSpeed -= hDir * FLOAT_SPIN_HIT * 0.5f;
                }
            }
        }
    }

    // --- 5. Float vs enemy collisions ---
    // Fast-moving floating objects deal slash-equivalent damage to enemies.
    {
        constexpr float HIT_SPEED_THRESHOLD = 80.0f;
        constexpr float HIT_DAMAGE          = SLASH_DAMAGE;
        constexpr float HIT_COOLDOWN        = 0.3f;

        auto liveEnemies = reg.view<EnemyTag, Transform, Collider, Health>(
            entt::exclude<DeadTag, FloatTag>);

        for (auto fEnt : floatView) {
            auto& fs       = floatView.get<FloatState>(fEnt);
            auto& ft       = floatView.get<Transform>(fEnt);
            const auto& fc = floatView.get<Collider>(fEnt);

            float speed = std::abs(fs.driftVx) + std::abs(fs.driftVy);
            if (speed < HIT_SPEED_THRESHOLD) continue;

            liveEnemies.each([&](entt::entity enemy, const Transform& et,
                                 const Collider& ec, Health& eh) {
                float ex = et.x, ey = et.y;
                if (const auto* co = reg.try_get<ColliderOffset>(enemy)) {
                    ex += co->x; ey += co->y;
                }
                if (ft.x + fc.w <= ex || ft.x >= ex + ec.w) return;
                if (ft.y + fc.h <= ey || ft.y >= ey + ec.h) return;

                if (reg.all_of<HitFlash>(enemy) &&
                    reg.get<HitFlash>(enemy).timer > HIT_COOLDOWN * 0.5f)
                    return;

                eh.current -= HIT_DAMAGE;

                float fCx = ft.x + fc.w * 0.5f;
                float eCx = ex + ec.w * 0.5f;
                float kbDir = (eCx >= fCx) ? 1.0f : -1.0f;
                if (auto* et2 = reg.try_get<Transform>(enemy))
                    et2->x += kbDir * 24.0f;

                if (!reg.all_of<HitFlash>(enemy))
                    reg.emplace<HitFlash>(enemy, HIT_COOLDOWN, HIT_COOLDOWN);
                else
                    reg.get<HitFlash>(enemy).timer = HIT_COOLDOWN;

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

                if (eh.current <= 0.0f) {
                    eh.current = 0.0f;
                    result.enemiesKilledByFloat.push_back(enemy);
                }

                fs.driftVx *= 0.5f;
                fs.driftVy *= 0.5f;
            });
        }
    }

    return result;
}
