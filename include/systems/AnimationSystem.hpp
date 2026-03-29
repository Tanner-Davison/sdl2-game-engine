#pragma once
#include <Components.hpp>
#include <entt/entt.hpp>

inline void AnimationSystem(entt::registry& reg, float dt) {
    // Exclude animated tiles — GameScene::Update owns their frame timing.
    auto view = reg.view<AnimationState>(entt::exclude<TileAnimTag>);
    view.each([dt](AnimationState& anim) {
        if (!anim.looping && anim.currentFrame == anim.totalFrames - 1)
            return;
        anim.timer += dt;
        float interval = 1.0f / anim.fps;
        while (anim.timer >= interval) {
            anim.currentFrame = (anim.currentFrame + 1) % anim.totalFrames;
            anim.timer -= interval;
        }
    });
}
