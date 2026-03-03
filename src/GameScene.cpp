#include "GameScene.hpp"
#include "GameConfig.hpp"
#include "GameEvents.hpp"
#include "LevelTwo.hpp"
#include "PauseMenuScene.hpp"
#include "SurfaceUtils.hpp"
#include <SDL3_image/SDL_image.h>
#include <cmath>
#include <cstdlib>
#include <print>

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

GameScene::GameScene(const std::string& levelPath, bool fromEditor)
    : mLevelPath(levelPath), mFromEditor(fromEditor) {}

// ─────────────────────────────────────────────────────────────────────────────
// Scene interface
// ─────────────────────────────────────────────────────────────────────────────

void GameScene::Load(Window& window) {
    mWindow  = &window;
    gameOver = false;

    // Load level from file if a path was provided
    if (!mLevelPath.empty())
        LoadLevel(mLevelPath, mLevel);

    // ── Frost Knight — individual PNG sequences, zero-padded 3-digit frame numbers ──
    // KW/KH drive the actual rendered sprite size; collider dims are independent.
    const int KW = PLAYER_SPRITE_WIDTH;
    const int KH = PLAYER_SPRITE_HEIGHT;
    auto loadAnim = [&](const std::string& folder, const std::string& prefix, int count) {
        return std::make_unique<SpriteSheet>(
            "game_assets/frost_knight_png_sequences/" + folder + "/",
            prefix, count, KW, KH, 3);
    };

    knightIdleSheet  = loadAnim("Idle",         "0_Knight_Idle_",         18);
    knightWalkSheet  = loadAnim("Walking",       "0_Knight_Walking_",      24);
    knightHurtSheet  = loadAnim("Hurt",          "0_Knight_Hurt_",         12);
    knightJumpSheet  = loadAnim("Jump Start",    "0_Knight_Jump Start_",    6);
    knightFallSheet  = loadAnim("Falling Down",  "0_Knight_Falling Down_",  6);
    knightSlideSheet = loadAnim("Sliding",       "0_Knight_Sliding_",       6);
    knightSlashSheet = loadAnim("Slashing",      "0_Knight_Slashing_",      12);

    idleFrames  = knightIdleSheet->GetAnimation("0_Knight_Idle_");
    walkFrames  = knightWalkSheet->GetAnimation("0_Knight_Walking_");
    jumpFrames  = knightJumpSheet->GetAnimation("0_Knight_Jump Start_");
    hurtFrames  = knightHurtSheet->GetAnimation("0_Knight_Hurt_");
    duckFrames  = knightSlideSheet->GetAnimation("0_Knight_Sliding_");
    frontFrames = knightFallSheet->GetAnimation("0_Knight_Falling Down_");
    slashFrames = knightSlashSheet->GetAnimation("0_Knight_Slashing_");

    enemySheet      = std::make_unique<SpriteSheet>(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");
    enemyWalkFrames = enemySheet->GetAnimation("slimeWalk");

    // Use the background from the level file if one was loaded, otherwise
    // fall back to the default so sandbox/freeplay mode still has a background.
    std::string bgPath = (!mLevelPath.empty() && !mLevel.background.empty())
                           ? mLevel.background
                           : "game_assets/backgrounds/deepspace_scene.png";
    background = std::make_unique<Image>(bgPath, nullptr, FitMode::PRESCALED);
    locationText = std::make_unique<Text>("You are in space!!", 20, 20);
    actionText   = std::make_unique<Text>(
        "Level 1: Collect ALL the coins!", SDL_Color{255, 255, 255, 0}, 20, 80, 20);

    gameOverText = std::make_unique<Text>("Game Over!",
                                          SDL_Color{255, 0, 0, 255},
                                          window.GetWidth() / 2 - 100,
                                          window.GetHeight() / 2 - 60,
                                          64);
    retryBtnText = std::make_unique<Text>("Retry",
                                          SDL_Color{0, 0, 0, 255},
                                          window.GetWidth() / 2 - 28,
                                          window.GetHeight() / 2 + 22,
                                          32);
    retryKeyText = std::make_unique<Text>("Press R to Retry",
                                          SDL_Color{200, 200, 200, 255},
                                          window.GetWidth() / 2 - 100,
                                          window.GetHeight() / 2 + 110,
                                          24);

    retryBtnRect = {window.GetWidth() / 2 - 75, window.GetHeight() / 2 + 10, 150, 55};
    retryButton  = std::make_unique<Rectangle>(retryBtnRect);
    retryButton->SetColor({255, 255, 255, 255});
    retryButton->SetHoverColor({180, 180, 180, 255});

    levelCompleteText = std::make_unique<Text>("Level Complete!",
                                               SDL_Color{255, 215, 0, 255},
                                               window.GetWidth() / 2 - 160,
                                               window.GetHeight() / 2 - 40,
                                               64);
    Spawn();
}

void GameScene::Unload() {
    reg.clear();
    for (auto* s : tileScaledSurfaces) SDL_DestroySurface(s);
    tileScaledSurfaces.clear();
    mWindow = nullptr;
}

std::unique_ptr<Scene> GameScene::NextScene() {
    if (mPauseRequested) {
        mPauseRequested = false;
        return std::make_unique<PauseMenuScene>(mLevelPath, mFromEditor);
    }
    if (levelComplete && levelCompleteTimer <= 0.0f)
        return std::make_unique<LevelTwo>();
    return nullptr;
}

bool GameScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT)
        return false;

    if (!gameOver) {
        // ESC during active play — request pause
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE && !levelComplete) {
            mPauseRequested = true;
            return true;
        }
        InputSystem(reg, e);
    } else {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_R)
            Respawn();

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
            e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x;
            int my = (int)e.button.y;
            if (mx >= retryBtnRect.x && mx <= retryBtnRect.x + retryBtnRect.w &&
                my >= retryBtnRect.y && my <= retryBtnRect.y + retryBtnRect.h)
                Respawn();
        }
        retryButton->HandleEvent(e);
    }
    return true;
}

void GameScene::Update(float dt) {
    if (levelComplete) {
        levelCompleteTimer -= dt;
        return;
    }
    if (gameOver) return;

    FloatingSystem(reg, dt);
    LadderSystem(reg, dt);
    MovementSystem(reg, dt, mWindow->GetWidth());
    BoundsSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight(),
                 mLevel.gravityMode == GravityMode::WallRun,
                 mLevelW, mLevelH);
    PlayerStateSystem(reg);
    AnimationSystem(reg, dt);

    // CollisionSystem now returns a result instead of mutating our variables directly
    CollisionResult collision = CollisionSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight());
    coinCount  += collision.coinsCollected;
    stompCount += collision.enemiesStomped + collision.enemiesSlashed;
    if (collision.playerDied) gameOver = true;

    // ── Hazard damage ─────────────────────────────────────────────────────────
    // While on a hazard tile: drain HP at HAZARD_DAMAGE_PER_SEC, advance flash
    // timer, and force the hurt animation via HazardState.  The moment the
    // player steps off, active is cleared and the animation reverts naturally
    // through PlayerStateSystem next frame.
    {
        auto hView = reg.view<PlayerTag, Health, HazardState, AnimationState,
                              Renderable, AnimationSet>();
        hView.each([&](Health& hp, HazardState& hz,
                       AnimationState& anim, Renderable& r,
                       const AnimationSet& set) {
            hz.active = collision.onHazard;
            if (hz.active) {
                // Drain HP
                hp.current -= HAZARD_DAMAGE_PER_SEC * dt;
                if (hp.current <= 0.0f) { hp.current = 0.0f; gameOver = true; }

                // Advance flash timer — 8 Hz pulse
                hz.flashTimer += dt;

                // Force hurt animation while on hazard
                if (anim.currentAnim != AnimationID::HURT) {
                    r.sheet           = set.hurtSheet;
                    r.frames          = set.hurt;
                    anim.currentFrame = 0;
                    anim.timer        = 0.0f;
                    anim.fps          = 12.0f;
                    anim.looping      = true;
                    anim.totalFrames  = (int)set.hurt.size();
                    anim.currentAnim  = AnimationID::HURT;
                }
            } else {
                // Reset flash timer so the flash stops instantly when leaving
                hz.flashTimer = 0.0f;
                // Let PlayerStateSystem restore the correct animation next frame
                if (anim.currentAnim == AnimationID::HURT) {
                    // Nudge the anim ID to NONE so PlayerStateSystem re-evaluates
                    anim.currentAnim = AnimationID::NONE;
                }
            }
        });
    }

    // Jump — skipped in OpenWorld mode (no gravity, no jumping)
    if (mLevel.gravityMode != GravityMode::OpenWorld) {
        auto jumpView = reg.view<PlayerTag, GravityState>();
        jumpView.each([](GravityState& g) {
            if (g.active && g.jumpHeld && g.isGrounded) {
                g.velocity   = -JUMP_FORCE;
                g.isGrounded = false;
                g.jumpHeld   = false;
            }
        });
    }

    if (totalCoins > 0 && coinCount >= totalCoins)
        levelComplete = true;

    // ── Camera update ───────────────────────────────────────────────────────────
    {
        auto pView = reg.view<PlayerTag, Transform, Collider>();
        pView.each([&](const Transform& pt, const Collider& pc) {
            float cx = pt.x + pc.w * 0.5f;
            float cy = pt.y + pc.h * 0.5f;
            mCamera.Update(cx, cy,
                           mWindow->GetWidth(), mWindow->GetHeight(),
                           mLevelW, mLevelH, dt);
        });
    }
}

void GameScene::Render(Window& window) {
    window.Render();
    background->Render(window.GetSurface());

    if (levelComplete) {
        RenderSystem(reg, window.GetSurface(), mCamera.x, mCamera.y);
        HUDSystem(reg, window.GetSurface(), window.GetWidth(),
                  healthText.get(), gravityText.get(),
                  coinText.get(), coinCount,
                  stompText.get(), stompCount);
        if (levelCompleteText)
            levelCompleteText->Render(window.GetSurface());
    } else if (gameOver) {
        gameOverText->Render(window.GetSurface());
        retryButton->Render(window.GetSurface());
        retryBtnText->Render(window.GetSurface());
        retryKeyText->Render(window.GetSurface());
    } else {
        locationText->Render(window.GetSurface());
        actionText->Render(window.GetSurface());
        RenderSystem(reg, window.GetSurface(), mCamera.x, mCamera.y);

        HUDSystem(reg, window.GetSurface(), window.GetWidth(),
                  healthText.get(), gravityText.get(),
                  coinText.get(), coinCount,
                  stompText.get(), stompCount);
    }

    window.Update();
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

void GameScene::Spawn() {
    healthText  = std::make_unique<Text>("100", SDL_Color{255, 255, 255, 255}, 0, 0, 16);
    gravityText = std::make_unique<Text>("",    SDL_Color{100, 200, 255, 255}, 0, 0, 20);
    coinText    = std::make_unique<Text>("Gold Collected: 0",   SDL_Color{255, 215, 0, 255},   0, 0, 16);
    stompText   = std::make_unique<Text>("Enemies Stomped: 0",  SDL_Color{255, 100, 100, 255}, 0, 0, 16);

    coinSheet = std::make_unique<SpriteSheet>("game_assets/gold_coins/", "Gold_", 30, 40, 40);
    std::vector<SDL_Rect> coinFrames = coinSheet->GetAnimation("Gold_");

    // ── Spawn coins ───────────────────────────────────────────────────────────
    // When a level file is provided, spawn exactly what's in it (even if empty).
    // Random fallback only runs in the no-file / sandbox mode.
    auto spawnCoin = [&](float cx, float cy) {
        auto coin = reg.create();
        reg.emplace<Transform>(coin, cx, cy);
        reg.emplace<Renderable>(coin, coinSheet->GetSurface(), coinFrames, false);
        reg.emplace<AnimationState>(coin, 0, (int)coinFrames.size(), 0.0f, 15.0f, true);
        reg.emplace<Collider>(coin, COIN_SIZE, COIN_SIZE);
        reg.emplace<CoinTag>(coin);
    };

    if (!mLevelPath.empty()) {
        // Level file loaded — spawn only what the level defines (may be zero)
        for (const auto& c : mLevel.coins)
            spawnCoin(c.x, c.y);
    } else {
        // No level file — sandbox/freeplay mode, use random wall placement
        for (int i = 0; i < COIN_COUNT; i++) {
            int   wall = rand() % 3;
            float cx = 0.0f, cy = 0.0f;
            int   pad = COIN_SIZE + 10;
            switch (wall) {
                case 0: cx = 5.0f;
                        cy = static_cast<float>(pad + rand() % (mWindow->GetHeight() - pad * 2));
                        break;
                case 1: cx = static_cast<float>(pad + rand() % (mWindow->GetWidth() - pad * 2));
                        cy = 5.0f;
                        break;
                case 2: cx = static_cast<float>(mWindow->GetWidth() - COIN_SIZE - 5);
                        cy = static_cast<float>(pad + rand() % (mWindow->GetHeight() - pad * 2));
                        break;
            }
            spawnCoin(cx, cy);
        }
    }

    totalCoins = static_cast<int>(reg.view<CoinTag>().size());

    // ── Compute level bounds from tile extents ────────────────────────────────
    // Use screen size as the minimum so single-screen levels still clamp correctly.
    mLevelW = static_cast<float>(mWindow->GetWidth());
    mLevelH = static_cast<float>(mWindow->GetHeight());
    for (const auto& ts : mLevel.tiles) {
        float right  = ts.x + ts.w;
        float bottom = ts.y + ts.h;
        if (right  > mLevelW) mLevelW = right;
        if (bottom > mLevelH) mLevelH = bottom;
    }
    // Add one screen of padding so tiles at the far edge aren't flush against the boundary
    mLevelW += static_cast<float>(mWindow->GetWidth())  * 0.25f;
    mLevelH += static_cast<float>(mWindow->GetHeight()) * 0.25f;

    // Snap camera to player position immediately so first frame has no black borders
    {
        float px = mLevel.player.x + PLAYER_STAND_WIDTH  * 0.5f;
        float py = mLevel.player.y + PLAYER_STAND_HEIGHT * 0.5f;
        mCamera.x = px - mWindow->GetWidth()  * 0.5f;
        mCamera.y = py - mWindow->GetHeight() * 0.5f;
        if (mCamera.x < 0.0f) mCamera.x = 0.0f;
        if (mCamera.y < 0.0f) mCamera.y = 0.0f;
        if (mCamera.x + mWindow->GetWidth()  > mLevelW) mCamera.x = mLevelW - mWindow->GetWidth();
        if (mCamera.y + mWindow->GetHeight() > mLevelH) mCamera.y = mLevelH - mWindow->GetHeight();
        if (mCamera.x < 0.0f) mCamera.x = 0.0f;
        if (mCamera.y < 0.0f) mCamera.y = 0.0f;
    }

    // ── Player spawn ──────────────────────────────────────────────────────────
    float playerX = mLevelPath.empty()
                      ? (float)(mWindow->GetWidth() / 2 - 33)
                      : mLevel.player.x;
    float playerY = mLevelPath.empty()
                      ? (float)(mWindow->GetHeight() - PLAYER_STAND_HEIGHT)
                      : mLevel.player.y;

    auto player = reg.create();
    reg.emplace<Transform>(player, playerX, playerY);
    reg.emplace<Velocity>(player);
    reg.emplace<AnimationState>(player, 0, (int)idleFrames.size(), 0.0f, 10.0f, true);
    reg.emplace<Renderable>(player, knightIdleSheet->GetSurface(), idleFrames, false);
    reg.emplace<PlayerTag>(player);
    reg.emplace<Health>(player);
    reg.emplace<Collider>(player, PLAYER_STAND_WIDTH, PLAYER_STAND_HEIGHT);
    reg.emplace<RenderOffset>(player, PLAYER_STAND_ROFF_X, PLAYER_STAND_ROFF_Y);
    reg.emplace<InvincibilityTimer>(player);
    {
        GravityState gs;
        if (mLevel.gravityMode == GravityMode::OpenWorld) {
            gs.active     = false;   // no gravity in open-world mode
            gs.isGrounded = true;    // never in a "falling" state
        }
        reg.emplace<GravityState>(player, gs);
    }
    if (mLevel.gravityMode == GravityMode::OpenWorld)
        reg.emplace<OpenWorldTag>(player);
    reg.emplace<ClimbState>(player);
    reg.emplace<HazardState>(player);
    reg.emplace<AttackState>(player);
    reg.emplace<AnimationSet>(player, AnimationSet{
        .idle       = idleFrames,  .idleSheet  = knightIdleSheet->GetSurface(),
        .walk       = walkFrames,  .walkSheet  = knightWalkSheet->GetSurface(),
        .jump       = jumpFrames,  .jumpSheet  = knightJumpSheet->GetSurface(),
        .hurt       = hurtFrames,  .hurtSheet  = knightHurtSheet->GetSurface(),
        .duck       = duckFrames,  .duckSheet  = knightSlideSheet->GetSurface(),
        .front      = frontFrames, .frontSheet = knightFallSheet->GetSurface(),
        .slash      = slashFrames, .slashSheet = knightSlashSheet->GetSurface(),
    });

    // ── Spawn tiles — only from level file ────────────────────────────────────
    for (const auto& ts : mLevel.tiles) {
        SDL_Surface* raw = IMG_Load(ts.imagePath.c_str());
        if (!raw) {
            std::print("Failed to load tile: {}\n", ts.imagePath);
            continue;
        }

        SDL_Surface* converted = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        if (!converted) {
            std::print("Failed to convert tile surface: {}\n", ts.imagePath);
            continue;
        }

        SDL_Surface* scaled = SDL_CreateSurface(ts.w, ts.h, SDL_PIXELFORMAT_ARGB8888);
        if (!scaled) { SDL_DestroySurface(converted); continue; }

        SDL_SetSurfaceBlendMode(converted, SDL_BLENDMODE_NONE);
        SDL_Rect src = {0, 0, converted->w, converted->h};
        SDL_Rect dst = {0, 0, ts.w, ts.h};
        SDL_BlitSurfaceScaled(converted, &src, scaled, &dst, SDL_SCALEMODE_LINEAR);
        SDL_DestroySurface(converted);
        SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);

        // Apply rotation if non-zero
        if (ts.rotation != 0) {
            SDL_Surface* rotated = RotateSurfaceDeg(scaled, ts.rotation);
            if (rotated) {
                SDL_DestroySurface(scaled);
                scaled = rotated;
            }
        }

        auto tile = reg.create();
        reg.emplace<Transform>(tile, ts.x, ts.y);
        // Props: visual only. Ladders: passthrough but tagged for climb detection.
        // Action tiles: rendered + solid until player contact, then invisible + passthrough.
        // Solid tiles: full collision.
        // Resolve effective hitbox size — custom if set, otherwise full tile
        bool hasCustomHitbox = (ts.hitboxW > 0 || ts.hitboxH > 0);
        int  colW = hasCustomHitbox ? (ts.hitboxW > 0 ? ts.hitboxW : ts.w) : ts.w;
        int  colH = hasCustomHitbox ? (ts.hitboxH > 0 ? ts.hitboxH : ts.h) : ts.h;

        if (ts.ladder) {
            reg.emplace<LadderTag>(tile);
            reg.emplace<Collider>(tile, colW, colH);
        } else if (ts.prop) {
            reg.emplace<PropTag>(tile);
        } else if (ts.action) {
            reg.emplace<ActionTag>(tile, ts.actionGroup);
            reg.emplace<TileTag>(tile);
            reg.emplace<Collider>(tile, colW, colH);
        } else if (ts.slope != SlopeType::None) {
            reg.emplace<TileTag>(tile);
            reg.emplace<Collider>(tile, colW, colH);
            reg.emplace<SlopeCollider>(tile, ts.slope);
        } else if (ts.hazard) {
            // Hazard: passthrough tile (no TileTag) — the player walks into it
            // and takes damage.  No push-out; overlap is detected by CollisionSystem.
            reg.emplace<HazardTag>(tile);
            reg.emplace<Collider>(tile, colW, colH);
        } else {
            reg.emplace<Collider>(tile, colW, colH);
            reg.emplace<TileTag>(tile);
        }

        // Anti-gravity tiles float and are pushable
        if (ts.antiGravity) {
            reg.emplace<FloatTag>(tile);
            FloatState fs;
            fs.baseY    = ts.y;
            fs.bobAmp   = 4.0f + (rand() % 50) * 0.08f; // 4–8 px
            fs.bobSpeed = 1.4f + (rand() % 80) * 0.01f;
            fs.bobPhase = (rand() % 628) * 0.01f;
            reg.emplace<FloatState>(tile, fs);
        }

        // Attach offset component when the hitbox doesn't start at tile origin
        if (hasCustomHitbox && (ts.hitboxOffX != 0 || ts.hitboxOffY != 0))
            reg.emplace<ColliderOffset>(tile, ts.hitboxOffX, ts.hitboxOffY);
        std::vector<SDL_Rect> tileFrame = {{0, 0, ts.w, ts.h}};
        reg.emplace<Renderable>(tile, scaled, tileFrame, false);
        reg.emplace<AnimationState>(tile, 0, 1, 0.0f, 1.0f, false);
        tileScaledSurfaces.push_back(scaled);
    }

    // ── Spawn enemies ─────────────────────────────────────────────────────────
    auto spawnEnemy = [&](float x, float y, float speed, bool antiGrav = false) {
        float dx    = (rand() % 2 == 0) ? speed : -speed;
        auto  enemy = reg.create();
        reg.emplace<Transform>(enemy, x, y);
        reg.emplace<Velocity>(enemy, dx, 0.0f, speed);
        reg.emplace<AnimationState>(enemy, 0, (int)enemyWalkFrames.size(), 0.0f, 7.0f, true);
        reg.emplace<Renderable>(enemy, enemySheet->GetSurface(), enemyWalkFrames, false);
        reg.emplace<Collider>(enemy, SLIME_SPRITE_WIDTH, SLIME_SPRITE_HEIGHT);
        reg.emplace<EnemyTag>(enemy);
        Health eh; eh.current = SLIME_MAX_HEALTH; eh.max = SLIME_MAX_HEALTH;
        reg.emplace<Health>(enemy, eh);
        if (antiGrav) {
            reg.emplace<FloatTag>(enemy);
            FloatState fs;
            fs.baseY     = y;
            fs.bobAmp    = 5.0f + (rand() % 40) * 0.1f; // 5–9 px
            fs.bobSpeed  = 1.6f + (rand() % 60) * 0.01f; // 1.6–2.2 rad/s
            fs.bobPhase  = (rand() % 628) * 0.01f;        // 0–2π
            reg.emplace<FloatState>(enemy, fs);
        }
    };

    if (!mLevelPath.empty()) {
        // Level file loaded — spawn only what the level defines (may be zero)
        for (const auto& e : mLevel.enemies)
            spawnEnemy(e.x, e.y, e.speed, e.antiGravity);
    } else {
        // No level file — sandbox/freeplay mode, spawn random enemies
        for (int i = 0; i < GRAVITYSLUGSCOUNT; ++i) {
            float x     = static_cast<float>(rand() % (mWindow->GetWidth() - 100));
            float y     = static_cast<float>(rand() % (mWindow->GetHeight() - SLIME_SPRITE_HEIGHT));
            float speed = 60.0f + static_cast<float>(rand() % 120);
            spawnEnemy(x, y, speed);
        }
    }
}

void GameScene::Respawn() {
    reg.clear();
    for (auto* s : tileScaledSurfaces) SDL_DestroySurface(s);
    tileScaledSurfaces.clear();
    gameOver           = false;
    levelComplete      = false;
    levelCompleteTimer = 2.0f;
    coinCount          = 0;
    stompCount         = 0;
    mCamera            = Camera{}; // reset to origin; Spawn() will snap it to player
    Spawn();
}
