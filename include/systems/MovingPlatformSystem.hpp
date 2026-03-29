#pragma once
#include <Components.hpp>
#include <cmath>
#include <entt/entt.hpp>
#include <unordered_map>

// Two-phase moving platform, called from GameScene::Update:
//   MovingPlatformTick(reg, dt)  — BEFORE CollisionSystem
//   MovingPlatformCarry(reg)     — AFTER  CollisionSystem
//
// Detection runs in Tick using last frame's positions: the tile hasn't moved
// yet and the player was floor-snapped by last frame's CollisionSystem, so
// pt.y + pc.h == old_tt.y exactly — most reliable moment for contact check.

inline void MovingPlatformTick(entt::registry& reg, float dt) {
    std::unordered_map<int, float> groupPhase;
    auto mpView     = reg.view<Transform, MovingPlatformState>();
    auto playerView = reg.view<PlayerTag, Transform, Collider, GravityState>();

    for (auto platEnt : mpView) {
        if (!reg.all_of<MovingPlatformTag>(platEnt)) continue;
        auto& mps = mpView.get<MovingPlatformState>(platEnt);
        mps.playerOnTop = false;

        const auto& tt = mpView.get<Transform>(platEnt);
        int tcW = 48;
        if (reg.all_of<Collider>(platEnt))
            tcW = reg.get<Collider>(platEnt).w;

        for (auto playerEnt : playerView) {
            const auto& gs = playerView.get<GravityState>(playerEnt);
            const auto& pt = playerView.get<Transform>(playerEnt);
            const auto& pc = playerView.get<Collider>(playerEnt);

            bool overlapX = (pt.x + pc.w > tt.x) && (pt.x < tt.x + tcW);
            if (!overlapX) continue;

            // Generous threshold catches the landing frame before CollisionSystem snaps.
            constexpr float THRESH = 6.0f;
            float feetY   = pt.y + pc.h;
            bool  onTop   = (feetY >= tt.y - THRESH) && (feetY <= tt.y + THRESH);
            bool  nearTop = (!gs.isGrounded && gs.velocity > 0.0f
                             && feetY < tt.y && (tt.y - feetY) < gs.velocity * 0.05f + 8.0f);

            if (onTop || nearTop) {
                mps.playerOnTop = true;
                break;
            }
        }
    }

    for (auto entity : mpView) {
        if (!reg.all_of<MovingPlatformTag>(entity)) continue;
        auto& mps = mpView.get<MovingPlatformState>(entity);
        if (mps.loop) continue;
        if (mps.trigger && !mps.triggered) continue;
        float omega = (mps.range > 0.0f) ? (mps.speed / mps.range) : 1.0f;
        mps.phase  += omega * dt;
        if (mps.phase > 6.28318f) mps.phase -= 6.28318f;
        if (mps.groupId != 0)
            groupPhase[mps.groupId] = mps.phase;
    }

    // Propagate trigger state: if ANY tile in a group is triggered (or has
    // playerOnTop), mark the whole group so multi-tile platforms start together.
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

    for (auto entity : mpView) {
        if (!reg.all_of<MovingPlatformTag>(entity)) continue;

        auto& t   = mpView.get<Transform>(entity);
        auto& mps = mpView.get<MovingPlatformState>(entity);

        if (!mps.loop && mps.groupId != 0) {
            auto it = groupPhase.find(mps.groupId);
            if (it != groupPhase.end())
                mps.phase = it->second;
        }

        if (mps.trigger && !mps.triggered) {
            mps.vx = 0.0f;
            mps.vy = 0.0f;
            if (mps.playerOnTop) mps.triggered = true;
            if (!mps.triggered) continue;
        }

        if (mps.loop) {
            // Ping-pong: travel to originX+range, reverse, repeat.
            mps.phase += mps.speed * mps.loopDir * dt;
            if (mps.phase >= mps.range) {
                mps.phase   = mps.range;
                mps.loopDir = -1;
            } else if (mps.phase <= 0.0f) {
                mps.phase   = 0.0f;
                mps.loopDir = 1;
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

    // For multi-tile platforms, pick the tile with greatest absolute motion
    // to avoid a bug where a stationary tile (vx==0 on the lag frame) sorts
    // first in EnTT's sparse_set and silently drops carry.
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

    // Snap player feet to tile top exactly so CollisionSystem has nothing to
    // correct next frame — eliminates the 1-frame oscillation where Carry
    // and CollisionSystem fight each other.
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
            // Platform moving down: snap feet to tile top.
            if (snapTileY >= 0.0f)
                pt.y = snapTileY - pc.h;
            else
                pt.y += carryVy;
            pg.isGrounded = true;
            pg.velocity   = 0.0f;
        } else if (carryVy < -0.5f) {
            // Platform moving up: just offset. Snapping here kills jump velocity.
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
