#pragma once
#include <Components.hpp>
#include <GameConfig.hpp>
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

inline void FloatingSystem(entt::registry& reg, float dt) {

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
    // FloatTag is an empty struct — passing it through view.each() triggers a
    // libc++ std::apply / deleted-function error on this compiler.
    // Iterate by FloatState+Transform+Collider and gate on all_of<FloatTag>.
    auto floatView = reg.view<FloatState, Transform, Collider>();
    for (auto entity : floatView) {
        if (!reg.all_of<FloatTag>(entity)) continue;

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
        fs.spinAngle += fs.spinSpeed * dt;
        if (fs.spinAngle >= 360.0f) fs.spinAngle -= 360.0f;
        if (fs.spinAngle <    0.0f) fs.spinAngle += 360.0f;

        // ── Apply position ────────────────────────────────────────────────────
        ft.x += fs.driftVx * dt;
        ft.y  = fs.baseY + bob + fs.driftVy * dt;

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

        bool bodyContact =
            playerX           < ft.x + fc.w  + CONTACT_MARGIN &&
            playerX + playerW > ft.x          - CONTACT_MARGIN &&
            playerY           < ft.y + fc.h  + CONTACT_MARGIN &&
            playerY + playerH > ft.y          - CONTACT_MARGIN;

        if (bodyContact) {
            float pCx = playerX + playerW * 0.5f;
            float fCx = ft.x    + fc.w   * 0.5f;
            float pCy = playerY + playerH * 0.5f;
            float fCy = ft.y    + fc.h   * 0.5f;

            // Horizontal push: entity flies away from the player's center.
            float hDir = (fCx >= pCx) ? 1.0f : -1.0f;
            // Scale by player's horizontal speed so walking fast hits harder;
            // minimum floor so a stationary player still registers contact.
            float hMag = std::max(80.0f, std::abs(playerVx));
            fs.driftVx  += hDir * hMag * (BODY_PUSH_H / PLAYER_SPEED);
            fs.spinSpeed += hDir * BODY_SPIN;

            // Vertical push: only when player is above entity center (landing /
            // standing on top).  Skip for pure side contact so a grounded
            // walk-in doesn't also smash the entity downward.
            if (pCy < fCy) {
                float vMag = std::max(60.0f, std::abs(playerVy));
                fs.driftVy += vMag * (BODY_PUSH_V / MAX_FALL_SPEED);
            }

            // Re-anchor baseY so the bob origin tracks the pushed position.
            fs.baseY = ft.y - bob - fs.driftVy * dt;
        }

        // ── Sword slash push ──────────────────────────────────────────────────
        // SLASH_PUSH_FORCE: horizontal impulse in px/s — dominant force.
        //   The enemy is knocked in the direction the player is FACING (slashDir),
        //   not derived from relative positions (which was the old inverted-direction bug).
        // SLASH_LIFT: small upward baseY shift so the enemy briefly lifts off its
        //   feet rather than launching vertically.
        // SLASH_SPIN: spin rate imparted on hit.
        constexpr float SLASH_PUSH_FORCE = 280.0f; // reduced from 420 — was travelling too far
        constexpr float SLASH_LIFT       =   6.0f; // px — barely lifts feet off ground
        constexpr float SLASH_SPIN       = 360.0f;

        if (playerSlashing) {
            bool swordOverlap =
                swordX          < ft.x + fc.w &&
                swordX + swordW > ft.x         &&
                swordY          < ft.y + fc.h  &&
                swordY + swordH > ft.y;

            if (swordOverlap) {
                // Use slashDir (player facing) as the authoritative knockback direction.
                // The old bug computed direction from sword-center vs enemy-center, which
                // is inverted when slashing left: the sword is to the left of the enemy,
                // so enemy-center > sword-center → pushDir=+1 (right) — wrong.
                // slashDir is always correct: it's exactly which way the player is looking.
                fs.driftVx   += slashDir * SLASH_PUSH_FORCE;
                fs.baseY      = (ft.y - bob) - SLASH_LIFT; // anchor with subtle lift
                fs.driftVy    = 0.0f;                       // suppress vertical drift
                fs.spinSpeed += slashDir * SLASH_SPIN;
            }
        }
    }
}
