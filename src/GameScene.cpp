#include "GameScene.hpp"
#include "AnimatedTile.hpp"
#include "EnemyProfile.hpp"
#include "GameConfig.hpp"
#include "GameEvents.hpp"
#include "LevelEditorScene.hpp"

#include "SurfaceUtils.hpp"
#include "TitleScene.hpp"
#include "audio/AudioEngine.hpp"
#include "audio/AudioEvents.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <print>
#include <thread>
#include <unordered_map>
#include <unordered_set>
namespace fs = std::filesystem;

static const std::vector<SDL_Rect> sBulletFrames = {{0, 0, 8, 8}};

static std::string TileCacheKey(const std::string& path, int w, int h, int rot) {
    return path + '|' + std::to_string(w) + 'x' + std::to_string(h) + "|r" +
           std::to_string(rot);
}

std::vector<SDL_Texture*>                     GameScene::sTileScaledTextures;
std::unordered_map<std::string, SDL_Texture*> GameScene::sTileTextureCache;
std::unordered_map<std::string, SDL_Surface*> GameScene::sRawSurfaceCache;

SDL_Surface* GameScene::GetRawSurface(const std::string& path) {
    auto it = sRawSurfaceCache.find(path);
    if (it != sRawSurfaceCache.end())
        return it->second;
    SDL_Surface* raw = IMG_Load(path.c_str());
    if (!raw) {
        std::print("Failed to load tile: {}\n", path);
        return nullptr;
    }
    SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(raw);
    if (conv)
        SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_BLEND);
    sRawSurfaceCache[path] = conv;
    return conv;
}

static SDL_Texture* LoadScaledTexture(
    SDL_Renderer* ren, const std::string& path, int tw, int th, int rotation = 0) {
    SDL_Surface* src = GameScene::GetRawSurface(path);
    if (!src)
        return nullptr;

    // Work on a copy since rotation/scale are destructive.
    SDL_Surface* work = SDL_DuplicateSurface(src);
    if (!work)
        return nullptr;

    SDL_SetSurfaceBlendMode(work, SDL_BLENDMODE_BLEND);

    if (rotation != 0) {
        SDL_Surface* rot = RotateSurfaceDeg(work, rotation);
        if (rot) {
            SDL_DestroySurface(work);
            work = rot;
        }
    }

    if (tw > 0 && th > 0 && (work->w != tw || work->h != th)) {
        SDL_Surface* scaled = SDL_CreateSurface(tw, th, SDL_PIXELFORMAT_ARGB8888);
        if (scaled) {
            SDL_SetSurfaceBlendMode(work, SDL_BLENDMODE_NONE);
            SDL_BlitSurfaceScaled(work, nullptr, scaled, nullptr, SDL_SCALEMODE_NEAREST);
            SDL_DestroySurface(work);
            work = scaled;
        }
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, work);
    SDL_DestroySurface(work);
    if (tex) {
        SDL_SetTextureScaleMode(tex, (tw <= 0 || th <= 0)
                                         ? SDL_SCALEMODE_LINEAR
                                         : SDL_SCALEMODE_NEAREST);
    }
    return tex;
}

SDL_Texture* GameScene::GetCachedTexture(SDL_Renderer* ren, const std::string& path,
                                         int w, int h, int rot) {
    std::string key = TileCacheKey(path, w, h, rot);
    auto it = sTileTextureCache.find(key);
    if (it != sTileTextureCache.end())
        return it->second;
    SDL_Texture* tex = LoadScaledTexture(ren, path, w, h, rot);
    if (tex) {
        sTileScaledTextures.push_back(tex);
        sTileTextureCache[key] = tex;
    }
    return tex;
}

void GameScene::ClearTextureCache() {
    for (auto* t : sTileScaledTextures)
        SDL_DestroyTexture(t);
    sTileScaledTextures.clear();
    sTileTextureCache.clear();
    for (auto& [k, s] : sRawSurfaceCache)
        if (s) SDL_DestroySurface(s);
    sRawSurfaceCache.clear();
}

void GameScene::PreloadRawSurfaces(const Level& level) {
    std::unordered_set<std::string> seen;
    std::vector<std::string> pending;

    auto enqueue = [&](const std::string& p) {
        if (!p.empty() && !seen.count(p) && !sRawSurfaceCache.count(p)) {
            seen.insert(p);
            pending.push_back(p);
        }
    };

    for (const auto& ts : level.tiles) enqueue(ts.imagePath);
    enqueue(level.background);
    for (const auto& pl : level.parallaxLayers) enqueue(pl.imagePath);

    if (pending.empty()) return;

    const int hw = std::max(1, (int)std::thread::hardware_concurrency());
    const int batch = std::max(1, (int)pending.size() / hw);

    std::vector<std::future<std::vector<std::pair<std::string, SDL_Surface*>>>> futures;
    for (int i = 0; i < (int)pending.size(); i += batch) {
        int end = std::min(i + batch, (int)pending.size());
        std::vector<std::string> slice(pending.begin() + i, pending.begin() + end);
        futures.push_back(std::async(std::launch::async, [slice = std::move(slice)]() {
            std::vector<std::pair<std::string, SDL_Surface*>> out;
            out.reserve(slice.size());
            for (const auto& p : slice) {
                SDL_Surface* raw = IMG_Load(p.c_str());
                if (!raw) continue;
                SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
                SDL_DestroySurface(raw);
                if (conv) SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_BLEND);
                out.emplace_back(p, conv);
            }
            return out;
        }));
    }

    for (auto& f : futures)
        for (auto& [p, s] : f.get())
            if (s) sRawSurfaceCache.emplace(p, s);

    std::print("PreloadRawSurfaces: {} assets across {} threads\n",
               pending.size(), std::min(hw, (int)pending.size()));
}

// --- Construction ---
GameScene::GameScene(const std::string& levelPath,
                     bool               fromEditor,
                     const std::string& profilePath)
    : mLevelPath(levelPath)
    , mFromEditor(fromEditor)
    , mProfilePath(profilePath) {}

// --- Scene interface ---
void GameScene::Load(Window& window) {
    mWindow           = &window;
    gameOver          = false;
    mPlayerDying      = false;
    mDeathAnimTimer   = 0.0f;
    SDL_Renderer* ren = window.GetRenderer();

    if (!mLevelPath.empty()) {
        std::string binPath = flvl::BinPath(mLevelPath);
        if (!flvl::LoadLevelBin(binPath, mLevel))
            LoadLevel(mLevelPath, mLevel);
    }

    PreloadRawSurfaces(mLevel);

    PlayerProfile profile;
    bool useProfile = !mProfilePath.empty() && LoadPlayerProfile(mProfilePath, profile);
    mHasProfile     = useProfile;

    const int KW =
        (useProfile && profile.spriteW > 0) ? profile.spriteW : PLAYER_SPRITE_WIDTH;
    const int KH =
        (useProfile && profile.spriteH > 0) ? profile.spriteH : PLAYER_SPRITE_HEIGHT;
    mPlayerSpriteW = KW;
    mPlayerSpriteH = KH;

    auto loadSlot = [&](PlayerAnimSlot     slot,
                        const std::string& fallbackFolder,
                        const std::string& fallbackPrefix,
                        int                fallbackCount) -> std::unique_ptr<SpriteSheet> {
        if (useProfile && profile.HasSlot(slot)) {
            const std::string&    dir = profile.Slot(slot).folderPath;
            std::vector<fs::path> pngs;
            for (const auto& e : fs::directory_iterator(dir))
                if (e.path().extension() == ".png")
                    pngs.push_back(e.path());
            if (!pngs.empty()) {
                std::sort(pngs.begin(), pngs.end());
                // Explicit sorted paths — no prefix filtering, so slot reuse works correctly.
                std::vector<std::string> pathStrs;
                pathStrs.reserve(pngs.size());
                for (const auto& p : pngs)
                    pathStrs.push_back(p.string());
                return std::make_unique<SpriteSheet>(pathStrs, KW, KH);
            }
        }
        // Fallback to frost knight at profile-configured dimensions (or native 120x160).
        return std::make_unique<SpriteSheet>(
            "game_assets/frost_knight_png_sequences/" + fallbackFolder + "/",
            fallbackPrefix,
            fallbackCount,
            KW,
            KH,
            3);
    };

    mSlotFps.fill(0.0f);
    mSlotSfxNext.fill(0);
    for (auto& v : mSlotSfx) v.clear();
    if (useProfile) {
        for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
            auto slot = static_cast<PlayerAnimSlot>(i);
            if (profile.HasFps(slot))
                mSlotFps[i] = profile.Slot(slot).fps;
            for (const auto& e : profile.slots[i].sfx)
                mSlotSfx[i].push_back({e.volume, e.timeStretch, e.trimStart, e.trimEnd});
        }
    }

    knightIdleSheet = loadSlot(PlayerAnimSlot::Idle, "Idle", "0_Knight_Idle_", 18);
    knightWalkSheet = loadSlot(PlayerAnimSlot::Walk, "Walking", "0_Knight_Walking_", 24);
    knightHurtSheet = loadSlot(PlayerAnimSlot::Hurt, "Hurt", "0_Knight_Hurt_", 12);
    knightJumpSheet =
        loadSlot(PlayerAnimSlot::Jump, "Jump Start", "0_Knight_Jump Start_", 6);
    knightFallSheet =
        loadSlot(PlayerAnimSlot::Fall, "Falling Down", "0_Knight_Falling Down_", 6);
    knightSlideSheet = loadSlot(PlayerAnimSlot::Crouch, "Sliding", "0_Knight_Sliding_", 6);
    knightSlashSheet = loadSlot(PlayerAnimSlot::Slash, "Slashing", "0_Knight_Slashing_", 12);
    knightDeathSheet = loadSlot(PlayerAnimSlot::Death, "Hurt", "0_Knight_Hurt_", 12);

    knightIdleSheet->CreateTexture(ren);
    knightIdleSheet->FreeSurface();
    knightWalkSheet->CreateTexture(ren);
    knightWalkSheet->FreeSurface();
    knightHurtSheet->CreateTexture(ren);
    knightHurtSheet->FreeSurface();
    knightJumpSheet->CreateTexture(ren);
    knightJumpSheet->FreeSurface();
    knightFallSheet->CreateTexture(ren);
    knightFallSheet->FreeSurface();
    knightSlideSheet->CreateTexture(ren);
    knightSlideSheet->FreeSurface();
    knightSlashSheet->CreateTexture(ren);
    knightSlashSheet->FreeSurface();
    knightDeathSheet->CreateTexture(ren);
    knightDeathSheet->FreeSurface();

    auto getFrames = [&](std::unique_ptr<SpriteSheet>& sheet,
                         PlayerAnimSlot                slot,
                         const std::string&            knightKey) -> std::vector<SDL_Rect> {
        // Custom sprites: use all frames. Frost knight fallback: use named key
        // (GetAnimation("") would match every frame on the fallback sheet).
        if (useProfile && profile.HasSlot(slot)) {
            return sheet->GetAnimation("");
        }
        return sheet->GetAnimation(knightKey);
    };

    idleFrames  = getFrames(knightIdleSheet, PlayerAnimSlot::Idle, "0_Knight_Idle_");
    walkFrames  = getFrames(knightWalkSheet, PlayerAnimSlot::Walk, "0_Knight_Walking_");
    jumpFrames  = getFrames(knightJumpSheet, PlayerAnimSlot::Jump, "0_Knight_Jump Start_");
    hurtFrames  = getFrames(knightHurtSheet, PlayerAnimSlot::Hurt, "0_Knight_Hurt_");
    duckFrames  = getFrames(knightSlideSheet, PlayerAnimSlot::Crouch, "0_Knight_Sliding_");
    frontFrames = getFrames(knightFallSheet, PlayerAnimSlot::Fall, "0_Knight_Falling Down_");
    slashFrames = getFrames(knightSlashSheet, PlayerAnimSlot::Slash, "0_Knight_Slashing_");
    deathFrames = getFrames(knightDeathSheet, PlayerAnimSlot::Death, "0_Knight_Hurt_");

    // Redirect unfilled slots to idle frames to avoid flashing to frost-knight sprites.
    if (useProfile && !idleFrames.empty()) {
        if (!profile.HasSlot(PlayerAnimSlot::Walk) && walkFrames.empty())
            walkFrames = idleFrames;
        if (!profile.HasSlot(PlayerAnimSlot::Jump) && jumpFrames.empty())
            jumpFrames = idleFrames;
        if (!profile.HasSlot(PlayerAnimSlot::Hurt) && hurtFrames.empty())
            hurtFrames = idleFrames;
        if (!profile.HasSlot(PlayerAnimSlot::Crouch) && duckFrames.empty())
            duckFrames = idleFrames;
        if (!profile.HasSlot(PlayerAnimSlot::Fall) && frontFrames.empty())
            frontFrames = idleFrames;
        if (!profile.HasSlot(PlayerAnimSlot::Slash) && slashFrames.empty())
            slashFrames = idleFrames;
        if (!profile.HasSlot(PlayerAnimSlot::Death) && deathFrames.empty())
            deathFrames = hurtFrames;
    }
    // For non-profile characters, fall back death to hurt frames
    if (deathFrames.empty())
        deathFrames = hurtFrames;

    enemySheet = std::make_unique<SpriteSheet>(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");
    enemySheet->CreateTexture(ren);
    enemyWalkFrames = enemySheet->GetAnimation("slimeWalk");

    std::string bgPath = (!mLevelPath.empty() && !mLevel.background.empty())
                             ? mLevel.background
                             : "game_assets/backgrounds/deepspace_scene.png";
    background = std::make_unique<Image>(bgPath, FitModeFromString(mLevel.bgFitMode));
    background->SetRepeat(mLevel.bgRepeat);

    mParallaxImages.clear();
    mParallaxFactors.clear();
    for (const auto& pl : mLevel.parallaxLayers) {
        if (pl.imagePath.empty()) continue;
        auto img = std::make_unique<Image>(pl.imagePath, FitMode::SCROLL);
        img->SetRepeat(true);
        mParallaxImages.push_back(std::move(img));
        mParallaxFactors.push_back(pl.scrollFactor);
    }

    locationText = std::make_unique<Text>("You are in space!!", 20, 20);
    actionText   = std::make_unique<Text>(
        "Collect all goals to complete the level!", SDL_Color{255, 255, 255, 0}, 20, 80, 20);

    {
        int cx = window.GetWidth() / 2;
        int cy = window.GetHeight() / 2;

        auto goSize = Text::Measure("Game Over!", 64);
        gameOverText = std::make_unique<Text>("Game Over!",
                                              SDL_Color{255, 0, 0, 255},
                                              cx - goSize.x / 2,
                                              cy - 80,
                                              64);

        int btnW = 160, btnH = 50;
        int btnY = cy + goSize.y / 2 - 50;
        retryBtnRect = {cx - btnW / 2, btnY, btnW, btnH};
        retryButton  = std::make_unique<Rectangle>(retryBtnRect);
        retryButton->SetColor({255, 255, 255, 255});
        retryButton->SetHoverColor({180, 180, 180, 255});

        auto btnTxtPos = Text::CenterInRect("Retry", 32, retryBtnRect);
        retryBtnText = std::make_unique<Text>("Retry",
                                              SDL_Color{0, 0, 0, 255},
                                              btnTxtPos.x, btnTxtPos.y,
                                              32);

        std::string hintStr = "R  or  A  to Retry";
        auto hintSize = Text::Measure(hintStr, 22);
        retryKeyText = std::make_unique<Text>(hintStr,
                                              SDL_Color{200, 200, 200, 255},
                                              cx - hintSize.x / 2,
                                              btnY + btnH + 18,
                                              22);
    }

    {
        auto lcSize = Text::Measure("Level Complete!", 64);
        levelCompleteText = std::make_unique<Text>("Level Complete!",
                                                   SDL_Color{255, 215, 0, 255},
                                                   window.GetWidth() / 2 - lcSize.x / 2,
                                                   window.GetHeight() / 2 - lcSize.y / 2,
                                                   64);
    }
    // --- Audio: load level music and player animation SFX ---
    if (Audio() && Audio()->IsReady()) {
        // Level music
        Audio()->StartLevelMusic(mLevel.musicPath, mLevel.musicVolume, 500);

        // Player animation SFX from profile
        if (useProfile) {
            for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
                const auto& slot = profile.slots[i];
                for (int fi = 0; fi < (int)slot.sfx.size(); ++fi) {
                    auto sfxId = audio::PlayerSlotSfxId(i, fi);
                    std::print("[Audio] Loading slot {} file {} sfxId='{}' path='{}'\n",
                               i, fi, sfxId, slot.sfx[fi].path);
                    if (!sfxId.empty()) {
                        bool ok = Audio()->Sfx().Load(sfxId, slot.sfx[fi].path);
                        std::print("[Audio]   -> {}\n", ok ? "OK" : "FAILED");
                    }
                }
            }
        } else {
            std::print("[Audio] No profile loaded, skipping SFX\n");
        }
    }

    Spawn();
}

void GameScene::Unload() {
    if (Audio() && Audio()->IsReady()) {
        Audio()->StopLevelMusic(300);
        Audio()->Sfx().UnloadAll();
    }
    reg.clear();
    tileAnimFrameMap.clear();
    mSortedTileRenderList.clear();
    mTileGrid.Clear();
    if (mBulletTex) { SDL_DestroyTexture(mBulletTex); mBulletTex = nullptr; }
    mWindow = nullptr;
}

std::unique_ptr<Scene> GameScene::NextScene() {
    if (mGoBackFromPause) {
        mGoBackFromPause = false;
        if (mFromEditor)
            return std::make_unique<LevelEditorScene>(mLevelPath, false, "", mProfilePath);
        else
            return std::make_unique<TitleScene>();
    }
    if (levelComplete && levelCompleteTimer <= 0.0f) {
        if (mFromEditor)
            return std::make_unique<LevelEditorScene>(mLevelPath, false, "", mProfilePath);
        return std::make_unique<TitleScene>();
    }
    return nullptr;
}

bool GameScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT)
        return false;

    // --- Gamepad hot-plug ---
    if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
        SDL_OpenGamepad(e.gdevice.which);
        return true;
    }

    if (mPaused) {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
            mPaused = false;
            return true;
        }
        // Gamepad: Start or B to resume, Back to go to title
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            if (e.gbutton.button == SDL_GAMEPAD_BUTTON_START ||
                e.gbutton.button == SDL_GAMEPAD_BUTTON_EAST) {
                mPaused = false;
                return true;
            }
            if (e.gbutton.button == SDL_GAMEPAD_BUTTON_BACK) {
                mGoBackFromPause = true;
                return true;
            }
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (mx >= mPauseResumeRect.x && mx < mPauseResumeRect.x + mPauseResumeRect.w &&
                my >= mPauseResumeRect.y && my < mPauseResumeRect.y + mPauseResumeRect.h) {
                mPaused = false;
                return true;
            }
            if (mx >= mPauseBackRect.x && mx < mPauseBackRect.x + mPauseBackRect.w &&
                my >= mPauseBackRect.y && my < mPauseBackRect.y + mPauseBackRect.h) {
                mGoBackFromPause = true;
                return true;
            }
        }
        if (mPauseResumeBtn)
            mPauseResumeBtn->HandleEvent(e);
        if (mPauseBackBtn)
            mPauseBackBtn->HandleEvent(e);
        return true;
    }

    if (!gameOver) {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F1) {
            mDebugHitboxes = !mDebugHitboxes;
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F11) {
            mWindow->ToggleFullscreen();
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE && !levelComplete) {
            mPaused = true;
            if (mWindow)
                BuildPauseUI(mWindow->GetWidth(), mWindow->GetHeight());
            return true;
        }
        // Gamepad: Start to pause
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
            e.gbutton.button == SDL_GAMEPAD_BUTTON_START && !levelComplete) {
            mPaused = true;
            if (mWindow)
                BuildPauseUI(mWindow->GetWidth(), mWindow->GetHeight());
            return true;
        }
        InputSystem(reg, e);
        GamepadInputEvent(reg, e);
    } else {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_R)
            Respawn();
        // Gamepad: A to retry
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
            e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH)
            Respawn();
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (mx >= retryBtnRect.x && mx <= retryBtnRect.x + retryBtnRect.w &&
                my >= retryBtnRect.y && my <= retryBtnRect.y + retryBtnRect.h)
                Respawn();
        }
        retryButton->HandleEvent(e);
    }
    return true;
}

void GameScene::Update(float dt) {
    // Clean up finished overlap SFX tracks
    if (Audio() && Audio()->IsReady())
        Audio()->Sfx().PruneOverlaps();

    if (mPaused)
        return;
    if (levelComplete) {
        levelCompleteTimer -= dt;
        return;
    }
    if (gameOver)
        return;

    // Cache gamepad for this frame to avoid repeated SDL_GetGamepads calls.
    {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        mCachedPad = (ids && count > 0) ? SDL_GetGamepadFromID(ids[0]) : nullptr;
        SDL_free(ids);
    }

    // Rebuild spatial grid for bullet broadphase.
    mTileGrid.Build(reg);

    if (mPlayerDying) {
        mDeathAnimTimer += dt;
        AnimationSystem(reg, dt);
        mCamera.TickShake(dt);

        if (mCachedPad) {
            float shakeAmp = std::abs(mCamera.shakeOffX) + std::abs(mCamera.shakeOffY);
            float punchAmp = std::abs(mCamera.punchOffX) + std::abs(mCamera.punchOffY);
            if (shakeAmp > 0.1f || punchAmp > 0.1f) {
                float lowFrac  = std::clamp(shakeAmp / 8.0f, 0.0f, 1.0f);
                float highFrac = std::clamp(punchAmp / 6.0f, 0.0f, 1.0f);
                Uint16 lo  = (Uint16)(lowFrac  * 0xFFFF);
                Uint16 hi  = (Uint16)(highFrac * 0xFFFF);
                lo = std::max(lo, hi);
                SDL_RumbleGamepad(mCachedPad, lo, hi, 50);
            }
        }

        bool animDone = false;
        auto pv = reg.view<PlayerTag, AnimationState>();
        pv.each([&](const AnimationState& anim) {
            if (!anim.looping && anim.currentFrame >= anim.totalFrames - 1)
                animDone = true;
        });

        constexpr float MIN_DEATH_HOLD = 1.0f;
        constexpr float MAX_DEATH_WAIT = 3.0f;
        bool timeUp = mDeathAnimTimer >= MAX_DEATH_WAIT;
        bool readyToEnd = animDone && mDeathAnimTimer >= MIN_DEATH_HOLD;
        if (timeUp || readyToEnd)
            gameOver = true;
        return;
    }

    GamepadPollSystem(reg, mCachedPad);
    MovingPlatformTick(reg, dt);
    FloatingResult floatResult = FloatingSystem(reg, dt);
    LadderSystem(reg, dt);
    // Snapshot player animation before state update for SFX trigger
    AnimationID prevPlayerAnim = AnimationID::NONE;
    int prevPlayerFrame = 0;
    if (Audio() && Audio()->IsReady()) {
        auto pview = reg.view<PlayerTag, AnimationState>();
        pview.each([&](const AnimationState& anim) {
            prevPlayerAnim  = anim.currentAnim;
            prevPlayerFrame = anim.currentFrame;
        });
    }

    PlayerStateSystem(reg);

    MovementSystem(reg, dt, mWindow->GetWidth(), mLevelW, mCachedPad, &mTileGrid);
    BoundsSystem(reg,
                 dt,
                 mWindow->GetWidth(),
                 mWindow->GetHeight(),
                 mLevel.gravityMode == GravityMode::WallRun,
                 mLevelW,
                 mLevelH);
    // Snapshot enemy animation frames + sheets BEFORE AnimationSystem for SFX triggers
    struct EnemySfxSnap { SDL_Texture* sheet; int frame; int totalFrames; };
    std::unordered_map<entt::entity, EnemySfxSnap> prevEnemySnap;
    if (Audio() && Audio()->IsReady()) {
        auto ev = reg.view<EnemyTag, EnemyAnimData, Renderable, AnimationState>();
        ev.each([&](entt::entity e, const EnemyAnimData&, const Renderable& r,
                     const AnimationState& anim) {
            prevEnemySnap[e] = {r.sheet, anim.currentFrame, anim.totalFrames};
        });
    }

    AnimationSystem(reg, dt);

    // Detect animation loop wraps and re-trigger multi-file SFX (round-robin)
    if (Audio() && Audio()->IsReady() && !prevEnemySnap.empty()) {
        auto ev = reg.view<EnemyTag, EnemyAnimData, AnimationState, Renderable>(
            entt::exclude<DeadTag>);
        ev.each([&](entt::entity e, EnemyAnimData& ead,
                     const AnimationState& anim, const Renderable& r) {
            auto pit = prevEnemySnap.find(e);
            if (pit == prevEnemySnap.end()) return;
            if (r.sheet != pit->second.sheet) return;  // sheet changed — handled below
            if (!anim.looping) return;

            // Detect frame wrap: was on last frame, now on frame 0
            bool looped = (pit->second.frame == pit->second.totalFrames - 1 &&
                           anim.currentFrame == 0 && pit->second.totalFrames > 1);
            if (!looped) return;

            int slotIdx = -1;
            if (r.sheet == ead.moveSheet)        slotIdx = 1;
            else if (r.sheet == ead.idleSheet)   slotIdx = 0;
            if (slotIdx < 0) return;

            auto& ss = ead.slotSfx[slotIdx];
            if ((int)ss.files.size() <= 1) return;  // single file loops via SDL mixer

            const auto& fi = ss.files[ss.nextIdx];
            std::string sfxId = audio::EnemySfxId(ead.typeName, slotIdx, ss.nextIdx);
            if (!sfxId.empty() && Audio()->Sfx().PlayOverlap(sfxId, fi.volume))
                ss.nextIdx = (ss.nextIdx + 1) % (int)ss.files.size();
        });
    }

    // Recover enemies from hurt/attack animation back to move animation
    {
        auto hurtView = reg.view<EnemyTag, EnemyAnimData, AnimationState, Renderable>(
            entt::exclude<DeadTag>);
        hurtView.each([&](entt::entity e, const EnemyAnimData& ead,
                          AnimationState& anim, Renderable& r) {
            if (anim.looping) return;
            if (r.sheet != ead.hurtSheet) return;
            if (anim.currentFrame < anim.totalFrames - 1) return;
            if (ead.moveSheet && !ead.moveFrames.empty()) {
                r.sheet         = ead.moveSheet;
                r.frames        = ead.moveFrames;
                r.renderW       = ead.spriteW;
                r.renderH       = ead.spriteH;
                anim.currentFrame = 0;
                anim.totalFrames  = (int)ead.moveFrames.size();
                anim.fps          = ead.moveFps;
                anim.looping      = true;
                ead.ApplyHitbox(ead.moveHitbox, reg, e);
            }
            // Clear attack state so the enemy can re-trigger an attack
            // after recovering from hurt (otherwise eas.attacking stays
            // true and the proximity trigger never fires again).
            if (auto* eas = reg.try_get<EnemyAttackState>(e))
                eas->attacking = false;
        });

        // Attack recovery: when attack anim finishes, restore move and tick cooldown
        auto atkView = reg.view<EnemyTag, EnemyAttackState, EnemyAnimData, AnimationState, Renderable>(
            entt::exclude<DeadTag>);
        atkView.each([&](entt::entity e, EnemyAttackState& eas, const EnemyAnimData& ead,
                         AnimationState& anim, Renderable& r) {
            if (eas.cooldown > 0.0f)
                eas.cooldown -= dt;
            if (eas.attacking && !anim.looping &&
                anim.currentFrame >= anim.totalFrames - 1) {
                if (ead.moveSheet && !ead.moveFrames.empty()) {
                    r.sheet         = ead.moveSheet;
                    r.frames        = ead.moveFrames;
                    r.renderW       = ead.spriteW;
                    r.renderH       = ead.spriteH;
                    anim.currentFrame = 0;
                    anim.totalFrames  = (int)ead.moveFrames.size();
                    anim.fps          = ead.moveFps;
                    anim.looping      = true;
                    ead.ApplyHitbox(ead.moveHitbox, reg, e);
                }
                eas.attacking = false;
            }
        });
    }

    // Advance animated tile frames
    for (auto& [ent, frames] : tileAnimFrameMap) {
        if (!reg.valid(ent) || frames.empty())
            continue;
        auto* anim = reg.try_get<AnimationState>(ent);
        auto* rend = reg.try_get<Renderable>(ent);
        if (!anim || !rend)
            continue;
        float dur = (anim->fps > 0.0f) ? 1.0f / anim->fps : 0.125f;
        anim->timer += dt;
        while (anim->timer >= dur) {
            anim->timer -= dur;
            if (anim->looping) {
                anim->currentFrame = (anim->currentFrame + 1) % (int)frames.size();
            } else {
                if (anim->currentFrame < (int)frames.size() - 1)
                    anim->currentFrame++;
                else
                    anim->timer = 0.0f;
            }
        }
        SDL_Texture* cur = frames[anim->currentFrame];
        if (cur)
            rend->sheet = cur;
    }

    // Destroy action tiles whose death animation finished
    {
        std::vector<entt::entity> toDestroy;
        auto                      destroyView = reg.view<DestroyAnimTag, AnimationState>();
        destroyView.each(
            [&](entt::entity e, DestroyAnimTag& dat, const AnimationState& anim) {
                if (!anim.looping && anim.currentFrame >= anim.totalFrames - 1) {
                    if (dat.reachedEnd)
                        toDestroy.push_back(e);
                    else
                        dat.reachedEnd = true;
                }
            });
        for (entt::entity e : toDestroy) {
            tileAnimFrameMap.erase(e);
            if (reg.valid(e)) {
                    // Goal only counts after destroy animation plays.
                    if (reg.all_of<GoalTag>(e)) {
                    goalsCollected++;
                    reg.remove<GoalTag>(e);
                }
                if (reg.all_of<Renderable>(e))
                    reg.remove<Renderable>(e);
                reg.destroy(e);
            }
        }
    }

    // Tick HitFlash timers
    {
        auto                      flashView = reg.view<HitFlash>();
        std::vector<entt::entity> expired;
        flashView.each([&](entt::entity e, HitFlash& hf) {
            hf.timer -= dt;
            if (hf.timer <= 0.0f)
                expired.push_back(e);
        });
        for (auto e : expired)
            reg.remove<HitFlash>(e);
    }

    // Tick teleport cooldown
    {
        std::vector<entt::entity> tpExpired;
        auto tcv = reg.view<TeleportCooldown>();
        tcv.each([&](entt::entity e, TeleportCooldown& tc) {
            tc.remaining -= dt;
            if (tc.remaining <= 0.0f)
                tpExpired.push_back(e);
        });
        for (auto e : tpExpired)
            reg.remove<TeleportCooldown>(e);
    }

    CollisionResult collision =
        CollisionSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight());
    for (auto e : floatResult.actionTilesTriggered)
        collision.actionTilesTriggered.push_back(e);
    MovingPlatformCarry(reg);

    // --- Power-up pickup ---
    {
        entt::entity playerEnt = entt::null;
        SDL_Rect     playerRect{};
        {
            auto pv = reg.view<PlayerTag, Transform, Collider>();
            pv.each([&](entt::entity e, const Transform& t, const Collider& c) {
                playerEnt  = e;
                playerRect = {(int)t.x, (int)t.y, c.w, c.h};
            });
        }
        if (playerEnt != entt::null) {
            std::vector<entt::entity> toConsume;
            auto                      puv = reg.view<PowerUpTag, Transform, Collider>();
            puv.each([&](entt::entity      e,
                         const PowerUpTag& pu,
                         const Transform&  t,
                         const Collider&   c) {
                SDL_Rect pr = {(int)t.x, (int)t.y, c.w, c.h};
                bool     overlap =
                    (playerRect.x < pr.x + pr.w && playerRect.x + playerRect.w > pr.x &&
                     playerRect.y < pr.y + pr.h && playerRect.y + playerRect.h > pr.y);
                if (overlap)
                    toConsume.push_back(e);
            });
            for (entt::entity e : toConsume) {
                if (!reg.valid(e))
                    continue;
                const PowerUpTag& pu = reg.get<PowerUpTag>(e);

                // Teleport: only entrances trigger; destinations are inert.
                if (pu.type == PowerUpType::Teleport) {
                    if (reg.all_of<TeleportDestTag>(e)) continue;
                    if (reg.all_of<TeleportCooldown>(playerEnt)) continue;

                    int grp = pu.teleportGroup;
                    bool teleported = false;
                    auto destView = reg.view<TeleportDestTag, Transform>();
                    destView.each([&](entt::entity de, const TeleportDestTag& dest,
                                      const Transform& dt) {
                        if (teleported || dest.group != grp) return;
                        auto& pt = reg.get<Transform>(playerEnt);
                        auto& pc = reg.get<Collider>(playerEnt);
                        int dw = 0, dh = 0;
                        if (auto* dc = reg.try_get<Collider>(de)) {
                            dw = dc->w; dh = dc->h;
                        } else if (auto* dr = reg.try_get<Renderable>(de)) {
                            dw = dr->renderW; dh = dr->renderH;
                        }
                        if (dw <= 0) dw = 38;
                        if (dh <= 0) dh = 38;
                        // Center on destination tile; gravity handles landing.
                        pt.x = dt.x + dw * 0.5f - pc.w * 0.5f;
                        pt.y = dt.y + dh * 0.5f - pc.h * 0.5f;
                        if (auto* prev = reg.try_get<PrevTransform>(playerEnt)) {
                            prev->x = pt.x;
                            prev->y = pt.y;
                        }
                        if (auto* g = reg.try_get<GravityState>(playerEnt)) {
                            g->velocity   = 0.0f;
                            g->isGrounded = false;
                        }
                        if (auto* v = reg.try_get<Velocity>(playerEnt))
                            v->dy = 0.0f;
                        teleported = true;
                    });
                    if (teleported)
                        reg.emplace_or_replace<TeleportCooldown>(playerEnt, 0.3f);
                    continue;
                }

                // Health boost is instant — no timed slot needed.
                if (pu.type == PowerUpType::HealthBoost) {
                    auto* hp = reg.try_get<Health>(playerEnt);
                    if (hp) {
                        float boost = hp->max * (pu.healthPct / 100.0f);
                        hp->max    += boost;
                        hp->current = hp->max;
                    }
                    auto it2 = std::find(
                        mSortedTileRenderList.begin(), mSortedTileRenderList.end(), e);
                    if (it2 != mSortedTileRenderList.end())
                        mSortedTileRenderList.erase(it2);
                    reg.destroy(e);
                    continue;
                }

                if (!reg.all_of<ActivePowerUps>(playerEnt))
                    reg.emplace<ActivePowerUps>(playerEnt);
                reg.get<ActivePowerUps>(playerEnt).add(pu.type, pu.duration);

                if (pu.type == PowerUpType::Turret) {
                    ActiveTurretPowerUp tp;
                    tp.remaining = pu.duration;
                    tp.fireRate  = pu.fireRate;
                    if (!pu.sfxPath.empty())
                        tp.sfxId = "powerup_" + std::to_string((uint32_t)e);
                    reg.emplace_or_replace<ActiveTurretPowerUp>(playerEnt, tp);
                }
                auto it2 =
                    std::find(mSortedTileRenderList.begin(), mSortedTileRenderList.end(), e);
                if (it2 != mSortedTileRenderList.end())
                    mSortedTileRenderList.erase(it2);
                reg.destroy(e);
            }
        }
    }

    // --- Active power-up tick ---
    {
        auto apv = reg.view<PlayerTag, ActivePowerUps, GravityState>();
        apv.each([&](entt::entity e, ActivePowerUps& aps, GravityState& g) {
            std::vector<int> expired;
            for (auto& [key, slot] : aps.slots) {
                slot.remaining -= dt;
                if (slot.remaining <= 0.f) {
                    expired.push_back(key);
                    // Restore effects when this type expires
                    if ((PowerUpType)key == PowerUpType::AntiGravity) {
                        g.active   = true;
                        g.velocity = 0.0f;
                    }
                    if ((PowerUpType)key == PowerUpType::Turret) {
                        if (reg.all_of<ActiveTurretPowerUp>(e))
                            reg.remove<ActiveTurretPowerUp>(e);
                    }
                }
            }
            for (int k : expired)
                aps.slots.erase(k);
            if (aps.slots.empty()) {
                reg.remove<ActivePowerUps>(e);
                return;
            }

            for (auto& [key, slot] : aps.slots) {
                switch ((PowerUpType)key) {
                    case PowerUpType::AntiGravity:
                        g.active   = false;
                        g.velocity = 0.0f;
                        break;
                    case PowerUpType::Turret:
                        break;
                    case PowerUpType::HealthBoost:
                        break;
                    case PowerUpType::Teleport:
                        break;
                    default:
                        break;
                }
            }
        });
    }

    // Float-killed enemies go through the same death pipeline as stomps/slashes.
    for (entt::entity e : floatResult.enemiesKilledByFloat) {
        if (!reg.valid(e) || reg.all_of<DeadTag>(e)) continue;
        if (reg.all_of<Velocity>(e)) {
            auto& v = reg.get<Velocity>(e);
            v.dx = v.dy = 0.0f;
        }
        if (reg.all_of<Renderable, AnimationState>(e)) {
            auto& r    = reg.get<Renderable>(e);
            auto& anim = reg.get<AnimationState>(e);
            if (auto* ead = reg.try_get<EnemyAnimData>(e);
                ead && ead->deadSheet && !ead->deadFrames.empty()) {
                r.sheet         = ead->deadSheet;
                r.frames        = ead->deadFrames;
                r.renderW       = ead->spriteW;
                r.renderH       = ead->spriteH;
                anim.currentFrame = 0;
                anim.totalFrames  = (int)ead->deadFrames.size();
                anim.fps          = ead->deadFps;
                anim.looping      = false;
                ead->ApplyHitbox(ead->deadHitbox, reg, e);
            } else {
                anim.looping = false;
            }
        }
        reg.emplace<DeadTag>(e);
        stompCount++;
    }

    // Fire enemy SFX on animation sheet transitions
    if (Audio() && Audio()->IsReady() && !prevEnemySnap.empty()) {
        auto ev = reg.view<EnemyTag, EnemyAnimData, AnimationState, Renderable>();
        ev.each([&](entt::entity e, EnemyAnimData& ead,
                     const AnimationState& anim, const Renderable& r) {
            auto pit = prevEnemySnap.find(e);
            if (pit == prevEnemySnap.end()) return;
            if (r.sheet == pit->second.sheet) return;  // no sheet change
            int slotIdx = -1;
            if (r.sheet == ead.idleSheet)        slotIdx = 0;
            else if (r.sheet == ead.moveSheet)   slotIdx = 1;
            else if (r.sheet == ead.attackSheet) slotIdx = 2;
            else if (r.sheet == ead.hurtSheet)   slotIdx = 3;
            else if (r.sheet == ead.deadSheet)   slotIdx = 4;
            if (slotIdx < 0 || ead.slotSfx[slotIdx].files.empty()) return;
            auto& ss = ead.slotSfx[slotIdx];
            const auto& fi = ss.files[ss.nextIdx];
            std::string sfxId = audio::EnemySfxId(ead.typeName, slotIdx, ss.nextIdx);
            if (sfxId.empty()) return;
            if (fi.timeStretch && anim.fps > 0.0f && anim.totalFrames > 0) {
                // Time-stretch: match sound duration to animation duration
                float dur = static_cast<float>(anim.totalFrames) / anim.fps;
                Audio()->Sfx().PlayTimed(sfxId, dur, anim.looping, fi.volume);
            } else {
                // No time-stretch: fire-and-forget, let sounds overlap naturally
                Audio()->Sfx().PlayOverlap(sfxId, fi.volume);
            }
            if (ss.files.size() > 1)
                ss.nextIdx = (ss.nextIdx + 1) % (int)ss.files.size();
        });
    }

    goalsCollected += collision.goalsCollected;
    stompCount += collision.enemiesStomped + collision.enemiesSlashed;
    if (collision.playerDied && !mPlayerDying) {
        mPlayerDying    = true;
        mDeathAnimTimer = 0.0f;
        auto dv = reg.view<PlayerTag, Velocity, AnimationState, Renderable, AnimationSet>();
        dv.each([](Velocity& v, AnimationState& anim, Renderable& r,
                   const AnimationSet& set) {
            v.dx = 0.0f; v.dy = 0.0f;
            const std::vector<SDL_Rect>* frames = nullptr;
            SDL_Texture* sheet = nullptr;
            float fps = 8.0f;
            AnimationID id = AnimationID::DEATH;
            if (!set.death.empty()) {
                frames = &set.death;
                sheet  = set.deathSheet;
                fps    = (set.deathFps > 0.0f) ? set.deathFps : 8.0f;
            } else if (!set.hurt.empty()) {
                frames = &set.hurt;
                sheet  = set.hurtSheet;
                fps    = (set.hurtFps > 0.0f) ? set.hurtFps : 12.0f;
                id     = AnimationID::HURT;
            }
            if (frames && sheet) {
                r.sheet           = sheet;
                r.frames          = *frames;
                anim.currentFrame = 0;
                anim.timer        = 0.0f;
                anim.fps          = fps;
                anim.looping      = false;
                anim.totalFrames  = (int)frames->size();
                anim.currentAnim  = id;
            }
        });
    }

    if (collision.playerHit)
        mCamera.StartShake(4.0f, 0.12f);

    if (collision.enemiesStomped > 0) {
        float progress  = 1.0f - collision.lowestHitHpFrac;
        float intensity = 3.0f + progress * 9.0f;
        float duration  = 0.06f + progress * 0.06f;
        mCamera.StartPunch(0.0f, 1.0f, intensity, duration);
    }
    if (collision.slashHits > 0) {
        float punchDir = 1.0f;
        auto pv = reg.view<PlayerTag, Velocity>();
        pv.each([&](const Velocity& v) {
            punchDir = (v.dx >= 0.0f) ? 1.0f : -1.0f;
        });
        float progress  = 1.0f - collision.lowestHitHpFrac;
        float intensity = 3.0f + progress * 10.0f;
        float duration  = 0.06f + progress * 0.07f;
        mCamera.StartPunch(punchDir, -0.25f, intensity, duration);
    }

    if (collision.shieldBounce)
        mCamera.StartShake(5.0f, 0.15f);

    // Process triggered action tiles
    SDL_Renderer* ren = mWindow->GetRenderer();
    for (entt::entity e : collision.actionTilesTriggered) {
        if (!reg.valid(e))
            continue;

        // Shield pickup
        if (auto* sp = reg.try_get<ShieldPickupTag>(e)) {
            auto* rend = reg.try_get<Renderable>(e);
            if (rend) {
                auto pv = reg.view<PlayerTag>();
                for (auto pe : pv) {
                    ShieldEntry se;
                    se.tex      = rend->sheet;
                    se.renderW  = rend->renderW > 0 ? rend->renderW : 20;
                    se.renderH  = rend->renderH > 0 ? rend->renderH : 20;
                    se.remaining = sp->duration;
                    auto* as = reg.try_get<ActiveShield>(pe);
                    if (as) {
                        as->shields.push_back(se);
                        as->orbitRadius = std::max(as->orbitRadius,
                            (float)std::max(se.renderW, se.renderH) * 1.2f);
                    } else {
                        ActiveShield newAs;
                        newAs.shields.push_back(se);
                        newAs.orbitRadius = std::max(se.renderW, se.renderH) * 1.2f;
                        reg.emplace<ActiveShield>(pe, std::move(newAs));
                    }
                }
            }
            auto it = std::find(mSortedTileRenderList.begin(),
                                mSortedTileRenderList.end(), e);
            if (it != mSortedTileRenderList.end())
                mSortedTileRenderList.erase(it);
            reg.destroy(e);
            continue;
        }

        // Goals with destroy animations are collected when the anim finishes (see DestroyAnimTag cleanup).
        const ActionTag* atagPeek = reg.try_get<ActionTag>(e);
        bool hasDestroyAnim = atagPeek && !atagPeek->destroyAnimPath.empty();
        if (reg.all_of<GoalTag>(e) && !hasDestroyAnim) {
            goalsCollected++;
            reg.remove<GoalTag>(e);
        }

        if (reg.all_of<TileTag>(e))
            reg.remove<TileTag>(e);
        if (reg.all_of<Collider>(e))
            reg.remove<Collider>(e);

        const ActionTag* atag = reg.try_get<ActionTag>(e);
        if (atag && !atag->destroyAnimPath.empty()) {
            AnimatedTileDef def;
            std::print("[DestroyAnim] tile triggered, path='{}' ", atag->destroyAnimPath);
            if (LoadAnimatedTileDef(atag->destroyAnimPath, def) && !def.framePaths.empty()) {
                std::print("loaded {} frames at {}fps\n", def.framePaths.size(), def.fps);

                int tileW = 38, tileH = 38;
                if (reg.all_of<Renderable>(e)) {
                    const auto& rend = reg.get<Renderable>(e);
                    if (rend.renderW > 0) tileW = rend.renderW;
                    if (rend.renderH > 0) tileH = rend.renderH;
                }

                std::vector<SDL_Texture*> frameTex;
                frameTex.reserve(def.framePaths.size());
                for (const auto& fp : def.framePaths)
                    frameTex.push_back(GetCachedTexture(ren, fp, 0, 0));

                if (!frameTex.empty() && frameTex[0]) {
                    tileAnimFrameMap[e]        = std::move(frameTex);
                    auto&                 fvec = tileAnimFrameMap[e];

                    std::vector<SDL_Rect> animRects;
                    animRects.reserve(fvec.size());
                    for (auto* ft : fvec) {
                        float fw = 0, fh = 0;
                        if (ft) SDL_GetTextureSize(ft, &fw, &fh);
                        animRects.push_back({0, 0, (int)fw, (int)fh});
                    }

                    // Centre the destroy animation on the original tile.
                    if (reg.all_of<Transform>(e)) {
                        auto& tr = reg.get<Transform>(e);
                        tr.x += tileW * 0.5f - tileW * 0.5f;  // no-op for same size
                        tr.y += tileH * 0.5f - tileH * 0.5f;
                        // Scale destroy anim to cover the tile at native aspect ratio.
                        float nativeW = 0, nativeH = 0;
                        SDL_GetTextureSize(fvec[0], &nativeW, &nativeH);
                        int animRenderW = tileW;
                        int animRenderH = tileH;
                        if (nativeW > 0 && nativeH > 0) {
                            float aspect = nativeW / nativeH;
                            // Scale to fit at least the tile, preserving aspect
                            animRenderW = std::max(tileW, (int)(tileH * aspect));
                            animRenderH = std::max(tileH, (int)(tileW / aspect));
                        }
                        float tileCX = tr.x + tileW * 0.5f;
                        float tileCY = tr.y + tileH * 0.5f;
                        tr.x = tileCX - animRenderW * 0.5f;
                        tr.y = tileCY - animRenderH * 0.5f;

                        if (reg.all_of<Renderable>(e))
                            reg.remove<Renderable>(e);
                        reg.emplace<Renderable>(e, fvec[0], std::move(animRects),
                                                false, animRenderW, animRenderH);
                    } else {
                        if (reg.all_of<Renderable>(e))
                            reg.remove<Renderable>(e);
                        reg.emplace<Renderable>(e, fvec[0], std::move(animRects),
                                                false, tileW, tileH);
                    }

                    if (reg.all_of<AnimationState>(e))
                        reg.remove<AnimationState>(e);
                    reg.emplace<AnimationState>(
                        e, 0, (int)fvec.size(), 0.0f, def.fps, false);
                    if (!reg.all_of<TileAnimTag>(e))
                        reg.emplace<TileAnimTag>(e);
                    if (!reg.all_of<DestroyAnimTag>(e))
                        reg.emplace<DestroyAnimTag>(e, (int)fvec.size(), def.fps, false);

                    if (atag->cameraShake)
                        mCamera.StartShake(6.0f, 0.25f);

                    continue;
                } else {
                    std::print("[DestroyAnim] frame texture load failed\n");
                    for (auto* t : frameTex)
                        if (t)
                            SDL_DestroyTexture(t);
                }
            } else {
                std::print("def load failed or empty\n");
            }
        }
        if (reg.all_of<Renderable>(e))
            reg.remove<Renderable>(e);
    }

    // Hazard damage
    {
        auto hView = reg.view<PlayerTag,
                              Health,
                              HazardState,
                              AnimationState,
                              Renderable,
                              AnimationSet>();
        hView.each([&](entt::entity        playerEnt,
                       Health&             hp,
                       HazardState&        hz,
                       AnimationState&     anim,
                       Renderable&         r,
                       const AnimationSet& set) {
            hz.active = collision.onHazard;
            if (hz.active) {
                mCamera.StartShake(2.5f, 0.15f);
                hp.current -= HAZARD_DAMAGE_PER_SEC * dt;
                if (hp.current <= 0.0f) {
                    hp.current = 0.0f;
                    if (!mPlayerDying) {
                        mPlayerDying    = true;
                        mDeathAnimTimer = 0.0f;
                        auto dv = reg.view<PlayerTag, Velocity>();
                        dv.each([](Velocity& v) { v.dx = 0.0f; v.dy = 0.0f; });
                        const std::vector<SDL_Rect>* deathFr = nullptr;
                        SDL_Texture* deathSh = nullptr;
                        float deathFps = 8.0f;
                        AnimationID deathId = AnimationID::DEATH;
                        if (!set.death.empty()) {
                            deathFr  = &set.death;
                            deathSh  = set.deathSheet;
                            deathFps = (set.deathFps > 0.0f) ? set.deathFps : 8.0f;
                        } else if (!set.hurt.empty()) {
                            deathFr  = &set.hurt;
                            deathSh  = set.hurtSheet;
                            deathFps = (set.hurtFps > 0.0f) ? set.hurtFps : 12.0f;
                            deathId  = AnimationID::HURT;
                        }
                        if (deathFr && deathSh) {
                            r.sheet           = deathSh;
                            r.frames          = *deathFr;
                            anim.currentFrame = 0;
                            anim.timer        = 0.0f;
                            anim.fps          = deathFps;
                            anim.looping      = false;
                            anim.totalFrames  = (int)deathFr->size();
                            anim.currentAnim  = deathId;
                        }
                    }
                }
                hz.flashTimer += dt;
                // Attack always takes priority — never stomp it while in lava.
                bool isAttacking = false;
                if (auto* atk = reg.try_get<AttackState>(playerEnt))
                    isAttacking = atk->isAttacking;
                if (!mPlayerDying && !isAttacking && !set.hurt.empty()) {
                    // Restart hurt from frame 0 whenever:
                    //   - we just entered lava (currentAnim != HURT), OR
                    //   - the previous hurt cycle finished (last frame reached)
                    bool justEntered = (anim.currentAnim != AnimationID::HURT);
                    bool cycleFinished =
                        (anim.currentAnim == AnimationID::HURT && !anim.looping &&
                         anim.currentFrame >= anim.totalFrames - 1);
                    if (justEntered || cycleFinished) {
                        r.sheet           = set.hurtSheet;
                        r.frames          = set.hurt;
                        anim.currentFrame = 0;
                        anim.timer        = 0.0f;
                        anim.fps          = (set.hurtFps > 0.0f) ? set.hurtFps : 12.0f;
                        anim.looping =
                            false; // play once; re-triggers next frame if still in lava
                        anim.totalFrames = (int)set.hurt.size();
                        anim.currentAnim = AnimationID::HURT;
                    }
                }
            } else {
                if (hz.flashTimer > 0.0f) {
                    if (Audio() && Audio()->IsReady())
                        Audio()->StopAllAnimSFX();
                }
                hz.flashTimer = 0.0f;
                if (anim.currentAnim == AnimationID::HURT)
                    anim.currentAnim = AnimationID::NONE;
            }
        });
    }

    // Fire animation SFX on transition, time-matched to animation duration.
    // Runs AFTER both PlayerStateSystem and hazard code so we see the final
    // animation state for the frame (not an intermediate that gets overridden).
    if (Audio() && Audio()->IsReady()) {
        auto pview = reg.view<PlayerTag, AnimationState>();
        pview.each([&](const AnimationState& anim) {
            bool animChanged = (anim.currentAnim != prevPlayerAnim);
            bool animRestarted = !animChanged && !anim.looping
                              && anim.currentFrame == 0
                              && prevPlayerFrame > 0;
            if (animChanged || animRestarted) {
                std::print("[Audio] Anim {}: {} -> {} (frames={} fps={:.1f} loop={})\n",
                           animRestarted ? "restart" : "transition",
                           (int)prevPlayerAnim, (int)anim.currentAnim,
                           anim.totalFrames, anim.fps, anim.looping);

                bool enteringDeath = (anim.currentAnim == AnimationID::DEATH);
                if (enteringDeath) {
                    // Fade looping track; one-shot/sequential tracks finish naturally.
                    Audio()->FadeOutAnimSFX(300);
                } else {
                    Audio()->StopAnimSFX();
                }

                // While dying, don't re-trigger hurt sounds from the queue
                if (mPlayerDying && anim.currentAnim == AnimationID::HURT)
                    return;

                float animDuration = (anim.fps > 0.0f && anim.totalFrames > 0)
                    ? static_cast<float>(anim.totalFrames) / anim.fps
                    : 0.0f;

                auto animToSlot = [](AnimationID a) -> int {
                    switch (a) {
                        case AnimationID::IDLE:  return 0;
                        case AnimationID::WALK:  return 1;
                        case AnimationID::DUCK:  return 2;
                        case AnimationID::JUMP:  return 3;
                        case AnimationID::FRONT: return 4;
                        case AnimationID::SLASH: return 5;
                        case AnimationID::HURT:  return 6;
                        case AnimationID::DEATH: return 7;
                        default:                 return -1;
                    }
                };
                int slotIdx = animToSlot(anim.currentAnim);
                if (slotIdx >= 0 && slotIdx < PLAYER_ANIM_SLOT_COUNT
                    && !mSlotSfx[slotIdx].empty()) {
                    int& nextIdx = mSlotSfxNext[slotIdx];
                    const auto& fi = mSlotSfx[slotIdx][nextIdx];
                    auto sfxId = audio::PlayerAnimSfxId(anim.currentAnim, nextIdx);
                    if (!sfxId.empty()) {
                        if (mSlotSfx[slotIdx].size() > 1) {
                            if (Audio()->Sfx().PlayOneShotSeq(sfxId, fi.volume))
                                nextIdx = (nextIdx + 1) % (int)mSlotSfx[slotIdx].size();
                        } else {
                            float targetDur = fi.timeStretch ? animDuration : 0.0f;
                            Audio()->Sfx().PlayTimed(sfxId, targetDur, anim.looping, fi.volume);
                        }
                    }
                }
            }
        });

    }

    if (mLevel.gravityMode != GravityMode::OpenWorld) {
        auto jumpView = reg.view<PlayerTag, GravityState, AnimationSet>();
        jumpView.each([](GravityState& g, const AnimationSet& set) {
            // Respect slot capability: no jump frames = jumping disabled.
            if (g.active && g.jumpHeld && g.isGrounded && !set.jump.empty()) {
                g.velocity   = -JUMP_FORCE;
                g.isGrounded = false;
                g.jumpHeld   = false;
            } else if (set.jump.empty()) {
                g.jumpHeld = false; // drain the held flag so it doesn't queue
            }
        });
    }

    if (totalGoals > 0 && goalsCollected >= totalGoals)
        levelComplete = true;

    // --- Shooter turrets ---
    if (!mPlayerDying && !gameOver) {
        float playerCX = 0, playerCY = 0;
        bool  havePlayer = false;
        {
            auto pv = reg.view<PlayerTag, Transform, Collider>();
            pv.each([&](const Transform& pt, const Collider& pc) {
                playerCX   = pt.x + pc.w * 0.5f;
                playerCY   = pt.y + pc.h * 0.5f;
                havePlayer = true;
            });
        }
        if (havePlayer) {
            auto sv = reg.view<ShooterTag, ShooterState, Transform, Collider>();
            sv.each([&](entt::entity turretEnt, const ShooterTag& sh,
                        ShooterState& ss, const Transform& tt, const Collider& tc) {
                ss.cooldownLeft -= dt;
                float tcx = tt.x + tc.w * 0.5f;
                float tcy = tt.y + tc.h * 0.5f;
                float ddx = playerCX - tcx, ddy = playerCY - tcy;
                float dist = std::sqrt(ddx * ddx + ddy * ddy);
                if (dist > sh.range || ss.cooldownLeft > 0.f) return;

                ss.cooldownLeft = 1.f / sh.fireRate;

                float aimX = playerCX - tcx, aimY = playerCY - tcy;
                float aimLen = std::sqrt(aimX * aimX + aimY * aimY);
                if (aimLen < 1.f) return;
                float bdx = aimX / aimLen, bdy = aimY / aimLen;

                // Spawn at barrel tip (matching the visual render)
                float baseW = std::max(8.f, (float)tc.w * 0.3f);
                float baseH = std::max(8.f, (float)tc.h * 0.3f);
                float barrelLen = std::max(6.f, std::min((float)tc.w, (float)tc.h) * 0.45f);
                float bx = tcx + bdx * (baseW * 0.5f + barrelLen);
                float by = tcy + bdy * (baseH * 0.5f + barrelLen);

                auto bullet = reg.create();
                reg.emplace<Transform>(bullet, bx, by);
                reg.emplace<Collider>(bullet, 8, 8);
                BulletTag bt;
                bt.dx      = bdx; bt.dy = bdy;
                bt.speed   = sh.bulletSpeed;
                bt.damage  = sh.damage;
                bt.originX = bx;  bt.originY = by;
                bt.maxRange = sh.range;
                bt.sourceTurret = turretEnt;
                reg.emplace<BulletTag>(bullet, bt);
                if (mBulletTex) {
                    reg.emplace<Renderable>(bullet, mBulletTex, sBulletFrames, false, 8, 8);
                    reg.emplace<AnimationState>(bullet, 0, 1, 0.0f, 1.0f, false);
                }

                if (!sh.sfxPath.empty() && Audio() && Audio()->IsReady()) {
                    std::string sfxId = "turret_" + std::to_string((uint32_t)turretEnt);
                    Audio()->Sfx().Play(sfxId);
                }
            });
        }
    }

    // --- Bullet movement + collision ---
    bool bulletKilledPlayer = false;
    {
        const float cullL = mCamera.x - 200.f;
        const float cullR = mCamera.x + mWindow->GetWidth() + 200.f;
        const float cullT = mCamera.y - 200.f;
        const float cullB = mCamera.y + mWindow->GetHeight() + 200.f;

        std::vector<entt::entity> bulletsToDestroy;
        auto bv = reg.view<BulletTag, Transform>();
        bv.each([&](entt::entity be, BulletTag& bt, Transform& btr) {
            btr.x += bt.dx * bt.speed * dt;
            btr.y += bt.dy * bt.speed * dt;
            float travelX = btr.x - bt.originX, travelY = btr.y - bt.originY;
            if (travelX * travelX + travelY * travelY > bt.maxRange * bt.maxRange) {
                bulletsToDestroy.push_back(be);
                return;
            }
            // Skip detailed collision for bullets far outside the viewport.
            if (btr.x < cullL || btr.x > cullR || btr.y < cullT || btr.y > cullB)
                return;
            SDL_FRect bulletR = {btr.x, btr.y, 8.f, 8.f};

            // Hit solid tiles via spatial grid (skip source turret; player bullets pass through hazards & floats)
            bool hitTile = false;
            mTileGrid.Query(btr.x - 4.f, btr.y - 4.f, 16.f, 16.f, [&](entt::entity te) {
                if (hitTile) return;
                if (!reg.valid(te)) return;
                if (te == bt.sourceTurret) return;
                if (!reg.all_of<TileTag>(te)) return;
                if (bt.playerOwned && reg.all_of<HazardTag>(te)) return;
                if (bt.playerOwned && reg.all_of<FloatTag>(te)) return;
                auto& tt2 = reg.get<Transform>(te);
                auto& tc2 = reg.get<Collider>(te);
                SDL_FRect tileR = {tt2.x, tt2.y, (float)tc2.w, (float)tc2.h};
                if (const auto* off = reg.try_get<ColliderOffset>(te)) {
                    tileR.x += off->x; tileR.y += off->y;
                }
                if (SDL_HasRectIntersectionFloat(&bulletR, &tileR)) {
                    bulletsToDestroy.push_back(be);
                    hitTile = true;
                }
            });
            if (hitTile) return;

            // Hit shield (absorbs bullet)
            {
                bool blocked = false;
                auto shv = reg.view<PlayerTag, Transform, Collider, ActiveShield>();
                shv.each([&](const Transform& pt, const Collider& pc, const ActiveShield& as) {
                    if (blocked) return;
                    float pcx = pt.x + pc.w * 0.5f;
                    float pcy = pt.y + pc.h * 0.5f;
                    int n = (int)as.shields.size();
                    for (int i = 0; i < n && !blocked; ++i) {
                        float a = as.angle + (float)i / n * 2.f * 3.14159265f;
                        const auto& se = as.shields[i];
                        float sx = pcx + std::cos(a) * as.orbitRadius - se.renderW * 0.5f;
                        float sy = pcy + std::sin(a) * as.orbitRadius - se.renderH * 0.5f;
                        SDL_FRect shieldR = {sx, sy, (float)se.renderW, (float)se.renderH};
                        if (SDL_HasRectIntersectionFloat(&bulletR, &shieldR)) {
                            bulletsToDestroy.push_back(be);
                            blocked = true;
                        }
                    }
                });
                if (blocked) return;
            }

            // Hit player (skip player-owned bullets)
            if (!bt.playerOwned) {
                auto pv2 = reg.view<PlayerTag, Transform, Collider, InvincibilityTimer>();
                pv2.each([&](entt::entity pe, Transform& pt, const Collider& pc,
                             InvincibilityTimer& inv) {
                    if (inv.isInvincible) return;
                    SDL_FRect playerR = {pt.x, pt.y, (float)pc.w, (float)pc.h};
                    if (const auto* off = reg.try_get<ColliderOffset>(pe)) {
                        playerR.x += off->x; playerR.y += off->y;
                    }
                    if (SDL_HasRectIntersectionFloat(&bulletR, &playerR)) {
                        if (auto* hp = reg.try_get<Health>(pe)) {
                            hp->current -= bt.damage;
                            if (hp->current <= 0.f) {
                                hp->current = 0.f;
                                bulletKilledPlayer = true;
                            }
                        }
                        inv.isInvincible = true;
                        inv.remaining    = 0.3f;

                        pt.x += bt.dx * -16.f;
                        pt.y += bt.dy * -16.f;

                        if (!bulletKilledPlayer) {
                            auto* anim = reg.try_get<AnimationState>(pe);
                            auto* r    = reg.try_get<Renderable>(pe);
                            auto* set  = reg.try_get<AnimationSet>(pe);
                            if (anim && r && set && !set->hurt.empty() && set->hurtSheet) {
                                r->sheet           = set->hurtSheet;
                                r->frames          = set->hurt;
                                anim->currentFrame = 0;
                                anim->timer        = 0.0f;
                                anim->fps          = (set->hurtFps > 0.f) ? set->hurtFps : 12.f;
                                anim->looping      = false;
                                anim->totalFrames  = (int)set->hurt.size();
                                anim->currentAnim  = AnimationID::HURT;
                            }
                        }

                        mCamera.StartShake(3.5f, 0.1f);
                        bulletsToDestroy.push_back(be);
                    }
                });
            }

            // Hit enemies
            auto ev = reg.view<EnemyTag, Transform, Collider, Health>(entt::exclude<DeadTag>);
            for (auto ee : ev) {
                auto& et = reg.get<Transform>(ee);
                auto& ec = reg.get<Collider>(ee);
                SDL_FRect enemyR = {et.x, et.y, (float)ec.w, (float)ec.h};
                if (SDL_HasRectIntersectionFloat(&bulletR, &enemyR)) {
                    auto& eh = reg.get<Health>(ee);
                    eh.current -= bt.damage;

                    et.x += bt.dx * 24.f;

                    if (eh.current <= 0.f) {
                        eh.current = 0.f;
                        if (reg.all_of<Velocity>(ee)) {
                            auto& v = reg.get<Velocity>(ee);
                            v.dx = v.dy = 0.f;
                        }
                        if (reg.all_of<Renderable, AnimationState>(ee)) {
                            auto& r    = reg.get<Renderable>(ee);
                            auto& anim = reg.get<AnimationState>(ee);
                            if (auto* ead = reg.try_get<EnemyAnimData>(ee);
                                ead && ead->deadSheet && !ead->deadFrames.empty()) {
                                r.sheet         = ead->deadSheet;
                                r.frames        = ead->deadFrames;
                                r.renderW       = ead->spriteW;
                                r.renderH       = ead->spriteH;
                                anim.currentFrame = 0;
                                anim.totalFrames  = (int)ead->deadFrames.size();
                                anim.fps          = ead->deadFps;
                                anim.looping      = false;
                                ead->ApplyHitbox(ead->deadHitbox, reg, ee);
                            } else {
                                anim.looping = false;
                            }
                        }
                        if (!reg.all_of<DeadTag>(ee))
                            reg.emplace<DeadTag>(ee);
                    } else {
                        // Hurt flash + animation
                        if (auto* ead = reg.try_get<EnemyAnimData>(ee);
                            ead && ead->hurtSheet && !ead->hurtFrames.empty()) {
                            if (reg.all_of<Renderable, AnimationState>(ee)) {
                                auto& r    = reg.get<Renderable>(ee);
                                auto& anim = reg.get<AnimationState>(ee);
                                r.sheet         = ead->hurtSheet;
                                r.frames        = ead->hurtFrames;
                                r.renderW       = ead->spriteW;
                                r.renderH       = ead->spriteH;
                                anim.currentFrame = 0;
                                anim.totalFrames  = (int)ead->hurtFrames.size();
                                anim.fps          = ead->hurtFps;
                                anim.looping      = false;
                                ead->ApplyHitbox(ead->hurtHitbox, reg, ee);
                            }
                        }
                        if (!reg.all_of<HitFlash>(ee))
                            reg.emplace<HitFlash>(ee);
                        else
                            reg.get<HitFlash>(ee).timer = HitFlash{}.duration;
                    }

                    bulletsToDestroy.push_back(be);
                    break;
                }
            }

            // Player-owned bullets destroy hazard tiles on contact
            if (bt.playerOwned) {
                entt::entity hitHazard = entt::null;
                mTileGrid.Query(btr.x - 4.f, btr.y - 4.f, 16.f, 16.f, [&](entt::entity he) {
                    if (hitHazard != entt::null) return;
                    if (!reg.valid(he) || !reg.all_of<HazardTag>(he)) return;
                    auto& ht = reg.get<Transform>(he);
                    auto& hc = reg.get<Collider>(he);
                    SDL_FRect hazR = {ht.x, ht.y, (float)hc.w, (float)hc.h};
                    if (SDL_HasRectIntersectionFloat(&bulletR, &hazR))
                        hitHazard = he;
                });
                if (hitHazard != entt::null) {
                    auto he = hitHazard;
                        if (reg.all_of<HazardTag>(he)) reg.remove<HazardTag>(he);
                        if (reg.all_of<TileTag>(he))   reg.remove<TileTag>(he);
                        if (reg.all_of<Collider>(he))   reg.remove<Collider>(he);

                        // If the tile has a destroy animation, play it
                        const ActionTag* atag = reg.try_get<ActionTag>(he);
                        bool playedAnim = false;
                        if (atag && !atag->destroyAnimPath.empty()) {
                            SDL_Renderer* ren2 = mWindow->GetRenderer();
                            AnimatedTileDef def;
                            if (LoadAnimatedTileDef(atag->destroyAnimPath, def) &&
                                !def.framePaths.empty()) {
                                int tileW = 38, tileH = 38;
                                if (reg.all_of<Renderable>(he)) {
                                    const auto& rnd = reg.get<Renderable>(he);
                                    if (rnd.renderW > 0) tileW = rnd.renderW;
                                    if (rnd.renderH > 0) tileH = rnd.renderH;
                                }
                                std::vector<SDL_Texture*> frameTex;
                                frameTex.reserve(def.framePaths.size());
                                for (const auto& fp : def.framePaths)
                                    frameTex.push_back(GetCachedTexture(ren2, fp, 0, 0));
                                if (!frameTex.empty() && frameTex[0]) {
                                    tileAnimFrameMap[he] = std::move(frameTex);
                                    auto& fvec = tileAnimFrameMap[he];
                                    std::vector<SDL_Rect> animRects;
                                    animRects.reserve(fvec.size());
                                    for (auto* ft : fvec) {
                                        float fw = 0, fh = 0;
                                        if (ft) SDL_GetTextureSize(ft, &fw, &fh);
                                        animRects.push_back({0, 0, (int)fw, (int)fh});
                                    }
                                    float nativeW = 0, nativeH = 0;
                                    SDL_GetTextureSize(fvec[0], &nativeW, &nativeH);
                                    int animRenderW = tileW, animRenderH = tileH;
                                    if (nativeW > 0 && nativeH > 0) {
                                        float aspect = nativeW / nativeH;
                                        animRenderW = std::max(tileW, (int)(tileH * aspect));
                                        animRenderH = std::max(tileH, (int)(tileW / aspect));
                                    }
                                    auto& tr = reg.get<Transform>(he);
                                    float tileCX = tr.x + tileW * 0.5f;
                                    float tileCY = tr.y + tileH * 0.5f;
                                    tr.x = tileCX - animRenderW * 0.5f;
                                    tr.y = tileCY - animRenderH * 0.5f;
                                    if (reg.all_of<Renderable>(he)) reg.remove<Renderable>(he);
                                    reg.emplace<Renderable>(he, fvec[0], std::move(animRects),
                                                            false, animRenderW, animRenderH);
                                    if (reg.all_of<AnimationState>(he)) reg.remove<AnimationState>(he);
                                    reg.emplace<AnimationState>(
                                        he, 0, (int)fvec.size(), 0.0f, def.fps, false);
                                    if (!reg.all_of<TileAnimTag>(he))
                                        reg.emplace<TileAnimTag>(he);
                                    if (!reg.all_of<DestroyAnimTag>(he))
                                        reg.emplace<DestroyAnimTag>(
                                            he, (int)fvec.size(), def.fps, false);
                                    playedAnim = true;
                                }
                            }
                        }
                        if (!playedAnim) {
                            auto it = std::find(mSortedTileRenderList.begin(),
                                                mSortedTileRenderList.end(), he);
                            if (it != mSortedTileRenderList.end())
                                mSortedTileRenderList.erase(it);
                            reg.destroy(he);
                        }
                    bulletsToDestroy.push_back(be);
                }

                // Player-owned bullets trigger action tiles (floating objects, breakables)
                entt::entity hitAction = entt::null;
                mTileGrid.Query(btr.x - 4.f, btr.y - 4.f, 16.f, 16.f, [&](entt::entity ae) {
                    if (hitAction != entt::null) return;
                    if (!reg.valid(ae) || !reg.all_of<ActionTag>(ae)) return;
                    if (reg.all_of<HazardTag>(ae)) return;
                    if (!reg.all_of<Collider>(ae)) return;
                    auto& at2 = reg.get<Transform>(ae);
                    auto& ac2 = reg.get<Collider>(ae);
                    SDL_FRect actR = {at2.x, at2.y, (float)ac2.w, (float)ac2.h};
                    if (const auto* off = reg.try_get<ColliderOffset>(ae)) {
                        actR.x += off->x; actR.y += off->y;
                    }
                    if (SDL_HasRectIntersectionFloat(&bulletR, &actR))
                        hitAction = ae;
                });
                if (hitAction != entt::null) {
                    auto ae = hitAction;
                    auto& atag = reg.get<ActionTag>(ae);
                    atag.hitsRemaining--;
                    if (atag.hitsRemaining <= 0) {
                        collision.actionTilesTriggered.push_back(ae);
                    } else {
                        if (reg.all_of<HitFlash>(ae))
                            reg.get<HitFlash>(ae).timer = HitFlash{}.duration;
                        else
                            reg.emplace<HitFlash>(ae);
                    }
                    if (reg.all_of<FloatTag>(ae)) {
                        auto* fs = reg.try_get<FloatState>(ae);
                        if (fs) {
                            fs->driftVx += bt.dx * bt.speed * 0.3f;
                            fs->driftVy += bt.dy * bt.speed * 0.3f;
                            fs->spinSpeed += bt.dx * 180.f;
                        }
                    }
                    bulletsToDestroy.push_back(be);
                }
            }
        });
        for (auto be : bulletsToDestroy)
            if (reg.valid(be)) reg.destroy(be);
    }

    if (bulletKilledPlayer && !mPlayerDying) {
        mPlayerDying    = true;
        mDeathAnimTimer = 0.0f;
        auto dv = reg.view<PlayerTag, Velocity, AnimationState, Renderable, AnimationSet>();
        dv.each([](Velocity& v, AnimationState& anim, Renderable& r,
                   const AnimationSet& set) {
            v.dx = 0.0f; v.dy = 0.0f;
            const std::vector<SDL_Rect>* frames = nullptr;
            SDL_Texture* sheet = nullptr;
            float fps = 8.0f;
            AnimationID id = AnimationID::DEATH;
            if (!set.death.empty()) {
                frames = &set.death;
                sheet  = set.deathSheet;
                fps    = (set.deathFps > 0.0f) ? set.deathFps : 8.0f;
            } else if (!set.hurt.empty()) {
                frames = &set.hurt;
                sheet  = set.hurtSheet;
                fps    = (set.hurtFps > 0.0f) ? set.hurtFps : 12.0f;
                id     = AnimationID::HURT;
            }
            if (frames && sheet) {
                r.sheet           = sheet;
                r.frames          = *frames;
                anim.currentFrame = 0;
                anim.timer        = 0.0f;
                anim.fps          = fps;
                anim.looping      = false;
                anim.totalFrames  = (int)frames->size();
                anim.currentAnim  = id;
            }
        });
        mCamera.StartShake(6.0f, 0.2f);
    }

    // --- Shield orbit ---
    {
        auto shv = reg.view<PlayerTag, Transform, Collider, ActiveShield>();
        shv.each([&](entt::entity pe, const Transform& pt, const Collider& pc,
                     ActiveShield& as) {
            for (auto it = as.shields.begin(); it != as.shields.end(); ) {
                it->remaining -= dt;
                if (it->remaining <= 0.f)
                    it = as.shields.erase(it);
                else
                    ++it;
            }
            if (as.shields.empty()) {
                reg.remove<ActiveShield>(pe);
                return;
            }

            bool gamepadAimed = false;
            if (mCachedPad) {
                float rx = SDL_GetGamepadAxis(mCachedPad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.f;
                float ry = SDL_GetGamepadAxis(mCachedPad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.f;
                constexpr float DEAD = 0.25f;
                if (rx * rx + ry * ry > DEAD * DEAD) {
                    as.angle = std::atan2(ry, rx);
                    gamepadAimed = true;
                }
            }
            if (!gamepadAimed) {
                constexpr float ROTATE_SPEED = 4.0f;
                const bool* keys = SDL_GetKeyboardState(nullptr);
                if (keys[SDL_SCANCODE_LEFT])  as.angle -= ROTATE_SPEED * dt;
                if (keys[SDL_SCANCODE_RIGHT]) as.angle += ROTATE_SPEED * dt;
            }
        });
    }

    // --- Turret power-up ---
    {
        auto tpv = reg.view<PlayerTag, Transform, Collider, ActiveTurretPowerUp>();
        tpv.each([&](entt::entity pe, const Transform& pt, const Collider& pc,
                      ActiveTurretPowerUp& tp) {
            tp.remaining -= dt;
            if (tp.remaining <= 0.f) {
                reg.remove<ActiveTurretPowerUp>(pe);
                return;
            }

            bool gpAimed = false;
            if (mCachedPad) {
                float rx = SDL_GetGamepadAxis(mCachedPad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.f;
                float ry = SDL_GetGamepadAxis(mCachedPad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.f;
                constexpr float DEAD = 0.25f;
                if (rx * rx + ry * ry > DEAD * DEAD) {
                    tp.angle = std::atan2(ry, rx);
                    gpAimed = true;
                }
            }
            if (!gpAimed) {
                constexpr float ROTATE_SPEED = 4.0f;
                const bool* keys = SDL_GetKeyboardState(nullptr);
                if (keys[SDL_SCANCODE_LEFT])  tp.angle -= ROTATE_SPEED * dt;
                if (keys[SDL_SCANCODE_RIGHT]) tp.angle += ROTATE_SPEED * dt;
            }

            // Firing — barrel is centered on player, extends outward
            constexpr float BARREL_LEN = 28.f;
            tp.cooldown -= dt;
            if (tp.cooldown <= 0.f) {
                tp.cooldown = 1.f / tp.fireRate;
                float pcx = pt.x + pc.w * 0.5f;
                float pcy = pt.y + pc.h * 0.5f;
                float bdx = std::cos(tp.angle);
                float bdy = std::sin(tp.angle);
                float bx = pcx + bdx * BARREL_LEN;
                float by = pcy + bdy * BARREL_LEN;

                auto bullet = reg.create();
                reg.emplace<Transform>(bullet, bx, by);
                reg.emplace<Collider>(bullet, 8, 8);
                BulletTag bt;
                bt.dx       = bdx; bt.dy = bdy;
                bt.speed    = tp.bulletSpeed;
                bt.damage   = tp.damage;
                bt.originX  = bx; bt.originY = by;
                bt.maxRange = tp.range;
                bt.sourceTurret = entt::null;
                bt.playerOwned  = true;
                reg.emplace<BulletTag>(bullet, bt);
                if (mBulletTex) {
                    reg.emplace<Renderable>(bullet, mBulletTex, sBulletFrames, false, 8, 8);
                    reg.emplace<AnimationState>(bullet, 0, 1, 0.0f, 1.0f, false);
                }

                if (!tp.sfxId.empty() && Audio() && Audio()->IsReady())
                    Audio()->Sfx().Play(tp.sfxId);
            }
        });
    }

    {
        auto pView = reg.view<PlayerTag, Transform, Collider>();
        pView.each([&](const Transform& pt, const Collider& pc) {
            float cx = pt.x + pc.w * 0.5f, cy = pt.y + pc.h * 0.5f;
            mCamera.Update(
                cx, cy, mWindow->GetWidth(), mWindow->GetHeight(), mLevelW, mLevelH, dt);
        });
    }
    mCamera.TickShake(dt);

    if (mCachedPad) {
        float shakeAmp = std::abs(mCamera.shakeOffX) + std::abs(mCamera.shakeOffY);
        float punchAmp = std::abs(mCamera.punchOffX) + std::abs(mCamera.punchOffY);
        if (shakeAmp > 0.1f || punchAmp > 0.1f) {
            float lowFrac  = std::clamp(shakeAmp / 8.0f, 0.0f, 1.0f);
            float highFrac = std::clamp(punchAmp / 6.0f, 0.0f, 1.0f);
            Uint16 lo  = (Uint16)(lowFrac  * 0xFFFF);
            Uint16 hi  = (Uint16)(highFrac * 0xFFFF);
            lo = std::max(lo, hi);
            SDL_RumbleGamepad(mCachedPad, lo, hi, 50);
        }
    }
}

void GameScene::Render(Window& window, float alpha) {
    SDL_Renderer* ren = window.GetRenderer();
    window.Render(); // clear

    float totalShakeX = mCamera.shakeOffX + mCamera.punchOffX;
    float totalShakeY = mCamera.shakeOffY + mCamera.punchOffY;
    mCamera.x += totalShakeX;
    mCamera.y += totalShakeY;

    if (background->GetFitMode() == FitMode::SCROLL)
        background->RenderScrolling(ren, mCamera.x, (float)mLevelW);
    else if (background->GetFitMode() == FitMode::SCROLL_WIDE)
        background->RenderScrollingWide(ren, mCamera.x, (float)mLevelW);
    else
        background->Render(ren);

    for (size_t i = 0; i < mParallaxImages.size(); ++i)
        mParallaxImages[i]->RenderScrolling(ren, mCamera.x * mParallaxFactors[i], (float)mLevelW);

    const int W = window.GetWidth();
    const int H = window.GetHeight();
    if (levelComplete) {
        RenderSystem(reg, ren, mCamera.x, mCamera.y, W, H, &mSortedTileRenderList, alpha, &mSortedFrontPropList);
        RenderTurrets(ren, W, H);
        RenderShield(ren, W, H);
        RenderTurretPowerUp(ren, W, H);
        HUDSystem(reg,
                  ren,
                  W,
                  healthText.get(),
                  gravityText.get(),
                  goalText.get(),
                  totalGoals - goalsCollected,
                  totalGoals,
                  stompText.get(),
                  stompCount);
        if (levelCompleteText)
            levelCompleteText->Render(ren);
    } else if (gameOver) {
        RenderSystem(reg, ren, mCamera.x, mCamera.y, W, H, &mSortedTileRenderList, alpha, &mSortedFrontPropList);
        RenderTurrets(ren, W, H);
        RenderShield(ren, W, H);
        RenderTurretPowerUp(ren, W, H);
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 140);
        SDL_FRect dimOverlay = {0, 0, (float)W, (float)H};
        SDL_RenderFillRect(ren, &dimOverlay);
        gameOverText->Render(ren);
        retryButton->Render(ren);
        retryBtnText->Render(ren);
        retryKeyText->Render(ren);
    } else {
        locationText->Render(ren);
        actionText->Render(ren);
        RenderSystem(reg, ren, mCamera.x, mCamera.y, W, H, &mSortedTileRenderList, alpha, &mSortedFrontPropList);
        RenderTurrets(ren, W, H);
        RenderShield(ren, W, H);
        RenderTurretPowerUp(ren, W, H);

        // --- Debug hitbox overlay (F1) ---
        if (mDebugHitboxes) {
            SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

            auto fill = [&](SDL_Rect r, Uint8 ri, Uint8 gi, Uint8 bi, Uint8 ai) {
                SDL_SetRenderDrawColor(ren, ri, gi, bi, ai);
                SDL_FRect fr = {(float)r.x, (float)r.y, (float)r.w, (float)r.h};
                SDL_RenderFillRect(ren, &fr);
            };
            auto outline = [&](SDL_Rect r, Uint8 ri, Uint8 gi, Uint8 bi) {
                SDL_SetRenderDrawColor(ren, ri, gi, bi, 255);
                SDL_FRect fr = {(float)r.x, (float)r.y, (float)r.w, (float)r.h};
                SDL_RenderRect(ren, &fr);
            };

            {
                auto pv = reg.view<PlayerTag, Transform, Collider, AnimationState>();
                pv.each([&](entt::entity pe, const Transform& t, const Collider& c,
                            const AnimationState& pa) {
                    int      sx = (int)(t.x - mCamera.x), sy = (int)(t.y - mCamera.y);
                    SDL_Rect r = {sx, sy, c.w, c.h};
                    fill(r, 0, 255, 255, 50);
                    outline(r, 0, 255, 255);

                    const char* animName = "??";
                    switch (pa.currentAnim) {
                        case AnimationID::IDLE:  animName = "IDLE";  break;
                        case AnimationID::WALK:  animName = "WALK";  break;
                        case AnimationID::JUMP:  animName = "JUMP";  break;
                        case AnimationID::HURT:  animName = "HURT";  break;
                        case AnimationID::DUCK:  animName = "DUCK";  break;
                        case AnimationID::FRONT: animName = "FALL";  break;
                        case AnimationID::SLASH: animName = "SLASH"; break;
                        case AnimationID::DEATH: animName = "DEATH"; break;
                        case AnimationID::NONE:  animName = "NONE";  break;
                    }
                    std::string info = std::string(animName) + " "
                                     + std::to_string(c.w) + "x" + std::to_string(c.h);
                    Text lbl(info, SDL_Color{0, 255, 255, 255}, sx + 2, sy - 14, 10);
                    lbl.Render(ren);

                    if (const auto* roff = reg.try_get<RenderOffset>(pe)) {
                        std::string offStr = "roff " + std::to_string(roff->x)
                                           + "," + std::to_string(roff->y);
                        Text offLbl(offStr, SDL_Color{0, 200, 200, 200},
                                    sx + 2, sy + 2, 9);
                        offLbl.Render(ren);
                    }
                });
            }
            {
                auto tv = reg.view<TileTag, Transform, Collider>();
                tv.each([&](entt::entity te, const Transform& t, const Collider& c) {
                    float tx = t.x, ty = t.y;
                    if (const auto* off = reg.try_get<ColliderOffset>(te)) {
                        tx += off->x;
                        ty += off->y;
                    }
                    SDL_Rect r = {(int)(tx - mCamera.x), (int)(ty - mCamera.y), c.w, c.h};
                    fill(r, 255, 255, 255, 18);
                    outline(r, 160, 160, 255);
                });
            }
            {
                auto hv = reg.view<HazardTag, Transform, Collider>();
                hv.each([&](entt::entity he, const Transform& t, const Collider& c) {
                    float hx = t.x, hy = t.y;
                    if (const auto* off = reg.try_get<ColliderOffset>(he)) {
                        hx += off->x;
                        hy += off->y;
                    }
                    SDL_Rect r = {(int)(hx - mCamera.x), (int)(hy - mCamera.y), c.w, c.h};
                    fill(r, 255, 40, 40, 60);
                    outline(r, 255, 40, 40);
                });
            }
            {
                auto lv = reg.view<LadderTag, Transform, Collider>();
                lv.each([&](const Transform& t, const Collider& c) {
                    SDL_Rect r = {(int)(t.x - mCamera.x), (int)(t.y - mCamera.y), c.w, c.h};
                    outline(r, 60, 255, 60);
                });
            }
            {
                auto ev = reg.view<EnemyTag, Transform, Collider>(entt::exclude<DeadTag>);
                ev.each([&](const Transform& t, const Collider& c) {
                    SDL_Rect r = {(int)(t.x - mCamera.x), (int)(t.y - mCamera.y), c.w, c.h};
                    fill(r, 255, 140, 0, 45);
                    outline(r, 255, 140, 0);
                });
            }

            SDL_FRect hintBg = {
                0, (float)(window.GetHeight() - 20), (float)window.GetWidth(), 20};
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 140);
            SDL_RenderFillRect(ren, &hintBg);
            Text hint(
                "[F1] Hitboxes  Cyan=Player  White=Solid  Red=Hazard  Green=Ladder  "
                "Orange=Enemy",
                SDL_Color{220, 220, 220, 255},
                8,
                window.GetHeight() - 16,
                11);
            hint.Render(ren);
        }

        HUDSystem(reg,
                  ren,
                  W,
                  healthText.get(),
                  gravityText.get(),
                  goalText.get(),
                  totalGoals - goalsCollected,
                  totalGoals,
                  stompText.get(),
                  stompCount);
    }

    mCamera.x -= totalShakeX;
    mCamera.y -= totalShakeY;

    if (mPaused)
        RenderPauseOverlay(window);
    window.Update();
}

// --- Pause overlay ---
void GameScene::BuildPauseUI(int W, int H) {
    int cx = W / 2, cy = H / 2;

    auto titleSize = Text::Measure("PAUSED", 36);
    mPauseTitleLbl = std::make_unique<Text>(
        "PAUSED", SDL_Color{255, 215, 0, 255},
        cx - titleSize.x / 2, cy - 140, 36);

    mPauseResumeRect = {cx - 130, cy - 60, 260, 55};
    mPauseResumeBtn  = std::make_unique<Rectangle>(mPauseResumeRect);
    mPauseResumeBtn->SetColor({40, 160, 80, 255});
    mPauseResumeBtn->SetHoverColor({60, 200, 100, 255});
    auto [rx, ry] = Text::CenterInRect("Resume", 28, mPauseResumeRect);
    mPauseResumeLbl =
        std::make_unique<Text>("Resume", SDL_Color{255, 255, 255, 255}, rx, ry, 28);

    std::string backLabel = mFromEditor ? "Back to Editor" : "Back to Title";
    mPauseBackRect        = {cx - 130, cy + 20, 260, 55};
    mPauseBackBtn         = std::make_unique<Rectangle>(mPauseBackRect);
    mPauseBackBtn->SetColor({120, 50, 50, 255});
    mPauseBackBtn->SetHoverColor({180, 70, 70, 255});
    auto [bx, by] = Text::CenterInRect(backLabel, 22, mPauseBackRect);
    mPauseBackLbl =
        std::make_unique<Text>(backLabel, SDL_Color{255, 220, 220, 255}, bx, by, 22);

    std::string hint1 = "ESC  or  Start  to resume";
    auto h1Size = Text::Measure(hint1, 14);
    mPauseHintLbl = std::make_unique<Text>(
        hint1, SDL_Color{140, 140, 160, 255}, cx - h1Size.x / 2, cy + 100, 14);

    std::string hint2 = "Back  to return to title";
    auto h2Size = Text::Measure(hint2, 14);
    mPauseHintLbl2 = std::make_unique<Text>(
        hint2, SDL_Color{110, 110, 130, 255}, cx - h2Size.x / 2, cy + 120, 14);
}

void GameScene::RenderPauseOverlay(Window& window) {
    SDL_Renderer* ren = window.GetRenderer();
    int           W = window.GetWidth(), H = window.GetHeight();

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
    SDL_FRect full = {0, 0, (float)W, (float)H};
    SDL_RenderFillRect(ren, &full);

    SDL_Rect panel = {W / 2 - 180, H / 2 - 160, 360, 320};
    SDL_SetRenderDrawColor(ren, 18, 20, 32, 230);
    SDL_FRect fp = {(float)panel.x, (float)panel.y, (float)panel.w, (float)panel.h};
    SDL_RenderFillRect(ren, &fp);

    SDL_SetRenderDrawColor(ren, 80, 120, 220, 255);
    SDL_RenderRect(ren, &fp);

    if (mPauseTitleLbl)
        mPauseTitleLbl->Render(ren);
    if (mPauseResumeBtn)
        mPauseResumeBtn->Render(ren);
    if (mPauseResumeLbl)
        mPauseResumeLbl->Render(ren);
    if (mPauseBackBtn)
        mPauseBackBtn->Render(ren);
    if (mPauseBackLbl)
        mPauseBackLbl->Render(ren);
    if (mPauseHintLbl)
        mPauseHintLbl->Render(ren);
    if (mPauseHintLbl2)
        mPauseHintLbl2->Render(ren);
}

// --- Turret barrel overlay ---
void GameScene::RenderTurrets(SDL_Renderer* ren, int W, int H) {
    float playerCX = 0, playerCY = 0;
    bool  havePlayer = false;
    {
        auto pv = reg.view<PlayerTag, Transform, Collider>();
        pv.each([&](const Transform& pt, const Collider& pc) {
            playerCX   = pt.x + pc.w * 0.5f;
            playerCY   = pt.y + pc.h * 0.5f;
            havePlayer = true;
        });
    }

    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

    auto sv = reg.view<ShooterTag, Transform, Collider>();
    sv.each([&](const ShooterTag& sh, const Transform& tt, const Collider& tc) {
        float sx = tt.x - mCamera.x;
        float sy = tt.y - mCamera.y;
        float sw = (float)tc.w;
        float sh2 = (float)tc.h;

        // Cull off-screen turrets
        if (sx + sw < 0 || sx > W || sy + sh2 < 0 || sy > H) return;

        float cx = sx + sw * 0.5f;
        float cy = sy + sh2 * 0.5f;

        float baseW = std::max(8.f, sw * 0.3f);
        float baseH = std::max(8.f, sh2 * 0.3f);
        SDL_FRect base = {cx - baseW / 2, cy - baseH / 2, baseW, baseH};
        SDL_SetRenderDrawColor(ren, 20, 20, 20, 230);
        SDL_RenderFillRect(ren, &base);
        SDL_SetRenderDrawColor(ren, 70, 70, 70, 255);
        SDL_RenderRect(ren, &base);

        // Barrel: if we have a player, aim at them; otherwise use the configured side
        float aimDx = 0, aimDy = 0;
        if (havePlayer) {
            float ax = (playerCX - mCamera.x) - cx;
            float ay = (playerCY - mCamera.y) - cy;
            float aLen = std::sqrt(ax * ax + ay * ay);
            if (aLen > 0.1f) { aimDx = ax / aLen; aimDy = ay / aLen; }
            else              { aimDx = 1; }
        } else {
            switch (sh.side) {
                case 0: aimDy = -1; break;
                case 1: aimDx =  1; break;
                case 2: aimDy =  1; break;
                case 3: aimDx = -1; break;
            }
        }

        float barrelLen = std::max(6.f, std::min(sw, sh2) * 0.45f);
        float barrelThk = std::max(4.f, std::min(baseW, baseH) * 0.45f);

        float perpX = -aimDy, perpY = aimDx;

        float tipX = cx + aimDx * (baseW * 0.5f + barrelLen);
        float tipY = cy + aimDy * (baseH * 0.5f + barrelLen);
        float startX = cx + aimDx * baseW * 0.4f;
        float startY = cy + aimDy * baseH * 0.4f;

        // SDL has no rotated rects; draw barrel with RenderGeometry.
        SDL_Vertex verts[4];
        SDL_FColor darkGray = {15.f/255.f, 15.f/255.f, 15.f/255.f, 240.f/255.f};
        float halfThk = barrelThk * 0.5f;

        verts[0].position = {startX - perpX * halfThk, startY - perpY * halfThk};
        verts[0].color = darkGray;
        verts[1].position = {startX + perpX * halfThk, startY + perpY * halfThk};
        verts[1].color = darkGray;
        verts[2].position = {tipX + perpX * halfThk, tipY + perpY * halfThk};
        verts[2].color = darkGray;
        verts[3].position = {tipX - perpX * halfThk, tipY - perpY * halfThk};
        verts[3].color = darkGray;

        int indices[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(ren, nullptr, verts, 4, indices, 6);

        SDL_SetRenderDrawColor(ren, 80, 80, 80, 255);
        SDL_RenderLine(ren, verts[0].position.x, verts[0].position.y,
                            verts[1].position.x, verts[1].position.y);
        SDL_RenderLine(ren, verts[1].position.x, verts[1].position.y,
                            verts[2].position.x, verts[2].position.y);
        SDL_RenderLine(ren, verts[2].position.x, verts[2].position.y,
                            verts[3].position.x, verts[3].position.y);
        SDL_RenderLine(ren, verts[3].position.x, verts[3].position.y,
                            verts[0].position.x, verts[0].position.y);

        float muzzleSz = std::max(3.f, barrelThk * 0.6f);
        SDL_FRect muzzle = {tipX - muzzleSz / 2, tipY - muzzleSz / 2, muzzleSz, muzzleSz};
        SDL_SetRenderDrawColor(ren, 255, 140, 30, 255);
        SDL_RenderFillRect(ren, &muzzle);
    });
}

void GameScene::RenderShield(SDL_Renderer* ren, int W, int H) {
    constexpr float PI2 = 2.f * 3.14159265f;
    auto sv = reg.view<PlayerTag, Transform, Collider, ActiveShield>();
    sv.each([&](const Transform& pt, const Collider& pc, const ActiveShield& as) {
        float pcx = pt.x + pc.w * 0.5f;
        float pcy = pt.y + pc.h * 0.5f;
        int n = (int)as.shields.size();
        if (n == 0) return;

        float minRemaining = as.shields[0].remaining;
        for (auto& se : as.shields) minRemaining = std::min(minRemaining, se.remaining);
        float circleAlpha = (minRemaining < 3.f) ? (minRemaining / 3.f) : 1.f;

        for (int i = 0; i < n; ++i) {
            const auto& se = as.shields[i];
            float a = as.angle + (float)i / n * PI2;
            float wx = pcx + std::cos(a) * as.orbitRadius;
            float wy = pcy + std::sin(a) * as.orbitRadius;

            float screenX = wx - mCamera.x;
            float screenY = wy - mCamera.y;
            if (screenX + se.renderW < 0 || screenX > W ||
                screenY + se.renderH < 0 || screenY > H)
                continue;

            SDL_FRect dst = {screenX - se.renderW * 0.5f,
                             screenY - se.renderH * 0.5f,
                             (float)se.renderW, (float)se.renderH};

            float alpha = (se.remaining < 3.f) ? (se.remaining / 3.f) : 1.f;
            Uint8 alphaU = (Uint8)(alpha * 255);
            if (se.tex) {
                SDL_SetTextureAlphaMod(se.tex, alphaU);
                SDL_RenderTexture(ren, se.tex, nullptr, &dst);
                SDL_SetTextureAlphaMod(se.tex, 255);
            } else {
                SDL_SetRenderDrawColor(ren, 100, 180, 255, alphaU);
                SDL_RenderFillRect(ren, &dst);
            }
        }

        SDL_SetRenderDrawColor(ren, 100, 180, 255, (Uint8)(40 * circleAlpha));
        constexpr int SEGS = 32;
        float rad = as.orbitRadius;
        float cx = pcx - mCamera.x, cy = pcy - mCamera.y;
        for (int i = 0; i < SEGS; ++i) {
            float a1 = (float)i / SEGS * PI2;
            float a2 = (float)(i + 1) / SEGS * PI2;
            SDL_RenderLine(ren, cx + std::cos(a1) * rad, cy + std::sin(a1) * rad,
                           cx + std::cos(a2) * rad, cy + std::sin(a2) * rad);
        }
    });
}

void GameScene::RenderTurretPowerUp(SDL_Renderer* ren, int W, int H) {
    auto tv = reg.view<PlayerTag, Transform, Collider, ActiveTurretPowerUp>();
    tv.each([&](const Transform& pt, const Collider& pc, const ActiveTurretPowerUp& tp) {
        float pcx = pt.x + pc.w * 0.5f;
        float pcy = pt.y + pc.h * 0.5f;
        float cx = pcx - mCamera.x;
        float cy = pcy - mCamera.y;
        if (cx < -30 || cx > W + 30 || cy < -30 || cy > H + 30) return;

        float aimDx = std::cos(tp.angle);
        float aimDy = std::sin(tp.angle);
        float perpX = -aimDy, perpY = aimDx;

        constexpr float BASE_SZ   = 14.f;
        constexpr float BARREL_L  = 28.f;
        constexpr float BARREL_TH = 6.f;

        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        float alpha = (tp.remaining < 3.f) ? (tp.remaining / 3.f) : 1.f;
        Uint8 a = (Uint8)(alpha * 255);

        SDL_FRect baseR = {cx - BASE_SZ * 0.5f, cy - BASE_SZ * 0.5f, BASE_SZ, BASE_SZ};
        SDL_SetRenderDrawColor(ren, 30, 30, 30, a);
        SDL_RenderFillRect(ren, &baseR);

        float tipX = cx + aimDx * BARREL_L;
        float tipY = cy + aimDy * BARREL_L;
        float hw = BARREL_TH * 0.5f;

        SDL_Vertex verts[4];
        SDL_FColor barrelCol = {0.15f, 0.15f, 0.15f, alpha};
        verts[0] = {{cx - perpX * hw, cy - perpY * hw}, barrelCol, {0, 0}};
        verts[1] = {{cx + perpX * hw, cy + perpY * hw}, barrelCol, {0, 0}};
        verts[2] = {{tipX + perpX * hw, tipY + perpY * hw}, barrelCol, {0, 0}};
        verts[3] = {{tipX - perpX * hw, tipY - perpY * hw}, barrelCol, {0, 0}};
        int idx[6] = {0, 1, 2, 0, 2, 3};
        SDL_RenderGeometry(ren, nullptr, verts, 4, idx, 6);

        constexpr float MUZ = 5.f;
        SDL_FRect muz = {tipX - MUZ * 0.5f, tipY - MUZ * 0.5f, MUZ, MUZ};
        SDL_SetRenderDrawColor(ren, 255, 160, 40, a);
        SDL_RenderFillRect(ren, &muz);

        SDL_SetRenderDrawColor(ren, 255, 160, 40, (Uint8)(30 * alpha));
        constexpr int SEGS = 24;
        float rad = BARREL_L;
        for (int i = 0; i < SEGS; ++i) {
            float a1 = (float)i / SEGS * 2.f * 3.14159265f;
            float a2 = (float)(i + 1) / SEGS * 2.f * 3.14159265f;
            SDL_RenderLine(ren, cx + std::cos(a1) * rad, cy + std::sin(a1) * rad,
                           cx + std::cos(a2) * rad, cy + std::sin(a2) * rad);
        }
    });
}

// --- Private helpers ---
void GameScene::Spawn() {
    SDL_Renderer* ren = mWindow->GetRenderer();

    healthText  = std::make_unique<Text>("100", SDL_Color{255, 255, 255, 255}, 0, 0, 16);
    gravityText = std::make_unique<Text>("", SDL_Color{100, 200, 255, 255}, 0, 0, 20);
    goalText =
        std::make_unique<Text>("Goals: 0", SDL_Color{0, 255, 120, 255}, 0, 0, 16);
    stompText = std::make_unique<Text>(
        "Enemies Stomped: 0", SDL_Color{255, 100, 100, 255}, 0, 0, 16);

    // Coins removed — goal tiles are now the level completion mechanic.
    mLevelW = (float)mWindow->GetWidth();
    mLevelH = (float)mWindow->GetHeight();
    for (const auto& ts : mLevel.tiles) {
        float right = ts.x + ts.w, bottom = ts.y + ts.h;
        if (ts.HasMoving()) {
            if (ts.moving->horiz)
                right = std::max(right, ts.x + ts.moving->range + ts.w);
            else
                bottom = std::max(bottom, ts.y + ts.moving->range + ts.h);
        }
        if (right > mLevelW)
            mLevelW = right;
        if (bottom > mLevelH)
            mLevelH = bottom;
    }
    mLevelW += (float)mWindow->GetWidth() * 0.25f;
    mLevelH += (float)mWindow->GetHeight() * 0.25f;

    // Load profile once for all spawn-time lookups.
    PlayerProfile spawnProfile;
    bool hasSpawnProfile = !mProfilePath.empty() && LoadPlayerProfile(mProfilePath, spawnProfile);

    int pColW, pColH, pROffX, pROffY;
    {
        const AnimHitbox& idleHB =
            hasSpawnProfile ? spawnProfile.Slot(PlayerAnimSlot::Idle).hitbox : AnimHitbox{};
        if (!idleHB.IsDefault()) {
            pColW  = idleHB.w;
            pColH  = idleHB.h;
            pROffX = -idleHB.x;
            pROffY = -idleHB.y;
        } else {
            const float sx     = (float)mPlayerSpriteW / PLAYER_SPRITE_WIDTH;
            const float sy     = (float)mPlayerSpriteH / PLAYER_SPRITE_HEIGHT;
            int         insetX = (int)(PLAYER_BODY_INSET_X * sx);
            int         insetT = (int)(PLAYER_BODY_INSET_TOP * sy);
            int         insetB = (int)(PLAYER_BODY_INSET_BOTTOM * sy);
            pColW              = mPlayerSpriteW - insetX * 2;
            pColH              = mPlayerSpriteH - insetT - insetB;
            pROffX             = -insetX;
            pROffY             = -insetT;
        }
    }

    {
        float px  = mLevel.player.x + pColW * 0.5f;
        float py  = mLevel.player.y + pColH * 0.5f;
        mCamera.x = px - mWindow->GetWidth() * 0.5f;
        mCamera.y = py - mWindow->GetHeight() * 0.5f;
        if (mCamera.x < 0)
            mCamera.x = 0;
        if (mCamera.y < 0)
            mCamera.y = 0;
        if (mCamera.x + mWindow->GetWidth() > mLevelW)
            mCamera.x = mLevelW - mWindow->GetWidth();
        if (mCamera.y + mWindow->GetHeight() > mLevelH)
            mCamera.y = mLevelH - mWindow->GetHeight();
        if (mCamera.x < 0)
            mCamera.x = 0;
        if (mCamera.y < 0)
            mCamera.y = 0;
    }

    float playerX = mLevel.player.x;
    float playerY = mLevel.player.y;

    auto player = reg.create();
    reg.emplace<Transform>(player, playerX, playerY);
    reg.emplace<PrevTransform>(player, playerX, playerY); // interpolation
    reg.emplace<Velocity>(player);
    {
        AnimationState as;
        as.currentFrame = 0;
        as.totalFrames  = (int)idleFrames.size();
        as.timer        = 0.0f;
        as.fps          = 10.0f;
        as.looping      = true;
        as.currentAnim  = AnimationID::IDLE; // pre-set so PlayerStateSystem never
                                             // sees NONE and triggers a spurious swap
        reg.emplace<AnimationState>(player, as);
    }
    reg.emplace<Renderable>(player, knightIdleSheet->GetTexture(), idleFrames, false,
                             mPlayerSpriteW, mPlayerSpriteH);
    reg.emplace<PlayerTag>(player);
    reg.emplace<Health>(player);
    reg.emplace<Collider>(player, pColW, pColH);
    reg.emplace<RenderOffset>(player, pROffX, pROffY);
    {
        PlayerBaseCollider base;
        base.standW     = pColW;
        base.standH     = pColH;
        base.standRoffX = pROffX;
        base.standRoffY = pROffY;

        const AnimHitbox& crouchHB =
            hasSpawnProfile ? spawnProfile.Slot(PlayerAnimSlot::Crouch).hitbox : AnimHitbox{};
        if (!crouchHB.IsDefault()) {
            base.duckW     = crouchHB.w;
            base.duckH     = crouchHB.h;
            base.duckRoffX = -crouchHB.x;
            base.duckRoffY = -crouchHB.y;
        } else {
            base.duckW     = pColW;
            base.duckH     = pColH / 2;
            base.duckRoffX = pROffX;
            base.duckRoffY = -(mPlayerSpriteH - base.duckH);
        }

        auto toAnimCol = [](const AnimHitbox& hb) -> AnimCollider {
            if (hb.IsDefault()) return {};
            return {hb.w, hb.h, -hb.x, -hb.y};
        };

        if (hasSpawnProfile) {
            base.walk  = toAnimCol(spawnProfile.Slot(PlayerAnimSlot::Walk).hitbox);
            base.jump  = toAnimCol(spawnProfile.Slot(PlayerAnimSlot::Jump).hitbox);
            base.fall  = toAnimCol(spawnProfile.Slot(PlayerAnimSlot::Fall).hitbox);
            base.slash = toAnimCol(spawnProfile.Slot(PlayerAnimSlot::Slash).hitbox);
            base.hurt  = toAnimCol(spawnProfile.Slot(PlayerAnimSlot::Hurt).hitbox);
            base.death = toAnimCol(spawnProfile.Slot(PlayerAnimSlot::Death).hitbox);
        }

        reg.emplace<PlayerBaseCollider>(player, base);
    }
    reg.emplace<InvincibilityTimer>(player);
    {
        GravityState gs;
        if (mLevel.gravityMode == GravityMode::OpenWorld) {
            gs.active     = false;
            gs.isGrounded = true;
        }
        reg.emplace<GravityState>(player, gs);
    }
    if (mLevel.gravityMode == GravityMode::OpenWorld)
        reg.emplace<OpenWorldTag>(player);
    reg.emplace<ClimbState>(player);
    reg.emplace<HazardState>(player);
    reg.emplace<AttackState>(player);
    reg.emplace<DashState>(player);
    auto slotFps = [&](PlayerAnimSlot slot) -> float {
        return mSlotFps[static_cast<int>(slot)];
    };
    // Unfilled profile slots have their frames patched to idleFrames (by copy, not alias),
    // so resolveSheet uses slot capability instead of pointer identity to pick the right texture.
    SDL_Texture* idleT = knightIdleSheet->GetTexture();
    std::unordered_set<int> customSlots;
    if (mHasProfile && hasSpawnProfile) {
        for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
            auto s = static_cast<PlayerAnimSlot>(i);
            if (spawnProfile.HasSlot(s))
                customSlots.insert(i);
        }
    }
    auto resolveSheet = [&](SDL_Texture* slotTex, PlayerAnimSlot slot) -> SDL_Texture* {
        if (mHasProfile && !customSlots.count(static_cast<int>(slot)))
            return idleT;
        return slotTex;
    };

    reg.emplace<AnimationSet>(
        player,
        AnimationSet{
            .idle       = idleFrames,
            .idleSheet  = idleT,
            .idleFps    = slotFps(PlayerAnimSlot::Idle),
            .walk       = walkFrames,
            .walkSheet  = resolveSheet(knightWalkSheet->GetTexture(),  PlayerAnimSlot::Walk),
            .walkFps    = slotFps(PlayerAnimSlot::Walk),
            .jump       = jumpFrames,
            .jumpSheet  = resolveSheet(knightJumpSheet->GetTexture(),  PlayerAnimSlot::Jump),
            .jumpFps    = slotFps(PlayerAnimSlot::Jump),
            .hurt       = hurtFrames,
            .hurtSheet  = resolveSheet(knightHurtSheet->GetTexture(),  PlayerAnimSlot::Hurt),
            .hurtFps    = slotFps(PlayerAnimSlot::Hurt),
            .duck       = duckFrames,
            .duckSheet  = resolveSheet(knightSlideSheet->GetTexture(), PlayerAnimSlot::Crouch),
            .duckFps    = slotFps(PlayerAnimSlot::Crouch),
            .front      = frontFrames,
            .frontSheet = resolveSheet(knightFallSheet->GetTexture(),  PlayerAnimSlot::Fall),
            .frontFps   = slotFps(PlayerAnimSlot::Fall),
            .slash      = slashFrames,
            .slashSheet = resolveSheet(knightSlashSheet->GetTexture(), PlayerAnimSlot::Slash),
            .slashFps   = slotFps(PlayerAnimSlot::Slash),
            .death      = deathFrames,
            .deathSheet = [&]() -> SDL_Texture* {
                if (customSlots.count(static_cast<int>(PlayerAnimSlot::Death)))
                    return knightDeathSheet->GetTexture();
                if (customSlots.count(static_cast<int>(PlayerAnimSlot::Hurt)))
                    return knightHurtSheet->GetTexture();
                if (mHasProfile)
                    return idleT;
                return knightDeathSheet->GetTexture();
            }(),
            .deathFps   = slotFps(PlayerAnimSlot::Death),
        });

    // --- Spawn tiles ---
    totalGoals = 0;
    std::unordered_map<std::string, AnimatedTileDef> animDefCache;
    for (const auto& ts : mLevel.tiles) {
        if (IsAnimatedTile(ts.imagePath)) {
            auto [defIt, inserted] = animDefCache.try_emplace(ts.imagePath);
            if (inserted) {
                if (!LoadAnimatedTileDef(ts.imagePath, defIt->second) ||
                    defIt->second.framePaths.empty()) {
                    std::print("Failed to load animated tile def: {}\n", ts.imagePath);
                    animDefCache.erase(defIt);
                    continue;
                }
            }
            const AnimatedTileDef& def = defIt->second;

            int animW = (ts.w > 0) ? ts.w : 38;
            int animH = (ts.h > 0) ? ts.h : 38;

            // Native-res frames (0,0) — GPU scales at render time. CPU pre-scale destroyed lava detail.
            std::vector<SDL_Texture*> frameTex;
            for (const auto& fp : def.framePaths)
                frameTex.push_back(GetCachedTexture(ren, fp, 0, 0, ts.rotation));
            if (frameTex.empty() || !frameTex[0])
                continue;

            std::vector<SDL_Rect> frameRects;
            frameRects.reserve(frameTex.size());
            for (auto* ft : frameTex) {
                float ftW = 0, ftH = 0;
                if (ft) SDL_GetTextureSize(ft, &ftW, &ftH);
                frameRects.push_back({0, 0, (int)ftW, (int)ftH});
            }
            auto                  tile = reg.create();
            reg.emplace<Transform>(tile, ts.x, ts.y);

            bool hasCustomHitbox = ts.HasHitbox();
            int  colW = hasCustomHitbox ? (ts.hitbox->w > 0 ? ts.hitbox->w : animW) : animW;
            int  colH = hasCustomHitbox ? (ts.hitbox->h > 0 ? ts.hitbox->h : animH) : animH;

            if (ts.ladder)
                reg.emplace<LadderTag>(tile);
            else if (ts.HasSlope()) {
                reg.emplace<TileTag>(tile);
                reg.emplace<SlopeCollider>(tile, ts.slope->type, ts.slope->heightFrac);
            } else if (ts.hazard) {
                reg.emplace<HazardTag>(tile);
                if (!ts.prop)
                    reg.emplace<TileTag>(tile);
            } else if (!ts.prop)
                reg.emplace<TileTag>(tile);
            if (ts.prop) {
                if (ts.propBehind)
                    reg.emplace<PropFrontTag>(tile);
                else
                    reg.emplace<PropTag>(tile);
            }
            if (ts.HasAction())
                reg.emplace<ActionTag>(tile,
                                       ts.action->group,
                                       ts.action->hitsRequired,
                                       ts.action->hitsRequired,
                                       ts.action->destroyAnimPath,
                                       ts.action->cameraShake);
            if (!ts.prop || ts.hazard || ts.goal)
                reg.emplace<Collider>(tile, colW, colH);
            if (hasCustomHitbox)
                reg.emplace<ColliderOffset>(tile, ts.hitbox->offX, ts.hitbox->offY);

            reg.emplace<TileAnimTag>(tile);
            reg.emplace<Renderable>(tile, frameTex[0], std::move(frameRects), false, animW, animH);
            reg.emplace<AnimationState>(tile, 0, (int)frameTex.size(), 0.0f, def.fps, true);
            tileAnimFrameMap[tile] = std::move(frameTex);
            if (ts.goal) {
                reg.emplace<GoalTag>(tile);
                totalGoals++;
            }
            if (ts.antiGravity) {
                reg.emplace<FloatTag>(tile);
                FloatState fs;
                fs.baseY    = ts.y;
                fs.bobAmp   = 4.0f + (rand() % 50) * 0.08f;
                fs.bobSpeed = 1.4f + (rand() % 80) * 0.01f;
                fs.bobPhase = (rand() % 628) * 0.01f;
                reg.emplace<FloatState>(tile, fs);
            }
            if (ts.HasMoving()) {
                const auto& mp = *ts.moving;
                if (!reg.all_of<PrevTransform>(tile))
                    reg.emplace<PrevTransform>(tile, ts.x, ts.y);
                reg.emplace<MovingPlatformTag>(tile);
                MovingPlatformState mps;
                mps.horiz     = mp.horiz;
                mps.range     = mp.range;
                mps.speed     = mp.speed;
                mps.groupId   = mp.groupId;
                mps.originX   = ts.x;
                mps.originY   = ts.y;
                mps.loop      = mp.loop;
                mps.trigger   = mp.trigger;
                mps.triggered = false;
                if (mp.loop) {
                    mps.phase   = mp.phase * mp.range;
                    mps.loopDir = mp.loopDir;
                    if (mp.horiz)
                        reg.get<Transform>(tile).x = ts.x + mps.phase;
                } else {
                    mps.phase   = mp.phase * 6.28318f;
                    mps.loopDir = 1;
                }
                reg.emplace<MovingPlatformState>(tile, mps);
            }
            if (ts.HasPowerUp() && !ts.powerUp->type.empty()) {
                PowerUpType puType = PowerUpType::None;
                if (ts.powerUp->type == "antigravity")
                    puType = PowerUpType::AntiGravity;
                else if (ts.powerUp->type == "turret")
                    puType = PowerUpType::Turret;
                else if (ts.powerUp->type == "healthboost")
                    puType = PowerUpType::HealthBoost;
                else if (ts.powerUp->type == "teleport")
                    puType = PowerUpType::Teleport;
                if (puType != PowerUpType::None) {
                    PowerUpTag pt2;
                    pt2.type          = puType;
                    pt2.duration      = ts.powerUp->duration;
                    pt2.fireRate      = ts.powerUp->fireRate;
                    pt2.healthPct     = ts.powerUp->healthPct;
                    pt2.teleportGroup = ts.powerUp->teleportGroup;
                    pt2.sfxPath       = ts.powerUp->sfxPath;
                    reg.emplace<PowerUpTag>(tile, pt2);
                    if (puType == PowerUpType::Teleport) {
                        if (ts.powerUp->teleportDest)
                            reg.emplace<TeleportDestTag>(tile, ts.powerUp->teleportGroup);
                        else
                            reg.emplace<TeleportEntranceTag>(tile, ts.powerUp->teleportGroup);
                    }
                }
            }
            if (ts.HasShooter()) {
                const auto& sd = *ts.shooter;
                ShooterTag st;
                st.side        = static_cast<int>(sd.side);
                st.range       = sd.range;
                st.fireRate    = sd.fireRate;
                st.bulletSpeed = sd.bulletSpeed;
                st.damage      = sd.damage;
                st.sfxPath     = sd.sfxPath;
                reg.emplace<ShooterTag>(tile, st);
                reg.emplace<ShooterState>(tile);
            }
            if (ts.HasShield()) {
                reg.emplace<ShieldPickupTag>(tile, ts.shield->duration);
            }
            continue;
        }

        // --- Normal PNG tile ---
        SDL_Texture* tex = GetCachedTexture(ren, ts.imagePath, ts.w, ts.h, ts.rotation);
        if (!tex)
            continue;

        auto tile = reg.create();
        reg.emplace<Transform>(tile, ts.x, ts.y);

        bool hasCustomHitbox = ts.HasHitbox();
        int  colW            = hasCustomHitbox ? (ts.hitbox->w > 0 ? ts.hitbox->w : ts.w) : ts.w;
        int  colH            = hasCustomHitbox ? (ts.hitbox->h > 0 ? ts.hitbox->h : ts.h) : ts.h;

        if (ts.ladder) {
            reg.emplace<LadderTag>(tile);
            reg.emplace<Collider>(tile, colW, colH);
        } else if (ts.HasSlope()) {
            reg.emplace<TileTag>(tile);
            reg.emplace<Collider>(tile, colW, colH);
            reg.emplace<SlopeCollider>(tile, ts.slope->type, ts.slope->heightFrac);
        } else if (ts.hazard) {
            reg.emplace<HazardTag>(tile);
            reg.emplace<Collider>(tile, colW, colH);
            if (!ts.prop)
                reg.emplace<TileTag>(tile);
        } else {
            reg.emplace<Collider>(tile, colW, colH);
            if (!ts.prop)
                reg.emplace<TileTag>(tile);
        }
        if (ts.prop) {
            if (ts.propBehind)
                reg.emplace<PropFrontTag>(tile);
            else
                reg.emplace<PropTag>(tile);
        }
        if (ts.HasAction())
            reg.emplace<ActionTag>(
                tile, ts.action->group, ts.action->hitsRequired, ts.action->hitsRequired,
                ts.action->destroyAnimPath, ts.action->cameraShake);

        if (ts.antiGravity) {
            reg.emplace<FloatTag>(tile);
            FloatState fs;
            fs.baseY    = ts.y;
            fs.bobAmp   = 4.0f + (rand() % 50) * 0.08f;
            fs.bobSpeed = 1.4f + (rand() % 80) * 0.01f;
            fs.bobPhase = (rand() % 628) * 0.01f;
            reg.emplace<FloatState>(tile, fs);
        }
        if (ts.HasMoving()) {
            const auto& mp = *ts.moving;
            // Moving platforms need PrevTransform so RenderSystem can
            // interpolate their draw position between physics ticks.
            if (!reg.all_of<PrevTransform>(tile))
                reg.emplace<PrevTransform>(tile, ts.x, ts.y);
            reg.emplace<MovingPlatformTag>(tile);
            MovingPlatformState mps;
            mps.horiz     = mp.horiz;
            mps.range     = mp.range;
            mps.speed     = mp.speed;
            mps.groupId   = mp.groupId;
            mps.originX   = ts.x;
            mps.originY   = ts.y;
            mps.loop      = mp.loop;
            mps.trigger   = mp.trigger;
            mps.triggered = false;
            if (mp.loop) {
                mps.phase   = mp.phase * mp.range;
                mps.loopDir = mp.loopDir;
                if (mp.horiz)
                    reg.get<Transform>(tile).x = ts.x + mps.phase;
            } else {
                mps.phase   = mp.phase * 6.28318f;
                mps.loopDir = 1;
            }
            reg.emplace<MovingPlatformState>(tile, mps);
        }
        if (hasCustomHitbox)
            reg.emplace<ColliderOffset>(tile, ts.hitbox->offX, ts.hitbox->offY);
        if (ts.HasPowerUp() && !ts.powerUp->type.empty()) {
            PowerUpType puType = PowerUpType::None;
            if (ts.powerUp->type == "antigravity")
                puType = PowerUpType::AntiGravity;
            else if (ts.powerUp->type == "turret")
                puType = PowerUpType::Turret;
            else if (ts.powerUp->type == "healthboost")
                puType = PowerUpType::HealthBoost;
            else if (ts.powerUp->type == "teleport")
                puType = PowerUpType::Teleport;
            if (puType != PowerUpType::None) {
                PowerUpTag pt2;
                pt2.type          = puType;
                pt2.duration      = ts.powerUp->duration;
                pt2.fireRate      = ts.powerUp->fireRate;
                pt2.healthPct     = ts.powerUp->healthPct;
                pt2.teleportGroup = ts.powerUp->teleportGroup;
                pt2.sfxPath       = ts.powerUp->sfxPath;
                reg.emplace<PowerUpTag>(tile, pt2);
                if (puType == PowerUpType::Teleport) {
                    if (ts.powerUp->teleportDest)
                        reg.emplace<TeleportDestTag>(tile, ts.powerUp->teleportGroup);
                    else
                        reg.emplace<TeleportEntranceTag>(tile, ts.powerUp->teleportGroup);
                }
            }
        }

        if (ts.goal) {
            reg.emplace<GoalTag>(tile);
            totalGoals++;
        }

        if (ts.HasShooter()) {
            const auto& sd = *ts.shooter;
            ShooterTag st;
            st.side        = static_cast<int>(sd.side);
            st.range       = sd.range;
            st.fireRate    = sd.fireRate;
            st.bulletSpeed = sd.bulletSpeed;
            st.damage      = sd.damage;
            st.sfxPath     = sd.sfxPath;
            reg.emplace<ShooterTag>(tile, st);
            reg.emplace<ShooterState>(tile);
        }

        if (ts.HasShield()) {
            reg.emplace<ShieldPickupTag>(tile, ts.shield->duration);
        }

        float texW = 0, texH = 0;
        SDL_GetTextureSize(tex, &texW, &texH);
        std::vector<SDL_Rect> tileFrame = {{0, 0, (int)texW, (int)texH}};
        reg.emplace<Renderable>(tile, tex, tileFrame, false, ts.w, ts.h);
        reg.emplace<AnimationState>(tile, 0, 1, 0.0f, 1.0f, false);
    }

    // --- Load turret fire SFX ---
    if (Audio() && Audio()->IsReady()) {
        auto sv = reg.view<ShooterTag>();
        sv.each([&](entt::entity e, const ShooterTag& sh) {
            if (!sh.sfxPath.empty()) {
                std::string sfxId = "turret_" + std::to_string((uint32_t)e);
                Audio()->Sfx().Load(sfxId, sh.sfxPath);
            }
        });
        auto puv = reg.view<PowerUpTag>();
        puv.each([&](entt::entity e, const PowerUpTag& pu) {
            if (!pu.sfxPath.empty()) {
                std::string sfxId = "powerup_" + std::to_string((uint32_t)e);
                Audio()->Sfx().Load(sfxId, pu.sfxPath);
            }
        });
    }

    // --- Spawn enemies ---
    // Enemy type cache: share GPU textures across same-type enemies.
    auto toEnemyHitbox = [](const EnemyAnimHitbox& hb) -> EnemyHitbox {
        if (hb.IsDefault()) return {};
        return {hb.w, hb.h, hb.x, hb.y, -hb.x, -hb.y};
    };

    struct EnemyTypeCache {
        SpriteSheet* idleSheet = nullptr;
        std::vector<SDL_Rect> idleFrames;
        SpriteSheet* moveSheet = nullptr;
        std::vector<SDL_Rect> moveFrames;
        float moveFps = 7.0f;
        SpriteSheet* attackSheet = nullptr;
        std::vector<SDL_Rect> attackFrames;
        float attackFps = 10.0f;
        SpriteSheet* hurtSheet = nullptr;
        std::vector<SDL_Rect> hurtFrames;
        float hurtFps = 8.0f;
        SpriteSheet* deadSheet = nullptr;
        std::vector<SDL_Rect> deadFrames;
        float deadFps = 6.0f;
        int spriteW = 40, spriteH = 40;
        float health = 30.0f;
        // Per-animation hitboxes from profile
        EnemyHitbox idleHitbox;
        EnemyHitbox moveHitbox;
        EnemyHitbox attackHitbox;
        EnemyHitbox hurtHitbox;
        EnemyHitbox deadHitbox;
        std::array<EnemyAnimData::SlotSfx, 5> slotSfx;
    };
    std::unordered_map<std::string, std::shared_ptr<EnemyTypeCache>> enemyTypeCache;
    mEnemySpriteSheets.clear();  // clear from previous Spawn (Respawn path)

    auto getEnemyTypeCache = [&](const std::string& typeName) -> std::shared_ptr<EnemyTypeCache> {
        auto it = enemyTypeCache.find(typeName);
        if (it != enemyTypeCache.end()) return it->second;

        EnemyProfile prof;
        if (!LoadEnemyProfile(EnemyProfilePath(typeName), prof))
            return nullptr;

        auto tc = std::make_shared<EnemyTypeCache>();
        tc->spriteW = (prof.spriteW > 0) ? prof.spriteW : 40;
        tc->spriteH = (prof.spriteH > 0) ? prof.spriteH : 40;
        tc->health  = (prof.health > 0)  ? prof.health  : 30.0f;

        auto loadSlot = [&](EnemyAnimSlot slot) -> std::pair<SpriteSheet*, std::vector<SDL_Rect>> {
            if (!prof.HasSlot(slot)) return {nullptr, {}};
            const auto& dir = prof.Slot(slot).folderPath;
            std::vector<fs::path> pngs;
            std::error_code ec;
            if (fs::is_directory(dir, ec) && !ec) {
                for (const auto& e : fs::directory_iterator(dir, ec))
                    if (!ec && (e.path().extension() == ".png" || e.path().extension() == ".PNG"))
                        pngs.push_back(e.path());
            }
            if (pngs.empty()) return {nullptr, {}};
            std::sort(pngs.begin(), pngs.end());
            std::vector<std::string> pathStrs;
            for (const auto& p : pngs) pathStrs.push_back(p.string());
            auto ss = std::make_unique<SpriteSheet>(pathStrs, tc->spriteW, tc->spriteH);
            ss->CreateTexture(ren);
            auto frames = ss->GetAnimation("");
            ss->FreeSurface();
            SpriteSheet* raw = ss.get();
            mEnemySpriteSheets.push_back(std::move(ss));
            return {raw, std::move(frames)};
        };

        auto [idleSS, idleFr]   = loadSlot(EnemyAnimSlot::Idle);
        auto [moveSS, moveFr]   = loadSlot(EnemyAnimSlot::Move);
        auto [attackSS, atkFr]  = loadSlot(EnemyAnimSlot::Attack);
        auto [hurtSS, hurtFr]   = loadSlot(EnemyAnimSlot::Hurt);
        auto [deadSS, deadFr]   = loadSlot(EnemyAnimSlot::Dead);
        tc->idleSheet    = idleSS;
        tc->idleFrames   = std::move(idleFr);
        tc->moveSheet    = moveSS;
        tc->moveFrames   = std::move(moveFr);
        tc->attackSheet  = attackSS;
        tc->attackFrames = std::move(atkFr);
        tc->hurtSheet    = hurtSS;
        tc->hurtFrames   = std::move(hurtFr);
        tc->deadSheet    = deadSS;
        tc->deadFrames   = std::move(deadFr);

        // Read FPS from profile
        if (prof.HasFps(EnemyAnimSlot::Move))   tc->moveFps   = prof.Slot(EnemyAnimSlot::Move).fps;
        if (prof.HasFps(EnemyAnimSlot::Attack)) tc->attackFps = prof.Slot(EnemyAnimSlot::Attack).fps;
        if (prof.HasFps(EnemyAnimSlot::Hurt))   tc->hurtFps   = prof.Slot(EnemyAnimSlot::Hurt).fps;
        if (prof.HasFps(EnemyAnimSlot::Dead))   tc->deadFps   = prof.Slot(EnemyAnimSlot::Dead).fps;

        // Read per-animation hitboxes from profile
        tc->idleHitbox   = toEnemyHitbox(prof.Slot(EnemyAnimSlot::Idle).hitbox);
        tc->moveHitbox   = toEnemyHitbox(prof.Slot(EnemyAnimSlot::Move).hitbox);
        tc->attackHitbox = toEnemyHitbox(prof.Slot(EnemyAnimSlot::Attack).hitbox);
        tc->hurtHitbox   = toEnemyHitbox(prof.Slot(EnemyAnimSlot::Hurt).hitbox);
        tc->deadHitbox   = toEnemyHitbox(prof.Slot(EnemyAnimSlot::Dead).hitbox);

        // Load per-slot SFX for this enemy type
        if (Audio() && Audio()->IsReady()) {
            for (int i = 0; i < ENEMY_ANIM_SLOT_COUNT; ++i) {
                const auto& slot = prof.slots[i];
                for (int fi = 0; fi < (int)slot.sfx.size(); ++fi) {
                    std::string sfxId = audio::EnemySfxId(typeName, i, fi);
                    if (!sfxId.empty())
                        Audio()->Sfx().Load(sfxId, slot.sfx[fi].path);
                    tc->slotSfx[i].files.push_back({slot.sfx[fi].volume, slot.sfx[fi].timeStretch, slot.sfx[fi].trimStart, slot.sfx[fi].trimEnd});
                }
            }
        }

        if (tc->moveFrames.empty() && !tc->idleFrames.empty()) {
            tc->moveFrames = tc->idleFrames;
        }
        if (tc->hurtFrames.empty() && !tc->idleFrames.empty()) {
            tc->hurtFrames = tc->idleFrames;
            tc->hurtSheet  = tc->idleSheet;
        }
        if (tc->deadFrames.empty()) {
            if (!tc->hurtFrames.empty()) {
                tc->deadFrames = tc->hurtFrames;
                tc->deadSheet  = tc->hurtSheet;
            } else if (!tc->idleFrames.empty()) {
                tc->deadFrames = tc->idleFrames;
                tc->deadSheet  = tc->idleSheet;
            }
        }

        enemyTypeCache[typeName] = tc;
        return tc;
    };

    for (const auto& es : mLevel.enemies) {
        float speed = es.speed;
        float dx    = es.startLeft ? -speed : speed;
        auto  enemy = reg.create();
        reg.emplace<Transform>(enemy, es.x, es.y);
        reg.emplace<PrevTransform>(enemy, es.x, es.y);
        reg.emplace<Velocity>(enemy, dx, 0.0f, speed);
        reg.emplace<EnemyTag>(enemy);
        reg.emplace<EnemyReaction>(enemy);

        bool usedProfile = false;
        if (!es.enemyType.empty()) {
            auto tc = getEnemyTypeCache(es.enemyType);
            if (tc && (!tc->moveFrames.empty() || !tc->idleFrames.empty())) {
                const auto& frames = !tc->moveFrames.empty() ? tc->moveFrames : tc->idleFrames;
                SDL_Texture* tex = tc->moveSheet ? tc->moveSheet->GetTexture()
                                 : tc->idleSheet ? tc->idleSheet->GetTexture()
                                 : nullptr;
                if (tex) {
                    float fps = 7.0f; // TODO: use profile Move slot fps
                    reg.emplace<AnimationState>(enemy, 0, (int)frames.size(), 0.0f, fps, true);
                    reg.emplace<Renderable>(enemy, tex, frames, false,
                                            tc->spriteW, tc->spriteH);
                    reg.emplace<Collider>(enemy, tc->spriteW, tc->spriteH);
                    Health eh;
                    eh.current = tc->health;
                    eh.max     = tc->health;
                    reg.emplace<Health>(enemy, eh);
                    reg.emplace<FaceRightTag>(enemy);

                    EnemyAnimData ead;
                    ead.moveSheet    = tex;
                    ead.moveFrames   = frames;
                    ead.moveFps      = fps;
                    ead.moveHitbox   = tc->moveHitbox;
                    ead.idleSheet    = tc->idleSheet ? tc->idleSheet->GetTexture() : nullptr;
                    ead.idleHitbox   = tc->idleHitbox;
                    ead.attackSheet  = tc->attackSheet ? tc->attackSheet->GetTexture() : nullptr;
                    ead.attackFrames = tc->attackFrames;
                    ead.attackFps    = tc->attackFps;
                    ead.attackHitbox = tc->attackHitbox;
                    ead.hurtSheet    = tc->hurtSheet ? tc->hurtSheet->GetTexture() : tex;
                    ead.hurtFrames   = tc->hurtFrames.empty() ? frames : tc->hurtFrames;
                    ead.hurtFps      = tc->hurtFps;
                    ead.hurtHitbox   = tc->hurtHitbox;
                    ead.deadSheet    = tc->deadSheet ? tc->deadSheet->GetTexture() : tex;
                    ead.deadFrames   = tc->deadFrames.empty() ? frames : tc->deadFrames;
                    ead.deadFps      = tc->deadFps;
                    ead.deadHitbox   = tc->deadHitbox;
                    ead.typeName     = es.enemyType;
                    ead.slotSfx      = tc->slotSfx;
                    ead.spriteW      = tc->spriteW;
                    ead.spriteH      = tc->spriteH;
                    reg.emplace<EnemyAnimData>(enemy, std::move(ead));
                    reg.emplace<EnemyAttackState>(enemy);
                    reg.emplace<EnemyClimbState>(enemy);

                    // Apply initial hitbox from profile (move > idle > full sprite).
                    const EnemyHitbox& initHB = !tc->moveHitbox.IsDefault() ? tc->moveHitbox
                                              : !tc->idleHitbox.IsDefault() ? tc->idleHitbox
                                              : EnemyHitbox{};
                    if (!initHB.IsDefault()) {
                        auto& eadRef = reg.get<EnemyAnimData>(enemy);
                        eadRef.ApplyHitbox(initHB, reg, enemy);
                    }

                    usedProfile = true;
                }
            }
        }

        // Fallback: generic slime
        if (!usedProfile) {
            reg.emplace<AnimationState>(enemy, 0, (int)enemyWalkFrames.size(), 0.0f, 7.0f, true);
            reg.emplace<Renderable>(enemy, enemySheet->GetTexture(), enemyWalkFrames, false);
            reg.emplace<Collider>(enemy, SLIME_SPRITE_WIDTH, SLIME_SPRITE_HEIGHT);
            Health eh;
            eh.current = SLIME_MAX_HEALTH;
            eh.max     = SLIME_MAX_HEALTH;
            reg.emplace<Health>(enemy, eh);
            reg.emplace<EnemyClimbState>(enemy);
        }

        if (es.antiGravity) {
            reg.emplace<FloatTag>(enemy);
            FloatState fs;
            fs.baseY    = es.y;
            fs.bobAmp   = 5.0f + (rand() % 40) * 0.1f;
            fs.bobSpeed = 1.6f + (rand() % 60) * 0.01f;
            fs.bobPhase = (rand() % 628) * 0.01f;
            reg.emplace<FloatState>(enemy, fs);
        }
    }

    // Create bullet texture (8x8 dark square) for turret projectiles
    if (!mBulletTex) {
        SDL_Surface* bs = SDL_CreateSurface(8, 8, SDL_PIXELFORMAT_ARGB8888);
        if (bs) {
            SDL_FillSurfaceRect(bs, nullptr,
                SDL_MapRGBA(SDL_GetPixelFormatDetails(bs->format), nullptr, 10, 10, 10, 255));
            mBulletTex = SDL_CreateTextureFromSurface(ren, bs);
            SDL_DestroySurface(bs);
        }
    }

    // Build the pre-sorted tile render lists used by RenderSystem.
    {
        mSortedTileRenderList.clear();
        auto tv = reg.view<TileTag, AnimationState, Renderable>();
        auto lv = reg.view<LadderTag, AnimationState, Renderable>();
        auto pv = reg.view<PropTag, AnimationState, Renderable>();
        mSortedTileRenderList.reserve(tv.size_hint() + lv.size_hint() + pv.size_hint());
        for (auto e : tv) mSortedTileRenderList.push_back(e);
        for (auto e : lv) mSortedTileRenderList.push_back(e);
        for (auto e : pv) mSortedTileRenderList.push_back(e);
        std::sort(mSortedTileRenderList.begin(), mSortedTileRenderList.end());

        mSortedFrontPropList.clear();
        auto fpv = reg.view<PropFrontTag, AnimationState, Renderable>();
        mSortedFrontPropList.reserve(fpv.size_hint());
        for (auto e : fpv) mSortedFrontPropList.push_back(e);
        std::sort(mSortedFrontPropList.begin(), mSortedFrontPropList.end());
    }
}

void GameScene::Respawn() {
    reg.clear();
    tileAnimFrameMap.clear();
    mSortedTileRenderList.clear();
    // sTileScaledTextures / sTileTextureCache are static and persist across scene
    // instances — textures stay on the GPU for instant reuse.
    gameOver           = false;
    mPlayerDying       = false;
    mDeathAnimTimer    = 0.0f;
    levelComplete      = false;
    levelCompleteTimer = 2.0f;
    goalsCollected    = 0;
    stompCount         = 0;
    mCamera            = Camera{};
    Spawn();
}
