#include "GameScene.hpp"
#include "AnimatedTile.hpp"
#include "GameConfig.hpp"
#include "GameEvents.hpp"
#include "LevelTwo.hpp"
#include "PauseMenuScene.hpp"
#include "SurfaceUtils.hpp"
#include <SDL3_image/SDL_image.h>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <print>
namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Construction
// ─────────────────────────────────────────────────────────────────────────────

GameScene::GameScene(const std::string& levelPath, bool fromEditor, const std::string& profilePath)
    : mLevelPath(levelPath), mFromEditor(fromEditor), mProfilePath(profilePath) {}

// ─────────────────────────────────────────────────────────────────────────────
// Scene interface
// ─────────────────────────────────────────────────────────────────────────────

void GameScene::Load(Window& window) {
    mWindow  = &window;
    gameOver = false;

    // Load level from file if a path was provided
    if (!mLevelPath.empty())
        LoadLevel(mLevelPath, mLevel);

    // ── Player animation sheets ────────────────────────────────────────────────
    // If a PlayerProfile is given, load from its slot folders.
    // Otherwise fall back to the built-in frost knight sequences.
    PlayerProfile profile;
    bool useProfile = !mProfilePath.empty() && LoadPlayerProfile(mProfilePath, profile);

    // Sprite render dimensions — use profile override if set, else engine default
    const int KW = (useProfile && profile.spriteW > 0) ? profile.spriteW : PLAYER_SPRITE_WIDTH;
    const int KH = (useProfile && profile.spriteH > 0) ? profile.spriteH : PLAYER_SPRITE_HEIGHT;
    mPlayerSpriteW = KW;
    mPlayerSpriteH = KH;

    // Helper: build a SpriteSheet from a profile slot folder, falling back to
    // the knight default if the slot is empty or the folder doesn't exist.
    auto loadSlot = [&](PlayerAnimSlot slot,
                        const std::string& fallbackFolder,
                        const std::string& fallbackPrefix,
                        int fallbackCount) -> std::unique_ptr<SpriteSheet> {
        if (useProfile && profile.HasSlot(slot)) {
            const std::string& dir = profile.Slot(slot).folderPath;
            // Detect prefix + padDigits the same way PlayerCreatorScene does
            std::vector<fs::path> pngs;
            for (const auto& e : fs::directory_iterator(dir))
                if (e.path().extension() == ".png") pngs.push_back(e.path());
            if (!pngs.empty()) {
                std::sort(pngs.begin(), pngs.end());
                std::string stem = pngs[0].stem().string();
                int pad = 0;
                { int k = (int)stem.size()-1; while(k>=0&&std::isdigit((unsigned char)stem[k])){++pad;--k;} }
                std::string pfx = stem;
                while (!pfx.empty() && std::isdigit((unsigned char)pfx.back())) pfx.pop_back();
                std::string numPart = stem.substr(pfx.size());
                int startIdx = numPart.empty() ? 0 : std::stoi(numPart);
                return std::make_unique<SpriteSheet>(dir + "/", pfx, (int)pngs.size(), KW, KH, pad, startIdx);
            }
        }
        // Fallback: frost knight
        return std::make_unique<SpriteSheet>(
            "game_assets/frost_knight_png_sequences/" + fallbackFolder + "/",
            fallbackPrefix, fallbackCount, KW, KH, 3);
    };

    // Store per-slot fps so Spawn() can access them without capturing local vars
    mSlotFps.fill(0.0f);
    if (useProfile) {
        for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
            auto slot = static_cast<PlayerAnimSlot>(i);
            mSlotFps[i] = profile.Slot(slot).fps > 0.0f ? profile.Slot(slot).fps : 0.0f;
        }
    }

    knightIdleSheet  = loadSlot(PlayerAnimSlot::Idle,  "Idle",         "0_Knight_Idle_",        18);
    knightWalkSheet  = loadSlot(PlayerAnimSlot::Walk,  "Walking",      "0_Knight_Walking_",     24);
    knightHurtSheet  = loadSlot(PlayerAnimSlot::Hurt,  "Hurt",         "0_Knight_Hurt_",        12);
    knightJumpSheet  = loadSlot(PlayerAnimSlot::Jump,  "Jump Start",   "0_Knight_Jump Start_",   6);
    knightFallSheet  = loadSlot(PlayerAnimSlot::Fall,  "Falling Down", "0_Knight_Falling Down_", 6);
    knightSlideSheet = loadSlot(PlayerAnimSlot::Run,   "Sliding",      "0_Knight_Sliding_",      6);
    knightSlashSheet = loadSlot(PlayerAnimSlot::Slash, "Slashing",     "0_Knight_Slashing_",    12);

    // GetAnimation key: for profile slots use empty prefix fallback detection;
    // for knight fallback use the known prefix.
    auto getFrames = [&](std::unique_ptr<SpriteSheet>& sheet,
                         PlayerAnimSlot slot,
                         const std::string& knightKey) -> std::vector<SDL_Rect> {
        if (useProfile && profile.HasSlot(slot)) {
            // For profile slots the sheet was built with directory-scanned PNGs.
            // GetAnimation("") matches every key and the new trailing-digit sort
            // puts them in the right order regardless of prefix.
            auto all = sheet->GetAnimation("");
            if (!all.empty()) return all;
        }
        return sheet->GetAnimation(knightKey);
    };

    idleFrames  = getFrames(knightIdleSheet,  PlayerAnimSlot::Idle,  "0_Knight_Idle_");
    walkFrames  = getFrames(knightWalkSheet,  PlayerAnimSlot::Walk,  "0_Knight_Walking_");
    jumpFrames  = getFrames(knightJumpSheet,  PlayerAnimSlot::Jump,  "0_Knight_Jump Start_");
    hurtFrames  = getFrames(knightHurtSheet,  PlayerAnimSlot::Hurt,  "0_Knight_Hurt_");
    duckFrames  = getFrames(knightSlideSheet, PlayerAnimSlot::Run,   "0_Knight_Sliding_");
    frontFrames = getFrames(knightFallSheet,  PlayerAnimSlot::Fall,  "0_Knight_Falling Down_");
    slashFrames = getFrames(knightSlashSheet, PlayerAnimSlot::Slash, "0_Knight_Slashing_");

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
        // F1 — toggle hitbox debug overlay
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F1) {
            mDebugHitboxes = !mDebugHitboxes;
            return true;
        }
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

    MovingPlatformTick(reg, dt);       // move tiles, record vx — BEFORE everything
    FloatingSystem(reg, dt);
    LadderSystem(reg, dt);
    MovementSystem(reg, dt, mWindow->GetWidth());
    BoundsSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight(),
                 mLevel.gravityMode == GravityMode::WallRun,
                 mLevelW, mLevelH);
    PlayerStateSystem(reg);
    AnimationSystem(reg, dt);

    // Advance animated tile frames.
    // Each animated tile's r.frames has N identical rects ({0,0,w,h} per frame)
    // so anim->currentFrame is always a valid index into both r.frames and
    // the raw surface vector. We just advance the timer, update currentFrame,
    // and point rend->sheet at the matching surface. AnimationSystem never
    // touches these entities (TileAnimTag exclude) so there is no interference.
    for (auto& [ent, frames] : tileAnimFrameMap) {
        if (!reg.valid(ent) || frames.empty()) continue;
        auto* anim = reg.try_get<AnimationState>(ent);
        auto* rend = reg.try_get<Renderable>(ent);
        if (!anim || !rend) continue;
        float dur = (anim->fps > 0.0f) ? 1.0f / anim->fps : 0.125f;
        anim->timer += dt;
        while (anim->timer >= dur) {
            anim->timer -= dur;
            anim->currentFrame = (anim->currentFrame + 1) % (int)frames.size();
        }
        SDL_Surface* cur = frames[anim->currentFrame];
        if (cur) rend->sheet = cur;
    }

    // Tick down HitFlash timers on action tiles and remove when expired
    {
        auto flashView = reg.view<HitFlash>();
        std::vector<entt::entity> expired;
        flashView.each([&](entt::entity e, HitFlash& hf) {
            hf.timer -= dt;
            if (hf.timer <= 0.0f) expired.push_back(e);
        });
        for (auto e : expired) reg.remove<HitFlash>(e);
    }

    // CollisionSystem now returns a result instead of mutating our variables directly
    CollisionResult collision = CollisionSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight());
    MovingPlatformCarry(reg);           // carry player X AFTER floor snap — exact onTop check
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
        hView.each([&](entt::entity playerEnt, Health& hp, HazardState& hz,
                       AnimationState& anim, Renderable& r,
                       const AnimationSet& set) {
            hz.active = collision.onHazard;
            if (hz.active) {
                // Drain HP
                hp.current -= HAZARD_DAMAGE_PER_SEC * dt;
                if (hp.current <= 0.0f) { hp.current = 0.0f; gameOver = true; }

                // Advance flash timer — 8 Hz pulse
                hz.flashTimer += dt;

                // Force hurt animation while on hazard, and cancel any
                // in-progress attack so isAttacking never gets stuck true.
                if (anim.currentAnim != AnimationID::HURT) {
                    if (auto* atk = reg.try_get<AttackState>(playerEnt)) {
                        atk->isAttacking   = false;
                        atk->attackPressed = false;
                    }
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

        // ── Debug hitbox overlay (F1) ───────────────────────────────────────
        if (mDebugHitboxes) {
            SDL_Surface* s = window.GetSurface();

            // Alpha-blended fill: create a small overlay surface, set blend mode,
            // then blit-scale it onto the screen rect so alpha is respected.
            auto fill = [&](SDL_Rect r, Uint8 ri, Uint8 gi, Uint8 bi, Uint8 ai) {
                SDL_Surface* ov = SDL_CreateSurface(r.w, r.h, SDL_PIXELFORMAT_ARGB8888);
                if (!ov) return;
                SDL_SetSurfaceBlendMode(ov, SDL_BLENDMODE_BLEND);
                const SDL_PixelFormatDetails* f = SDL_GetPixelFormatDetails(ov->format);
                SDL_FillSurfaceRect(ov, nullptr, SDL_MapRGBA(f, nullptr, ri, gi, bi, ai));
                SDL_BlitSurface(ov, nullptr, s, &r);
                SDL_DestroySurface(ov);
            };
            // Solid 1px outline — no blending needed for lines
            auto outline = [&](SDL_Rect r, Uint8 ri, Uint8 gi, Uint8 bi) {
                SDL_Surface* ov = SDL_CreateSurface(r.w, r.h, SDL_PIXELFORMAT_ARGB8888);
                if (!ov) return;
                SDL_SetSurfaceBlendMode(ov, SDL_BLENDMODE_BLEND);
                const SDL_PixelFormatDetails* f = SDL_GetPixelFormatDetails(ov->format);
                SDL_FillSurfaceRect(ov, nullptr, SDL_MapRGBA(f, nullptr, 0, 0, 0, 0));
                Uint32 c = SDL_MapRGBA(f, nullptr, ri, gi, bi, 255);
                SDL_Rect sides[4] = {
                    {0, 0,         r.w, 1},
                    {0, r.h-1,     r.w, 1},
                    {0, 0,         1,   r.h},
                    {r.w-1, 0,     1,   r.h}
                };
                for (auto& sr : sides) SDL_FillSurfaceRect(ov, &sr, c);
                SDL_BlitSurface(ov, nullptr, s, &r);
                SDL_DestroySurface(ov);
            };

            // Player — cyan
            {
                auto pv = reg.view<PlayerTag, Transform, Collider>();
                pv.each([&](const Transform& t, const Collider& c) {
                    int sx = (int)(t.x - mCamera.x);
                    int sy = (int)(t.y - mCamera.y);
                    SDL_Rect r = {sx, sy, c.w, c.h};
                    fill(r, 0, 255, 255, 50);
                    outline(r, 0, 255, 255);
                    Text lbl(std::to_string(c.w) + "x" + std::to_string(c.h),
                             SDL_Color{0,255,255,255}, sx+2, sy+2, 10);
                    lbl.Render(s);
                });
            }

            // Solid tiles — white
            {
                auto tv = reg.view<TileTag, Transform, Collider>();
                tv.each([&](entt::entity te, const Transform& t, const Collider& c) {
                    float tx = t.x, ty = t.y;
                    if (const auto* off = reg.try_get<ColliderOffset>(te)) { tx += off->x; ty += off->y; }
                    int sx = (int)(tx - mCamera.x), sy = (int)(ty - mCamera.y);
                    SDL_Rect r = {sx, sy, c.w, c.h};
                    fill(r, 255, 255, 255, 18);
                    outline(r, 160, 160, 255);
                });
            }

            // Hazard tiles — red
            {
                auto hv = reg.view<HazardTag, Transform, Collider>();
                hv.each([&](entt::entity he, const Transform& t, const Collider& c) {
                    float hx = t.x, hy = t.y;
                    if (const auto* off = reg.try_get<ColliderOffset>(he)) { hx += off->x; hy += off->y; }
                    int sx = (int)(hx - mCamera.x), sy = (int)(hy - mCamera.y);
                    SDL_Rect r = {sx, sy, c.w, c.h};
                    fill(r, 255, 40, 40, 60);
                    outline(r, 255, 40, 40);
                });
            }

            // Ladder tiles — green outline only
            {
                auto lv = reg.view<LadderTag, Transform, Collider>();
                lv.each([&](const Transform& t, const Collider& c) {
                    SDL_Rect r = {(int)(t.x - mCamera.x), (int)(t.y - mCamera.y), c.w, c.h};
                    outline(r, 60, 255, 60);
                });
            }

            // Enemies — orange
            {
                auto ev = reg.view<EnemyTag, Transform, Collider>(entt::exclude<DeadTag>);
                ev.each([&](const Transform& t, const Collider& c) {
                    SDL_Rect r = {(int)(t.x - mCamera.x), (int)(t.y - mCamera.y), c.w, c.h};
                    fill(r, 255, 140, 0, 45);
                    outline(r, 255, 140, 0);
                });
            }

            // HUD hint bar
            SDL_Rect hintBg = {0, window.GetHeight()-20, window.GetWidth(), 20};
            fill(hintBg, 0, 0, 0, 140);
            Text hint("[F1] Hitboxes  Cyan=Player  White=Solid  Red=Hazard  Green=Ladder  Orange=Enemy",
                      SDL_Color{220, 220, 220, 255}, 8, window.GetHeight()-16, 11);
            hint.Render(s);
        }

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
        // For moving platforms, extend bounds to include the full travel range
        // so BoundsSystem never clamps the player before the platform reaches
        // its rightmost/bottommost position.
        if (ts.moving) {
            if (ts.moveHoriz) {
                float travelRight = ts.moveLoop
                    ? ts.x + ts.moveRange + ts.w   // loop: travels originX → originX+range
                    : ts.x + ts.moveRange + ts.w;  // sine: originX ± range, rightmost = +range
                right = std::max(right, travelRight);
            } else {
                float travelBottom = ts.y + ts.moveRange + ts.h;
                bottom = std::max(bottom, travelBottom);
            }
        }
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
    // Derive collider + render-offset from the resolved sprite size.
    // If the profile has an explicit hitbox on the Idle slot, use it directly
    // (values are in sprite-local px, so x/y become the render offset insets).
    // Otherwise fall back to proportionally-scaled frost-knight insets.
    int pColW, pColH, pROffX, pROffY;
    {
        // Try to load the profile to read the Idle hitbox
        PlayerProfile tmpProfile;
        bool hasProfile = !mProfilePath.empty() && LoadPlayerProfile(mProfilePath, tmpProfile);
        const AnimHitbox& idleHB = hasProfile
            ? tmpProfile.Slot(PlayerAnimSlot::Idle).hitbox
            : AnimHitbox{};

        if (!idleHB.IsDefault()) {
            // Profile hitbox is in sprite-local px.
            // Strategy: Transform = sprite top-left, ColliderOffset shifts the
            // physics box to (hb.x, hb.y) within the sprite. RenderOffset = 0.
            // CollisionSystem already handles ColliderOffset for tiles; the
            // player path in CollisionSystem uses pt directly (no offset), so
            // we keep Transform = collider top-left for physics but adjust
            // playerX/Y so the sprite-origin lands at the right screen position.
            //
            // Simplest correct model that requires NO CollisionSystem changes:
            //   Transform  = collider top-left  (physics origin, unchanged)
            //   RenderOffset.x = -hb.x          (sprite draws hb.x left of collider)
            //   RenderOffset.y = -(spriteH - hb.h - (spriteH - hb.y - hb.h))
            //                  = -hb.y           ... but bottom-align overrides this:
            //   Bottom-align: sprite bottom == collider bottom
            //     roff.y = hb.h - spriteH        (negative: sprite top is above collider top)
            //   Horizontal-center: sprite center == collider center
            //     roff.x = hb.w/2 - spriteW/2 + (hb.x - hb.w/2) ... simplifies to:
            //     roff.x = hb.x - (spriteW - hb.w) / 2 ... no, just use -hb.x
            //     Actually: collider left = transform.x
            //               sprite left  = transform.x + roff.x
            //               hb.x pixels into sprite = collider left
            //               => transform.x + roff.x + hb.x = transform.x
            //               => roff.x = -hb.x  ✓
            pColW  = idleHB.w;
            pColH  = idleHB.h;
            // X: sprite-pixel hb.x aligns with collider left
            //    transform.x + roff.x + hb.x = transform.x  =>  roff.x = -hb.x
            pROffX = -idleHB.x;
            // Y: sprite-pixel hb.y aligns with collider top
            //    transform.y + roff.y + hb.y = transform.y  =>  roff.y = -hb.y
            pROffY = -idleHB.y;
        } else {
            // Fallback: scale frost-knight insets proportionally
            const float scaleX    = (float)mPlayerSpriteW / PLAYER_SPRITE_WIDTH;
            const float scaleY    = (float)mPlayerSpriteH / PLAYER_SPRITE_HEIGHT;
            const int   pInsetX   = (int)(PLAYER_BODY_INSET_X      * scaleX);
            const int   pInsetTop = (int)(PLAYER_BODY_INSET_TOP    * scaleY);
            const int   pInsetBot = (int)(PLAYER_BODY_INSET_BOTTOM * scaleY);
            pColW  = mPlayerSpriteW - pInsetX * 2;
            pColH  = mPlayerSpriteH - pInsetTop - pInsetBot;
            pROffX = -pInsetX;
            pROffY = -pInsetTop;
        }
    }

    float playerX = mLevelPath.empty()
                      ? (float)(mWindow->GetWidth() / 2 - pColW / 2)
                      : mLevel.player.x;
    float playerY = mLevelPath.empty()
                      ? (float)(mWindow->GetHeight() - pColH)
                      : mLevel.player.y;

    auto player = reg.create();
    reg.emplace<Transform>(player, playerX, playerY);
    reg.emplace<Velocity>(player);
    reg.emplace<AnimationState>(player, 0, (int)idleFrames.size(), 0.0f, 10.0f, true);
    reg.emplace<Renderable>(player, knightIdleSheet->GetSurface(), idleFrames, false);
    reg.emplace<PlayerTag>(player);
    reg.emplace<Health>(player);
    reg.emplace<Collider>(player, pColW, pColH);
    reg.emplace<RenderOffset>(player, pROffX, pROffY);
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
    // Helper to resolve per-slot FPS: use profile value when > 0, else 0 (PlayerStateSystem uses its own default)
    auto slotFps = [&](PlayerAnimSlot slot) -> float {
        return mSlotFps[static_cast<int>(slot)];
    };
    reg.emplace<AnimationSet>(player, AnimationSet{
        .idle       = idleFrames,  .idleSheet  = knightIdleSheet->GetSurface(),  .idleFps  = slotFps(PlayerAnimSlot::Idle),
        .walk       = walkFrames,  .walkSheet  = knightWalkSheet->GetSurface(),  .walkFps  = slotFps(PlayerAnimSlot::Walk),
        .jump       = jumpFrames,  .jumpSheet  = knightJumpSheet->GetSurface(),  .jumpFps  = slotFps(PlayerAnimSlot::Jump),
        .hurt       = hurtFrames,  .hurtSheet  = knightHurtSheet->GetSurface(),  .hurtFps  = slotFps(PlayerAnimSlot::Hurt),
        .duck       = duckFrames,  .duckSheet  = knightSlideSheet->GetSurface(), .duckFps  = slotFps(PlayerAnimSlot::Run),
        .front      = frontFrames, .frontSheet = knightFallSheet->GetSurface(),  .frontFps = slotFps(PlayerAnimSlot::Fall),
        .slash      = slashFrames, .slashSheet = knightSlashSheet->GetSurface(), .slashFps = slotFps(PlayerAnimSlot::Slash),
    });

    // ── Spawn tiles — only from level file ────────────────────────────────────
    for (const auto& ts : mLevel.tiles) {

        // ── Animated tile: load all frames, register runtime state ─────────
        if (IsAnimatedTile(ts.imagePath)) {
            AnimatedTileDef def;
            if (!LoadAnimatedTileDef(ts.imagePath, def) || def.framePaths.empty()) {
                std::print("Failed to load animated tile def: {}\n", ts.imagePath);
                continue;
            }

            // Load and scale all frames to tile dimensions
            std::vector<SDL_Surface*> frameSurfs;
            for (const auto& fp : def.framePaths) {
                SDL_Surface* raw = IMG_Load(fp.c_str());
                if (!raw) { frameSurfs.push_back(nullptr); continue; }
                SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
                SDL_DestroySurface(raw);
                if (!conv) { frameSurfs.push_back(nullptr); continue; }
                SDL_Surface* scaled = SDL_CreateSurface(ts.w, ts.h, SDL_PIXELFORMAT_ARGB8888);
                if (scaled) {
                    SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_NONE);
                    SDL_BlitSurfaceScaled(conv, nullptr, scaled, nullptr, SDL_SCALEMODE_LINEAR);
                    SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);
                    if (ts.rotation != 0) {
                        SDL_Surface* rot = RotateSurfaceDeg(scaled, ts.rotation);
                        if (rot) { SDL_DestroySurface(scaled); scaled = rot; }
                    }
                }
                SDL_DestroySurface(conv);
                frameSurfs.push_back(scaled);
            }

            if (frameSurfs.empty() || !frameSurfs[0]) {
                for (auto* s : frameSurfs) if (s) SDL_DestroySurface(s);
                continue;
            }

            // Build frame rects (each surface is exactly ts.w x ts.h, so rect is always {0,0,w,h})
            std::vector<SDL_Rect> frameRects;
            for (auto* s : frameSurfs)
                frameRects.push_back({0, 0, ts.w, ts.h});

            // Store ALL frame surfaces in tileScaledSurfaces so they get freed on Unload
            for (auto* s : frameSurfs)
                if (s) tileScaledSurfaces.push_back(s);

            // Use frame 0 as the initial sheet pointer for Renderable
            // AnimatedTileState drives frame advancement each Update tick
            auto tile = reg.create();
            reg.emplace<Transform>(tile, ts.x, ts.y);

            bool hasCustomHitbox = (ts.hitboxW > 0 || ts.hitboxH > 0);
            int  colW = hasCustomHitbox ? (ts.hitboxW > 0 ? ts.hitboxW : ts.w) : ts.w;
            int  colH = hasCustomHitbox ? (ts.hitboxH > 0 ? ts.hitboxH : ts.h) : ts.h;

            if (ts.ladder)                         reg.emplace<LadderTag>(tile);
            else if (ts.slope != SlopeType::None) { reg.emplace<TileTag>(tile); reg.emplace<SlopeCollider>(tile, ts.slope, ts.slopeHeightFrac); }
            else if (ts.hazard)                   { reg.emplace<HazardTag>(tile); if (!ts.prop) reg.emplace<TileTag>(tile); }
            else if (!ts.prop)                      reg.emplace<TileTag>(tile);
            if (ts.prop)                            reg.emplace<PropTag>(tile);
            // Action tag is independent — always emitted when set, but TileTag
            // (solid collision) is only added above when prop=false.
            if (ts.action)                          reg.emplace<ActionTag>(tile, ts.actionGroup, ts.actionHits, ts.actionHits);

            if (!ts.prop) reg.emplace<Collider>(tile, colW, colH);
            // Emit ColliderOffset whenever any hitbox customization exists,
            // even if offset is 0,0 but size was trimmed — so the collision
            // system can trust the Collider size came from editor data.
            if (hasCustomHitbox)
                reg.emplace<ColliderOffset>(tile, ts.hitboxOffX, ts.hitboxOffY);

            // Build N identical frame rects (one per surface) so anim->currentFrame
            // is always a valid index into both r.frames and our frameSurfs vector.
            // RenderSystem reads r.frames[anim->currentFrame] for the src rect and
            // r.sheet for the surface -- we swap r.sheet each tick in Update.
            std::vector<SDL_Rect> animFrameRects((int)frameSurfs.size(),
                                                  SDL_Rect{0, 0, ts.w, ts.h});
            reg.emplace<TileAnimTag>(tile);
            reg.emplace<Renderable>(tile, frameSurfs[0], std::move(animFrameRects), false);
            reg.emplace<AnimationState>(tile, 0, (int)frameSurfs.size(), 0.0f, def.fps, true);
            tileAnimFrameMap[tile] = std::move(frameSurfs);
            continue;
        }

        // ── Normal PNG tile ────────────────────────────────────────────────
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
        } else if (ts.slope != SlopeType::None) {
            reg.emplace<TileTag>(tile);
            reg.emplace<Collider>(tile, colW, colH);
            reg.emplace<SlopeCollider>(tile, ts.slope, ts.slopeHeightFrac);
        } else if (ts.hazard) {
            // Hazard: damages the player on overlap. Also solid (TileTag) unless
            // prop is set, in which case the player can walk through it.
            reg.emplace<HazardTag>(tile);
            reg.emplace<Collider>(tile, colW, colH);
            if (!ts.prop) reg.emplace<TileTag>(tile);
        } else {
            reg.emplace<Collider>(tile, colW, colH);
            if (!ts.prop) reg.emplace<TileTag>(tile);
        }
        // Prop and Action are orthogonal to the collision type above.
        // prop=true  -> visual only, walk-through regardless of action
        // action=true -> slashable; solid by default, walk-through if prop also set
        if (ts.prop)   reg.emplace<PropTag>(tile);
        if (ts.action) reg.emplace<ActionTag>(tile, ts.actionGroup, ts.actionHits, ts.actionHits);

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

        // Moving platform
        if (ts.moving) {
            reg.emplace<MovingPlatformTag>(tile);
            MovingPlatformState mps;
            mps.horiz     = ts.moveHoriz;
            mps.range     = ts.moveRange;
            mps.speed     = ts.moveSpeed;
            mps.groupId   = ts.moveGroupId;
            mps.originX   = ts.x;
            mps.originY   = ts.y;
            mps.loop      = ts.moveLoop;
            mps.trigger   = ts.moveTrigger;
            mps.triggered = false;
            // Use authored phase/direction from the editor.
            // movePhase is a 0..1 fraction of range for loop, 0..1 fraction of 2pi for sine.
            if (ts.moveLoop) {
                mps.phase   = ts.movePhase * ts.moveRange; // 0..1 -> pixels into range
                mps.loopDir = ts.moveLoopDir;              // +1 or -1
                // Position tile at its authored start
                if (ts.moveHoriz)
                    reg.get<Transform>(tile).x = ts.x + mps.phase;
            } else {
                mps.phase   = ts.movePhase * 6.28318f;     // 0..1 -> radians
                mps.loopDir = 1;
            }
            reg.emplace<MovingPlatformState>(tile, mps);
        }

        // Attach offset component whenever hitbox was customized (even if offset is 0,0
        // but size was trimmed) so CollisionSystem can trust Collider came from editor data.
        if (hasCustomHitbox)
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
    tileAnimFrameMap.clear(); // entity handles are invalid after reg.clear()
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
