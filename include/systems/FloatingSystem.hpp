#pragma once
#include <Components.hpp>
#include <GameConfig.hpp>
#include <GameEvents.hpp>
#include <cmath>
#include <entt/entt.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// FloatingSystem
//
// Updates every entity that has both FloatTag and FloatState each frame:
//
//   Bob   — smooth vertical sine-wave oscillation around baseY.
//   Drift — velocity imparted by player contact/slash decays with drag.
//   Spin  — visual rotation that decays independently of drift.
//   Push  — detects AABB overlap between the player body (or sword rect) and
//            each floating entity this frame; imparts drift + spin proportional
//            to the player's movement velocity.
//
// The system also applies simple downward gravity to all EnemyTag entities
// that do NOT have FloatTag, grounding them on TileTag surfaces.
// ─────────────────────────────────────────────────────────────────────────────

inline FloatingResult FloatingSystem(entt::registry& reg, float dt) {
    FloatingResult result;

    // ── 1. Gravity for non-floating enemies ──────────────────────────────────
    {
        constexpr float ENEMY_GRAVITY  = 800.0f;
        constexpr float ENEMY_MAX_FALL = 900.0f;

        auto tileView  = reg.view<TileTag, Transform, Collider>();
        auto enemyView = reg.view<EnemyTag, Transform, Collider, Velocity>(
            entt::exclude<DeadTag, FloatTag>);

        enemyView.each([&](Transform& et, const Collider& ec, Velocity& ev) {
            ev.dy = std::min(ev.dy + ENEMY_GRAVITY * dt, ENEMY_MAX_FALL);
            et.y += ev.dy * dt;

            tileView.each([&](const Transform& tt, const Collider& tc) {
                if (et.x + ec.w <= tt.x || et.x >= tt.x + tc.w) return;
                if (et.y + ec.h >= tt.y && et.y + ec.h <= tt.y + tc.h + ev.dy * dt + 2.0f) {
                    et.y  = tt.y - ec.h;
                    ev.dy = 0.0f;
                }
            });
        });
    }

    // ── 2. Read player state for push / sword detection ──────────────────────
    float playerX  = 0.0f, playerY  = 0.0f;
    float playerW  = 0.0f, playerH  = 0.0f;
    float playerVx = 0.0f, playerVy = 0.0f;
    float slashDir = 0.0f; // +1 = right, -1 = left — the direction the player is facing
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
            playerVy = pg.velocity; // gravity-axis velocity (positive = falling down)

            playerSlashing = atk.isAttacking;
            if (playerSlashing) {
                // fx is the direction the player is FACING (slash direction)
                float fx = pr.flipH ? -1.0f : 1.0f;
                slashDir = fx;
                swordW   = SWORD_REACH;
                swordH   = pc.h * SWORD_HEIGHT;
                swordY   = pt.y + pc.h * (1.0f - SWORD_HEIGHT) * 0.5f;
                swordX   = (fx > 0.0f) ? pt.x + pc.w : pt.x - SWORD_REACH;
            }
        });
    }

    // ── 3. Float update ───────────────────────────────────────────────────────
    // FloatTag is an empty struct. Including it in the view filter (not as a
    // lambda parameter) lets EnTT build the correct intersection without the
    // libc++ std::apply deleted-function error that triggered when it was
    // unpacked in view.each(). The per-entity all_of<FloatTag> guard is gone.
    auto floatView = reg.view<FloatState, Transform, Collider, FloatTag>();
    for (auto entity : floatView) {

        auto& fs        = floatView.get<FloatState>(entity);
        auto& ft        = floatView.get<Transform>(entity);
        const auto& fc  = floatView.get<Collider>(entity);

        // ── Bob ───────────────────────────────────────────────────────────────
        fs.bobTimer += dt;
        float bob = fs.bobAmp * std::sin(fs.bobTimer * fs.bobSpeed + fs.bobPhase);

        // ── Drag decay ────────────────────────────────────────────────────────
        float drag = std::exp(-FloatState::DRAG * dt);
        fs.driftVx *= drag;
        fs.driftVy *= drag;
        if (std::abs(fs.driftVx) < 0.5f) fs.driftVx = 0.0f;
        if (std::abs(fs.driftVy) < 0.5f) fs.driftVy = 0.0f;

        fs.spinSpeed *= drag;
        if (std::abs(fs.spinSpeed) < 0.5f) fs.spinSpeed = 0.0f;
        // Clamp spinAngle to (-89, +89) so it never crosses a 90-degree boundary.
        // RotateSurfaceDeg only handles 0/90/180/270 — if spinAngle reaches 90 it
        // snaps the sprite sideways for one frame then flips back, causing a flicker.
        // Floating spin is purely cosmetic wobble so limiting to <90deg is fine.
        fs.spinAngle += fs.spinSpeed * dt;
        if (fs.spinAngle >= 89.0f)  { fs.spinAngle =  89.0f; fs.spinSpeed = 0.0f; }
        if (fs.spinAngle <= -89.0f) { fs.spinAngle = -89.0f; fs.spinSpeed = 0.0f; }

        // ── Apply position ────────────────────────────────────────────────────
        ft.x      += fs.driftVx * dt;
        float prevY = ft.y;
        fs.baseY  += fs.driftVy * dt;  // integrate vertical drift into the bob anchor
        ft.y       = fs.baseY + bob;
        fs.dyThisFrame = ft.y - prevY;  // record how far the object moved vertically

        // Capture speed before bounce so the hit check below still sees the
        // pre-impact velocity even after driftVx/driftVy get reflected/damped.
        float preBounceSpeed = std::abs(fs.driftVx) + std::abs(fs.driftVy);

        // ── Bounce off solid surfaces (TileTag, ActionTag, HazardTag) ─────────
        // Test against all tiles that have a physical presence: solid tiles,
        // action tiles (still have TileTag+Collider until destroyed), and hazard
        // tiles. When overlapping, reflect the relevant drift axis and push out.
        constexpr float BOUNCE_DAMP = 0.55f; // fraction of velocity kept after bounce
        constexpr float MIN_BOUNCE  = 30.0f; // px/s minimum to bother reflecting
        {
            // Collect all collidable tile entities: TileTag OR HazardTag tiles
            // ActionTag tiles already have TileTag so they're covered.
            // Use two views to cover both.
            auto solidView  = reg.view<TileTag,   Transform, Collider>();
            auto hazardView = reg.view<HazardTag,  Transform, Collider>();

            auto bounce = [&](entt::entity te, const Transform& tt, const Collider& tc) {
                if (te == entity) return; // never bounce against self
                // Apply ColliderOffset if present so we test the actual hitbox
                float tx = tt.x, ty = tt.y;
                if (const auto* co = reg.try_get<ColliderOffset>(te)) { tx += co->x; ty += co->y; }
                // Skip if no overlap
                if (ft.x + fc.w <= tx || ft.x >= tx + tc.w) return;
                if (ft.y + fc.h <= ty || ft.y >= ty + tc.h) return;

                // ── Action tile hit: register here while we know there's overlap ──
                // Doing this inside bounce (before push-out) means ft.x/ft.y still
                // overlaps the hitbox — the separate hit check below runs after
                // push-out and always misses.
                if (preBounceSpeed >= 80.0f) {
                    if (auto* tag = reg.try_get<ActionTag>(te)) {
                        bool recentlyHit = reg.all_of<HitFlash>(te) &&
                                           reg.get<HitFlash>(te).timer > HitFlash{}.duration * 0.5f;
                        if (!recentlyHit) {
                            tag->hitsRemaining--;
                            if (tag->hitsRemaining <= 0) {
                                // Don't strip components here — defer to GameScene's
                                // destroy-animation pipeline so death anims play correctly.
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

                // Compute overlap on each axis to find the shallowest penetration
                float oLeft   = (ft.x + fc.w) - tx;
                float oRight  = (tx + tc.w)   - ft.x;
                float oTop    = (ft.y + fc.h) - ty;
                float oBottom = (ty + tc.h)   - ft.y;

                float minH = oLeft < oRight  ? oLeft  : oRight;
                float minV = oTop  < oBottom ? oTop   : oBottom;

                // Use velocity direction to break ties: if the object is moving
                // mostly horizontally, prefer a horizontal bounce even if the
                // vertical overlap is slightly smaller (avoids spurious upward
                // launches when hitting a narrow offset hitbox from the side).
                float absVx = std::abs(fs.driftVx);
                float absVy = std::abs(fs.driftVy);
                bool horizBounce = (absVx >= absVy)
                    ? (minH <= minV * 1.5f)   // moving horizontally — bias toward H
                    : (minH < minV * 0.5f);   // moving vertically  — only H if much shallower

                if (horizBounce) {
                    // Horizontal bounce — only reflect X, never touch driftVy
                    if (absVx >= MIN_BOUNCE) {
                        fs.driftVx = -fs.driftVx * BOUNCE_DAMP;
                        fs.spinSpeed += fs.driftVx * 0.3f;
                    } else {
                        fs.driftVx = 0.0f;
                    }
                    // Push out horizontally
                    if (oLeft < oRight) ft.x = tx - fc.w;
                    else                ft.x = tx + tc.w;
                } else {
                    // Vertical bounce — only allow upward rebound from a bottom hit
                    // (oBottom < oTop means the object's bottom face is inside the tile,
                    // i.e. the object hit the tile from below — only case that goes up).
                    bool hitFromBelow = (oBottom < oTop);
                    if (hitFromBelow) {
                        // Bottom hit: reflect upward
                        if (absVy >= MIN_BOUNCE)
                            fs.driftVy = -fs.driftVy * BOUNCE_DAMP;
                        else
                            fs.driftVy = 0.0f;
                        fs.baseY = ty + tc.h - bob;
                        ft.y = fs.baseY + bob;
                    } else {
                        // Top hit (landed on something): just stop vertical drift, no bounce up
                        fs.driftVy = 0.0f;
                        fs.baseY = ty - fc.h - bob;
                        ft.y = fs.baseY + bob;
                    }
                }
            };

            solidView.each( [&](entt::entity te, const Transform& tt, const Collider& tc) { bounce(te, tt, tc); });
            hazardView.each([&](entt::entity te, const Transform& tt, const Collider& tc) { bounce(te, tt, tc); });
        }

        // ── Player body push ──────────────────────────────────────────────────
        // Fires every frame the player is in contact with the floating entity.
        //
        // IMPORTANT: FloatingSystem runs after CollisionSystem in the update
        // order.  CollisionSystem pushes the player out of any solid (TileTag)
        // floating entity, so by the time we get here there is zero penetration
        // — a pure overlap test always misses when the player is grounded and
        // walking into the entity's side.
        //
        // Fix: expand the AABB by CONTACT_MARGIN px on all sides.  Any player
        // position within that margin counts as contact.  8 px comfortably
        // survives the CollisionSystem push-out at 60 Hz (250 px/s → ~4 px/frame)
        // while being too small to fire from a non-adjacent tile.
        //
        // BODY_PUSH_H: horizontal impulse (px/s) per frame of contact.
        // BODY_PUSH_V: downward impulse (px/s) when player is above entity center.
        // BODY_SPIN:   spin rate (deg/s) from body contact.
        constexpr float BODY_PUSH_H    = 240.0f;
        constexpr float BODY_PUSH_V    = 180.0f;
        constexpr float BODY_SPIN      =  90.0f;
        constexpr float CONTACT_MARGIN =   8.0f;
        // Bottom-hit threshold: how many px the player head can be below
        // the tile's bottom face and still count as a hit. Kept small (6px)
        // so only genuine head contact registers, not just walking nearby.
        constexpr float BOTTOM_MARGIN  =   6.0f;

        // Standard 4-side contact (sides + top)
        bool bodyContact =
            playerX           < ft.x + fc.w  + CONTACT_MARGIN &&
            playerX + playerW > ft.x          - CONTACT_MARGIN &&
            playerY           < ft.y + fc.h  + CONTACT_MARGIN &&
            playerY + playerH > ft.y          - CONTACT_MARGIN;

        // Bottom-hit: player head is at or just below the tile's bottom face.
        // Require the player to be JUMPING (velocity < 0) so a barrel returning
        // at head height while the player is grounded never triggers this.
        bool bottomHit = false;
        {
            float headY   = playerY;
            float tileBot = ft.y + fc.h;
            bool hOverlap = (playerX + playerW > ft.x) && (playerX < ft.x + fc.w);
            bool nearBot  = (headY >= tileBot - BOTTOM_MARGIN) && (headY < tileBot + BOTTOM_MARGIN);
            bool jumping  = (playerVy < -10.0f); // only when player is actually moving upward
            bottomHit = hOverlap && nearBot && jumping;
        }

        if (bodyContact || bottomHit) {
            float pCx = playerX + playerW * 0.5f;
            float fCx = ft.x    + fc.w   * 0.5f;
            float pCy = playerY + playerH * 0.5f;
            float fCy = ft.y    + fc.h   * 0.5f;

            float hDir = (fCx >= pCx) ? 1.0f : -1.0f;
            float vDir = (fCy >= pCy) ? 1.0f : -1.0f; // +1 = entity below player, -1 = above

            // Determine dominant contact axis by overlap amounts
            float overlapH = (playerW * 0.5f + fc.w * 0.5f) - std::abs(pCx - fCx);
            float overlapV = (playerH * 0.5f + fc.h * 0.5f) - std::abs(pCy - fCy);
            bool topBottomHit = overlapV < overlapH || bottomHit;

            if (!fs.wasInContact) {
                // First frame of contact: full impulse for side hits and top hits.
                if (topBottomHit && !bottomHit) {
                    // Top hit (player landing on object) — push down only, no upward
                    float vMag = std::max(120.0f, std::abs(playerVy));
                    fs.driftVy  += vDir * vMag * (BODY_PUSH_V / MAX_FALL_SPEED);
                    if (std::abs(playerVx) > 10.0f) {
                        float hMag = std::abs(playerVx) * 0.6f;
                        fs.driftVx  += (playerVx > 0.0f ? 1.0f : -1.0f) * hMag * (BODY_PUSH_H / PLAYER_SPEED);
                        fs.spinSpeed += (playerVx > 0.0f ? 1.0f : -1.0f) * BODY_SPIN * 0.5f;
                    }
                    fs.spinSpeed += hDir * BODY_SPIN * 0.5f;
                } else if (!topBottomHit) {
                    // Side hit — horizontal push only, never add vertical velocity
                    float hMag = std::max(80.0f, std::abs(playerVx));
                    fs.driftVx  += hDir * hMag * (BODY_PUSH_H / PLAYER_SPEED);
                    fs.spinSpeed += hDir * BODY_SPIN;
                    // No driftVy here — side contacts must not launch the object up or down
                }
                // Re-anchor baseY on first contact so bob origin is correct.
                fs.baseY = ft.y - bob;
            }

            // Bottom hit (player jumping up into the object's underside):
            // CollisionSystem snaps the player back each frame, so wasInContact
            // always resets — the first-frame guard never accumulates enough
            // impulse. Instead apply a continuous upward push each frame while
            // the player is pressing up against the bottom, capped so it doesn't
            // accelerate forever.
            if (bottomHit) {
                constexpr float BOTTOM_HIT_PUSH  = 220.0f; // upward px/s impulse per frame
                constexpr float BOTTOM_HIT_CAP   = 400.0f; // max upward drift speed
                if (fs.driftVy > -BOTTOM_HIT_CAP) {
                    float jMag = std::max(BOTTOM_HIT_PUSH, std::abs(playerVy) * 0.8f);
                    fs.driftVy -= jMag * dt * 8.0f; // integrate each frame of contact
                    fs.driftVy  = std::max(fs.driftVy, -BOTTOM_HIT_CAP);
                }
                fs.spinSpeed += hDir * BODY_SPIN * dt * 6.0f;
                fs.baseY = ft.y - bob; // keep anchor fresh while bouncing
            }

            fs.wasInContact = true;
        } else {
            fs.wasInContact = false;
        }

        // ── Floating-object vs floating-object push ────────────────────────────
        // When two floating objects overlap, impart mutual impulses so they push
        // each other apart — like billiard balls. We only fire on the first frame
        // of overlap (wasFloatContact) to avoid double-accumulating every frame.
        // Each object gets half the relative closing velocity so momentum is
        // roughly conserved without a full rigid-body solver.
        constexpr float FLOAT_PUSH_H   = 200.0f; // base horizontal impulse (px/s)
        constexpr float FLOAT_PUSH_V   = 120.0f; // base vertical   impulse (px/s)
        constexpr float FLOAT_SPIN_HIT =  90.0f; // spin imparted on collision

        for (auto other : floatView) {
            if (other == entity) continue;

            auto& ofs = floatView.get<FloatState>(other);
            auto& oft = floatView.get<Transform>(other);
            const auto& ofc = floatView.get<Collider>(other);

            // AABB overlap test
            bool overlap =
                ft.x          < oft.x + ofc.w &&
                ft.x + fc.w   > oft.x          &&
                ft.y          < oft.y + ofc.h  &&
                ft.y + fc.h   > oft.y;

            if (!overlap) {
                fs.wasFloatContact = false;
                continue;
            }

            // Push-out: resolve on the shallowest penetration axis
            float oLeft   = (ft.x  + fc.w)  - oft.x;
            float oRight  = (oft.x + ofc.w) - ft.x;
            float oTop    = (ft.y  + fc.h)  - oft.y;
            float oBottom = (oft.y + ofc.h) - ft.y;

            float minH = oLeft  < oRight  ? oLeft  : oRight;
            float minV = oTop   < oBottom ? oTop   : oBottom;

            float dx = (ft.x + fc.w * 0.5f) - (oft.x + ofc.w * 0.5f);
            float dy = (ft.y + fc.h * 0.5f) - (oft.y + ofc.h * 0.5f);

            // Separate the two objects equally along the shallowest axis
            if (minH < minV) {
                float half = minH * 0.5f;
                if (dx >= 0.0f) { ft.x  += half; oft.x -= half; }
                else            { ft.x  -= half; oft.x += half; }
            } else {
                float half = minV * 0.5f;
                if (dy >= 0.0f) { ft.y  += half; fs.baseY  += half;
                                  oft.y -= half; ofs.baseY -= half; }
                else            { ft.y  -= half; fs.baseY  -= half;
                                  oft.y += half; ofs.baseY += half; }
            }

            // Impulse — only on first frame of contact to avoid per-frame pumping
            if (!fs.wasFloatContact) {
                float hDir = (dx >= 0.0f) ? 1.0f : -1.0f;
                float vDir = (dy >= 0.0f) ? 1.0f : -1.0f;

                // Scale impulse by incoming closing speed so a fast hit hits hard
                float relVx = fs.driftVx - ofs.driftVx;
                float relVy = fs.driftVy - ofs.driftVy;
                float hImp  = std::max(FLOAT_PUSH_H, std::abs(relVx) * 0.8f);
                float vImp  = std::max(FLOAT_PUSH_V, std::abs(relVy) * 0.8f);

                // Apply to both objects (equal and opposite)
                fs.driftVx  +=  hDir * hImp * 0.5f;
                fs.driftVy  +=  vDir * vImp * 0.5f;
                fs.spinSpeed += hDir * FLOAT_SPIN_HIT;

                ofs.driftVx  -= hDir * hImp * 0.5f;
                ofs.driftVy  -= vDir * vImp * 0.5f;
                ofs.spinSpeed -= hDir * FLOAT_SPIN_HIT;
            }

            fs.wasFloatContact = true;
        }

        // ── Sword slash push ──────────────────────────────────────────────────
        // SLASH_PUSH_FORCE: horizontal impulse in px/s — dominant force.
        //   The enemy is knocked in the direction the player is FACING (slashDir),
        //   not derived from relative positions (which was the old inverted-direction bug).
        // SLASH_LIFT: small upward baseY shift so the enemy briefly lifts off its
        //   feet rather than launching vertically.
        // SLASH_SPIN: spin rate imparted on hit.
        constexpr float SLASH_PUSH_FORCE = 280.0f;
        constexpr float SLASH_SPIN       = 360.0f;

        // ── Float carry: if player is standing on top, move them with the object ──
        // Check if player feet are within a small margin of the object's top.
        // Only carry downward (dyThisFrame > 0) — upward carry is handled by
        // the player's own jump/gravity. Actually carry both directions so
        // the player rides the object up and down.
        if (fs.wasInContact && std::abs(fs.dyThisFrame) > 0.001f) {
            auto pCarry = reg.view<PlayerTag, Transform, Collider, GravityState>();
            pCarry.each([&](Transform& pt, const Collider& pc, GravityState& pg) {
                float pFeet    = pt.y + pc.h;
                float objTop   = ft.y;  // ft.y already updated
                constexpr float CARRY_MARGIN = 12.0f;
                bool onTop = std::abs(pFeet - objTop) < CARRY_MARGIN &&
                             pt.x + pc.w > ft.x && pt.x < ft.x + fc.w;
                if (onTop) {
                    pt.y += fs.dyThisFrame;
                    // If object moved down, player is no longer grounded by collision
                    // — let gravity handle re-grounding naturally next frame.
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
                // No vertical component — slash is a purely horizontal push
                fs.spinSpeed += slashDir * SLASH_SPIN;
            }
        }
    }

    return result;
}
