#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <entt/entt.hpp>

inline void InputSystem(entt::registry& reg, SDL_Event& e) {
    // F key — set attackPressed on the player's AttackState
    if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F) {
        auto atk = reg.view<PlayerTag, AttackState>();
        atk.each([](AttackState& a) {
            if (!a.isAttacking) a.attackPressed = true;
        });
    }

    auto view = reg.view<PlayerTag, Velocity, Renderable, GravityState, ClimbState>();
    view.each([&e](Velocity& v, Renderable& r, GravityState& g, ClimbState& climb) {
        // On the top wall the sprite is rotated 180 so left/right facing is inverted
        bool invertFlip = g.active && g.direction == GravityDir::UP;
        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_A: case SDLK_LEFT:
                    if (!g.isCrouching) {
                        v.dx    = -v.speed;
                        // Only set flip for horizontal movement on horizontal-gravity walls
                        if (g.direction == GravityDir::DOWN || g.direction == GravityDir::UP)
                            r.flipH = !invertFlip;
                    }
                    break;
                case SDLK_D: case SDLK_RIGHT:
                    if (!g.isCrouching) {
                        v.dx    = v.speed;
                        if (g.direction == GravityDir::DOWN || g.direction == GravityDir::UP)
                            r.flipH = invertFlip;
                    }
                    break;
                case SDLK_W: case SDLK_UP:
                    if (!g.isCrouching) {
                        if (g.direction == GravityDir::LEFT) {
                            // 90CW: flipH=true (face left) -> face up the left wall
                            r.flipH = true;
                        } else if (g.direction == GravityDir::RIGHT) {
                            // 90CCW: flipH=false (face right) -> face up the right wall
                            r.flipH = false;
                        }
                    }
                    break;
                case SDLK_S: case SDLK_DOWN:
                    if (!g.isCrouching) {
                        if (g.direction == GravityDir::LEFT) {
                            // 90CW: flipH=false (face right) -> face down the left wall
                            r.flipH = false;
                        } else if (g.direction == GravityDir::RIGHT) {
                            // 90CCW: flipH=true (face left) -> face down the right wall
                            r.flipH = true;
                        }
                    }
                    break;
                case SDLK_LCTRL:
                    g.isCrouching = true;
                    break;
            }
        }

        if (e.type == SDL_EVENT_KEY_UP && e.key.key == SDLK_LCTRL)
            g.isCrouching = false;

        // ── Event-driven W/S tracking for ladder climbing ─────────────────────────
        // These flags are set/cleared by events so LadderSystem never polls the
        // keyboard state — a tap only moves for exactly the frames the key is down.
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_W || e.key.key == SDLK_UP)   climb.wPressed = true;
            if (e.key.key == SDLK_S || e.key.key == SDLK_DOWN) climb.sPressed = true;
        }
        if (e.type == SDL_EVENT_KEY_UP) {
            if (e.key.key == SDLK_W || e.key.key == SDLK_UP)   climb.wPressed = false;
            if (e.key.key == SDLK_S || e.key.key == SDLK_DOWN) climb.sPressed = false;
        }

        if (!g.active) {
            // Only drive v.dy from input in free-float mode (wall-run gravity off).
            // During ladder climbing LadderSystem owns all vertical movement.
            if (!climb.climbing && !climb.atTop) {
                if (e.type == SDL_EVENT_KEY_DOWN) {
                    switch (e.key.key) {
                        case SDLK_W: case SDLK_UP:   v.dy = -v.speed; break;
                        case SDLK_S: case SDLK_DOWN: v.dy =  v.speed; break;
                    }
                }
            }
        } else {
            // Track spacebar held state via events — actual jump fires each frame
            // in MovementSystem after CollisionSystem has settled isGrounded.
            if (e.type == SDL_EVENT_KEY_DOWN && (e.key.key == SDLK_SPACE || e.key.key == SDLK_UP))
                g.jumpHeld = true;
            if (e.type == SDL_EVENT_KEY_UP && (e.key.key == SDLK_SPACE || e.key.key == SDLK_UP))
                g.jumpHeld = false;
        }
    });
}
