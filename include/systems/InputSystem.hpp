#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <cmath>

// Gamepad polling — called once per physics tick (not per-event).
// pad1 = gamepad for PlayerIndex 0 (P1).  pad2 = gamepad for PlayerIndex 1 (P2).
// Both are optional; nullptr = that player uses keyboard only.
inline void GamepadPollSystem(entt::registry& reg,
                              SDL_Gamepad* pad1 = nullptr,
                              SDL_Gamepad* pad2 = nullptr) {
    constexpr float DEAD_ZONE       = 0.25f;
    constexpr float CLIMB_DEAD_ZONE = 0.5f;

    const bool* keys = SDL_GetKeyboardState(nullptr);
    bool kbW = keys[SDL_SCANCODE_W];
    bool kbS = keys[SDL_SCANCODE_S];

    // Read P1 pad axes/buttons
    float rawY1 = 0.0f;
    bool btnJump1 = false, btnCrouch1 = false, btnSprint1 = false;
    if (pad1) {
        rawY1      = SDL_GetGamepadAxis(pad1, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        btnJump1   = SDL_GetGamepadButton(pad1, SDL_GAMEPAD_BUTTON_SOUTH)       != 0;
        btnCrouch1 = SDL_GetGamepadButton(pad1, SDL_GAMEPAD_BUTTON_LEFT_STICK)  != 0;
        btnSprint1 = SDL_GetGamepadButton(pad1, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) != 0;
    }

    // Read P2 pad axes/buttons
    float rawY2 = 0.0f;
    bool btnJump2 = false, btnCrouch2 = false, btnSprint2 = false;
    if (pad2) {
        rawY2      = SDL_GetGamepadAxis(pad2, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        btnJump2   = SDL_GetGamepadButton(pad2, SDL_GAMEPAD_BUTTON_SOUTH)       != 0;
        btnCrouch2 = SDL_GetGamepadButton(pad2, SDL_GAMEPAD_BUTTON_LEFT_STICK)  != 0;
        btnSprint2 = SDL_GetGamepadButton(pad2, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER) != 0;
    }

    auto view = reg.view<PlayerTag, GravityState, ClimbState>();
    view.each([&](entt::entity ent, GravityState& g, ClimbState& climb) {
        const auto* pi  = reg.try_get<PlayerIndex>(ent);
        int          idx = pi ? pi->index : 0;

        bool padUp, padDown, btnJump, btnCrouch, btnSprint;
        if (idx == 1) {
            padUp     = rawY2 < -CLIMB_DEAD_ZONE;
            padDown   = rawY2 >  CLIMB_DEAD_ZONE;
            btnJump   = btnJump2;
            btnCrouch = btnCrouch2;
            btnSprint = btnSprint2;
        } else {
            padUp     = rawY1 < -CLIMB_DEAD_ZONE;
            padDown   = rawY1 >  CLIMB_DEAD_ZONE;
            btnJump   = btnJump1;
            btnCrouch = btnCrouch1;
            btnSprint = btnSprint1;
        }

        // P1 (idx == 0) merges keyboard and pad; P2 uses pad only.
        climb.wPressed = (idx == 0) ? (kbW || padUp)   : padUp;
        climb.sPressed = (idx == 0) ? (kbS || padDown)  : padDown;

        if (g.active)
            g.jumpHeld = g.jumpHeld || btnJump;

        g.isCrouching = g.isCrouching || btnCrouch;
        g.sprinting   = g.sprinting   || btnSprint;
    });
}

// Gamepad event handler — discrete actions: attack, dash, jump release.
// pad1/pad2 are used to resolve which player triggered the event via joystick ID.
inline void GamepadInputEvent(entt::registry& reg, SDL_Event& e,
                              SDL_Gamepad* pad1 = nullptr,
                              SDL_Gamepad* pad2 = nullptr) {
    constexpr float TRIGGER_THRESHOLD = 0.3f;
    constexpr float STICK_DEAD_ZONE   = 0.25f;

    // Determine which joystick ID fired the event
    SDL_JoystickID eventId = 0;
    if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
        eventId = e.gaxis.which;
    else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP)
        eventId = e.gbutton.which;

    // Map to a player index by comparing against known pad IDs
    int eventPlayerIdx = 0;
    if (pad2 && eventId != 0) {
        SDL_JoystickID pad2id = SDL_GetGamepadID(pad2);
        if (eventId == pad2id)
            eventPlayerIdx = 1;
    }

    // Helper: does this entity belong to the player who triggered the event?
    auto isThisPlayer = [&](entt::entity ent) -> bool {
        const auto* pi = reg.try_get<PlayerIndex>(ent);
        return (pi ? pi->index : 0) == eventPlayerIdx;
    };

    // Right-trigger attack
    if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION &&
        e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        float val = e.gaxis.value / 32767.0f;
        if (val > TRIGGER_THRESHOLD) {
            auto atk = reg.view<PlayerTag, AttackState>();
            atk.each([&](entt::entity ent, AttackState& a) {
                if (!isThisPlayer(ent)) return;
                if (!a.isAttacking) a.attackPressed = true;
            });
        }
    }

    // West button (X/Square) dash
    if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
        e.gbutton.button == SDL_GAMEPAD_BUTTON_WEST) {
        // Use the pad that matches this event to read stick position
        SDL_Gamepad* thisPad = (eventPlayerIdx == 1 && pad2) ? pad2 : pad1;
        float stickX = 0.0f;
        if (thisPad)
            stickX = SDL_GetGamepadAxis(thisPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;

        auto dashView = reg.view<PlayerTag, DashState, GravityState, ClimbState, Renderable>();
        dashView.each([&](entt::entity ent, DashState& dash, const GravityState& g,
                          const ClimbState& climb, const Renderable& r) {
            if (!isThisPlayer(ent)) return;
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

    // South button release — cancel jump hold
    if (e.type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
        e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
        auto view = reg.view<PlayerTag, GravityState>();
        view.each([&](entt::entity ent, GravityState& g) {
            if (!isThisPlayer(ent)) return;
            g.jumpHeld = false;
        });
    }

    // Left-stick click release — stop crouching
    if (e.type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
        e.gbutton.button == SDL_GAMEPAD_BUTTON_LEFT_STICK) {
        auto view = reg.view<PlayerTag, GravityState>();
        view.each([&](entt::entity ent, GravityState& g) {
            if (!isThisPlayer(ent)) return;
            g.isCrouching = false;
        });
    }
}

// Keyboard event handler — only drives PlayerIndex 0 (P1).
// P2 is gamepad-only so these events are ignored for index-1 entities.
inline void InputSystem(entt::registry& reg, SDL_Event& e) {
    if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F) {
        auto atk = reg.view<PlayerTag, AttackState>();
        atk.each([&](entt::entity ent, AttackState& a) {
            const auto* pi = reg.try_get<PlayerIndex>(ent);
            if (pi && pi->index != 0) return;
            if (!a.isAttacking) a.attackPressed = true;
        });
    }

    // --- Double-tap dash detection (keyboard, P1 only) ---
    if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat) {
        auto dashView = reg.view<PlayerTag, DashState, GravityState, ClimbState>();
        dashView.each([&](entt::entity ent, DashState& dash,
                          const GravityState& g, const ClimbState& climb) {
            const auto* pi = reg.try_get<PlayerIndex>(ent);
            if (pi && pi->index != 0) return;
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
        dashView.each([&](entt::entity ent, DashState& dash) {
            const auto* pi = reg.try_get<PlayerIndex>(ent);
            if (pi && pi->index != 0) return;
            if (e.key.key == SDLK_A) dash.releasedLeft  = true;
            if (e.key.key == SDLK_D) dash.releasedRight = true;
        });
    }

    auto view = reg.view<PlayerTag, Velocity, Renderable, GravityState, ClimbState>();
    view.each([&](entt::entity ent, Velocity& v, Renderable& r,
                  GravityState& g, ClimbState& climb) {
        // Keyboard drives P1 (index 0) only
        const auto* pi = reg.try_get<PlayerIndex>(ent);
        if (pi && pi->index != 0) return;

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
                            r.flipH = true;
                        } else if (g.direction == GravityDir::RIGHT) {
                            r.flipH = false;
                        }
                    }
                    break;
                case SDLK_S: case SDLK_DOWN:
                    if (!g.isCrouching) {
                        if (g.direction == GravityDir::LEFT) {
                            r.flipH = false;
                        } else if (g.direction == GravityDir::RIGHT) {
                            r.flipH = true;
                        }
                    }
                    break;
                case SDLK_LCTRL:
                    g.isCrouching = true;
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

        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_W || e.key.key == SDLK_UP)   climb.wPressed = true;
            if (e.key.key == SDLK_S || e.key.key == SDLK_DOWN) climb.sPressed = true;
        }
        if (e.type == SDL_EVENT_KEY_UP) {
            if (e.key.key == SDLK_W || e.key.key == SDLK_UP)   climb.wPressed = false;
            if (e.key.key == SDLK_S || e.key.key == SDLK_DOWN) climb.sPressed = false;
        }

        if (!g.active) {
            if (!climb.climbing && !climb.atTop) {
                if (e.type == SDL_EVENT_KEY_DOWN) {
                    switch (e.key.key) {
                        case SDLK_W: case SDLK_UP:   v.dy = -v.speed; break;
                        case SDLK_S: case SDLK_DOWN: v.dy =  v.speed; break;
                    }
                }
            }
        } else {
            if (e.type == SDL_EVENT_KEY_DOWN && (e.key.key == SDLK_SPACE || e.key.key == SDLK_UP))
                g.jumpHeld = true;
            if (e.type == SDL_EVENT_KEY_UP   && (e.key.key == SDLK_SPACE || e.key.key == SDLK_UP))
                g.jumpHeld = false;
        }
    });
}
