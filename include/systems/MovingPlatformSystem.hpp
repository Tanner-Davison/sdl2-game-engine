#pragma once
#include <Components.hpp>
#include <cmath>
#include <entt/entt.hpp>
#include <unordered_map>

// ─────────────────────────────────────────────────────────────────────────────
// Moving Platform — two-phase, called from GameScene::Update:
//
//   MovingPlatformTick(reg, dt)   — BEFORE CollisionSystem
//     Moves tiles, records vx. Also detects which platform tile the player
//     is currently standing on and stores it in mps.playerOnTop, so Carry
//     doesn't need fragile threshold checks.
//
//   MovingPlatformCarry(reg)      — AFTER CollisionSystem
//     Applies vx to the player if they were detected as standing on the
//     platform before this frame's movement.
//
// Detection logic (in Tick, using LAST FRAME's positions):
//   Before we move the tile this frame, the tile is at its old position and
//   the player has just been floor-snapped by last frame's CollisionSystem.
//   So pt.y + pc.h == old_tt.y exactly — a perfect time to detect contact.
//   We do the check here, store a bool, and Carry blindly applies vx if set.
// ─────────────────────────────────────────────────────────────────────────────

inline void MovingPlatformTick(entt::registry& reg, float dt) {
    std::unordered_map<int, float> groupPhase;
    auto mpView     = reg.view<Transform, MovingPlatformState>();
    auto playerView = reg.view<PlayerTag, Transform, Collider, GravityState>();

    // ── Detect which platform the player is standing on (using last frame pos) ─
    // At this point: tile is at old position, player was floor-snapped last frame.
    // pt.y + pc.h == tt.y exactly (or very close) — most reliable moment to check.
    for (auto platEnt : mpView) {
        if (!reg.all_of<MovingPlatformTag>(platEnt)) continue;
        auto& mps = mpView.get<MovingPlatformState>(platEnt);
        mps.playerOnTop = false;  // reset each frame

        const auto& tt = mpView.get<Transform>(platEnt);
        int tcW = 48;
        if (reg.all_of<Collider>(platEnt))
            tcW = reg.get<Collider>(platEnt).w;

        for (auto playerEnt : playerView) {
            const auto& gs = playerView.get<GravityState>(playerEnt);
            const auto& pt = playerView.get<Transform>(playerEnt);
            const auto& pc = playerView.get<Collider>(playerEnt);

            // Horizontal overlap check
            bool overlapX = (pt.x + pc.w > tt.x) && (pt.x < tt.x + tcW);
            if (!overlapX) continue;

            // Grounded check: use a generous threshold so we catch the landing
            // frame (when isGrounded is still false but feet are right at the
            // tile top — CollisionSystem hasn't snapped them yet this frame).
            constexpr float THRESH = 6.0f;
            float feetY   = pt.y + pc.h;
            bool  onTop   = (feetY >= tt.y - THRESH) && (feetY <= tt.y + THRESH);
            // Also accept the frame just before landing: player falling, feet
            // within one frame of travel above the tile top.
            bool  nearTop = (!gs.isGrounded && gs.velocity > 0.0f
                             && feetY < tt.y && (tt.y - feetY) < gs.velocity * 0.05f + 8.0f);

            if (onTop || nearTop) {
                mps.playerOnTop = true;
                break;
            }
        }
    }

    // ── Advance phase for non-loop platforms & collect group phases ────────────
    for (auto entity : mpView) {
        if (!reg.all_of<MovingPlatformTag>(entity)) continue;
        auto& mps = mpView.get<MovingPlatformState>(entity);
        if (mps.loop) continue; // loop platforms advance phase in the move block
        if (mps.trigger && !mps.triggered) continue; // waiting for player
        float omega = (mps.range > 0.0f) ? (mps.speed / mps.range) : 1.0f;
        mps.phase  += omega * dt;
        if (mps.phase > 6.28318f) mps.phase -= 6.28318f;
        if (mps.groupId != 0)
            groupPhase[mps.groupId] = mps.phase;
    }

    // ── Propagate trigger state across group members ──────────────────────────
    // If ANY tile in a group is triggered (or has playerOnTop), mark the whole
    // group triggered so multi-tile platforms all start at the same time.
    {
        std::unordered_map<int, bool> groupTriggered;
        std::unordered_map<int, bool> groupPlayerOnTop;
        for (auto entity : mpView) {
            if (!reg.all_of<MovingPlatformTag>(entity)) continue;
            const auto& mps = mpView.get<MovingPlatformState>(entity);
            if (mps.groupId == 0) continue;
            if (mps.triggered)    groupTriggered[mps.groupId]    = true;
            if (mps.playerOnTop)  groupPlayerOnTop[mps.groupId]  = true;
        }
        for (auto entity : mpView) {
            if (!reg.all_of<MovingPlatformTag>(entity)) continue;
            auto& mps = mpView.get<MovingPlatformState>(entity);
            if (mps.groupId == 0) continue;
            if (groupTriggered.count(mps.groupId))   mps.triggered   = true;
            if (groupPlayerOnTop.count(mps.groupId)) mps.playerOnTop = true;
        }
    }

    // ── Sync group phases, move tile, record vx ──────────────────────────────
    for (auto entity : mpView) {
        if (!reg.all_of<MovingPlatformTag>(entity)) continue;

        auto& t   = mpView.get<Transform>(entity);
        auto& mps = mpView.get<MovingPlatformState>(entity);

        // Sync sine-oscillator phase within group
        if (!mps.loop && mps.groupId != 0) {
            auto it = groupPhase.find(mps.groupId);
            if (it != groupPhase.end())
                mps.phase = it->second;
        }

        // Trigger: don't move until player has landed on it
        if (mps.trigger && !mps.triggered) {
            mps.vx = 0.0f;
            mps.vy = 0.0f;
            if (mps.playerOnTop) mps.triggered = true;
            if (!mps.triggered) continue;
        }

        if (mps.loop) {
            // Ping-pong: travel right to originX+range, reverse, travel back, repeat.
            mps.phase += mps.speed * mps.loopDir * dt;
            if (mps.phase >= mps.range) {
                mps.phase   = mps.range;  // clamp, don't overshoot
                mps.loopDir = -1;         // reverse: head back left
            } else if (mps.phase <= 0.0f) {
                mps.phase   = 0.0f;       // clamp at origin
                mps.loopDir = 1;          // reverse: head right again
            }
            float newX = mps.originX + mps.phase;
            mps.vx = newX - t.x;
            mps.vy = 0.0f;
            t.x    = newX;
        } else {
            float omega  = (mps.range > 0.0f) ? (mps.speed / mps.range) : 1.0f;
            float offset = mps.range * std::sin(mps.phase);

            if (mps.horiz) {
                float newX = mps.originX + offset;
                mps.vx = newX - t.x;
                mps.vy = 0.0f;
                t.x    = newX;
            } else {
                float newY = mps.originY + offset;
                mps.vx = 0.0f;
                mps.vy = newY - t.y;
                t.y    = newY;
            }
        }
    }
}

inline void MovingPlatformCarry(entt::registry& reg) {
    auto mpView     = reg.view<Transform, MovingPlatformState>();
    auto playerView = reg.view<PlayerTag, Transform>();

    // Collect the carry to apply.
    // For multi-tile platforms, pick the tile with the greatest absolute motion
    // rather than whichever happens to be first in EnTT's sparse_set.
    // This avoids a rare bug where a stationary tile (vx==0 on the lag frame)
    // sorts before the actually-moving tile and silently drops carry.
    float carryVx = 0.0f;
    float carryVy = 0.0f;
    bool  carried = false;
    float bestMotion = 0.0f;
    for (auto platEnt : mpView) {
        if (!reg.all_of<MovingPlatformTag>(platEnt)) continue;
        const auto& mps = mpView.get<MovingPlatformState>(platEnt);
        if (!mps.playerOnTop) continue;
        float motion = mps.horiz ? std::abs(mps.vx) : std::abs(mps.vy);
        // Always accept triggered platforms even on vx==0 start frame.
        bool forceAccept = (mps.trigger && mps.triggered && !carried);
        if (motion > bestMotion || forceAccept) {
            bestMotion = motion;
            carryVx = mps.vx;
            carryVy = mps.vy;
            carried = true;
        }
    }

    if (!carried) return;

    // Snap player feet to tile top exactly (both up and down) so
    // CollisionSystem has nothing to correct next frame — eliminates the
    // 1-frame oscillation where Carry and CollisionSystem fight each other.
    float snapTileY = -1.0f;
    if (carryVy > 0.5f) {
        for (auto platEnt : mpView) {
            if (!reg.all_of<MovingPlatformTag>(platEnt)) continue;
            const auto& mps = mpView.get<MovingPlatformState>(platEnt);
            if (!mps.playerOnTop) continue;
            const auto& tt = mpView.get<Transform>(platEnt);
            snapTileY = tt.y;
            break;
        }
    }

    auto pView = reg.view<PlayerTag, Transform, GravityState, Collider>();
    pView.each([&](Transform& pt, GravityState& pg, const Collider& pc) {
        pt.x += carryVx;
        if (carryVy > 0.5f) {
            // Platform moving down: snap player feet to tile top exactly so
            // CollisionSystem has nothing to correct next frame.
            if (snapTileY >= 0.0f)
                pt.y = snapTileY - pc.h;
            else
                pt.y += carryVy;
            pg.isGrounded = true;
            pg.velocity   = 0.0f;
        } else if (carryVy < -0.5f) {
            // Platform moving up: just offset, don't force grounded/velocity.
            // Snapping here kills jump velocity the same frame it's set.
            pt.y += carryVy;
        } else {
            pt.y += carryVy;
        }
    });
}

// Legacy single-call wrapper
inline void MovingPlatformSystem(entt::registry& reg, float dt) {
    MovingPlatformTick(reg, dt);
}
