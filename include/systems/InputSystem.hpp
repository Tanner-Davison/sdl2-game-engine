#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <cmath>

// Gamepad polling — called once per physics tick (not per-event).
// Movement is handled by MovementSystem which reads the stick directly.
inline SDL_Gamepad* GetFirstGamepad() {
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (!ids || count == 0) { SDL_free(ids); return nullptr; }
    SDL_Gamepad* pad = SDL_GetGamepadFromID(ids[0]);
    SDL_free(ids);
    return pad;
}

inline void GamepadPollSystem(entt::registry& reg) {
    SDL_Gamepad* pad = GetFirstGamepad();
    if (!pad) return;

    constexpr float DEAD_ZONE       = 0.25f;
    constexpr float CLIMB_DEAD_ZONE = 0.5f;

    float rawY = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;

    bool btnJump   = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH);
    bool btnCrouch = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_STICK);
    bool btnSprint = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);

    const bool* keys = SDL_GetKeyboardState(nullptr);
    bool kbW = keys[SDL_SCANCODE_W];
    bool kbS = keys[SDL_SCANCODE_S];

    auto view = reg.view<PlayerTag, GravityState, ClimbState>();
    view.each([&](GravityState& g, ClimbState& climb) {
        bool padUp   = rawY < -CLIMB_DEAD_ZONE;
        bool padDown = rawY >  CLIMB_DEAD_ZONE;
        climb.wPressed = kbW || padUp;
        climb.sPressed = kbS || padDown;

        if (g.active)
            g.jumpHeld = g.jumpHeld || btnJump;

        g.isCrouching = g.isCrouching || btnCrouch;
        g.sprinting   = g.sprinting   || btnSprint;
    });
}

// Gamepad event handler — discrete actions: attack, dash, ladder climb overrides.
inline void GamepadInputEvent(entt::registry& reg, SDL_Event& e) {
    constexpr float TRIGGER_THRESHOLD = 0.3f;
    constexpr float STICK_DEAD_ZONE   = 0.25f;

    if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION &&
        e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        float val = e.gaxis.value / 32767.0f;
        if (val > TRIGGER_THRESHOLD) {
            auto atk = reg.view<PlayerTag, AttackState>();
            atk.each([](AttackState& a) {
                if (!a.isAttacking) a.attackPressed = true;
            });
        }
    }

    if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
        e.gbutton.button == SDL_GAMEPAD_BUTTON_WEST) {
        SDL_Gamepad* pad = GetFirstGamepad();
        float stickX = 0.0f;
        if (pad)
            stickX = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;

        auto dashView = reg.view<PlayerTag, DashState, GravityState, ClimbState, Renderable>();
        dashView.each([&](DashState& dash, const GravityState& g,
                          const ClimbState& climb, const Renderable& r) {
            if (dash.active || dash.cooldown > 0.0f) return;
            if (climb.climbing || climb.atTop) return;
            if (g.isCrouching) return;
            float dir = 0.0f;
            if (std::abs(stickX) > STICK_DEAD_ZONE)
                dir = (stickX > 0.0f) ? 1.0f : -1.0f;
            else
                dir = r.flipH ? -1.0f : 1.0f;
            dash.active    = true;
            dash.remaining = DASH_DURATION;
            dash.cooldown  = DASH_COOLDOWN;
            dash.direction = dir;
        });
    }

    if (e.type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
        e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
        auto view = reg.view<PlayerTag, GravityState>();
        view.each([](GravityState& g) { g.jumpHeld = false; });
    }

    if (e.type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
        e.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK) {
        auto view = reg.view<PlayerTag, GravityState>();
        view.each([](GravityState& g) { g.isCrouching = false; });
    }
}

inline void InputSystem(entt::registry& reg, SDL_Event& e) {
    if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F) {
        auto atk = reg.view<PlayerTag, AttackState>();
        atk.each([](AttackState& a) {
            if (!a.isAttacking) a.attackPressed = true;
        });
    }

    // --- Double-tap dash detection ---
    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
        auto dashView = reg.view<PlayerTag, DashState, GravityState, ClimbState>();
        dashView.each([&e](DashState& dash, const GravityState& g, const ClimbState& climb) {
            if (dash.active || dash.cooldown > 0.0f) return;
            if (climb.climbing || climb.atTop) return;
            if (g.isCrouching) return;

            bool isLeft  = (e.key.key == SDLK_A);
            bool isRight = (e.key.key == SDLK_D);

            if (isLeft) {
                if (dash.tapTimerLeft > 0.0f && dash.releasedLeft) {
                    dash.active    = true;
                    dash.remaining = DASH_DURATION;
                    dash.cooldown  = DASH_COOLDOWN;
                    dash.direction = -1.0f;
                    dash.tapTimerLeft = 0.0f;
                } else {
                    dash.tapTimerLeft  = DASH_TAP_WINDOW;
                    dash.releasedLeft  = false;
                }
                dash.tapTimerRight = 0.0f;
            }
            if (isRight) {
                if (dash.tapTimerRight > 0.0f && dash.releasedRight) {
                    dash.active    = true;
                    dash.remaining = DASH_DURATION;
                    dash.cooldown  = DASH_COOLDOWN;
                    dash.direction = 1.0f;
                    dash.tapTimerRight = 0.0f;
                } else {
                    dash.tapTimerRight = DASH_TAP_WINDOW;
                    dash.releasedRight = false;
                }
                dash.tapTimerLeft = 0.0f;
            }
        });
    }
    if (e.type == SDL_EVENT_KEY_UP) {
        auto dashView = reg.view<PlayerTag, DashState>();
        dashView.each([&e](DashState& dash) {
            if (e.key.key == SDLK_A)
                dash.releasedLeft = true;
            if (e.key.key == SDLK_D)
                dash.releasedRight = true;
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
                            r.flipH = true;   // 90CW: face up the left wall
                        } else if (g.direction == GravityDir::RIGHT) {
                            r.flipH = false;  // 90CCW: face up the right wall
                        }
                    }
                    break;
                case SDLK_S: case SDLK_DOWN:
                    if (!g.isCrouching) {
                        if (g.direction == GravityDir::LEFT) {
                            r.flipH = false;  // 90CW: face down the left wall
                        } else if (g.direction == GravityDir::RIGHT) {
                            r.flipH = true;   // 90CCW: face down the right wall
                        }
                    }
                    break;
                case SDLK_LCTRL:
                    g.isCrouching = true;
                    // Don't zero velocity — let MovementSystem apply friction
                    // so the character slides to a gradual stop.
                    break;
                case SDLK_LSHIFT:
                    g.sprinting = true;
                    break;
            }
        }

        if (e.type == SDL_EVENT_KEY_UP) {
            if (e.key.key == SDLK_LCTRL)  g.isCrouching = false;
            if (e.key.key == SDLK_LSHIFT) g.sprinting   = false;
        }

        // Event-driven W/S for ladder: tap only moves for the frames the key is down.
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
            // Track spacebar held — actual jump fires each frame in MovementSystem
            // after CollisionSystem has settled isGrounded.
            if (e.type == SDL_EVENT_KEY_DOWN && (e.key.key == SDLK_SPACE || e.key.key == SDLK_UP))
                g.jumpHeld = true;
            if (e.type == SDL_EVENT_KEY_UP && (e.key.key == SDLK_SPACE || e.key.key == SDLK_UP))
                g.jumpHeld = false;
        }
    });
}
