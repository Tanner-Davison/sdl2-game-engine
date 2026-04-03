#pragma once
#include <Components.hpp>
#include <SpatialGrid.hpp>
#include <SDL3/SDL.h>
#include <cmath>
#include <entt/entt.hpp>

// pad1 = gamepad for PlayerIndex 0 (P1), pad2 = gamepad for PlayerIndex 1 (P2).
inline void MovementSystem(entt::registry& reg, float dt, int windowW, float levelW = 0.0f,
                           SDL_Gamepad* pad1 = nullptr,
                           const SpatialGrid* tileGrid = nullptr,
                           SDL_Gamepad* pad2 = nullptr) {
    const bool* keys = SDL_GetKeyboardState(nullptr);

    constexpr float PAD_DEAD_ZONE = 0.25f;

    // Read axes for each gamepad slot upfront
    float padAxisX1 = 0.0f, padAxisY1 = 0.0f;
    if (pad1) {
        float rx = SDL_GetGamepadAxis(pad1, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        float ry = SDL_GetGamepadAxis(pad1, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        if (std::abs(rx) > PAD_DEAD_ZONE) padAxisX1 = rx;
        if (std::abs(ry) > PAD_DEAD_ZONE) padAxisY1 = ry;
    }
    float padAxisX2 = 0.0f, padAxisY2 = 0.0f;
    if (pad2) {
        float rx = SDL_GetGamepadAxis(pad2, SDL_GAMEPAD_AXIS_LEFTX) / 32767.0f;
        float ry = SDL_GetGamepadAxis(pad2, SDL_GAMEPAD_AXIS_LEFTY) / 32767.0f;
        if (std::abs(rx) > PAD_DEAD_ZONE) padAxisX2 = rx;
        if (std::abs(ry) > PAD_DEAD_ZONE) padAxisY2 = ry;
    }

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
    playerView.each([&reg, dt, keys, padAxisX1, padAxisY1, padAxisX2, padAxisY2](entt::entity ent, Transform& t, Velocity& v, GravityState& g, const ClimbState& climb, Renderable& r) {
        // Route to the correct gamepad based on PlayerIndex component
        const auto* pi   = reg.try_get<PlayerIndex>(ent);
        int          idx = pi ? pi->index : 0;
        float padAxisX   = (idx == 1) ? padAxisX2 : padAxisX1;
        float padAxisY   = (idx == 1) ? padAxisY2 : padAxisY1;
        // P2 is gamepad-only; P1 also reads keyboard
        const bool* effectiveKeys = (idx == 0) ? keys : nullptr;
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
                bool leftHeld  = (effectiveKeys && effectiveKeys[SDL_SCANCODE_A]) || padAxisX < 0.0f;
                bool rightHeld = (effectiveKeys && effectiveKeys[SDL_SCANCODE_D]) || padAxisX > 0.0f;
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
            bool kbMoving = effectiveKeys && (effectiveKeys[SDL_SCANCODE_W] || effectiveKeys[SDL_SCANCODE_S] ||
                            effectiveKeys[SDL_SCANCODE_A] || effectiveKeys[SDL_SCANCODE_D]);
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
                    bool kbLeft  = effectiveKeys && effectiveKeys[SDL_SCANCODE_A];
                    bool kbRight = effectiveKeys && effectiveKeys[SDL_SCANCODE_D];
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
                    bool kbUp   = effectiveKeys && effectiveKeys[SDL_SCANCODE_W];
                    bool kbDown = effectiveKeys && effectiveKeys[SDL_SCANCODE_S];
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
            // Ground-stick: constant downward nudge while grounded guarantees
            // oTop > 0 in CollisionSystem's floor snap every frame, so isGrounded
            // is reliably re-set. Without this, FP precision can produce oTop == 0
            // exactly, causing a spurious one-frame airborne state (jump anim flash).
            // CollisionSystem resets g.velocity to 0 on any floor snap.
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

    // Collect all living player positions (multiplayer-aware).
    // Enemies will chase the nearest player center-X.
    struct PlayerPos { float x, y, w, h; };
    std::vector<PlayerPos> allPlayers;
    {
        auto pv = reg.view<PlayerTag, Transform, Collider>(entt::exclude<DeadTag>);
        pv.each([&](const Transform& pt, const Collider& pc) {
            allPlayers.push_back({pt.x, pt.y, (float)pc.w, (float)pc.h});
        });
    }
    // Fall back to P1 values for single-player compatibility
    float playerX = allPlayers.empty() ? 0.0f : allPlayers[0].x;
    float playerW = allPlayers.empty() ? 0.0f : allPlayers[0].w;
    float playerY = allPlayers.empty() ? 0.0f : allPlayers[0].y;
    float playerH = allPlayers.empty() ? 0.0f : allPlayers[0].h;

    // Helper: find center-X/Y of the nearest player to a world point.
    auto nearestPlayerCenter = [&](float fromX, float fromY) -> std::pair<float,float> {
        float bestDist = 1e9f;
        float bestCX = playerX + playerW * 0.5f;
        float bestCY = playerY + playerH * 0.5f;
        for (const auto& pp : allPlayers) {
            float cx = pp.x + pp.w * 0.5f;
            float cy = pp.y + pp.h * 0.5f;
            float dx = cx - fromX, dy = cy - fromY;
            float d  = dx * dx + dy * dy;
            if (d < bestDist) { bestDist = d; bestCX = cx; bestCY = cy; }
        }
        return {bestCX, bestCY};
    };

    auto ladderView = reg.view<LadderTag, Transform, Collider>();

    auto enemyView = reg.view<EnemyTag, Transform, Velocity, Collider, Renderable>(
        entt::exclude<DeadTag>);
    enemyView.each([&](entt::entity ent, Transform& t, Velocity& v, const Collider& c, Renderable& r) {
        bool stunned = false;
        if (auto* hf = reg.try_get<HitFlash>(ent))
            stunned = hf->timer > 0.0f;

        if (!stunned) {
            if (auto* ead = reg.try_get<EnemyAnimData>(ent)) {
                if (ead->hurtSheet && r.sheet == ead->hurtSheet && !reg.all_of<DeadTag>(ent))
                    stunned = true;
            }
        }

        auto* climbState = reg.try_get<EnemyClimbState>(ent);

        // Stepping off ladder: walk horizontally toward player until clear of
        // all ladder tiles, then release so gravity handles the landing.
        if (climbState && climbState->steppingOff && !reg.all_of<FloatTag>(ent)) {
            float enemyCX  = t.x + c.w * 0.5f;
            auto [playerCX, playerCY_step] = nearestPlayerCenter(enemyCX, t.y + c.h * 0.5f);
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

        if (climbState && climbState->climbing && !reg.all_of<FloatTag>(ent)) {
            constexpr float ENEMY_CLIMB_SPEED = 120.0f;
            v.dx = 0.0f;
            t.x = climbState->ladderCX - c.w * 0.5f;

            if (climbState->goingUp) {
                t.y -= ENEMY_CLIMB_SPEED * dt;

                // While ascending, probe for an adjacent solid platform at
                // feet level so the enemy can step off mid-ladder.
                float feetY = t.y + c.h;
                float ladL  = climbState->ladderCX - climbState->ladderW * 0.5f;
                float ladR  = climbState->ladderCX + climbState->ladderW * 0.5f;

                bool  canStep   = false;
                float stepDir   = 0.0f;
                constexpr float PLAT_PROBE_H = 10.0f;

                auto probeClimbPlatform = [&](const Transform& tt, const Collider& tc) {
                    if (canStep) return;
                    if (feetY < tt.y || feetY > tt.y + PLAT_PROBE_H) return;
                    float tileR2 = tt.x + tc.w;
                    if (tileR2 > ladL - 2.0f && tileR2 <= ladL + 8.0f) canStep = true;
                    if (tt.x >= ladR - 8.0f && tt.x < ladR + 2.0f)  canStep = true;
                };
                if (tileGrid) {
                    tileGrid->Query(ladL - 10.f, feetY - 1.f, (ladR - ladL) + 20.f, PLAT_PROBE_H + 2.f,
                        [&](entt::entity te) {
                            if (canStep) return;
                            if (!reg.valid(te) || !reg.all_of<TileTag>(te)) return;
                            probeClimbPlatform(reg.get<Transform>(te), reg.get<Collider>(te));
                        });
                } else {
                    tileView.each([&](const Transform& tt, const Collider& tc) {
                        probeClimbPlatform(tt, tc);
                    });
                }

                if (canStep) {
                    auto [nearCX_up, nearCY_up] = nearestPlayerCenter(t.x + c.w * 0.5f, t.y + c.h * 0.5f);
                    stepDir = (nearCX_up > climbState->ladderCX) ? 1.0f : -1.0f;
                    t.y = feetY - c.h;
                    t.x = (stepDir < 0.0f) ? ladL - c.w : ladR;
                    climbState->climbing    = false;
                    climbState->steppingOff = false;
                    v.dx = stepDir * v.speed;
                } else if (feetY <= climbState->columnTop) {
                    // Reached the very top with no adjacent platform
                    t.y = climbState->columnTop - c.h;
                    climbState->climbing    = false;
                    climbState->steppingOff = true;
                }
            } else {
                t.y += ENEMY_CLIMB_SPEED * dt;

                float feetY = t.y + c.h;
                float ladL  = climbState->ladderCX - climbState->ladderW * 0.5f;
                float ladR  = climbState->ladderCX + climbState->ladderW * 0.5f;

                bool  canStep = false;
                constexpr float PLAT_PROBE_H = 10.0f;

                auto probeDescPlatform = [&](const Transform& tt, const Collider& tc) {
                    if (canStep) return;
                    if (feetY < tt.y || feetY > tt.y + PLAT_PROBE_H) return;
                    float tileR2 = tt.x + tc.w;
                    if (tileR2 > ladL - 2.0f && tileR2 <= ladL + 8.0f) canStep = true;
                    if (tt.x >= ladR - 8.0f && tt.x < ladR + 2.0f)  canStep = true;
                };
                if (tileGrid) {
                    tileGrid->Query(ladL - 10.f, feetY - 1.f, (ladR - ladL) + 20.f, PLAT_PROBE_H + 2.f,
                        [&](entt::entity te) {
                            if (canStep) return;
                            if (!reg.valid(te) || !reg.all_of<TileTag>(te)) return;
                            probeDescPlatform(reg.get<Transform>(te), reg.get<Collider>(te));
                        });
                } else {
                    tileView.each([&](const Transform& tt, const Collider& tc) {
                        probeDescPlatform(tt, tc);
                    });
                }

                // Only step off when descending if the player is at this level
                auto [nearCX_desc, nearCY_desc] = nearestPlayerCenter(t.x + c.w * 0.5f, t.y + c.h * 0.5f);
                bool  playerAtLevel = std::abs(nearCY_desc - feetY) < 80.0f;

                if (canStep && playerAtLevel) {
                    float playerCX = nearCX_desc;
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

            if (reg.all_of<FaceRightTag>(ent)) {
                auto [nearCX_face, nearCY_face] = nearestPlayerCenter(t.x + c.w * 0.5f, t.y + c.h * 0.5f);
                r.flipH = nearCX_face < (t.x + c.w * 0.5f);
            }
            return;
        }

        auto* react = reg.try_get<EnemyReaction>(ent);
        if (react && react->turnCooldown > 0.0f)
            react->turnCooldown -= dt;

        if (!stunned) {
            constexpr float AGGRO_RANGE = 300.0f;
            float enemyCX  = t.x + c.w * 0.5f;
            auto [playerCX, playerCY_aggro] = nearestPlayerCenter(enemyCX, t.y + c.h * 0.5f);
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

        if (climbState && !stunned && !reg.all_of<FloatTag>(ent)) {
            constexpr float LADDER_AGGRO = 200.0f;
            float enemyCX   = t.x + c.w * 0.5f;
            float enemyFeet = t.y + c.h;

            float bestColTop = 1e9f, bestColBot = -1e9f;
            float ladMinX = 1e9f, ladMaxX = -1e9f;
            bool  inLadderColumn = false;
            // Nearest ladder-tile top at or below enemy feet for climb-down entry.
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

                float enemyCY  = t.y + c.h * 0.5f;
                auto [nearCX_lad, nearCY_lad] = nearestPlayerCenter(enemyCX, enemyCY);
                float playerCY = nearCY_lad;
                float vertDist = playerCY - enemyCY;
                float hDist    = std::abs(enemyCX - nearCX_lad);

                bool shouldClimbUp = hDist < LADDER_AGGRO
                    && vertDist < -40.0f
                    && enemyFeet > bestColTop + 4.0f;

                // Tolerance matches STEP_UP_HEIGHT so the enemy can drop in
                // from an adjacent platform one tile above.
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

        // Ledge detection: probe below leading foot; reverse if no ground.
        if (!reg.all_of<FloatTag>(ent) && !stunned) {
            constexpr float PROBE_DEPTH = 12.0f;
            float halfW  = c.w * 0.5f;
            float probeX = (v.dx > 0.0f) ? (t.x + c.w - halfW) : t.x;
            float probeY = t.y + c.h - 2.0f;
            float probeW = halfW;
            float probeH = PROBE_DEPTH;

            bool groundBelow = false;
            if (tileGrid) {
                tileGrid->Query(probeX, probeY, probeW, probeH, [&](entt::entity te) {
                    if (groundBelow) return;
                    if (!reg.valid(te) || !reg.all_of<TileTag>(te)) return;
                    if (reg.all_of<PropTag>(te) || reg.all_of<HazardTag>(te)) return;
                    auto& tt = reg.get<Transform>(te);
                    auto& tc = reg.get<Collider>(te);
                    if (probeX + probeW > tt.x && probeX < tt.x + tc.w &&
                        probeY + probeH > tt.y && probeY < tt.y + tc.h)
                        groundBelow = true;
                });
            } else {
                tileView.each([&](const Transform& tt, const Collider& tc) {
                    if (groundBelow) return;
                    if (probeX + probeW > tt.x && probeX < tt.x + tc.w &&
                        probeY + probeH > tt.y && probeY < tt.y + tc.h)
                        groundBelow = true;
                });
            }

            if (!groundBelow && climbState) {
                ladderView.each([&](const Transform& lt, const Collider& lc) {
                    if (groundBelow) return;
                    float ladderTop = lt.y;
                    if (probeX + probeW > lt.x && probeX < lt.x + lc.w &&
                        probeY >= ladderTop - 4.0f && probeY <= ladderTop + 4.0f)
                        groundBelow = true;
                });
            }

            if (!groundBelow) {
                v.dx = -v.dx;
                t.x += v.dx * dt;
            }
        }

        // FaceRightTag sprites face right by default; legacy slime sprites face left.
        if (reg.all_of<FaceRightTag>(ent))
            r.flipH = v.dx < 0.0f;
        else
            r.flipH = v.dx > 0.0f;
    });
}
