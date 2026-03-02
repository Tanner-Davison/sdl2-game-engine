#pragma once
#include <Components.hpp>
#include <entt/entt.hpp>

inline void CenterPullSystem(entt::registry& reg, float dt, int windowW, int windowH) {
    auto view = reg.view<Transform, Velocity, GravityState, ClimbState, PlayerTag>();
    view.each([dt, windowW, windowH](Transform& t, Velocity& v, GravityState& g, const ClimbState& climb) {
        if (g.active)
            return; // only runs in free mode (gravity off)
        if (climb.climbing || climb.atTop)
            return; // ladder system owns position — don't fight it

        float centerX = windowW / 2.0f;
        float centerY = windowH / 2.0f;

        float dx   = centerX - t.x;
        float dy   = centerY - t.y;
        float dist = std::sqrt(dx * dx + dy * dy);

        if (dist > 5.0f) {
            constexpr float pullSpeed = 200.0f;
            float           norm      = pullSpeed / dist;
            t.x += dx * norm * dt;
            t.y += dy * norm * dt;
        }
    });
}
