#pragma once
#include <Components.hpp>
#include <entt/entt.hpp>

/**
 * @brief Advances frame counters for all animated entities each tick.
 *
 * Accumulates @p dt into each entity's @ref AnimationState::timer and steps
 * @ref AnimationState::currentFrame forward whenever the accumulated time
 * exceeds one frame interval (1.0 / fps). Multiple frames may advance in a
 * single call if dt is large, keeping animations time-accurate rather than
 * frame-rate dependent.
 *
 * Non-looping animations are frozen on their final frame until an external
 * system (e.g. @ref PlayerStateSystem) transitions them to a new state.
 *
 * @param reg  The EnTT registry. Queries all entities with @ref AnimationState.
 * @param dt   Delta time in seconds since the last frame.
 */
inline void AnimationSystem(entt::registry& reg, float dt) {
    // Exclude animated tile entities — GameScene::Update owns their frame timing.
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
