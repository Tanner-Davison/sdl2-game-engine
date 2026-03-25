#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <cmath>
#include <entt/entt.hpp>

inline void MovementSystem(entt::registry& reg, float dt, int windowW, float levelW = 0.0f) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    // ── Gamepad analog stick ─────────────────────────────────────────────
    constexpr float PAD_DEAD_ZONE = 0.25f;
    float padAxisX = 0.0f, padAxisY = 0.0f;
    {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        if (ids && count > 0) {
            SDL_Gamepad* pad = SDL_GetGamepadFromID(ids[0]);
            if (pad) {
                float rx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
                float ry = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
                if (std::abs(rx) > PAD_DEAD_ZONE) padAxisX = rx;
                if (std::abs(ry) > PAD_DEAD_ZONE) padAxisY = ry;
            }
        }
        SDL_free(ids);
    }

    // ── Dash tick ────────────────────────────────────────────────────────
    {
        auto dashView = reg.view<PlayerTag, Transform, DashState>();
        dashView.each([dt](Transform& t, DashState& dash) {
            if (dash.tapTimerLeft  > 0.0f) dash.tapTimerLeft  -= dt;
            if (dash.tapTimerRight > 0.0f) dash.tapTimerRight -= dt;
            if (dash.cooldown      > 0.0f) dash.cooldown      -= dt;

            if (dash.active) {
                t.x += dash.direction * DASH_SPEED * dt;
                dash.remaining -= dt;
                if (dash.remaining <= 0.0f) {
                    dash.active    = false;
                    dash.remaining = 0.0f;
                }
            }
        });
    }

    auto playerView = reg.view<Transform, Velocity, GravityState, PlayerTag, ClimbState, Renderable>();
    playerView.each([&reg, dt, keys, padAxisX, padAxisY](entt::entity ent, Transform& t, Velocity& v, GravityState& g, const ClimbState& climb, Renderable& r) {
        // While dashing, skip normal horizontal input — dash tick above owns t.x.
        // Gravity still applies so the player follows arcs / stays grounded.
        if (auto* dash = reg.try_get<DashState>(ent); dash && dash->active) {
            if (!g.isGrounded) {
                g.velocity = std::min(g.velocity + GRAVITY_FORCE * dt, MAX_FALL_SPEED);
            } else if (g.direction == GravityDir::DOWN) {
                g.velocity = std::min(g.velocity + SLOPE_STICK_VELOCITY, MAX_FALL_SPEED);
            }
            switch (g.direction) {
                case GravityDir::DOWN:  t.y += g.velocity * dt; break;
                case GravityDir::UP:    t.y -= g.velocity * dt; break;
                case GravityDir::LEFT:  t.x -= g.velocity * dt; break;
                case GravityDir::RIGHT: t.x += g.velocity * dt; break;
            }
            g.timer += dt;
            return;
        }

        constexpr float friction = 3.0f;

        if (!g.active) {
            if (climb.climbing || climb.atTop) {
                bool leftHeld  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT] || padAxisX < 0.0f;
                bool rightHeld = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT] || padAxisX > 0.0f;
                if (leftHeld && !rightHeld) {
                    v.dx = -CLIMB_STRAFE_SPEED;
                } else if (rightHeld && !leftHeld) {
                    v.dx =  CLIMB_STRAFE_SPEED;
                } else {
                    v.dx -= v.dx * friction * dt;
                    if (std::abs(v.dx) < 0.5f) v.dx = 0.0f;
                }
                t.x += v.dx * dt;
                return;
            }
            bool kbMoving = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_S] ||
                            keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D] ||
                            keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_DOWN] ||
                            keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_RIGHT];
            bool padMoving = std::abs(padAxisX) > 0.0f || std::abs(padAxisY) > 0.0f;
            if (padMoving) {
                v.dx = padAxisX * v.speed;
                v.dy = padAxisY * v.speed;
            } else if (!kbMoving) {
                v.dx -= v.dx * friction * dt;
                v.dy -= v.dy * friction * dt;
                if (std::abs(v.dx) < 0.5f)
                    v.dx = 0.0f;
                if (std::abs(v.dy) < 0.5f)
                    v.dy = 0.0f;
            }
            t.x += v.dx * dt;
            t.y += v.dy * dt;
            return;
        }

        const float effSpeed = v.speed * (g.sprinting ? SPRINT_MULTIPLIER : 1.0f);

        if (g.isCrouching) {
            v.dx -= v.dx * CROUCH_FRICTION * dt;
            v.dy -= v.dy * CROUCH_FRICTION * dt;
            if (std::abs(v.dx) < 0.5f)
                v.dx = 0.0f;
            if (std::abs(v.dy) < 0.5f)
                v.dy = 0.0f;
            t.x += v.dx * dt;
            t.y += v.dy * dt;
        } else {
            v.dx = v.dy = 0.0f;

            switch (g.direction) {
                case GravityDir::DOWN:
                case GravityDir::UP: {
                    bool invertFlip = g.direction == GravityDir::UP;
                    bool kbLeft  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
                    bool kbRight = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
                    if (kbLeft)  v.dx = -effSpeed;
                    if (kbRight) v.dx =  effSpeed;
                    if (std::abs(padAxisX) > 0.0f) {
                        v.dx = padAxisX * effSpeed;
                        r.flipH = (padAxisX < 0.0f) ? !invertFlip : invertFlip;
                    }
                    if (!kbLeft && !kbRight && std::abs(padAxisX) == 0.0f) {
                        v.dx -= v.dx * friction * dt;
                        if (std::abs(v.dx) < 0.5f)
                            v.dx = 0.0f;
                    }
                    t.x += v.dx * dt;
                    break;
                }
                case GravityDir::LEFT:
                case GravityDir::RIGHT: {
                    bool kbUp   = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
                    bool kbDown = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
                    if (kbUp)   v.dy = -effSpeed;
                    if (kbDown) v.dy =  effSpeed;
                    if (std::abs(padAxisY) > 0.0f)
                        v.dy = padAxisY * effSpeed;
                    if (!kbUp && !kbDown && std::abs(padAxisY) == 0.0f) {
                        v.dy -= v.dy * friction * dt;
                        if (std::abs(v.dy) < 0.5f)
                            v.dy = 0.0f;
                    }
                    t.y += v.dy * dt;
                    break;
                }
            }
        }

        if (!g.isGrounded) {
            g.velocity = std::min(g.velocity + GRAVITY_FORCE * dt, MAX_FALL_SPEED);
        } else if (g.direction == GravityDir::DOWN) {
            // Ground-stick: always apply a small constant downward nudge while
            // grounded, regardless of whether the player is moving or standing
            // still. This guarantees oTop > 0 in CollisionSystem's floor snap
            // every frame, so isGrounded is reliably re-set to true even when
            // floating-point precision would otherwise produce oTop == 0 exactly
            // and cause a spurious one-frame airborne state (jump anim flash).
            // CollisionSystem resets g.velocity to 0 on any floor snap, so this
            // tiny nudge never accumulates into real downward movement.
            g.velocity = std::min(g.velocity + SLOPE_STICK_VELOCITY, MAX_FALL_SPEED);
        }

        if (g.jumpHeld && !g.isGrounded && g.velocity < 0.0f) {
            g.velocity -= JUMP_FORCE * 0.5f * dt;
        }

        switch (g.direction) {
            case GravityDir::DOWN:
                t.y += g.velocity * dt;
                break;
            case GravityDir::UP:
                t.y -= g.velocity * dt;
                break;
            case GravityDir::LEFT:
                t.x -= g.velocity * dt;
                break;
            case GravityDir::RIGHT:
                t.x += g.velocity * dt;
                break;
        }

        g.timer += dt;
    });

    auto tileView  = reg.view<TileTag, Transform, Collider>(entt::exclude<PropTag, HazardTag>);
    // Find player position for enemy AI targeting
    float playerX = 0.0f, playerW = 0.0f;
    float playerY = 0.0f, playerH = 0.0f;
    {
        auto pv = reg.view<PlayerTag, Transform, Collider>();
        pv.each([&](const Transform& pt, const Collider& pc) {
            playerX = pt.x;
            playerW = (float)pc.w;
            playerY = pt.y;
            playerH = (float)pc.h;
        });
    }

    auto ladderView = reg.view<LadderTag, Transform, Collider>();

    auto enemyView = reg.view<EnemyTag, Transform, Velocity, Collider, Renderable>(
        entt::exclude<DeadTag>);
    enemyView.each([&](entt::entity ent, Transform& t, Velocity& v, const Collider& c, Renderable& r) {
        // Enemy AI: if within aggro range, walk toward the player.
        // Skip chasing while stunned (HitFlash active = just got hit).
        bool stunned = false;
        if (auto* hf = reg.try_get<HitFlash>(ent))
            stunned = hf->timer > 0.0f;

        auto* climbState = reg.try_get<EnemyClimbState>(ent);

        // ── Stepping off ladder ──────────────────────────────────────────
        // Y is preserved from the climb exit. FloatingSystem skips gravity.
        // Walk horizontally toward the player until completely clear of all
        // ladder tiles, then release so gravity handles the landing.
        if (climbState && climbState->steppingOff && !reg.all_of<FloatTag>(ent)) {
            float playerCX = playerX + playerW * 0.5f;
            float enemyCX  = t.x + c.w * 0.5f;
            float stepDir  = (playerCX > enemyCX) ? 1.0f : -1.0f;
            t.x += stepDir * v.speed * dt;

            bool overLadder = false;
            ladderView.each([&](const Transform& lt, const Collider& lc) {
                if (overLadder) return;
                if (t.x < lt.x + lc.w && t.x + c.w > lt.x &&
                    t.y < lt.y + lc.h && t.y + c.h > lt.y)
                    overLadder = true;
            });

            if (!overLadder) {
                climbState->steppingOff = false;
                v.dx = stepDir * v.speed;
            }

            if (reg.all_of<FaceRightTag>(ent))
                r.flipH = stepDir < 0.0f;
            else
                r.flipH = stepDir > 0.0f;
            return;
        }

        // ── Ladder climbing ─────────────────────────────────────────────
        if (climbState && climbState->climbing && !reg.all_of<FloatTag>(ent)) {
            constexpr float ENEMY_CLIMB_SPEED = 120.0f;
            v.dx = 0.0f;
            // Stay centered on the ladder throughout the climb
            t.x = climbState->ladderCX - c.w * 0.5f;

            if (climbState->goingUp) {
                t.y -= ENEMY_CLIMB_SPEED * dt;

                // While ascending, probe for a solid platform adjacent to
                // the ladder at the enemy's current feet level. This lets
                // the enemy step off mid-ladder instead of climbing all
                // the way to the very top of a tall ladder.
                float feetY = t.y + c.h;
                float ladL  = climbState->ladderCX - climbState->ladderW * 0.5f;
                float ladR  = climbState->ladderCX + climbState->ladderW * 0.5f;

                bool  canStep   = false;
                float stepDir   = 0.0f;
                constexpr float PLAT_PROBE_H = 10.0f;

                tileView.each([&](const Transform& tt, const Collider& tc) {
                    if (canStep) return;
                    if (feetY < tt.y || feetY > tt.y + PLAT_PROBE_H) return;
                    float tileR = tt.x + tc.w;
                    // Platform to the LEFT of the ladder
                    if (tileR > ladL - 2.0f && tileR <= ladL + 8.0f) canStep = true;
                    // Platform to the RIGHT of the ladder
                    if (tt.x >= ladR - 8.0f && tt.x < ladR + 2.0f)  canStep = true;
                });

                if (canStep) {
                    // Prefer stepping toward the player
                    float playerCX = playerX + playerW * 0.5f;
                    stepDir = (playerCX > climbState->ladderCX) ? 1.0f : -1.0f;
                    t.y = feetY - c.h;
                    t.x = (stepDir < 0.0f) ? ladL - c.w : ladR;
                    climbState->climbing    = false;
                    climbState->steppingOff = false;
                    v.dx = stepDir * v.speed;
                } else if (feetY <= climbState->columnTop) {
                    // Fallback: reached the very top with no adjacent platform
                    t.y = climbState->columnTop - c.h;
                    climbState->climbing    = false;
                    climbState->steppingOff = true;
                }
            } else {
                t.y += ENEMY_CLIMB_SPEED * dt;

                // While descending, also check for adjacent platforms
                float feetY = t.y + c.h;
                float ladL  = climbState->ladderCX - climbState->ladderW * 0.5f;
                float ladR  = climbState->ladderCX + climbState->ladderW * 0.5f;

                bool  canStep = false;
                constexpr float PLAT_PROBE_H = 10.0f;

                tileView.each([&](const Transform& tt, const Collider& tc) {
                    if (canStep) return;
                    if (feetY < tt.y || feetY > tt.y + PLAT_PROBE_H) return;
                    float tileR = tt.x + tc.w;
                    if (tileR > ladL - 2.0f && tileR <= ladL + 8.0f) canStep = true;
                    if (tt.x >= ladR - 8.0f && tt.x < ladR + 2.0f)  canStep = true;
                });

                // Only step off when descending if the player is at this level
                float playerCY = playerY + playerH * 0.5f;
                bool  playerAtLevel = std::abs(playerCY - feetY) < 80.0f;

                if (canStep && playerAtLevel) {
                    float playerCX = playerX + playerW * 0.5f;
                    float stepDir = (playerCX > climbState->ladderCX) ? 1.0f : -1.0f;
                    t.y = feetY - c.h;
                    t.x = (stepDir < 0.0f) ? ladL - c.w : ladR;
                    climbState->climbing = false;
                    v.dx = stepDir * v.speed;
                } else if (feetY >= climbState->columnBot) {
                    t.y = climbState->columnBot - c.h;
                    climbState->climbing = false;
                }
            }

            if (reg.all_of<FaceRightTag>(ent))
                r.flipH = (playerX + playerW * 0.5f) < (t.x + c.w * 0.5f);
            return;
        }

        auto* react = reg.try_get<EnemyReaction>(ent);
        if (react && react->turnCooldown > 0.0f)
            react->turnCooldown -= dt;

        if (!stunned) {
            constexpr float AGGRO_RANGE = 300.0f;
            float enemyCX  = t.x + c.w * 0.5f;
            float playerCX = playerX + playerW * 0.5f;
            float dist     = std::abs(enemyCX - playerCX);
            if (dist < AGGRO_RANGE && dist > 4.0f) {
                float desiredDir = (playerCX > enemyCX) ? 1.0f : -1.0f;

                bool wantsToTurn = react
                    && react->lastDirSign != 0.0f
                    && desiredDir != react->lastDirSign;

                if (wantsToTurn && react->turnCooldown > 0.0f) {
                    v.dx *= 0.88f;
                    if (std::abs(v.dx) < 1.0f) v.dx = 0.0f;
                } else {
                    if (wantsToTurn && react)
                        react->turnCooldown = 0.3f;
                    v.dx = desiredDir * v.speed;
                    if (react) react->lastDirSign = desiredDir;
                }
            }
        } else {
            v.dx *= 0.85f;
            if (std::abs(v.dx) < 1.0f) v.dx = 0.0f;
        }

        // ── Check if enemy should grab a ladder ───────────────────────
        if (climbState && !stunned && !reg.all_of<FloatTag>(ent)) {
            constexpr float LADDER_AGGRO = 200.0f;
            float enemyCX   = t.x + c.w * 0.5f;
            float enemyFeet = t.y + c.h;

            float bestColTop = 1e9f, bestColBot = -1e9f;
            float ladMinX = 1e9f, ladMaxX = -1e9f;
            bool  inLadderColumn = false;
            // Nearest ladder-tile top that the enemy can step onto for
            // climbing down (at or just below the enemy's feet level).
            float entryTop = -1e9f;

            ladderView.each([&](const Transform& lt, const Collider& lc) {
                constexpr float inset = 4.0f;
                bool alignX = (t.x + inset) < (lt.x + lc.w) && (t.x + c.w - inset) > lt.x;
                if (!alignX) return;
                inLadderColumn = true;
                bestColTop = std::min(bestColTop, lt.y);
                bestColBot = std::max(bestColBot, lt.y + (float)lc.h);
                ladMinX    = std::min(ladMinX,    lt.x);
                ladMaxX    = std::max(ladMaxX,    lt.x + (float)lc.w);
                if (lt.y <= enemyFeet + 2.0f && lt.y > entryTop)
                    entryTop = lt.y;
            });

            if (inLadderColumn) {
                float ladCX  = (ladMinX + ladMaxX) * 0.5f;
                float ladW   = ladMaxX - ladMinX;

                float playerCY = playerY + playerH * 0.5f;
                float enemyCY  = t.y + c.h * 0.5f;
                float vertDist = playerCY - enemyCY;
                float hDist    = std::abs(enemyCX - (playerX + playerW * 0.5f));

                bool shouldClimbUp = hDist < LADDER_AGGRO
                    && vertDist < -40.0f
                    && enemyFeet > bestColTop + 4.0f;

                // Climb down: use nearest reachable ladder entry (not the
                // global top). Tolerance matches STEP_UP_HEIGHT so the
                // enemy can drop in from an adjacent platform one tile above.
                bool shouldClimbDown = entryTop > -1e9f
                    && hDist < LADDER_AGGRO
                    && vertDist > 40.0f
                    && enemyFeet >= entryTop - 2.0f
                    && enemyFeet <= entryTop + STEP_UP_HEIGHT;

                if (shouldClimbUp) {
                    climbState->climbing    = true;
                    climbState->goingUp     = true;
                    climbState->steppingOff = false;
                    climbState->columnTop   = bestColTop;
                    climbState->columnBot   = bestColBot;
                    climbState->ladderCX    = ladCX;
                    climbState->ladderW     = ladW;
                    t.x  = ladCX - c.w * 0.5f;
                    v.dx = 0.0f;
                    return;
                }
                if (shouldClimbDown) {
                    climbState->climbing    = true;
                    climbState->goingUp     = false;
                    climbState->steppingOff = false;
                    climbState->columnTop   = bestColTop;
                    climbState->columnBot   = bestColBot;
                    climbState->ladderCX    = ladCX;
                    climbState->ladderW     = ladW;
                    t.x  = ladCX - c.w * 0.5f;
                    t.y  = entryTop + 1.0f;
                    v.dx = 0.0f;
                    return;
                }
            }
        }

        t.x += v.dx * dt;

        {
            float boundR = (levelW > 0.0f) ? levelW : static_cast<float>(windowW);
            if (t.x < 0.0f) {
                t.x  = 0.0f;
                v.dx = std::abs(v.dx);
            } else if (t.x + c.w > boundR) {
                t.x  = boundR - c.w;
                v.dx = -std::abs(v.dx);
            }
        }

        // ── Ledge detection ───────────────────────────────────────────────
        // Probe below the enemy's leading foot. If no solid tile or ladder
        // tile exists there, the enemy is about to walk off a ledge — reverse.
        // Ladder tiles count as ground so enemies can walk over ladder
        // columns to align themselves for climbing.
        if (!reg.all_of<FloatTag>(ent) && !stunned) {
            constexpr float PROBE_DEPTH = 12.0f;
            float halfW  = c.w * 0.5f;
            float probeX = (v.dx > 0.0f) ? (t.x + c.w - halfW) : t.x;
            float probeY = t.y + c.h - 2.0f;
            float probeW = halfW;
            float probeH = PROBE_DEPTH;

            bool groundBelow = false;
            tileView.each([&](const Transform& tt, const Collider& tc) {
                if (groundBelow) return;
                if (probeX + probeW > tt.x &&
                    probeX          < tt.x + tc.w &&
                    probeY + probeH > tt.y &&
                    probeY          < tt.y + tc.h) {
                    groundBelow = true;
                }
            });

            if (!groundBelow && climbState) {
                ladderView.each([&](const Transform& lt, const Collider& lc) {
                    if (groundBelow) return;
                    float ladderTop = lt.y;
                    if (probeX + probeW > lt.x &&
                        probeX          < lt.x + lc.w &&
                        probeY >= ladderTop - 4.0f &&
                        probeY <= ladderTop + 4.0f) {
                        groundBelow = true;
                    }
                });
            }

            if (!groundBelow) {
                v.dx = -v.dx;
                t.x += v.dx * dt;
            }
        }

        // Custom enemy sprites face right by default (FaceRightTag);
        // legacy slime sprites face left by default.
        if (reg.all_of<FaceRightTag>(ent))
            r.flipH = v.dx < 0.0f;   // flip when moving left
        else
            r.flipH = v.dx > 0.0f;   // flip when moving right (legacy slime)
    });
}
