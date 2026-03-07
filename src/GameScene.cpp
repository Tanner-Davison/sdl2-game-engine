#include "GameScene.hpp"
#include "AnimatedTile.hpp"
#include "GameConfig.hpp"
#include "GameEvents.hpp"
#include "LevelEditorScene.hpp"
#include "LevelTwo.hpp"
#include "TitleScene.hpp"
#include "SurfaceUtils.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <unordered_map>
namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Level-scoped tile texture cache
//
// Key: "<path>|<w>x<h>|r<rotation>"  Value: non-owning ptr into tileScaledTextures
// Populated during Spawn(); cleared in Unload() along with tileScaledTextures.
// This avoids re-loading and re-uploading tile images on every Respawn().
// ─────────────────────────────────────────────────────────────────────────────
static std::string TileCacheKey(const std::string& path, int w, int h, int rot) {
    return path + '|' + std::to_string(w) + 'x' + std::to_string(h)
                + "|r" + std::to_string(rot);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: load a surface, scale it, convert to texture, free the surface.
// Returns nullptr on failure. Caller owns the texture.
// ─────────────────────────────────────────────────────────────────────────────
static SDL_Texture* LoadScaledTexture(SDL_Renderer* ren,
                                       const std::string& path,
                                       int tw, int th, int rotation = 0) {
    SDL_Surface* raw = IMG_Load(path.c_str());
    if (!raw) { std::print("Failed to load tile: {}\n", path); return nullptr; }

    SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(raw);
    if (!conv) return nullptr;

    SDL_Surface* scaled = SDL_CreateSurface(tw, th, SDL_PIXELFORMAT_ARGB8888);
    if (!scaled) { SDL_DestroySurface(conv); return nullptr; }
    SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_NONE);
    SDL_BlitSurfaceScaled(conv, nullptr, scaled, nullptr, SDL_SCALEMODE_LINEAR);
    SDL_DestroySurface(conv);
    SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);

    if (rotation != 0) {
        SDL_Surface* rot = RotateSurfaceDeg(scaled, rotation);
        if (rot) { SDL_DestroySurface(scaled); scaled = rot; }
    }

    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, scaled);
    SDL_DestroySurface(scaled);
    return tex;
}

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
    SDL_Renderer* ren = window.GetRenderer();

    if (!mLevelPath.empty())
        LoadLevel(mLevelPath, mLevel);

    PlayerProfile profile;
    bool useProfile = !mProfilePath.empty() && LoadPlayerProfile(mProfilePath, profile);
    mHasProfile = useProfile;

    const int KW = (useProfile && profile.spriteW > 0) ? profile.spriteW : PLAYER_SPRITE_WIDTH;
    const int KH = (useProfile && profile.spriteH > 0) ? profile.spriteH : PLAYER_SPRITE_HEIGHT;
    mPlayerSpriteW = KW;
    mPlayerSpriteH = KH;

    auto loadSlot = [&](PlayerAnimSlot slot,
                        const std::string& fallbackFolder,
                        const std::string& fallbackPrefix,
                        int fallbackCount) -> std::unique_ptr<SpriteSheet> {
        if (useProfile && profile.HasSlot(slot)) {
            const std::string& dir = profile.Slot(slot).folderPath;
            std::vector<fs::path> pngs;
            for (const auto& e : fs::directory_iterator(dir))
                if (e.path().extension() == ".png") pngs.push_back(e.path());
            if (!pngs.empty()) {
                std::sort(pngs.begin(), pngs.end());
                // Derive pad width and start index from the first file alphabetically,
                // but load ALL PNGs in the folder regardless of filename prefix.
                // This intentionally supports reuse: you can point the Walk slot and
                // the Run slot at the same folder, then use fps to differentiate speed.
                // Mixed-prefix folders (e.g. Run_000..005 + Walk_000..007) will play
                // all frames in alphabetical order — keep folders single-purpose if
                // you want clean animation boundaries.
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
        // No custom sprites for this slot — fall back to frost knight at its
        // native dimensions. We deliberately ignore KW/KH here: those are the
        // custom character's sprite size and must NOT be applied to the frost
        // knight sheets (doing so squashes/stretches the wrong character).
        return std::make_unique<SpriteSheet>(
            "game_assets/frost_knight_png_sequences/" + fallbackFolder + "/",
            fallbackPrefix, fallbackCount, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT, 3);
    };

    mSlotFps.fill(0.0f);
    if (useProfile) {
        // Apply FPS overrides for every slot, regardless of whether that slot
        // has custom sprites. A hitbox-only profile (e.g. "bones") may still
        // define custom FPS values that should take effect with the fallback visuals.
        for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
            auto slot = static_cast<PlayerAnimSlot>(i);
            if (profile.HasFps(slot))
                mSlotFps[i] = profile.Slot(slot).fps;
        }
    }

    knightIdleSheet  = loadSlot(PlayerAnimSlot::Idle,  "Idle",         "0_Knight_Idle_",        18);
    knightWalkSheet  = loadSlot(PlayerAnimSlot::Walk,  "Walking",      "0_Knight_Walking_",     24);
    knightHurtSheet  = loadSlot(PlayerAnimSlot::Hurt,  "Hurt",         "0_Knight_Hurt_",        12);
    knightJumpSheet  = loadSlot(PlayerAnimSlot::Jump,  "Jump Start",   "0_Knight_Jump Start_",   6);
    knightFallSheet  = loadSlot(PlayerAnimSlot::Fall,  "Falling Down", "0_Knight_Falling Down_", 6);
    knightSlideSheet = loadSlot(PlayerAnimSlot::Crouch, "Sliding",      "0_Knight_Sliding_",      6);
    knightSlashSheet = loadSlot(PlayerAnimSlot::Slash, "Slashing",     "0_Knight_Slashing_",    12);

    // Upload all sprite sheets to GPU then free the CPU surfaces — GameScene
    // only needs the GPU textures at runtime. PlayerCreatorScene skips FreeSurface()
    // so its preview blits can still read from the CPU surface.
    knightIdleSheet->CreateTexture(ren);  knightIdleSheet->FreeSurface();
    knightWalkSheet->CreateTexture(ren);  knightWalkSheet->FreeSurface();
    knightHurtSheet->CreateTexture(ren);  knightHurtSheet->FreeSurface();
    knightJumpSheet->CreateTexture(ren);  knightJumpSheet->FreeSurface();
    knightFallSheet->CreateTexture(ren);  knightFallSheet->FreeSurface();
    knightSlideSheet->CreateTexture(ren); knightSlideSheet->FreeSurface();
    knightSlashSheet->CreateTexture(ren); knightSlashSheet->FreeSurface();

    auto getFrames = [&](std::unique_ptr<SpriteSheet>& sheet,
                         PlayerAnimSlot slot,
                         const std::string& knightKey) -> std::vector<SDL_Rect> {
        // Only use the "get all frames" path when the slot actually has custom
        // sprites loaded. If the profile has a hitbox/fps override but no folder
        // (e.g. "bones"), the sheet is still the frost knight fallback and we
        // must use the named key — GetAnimation("") would match every frame.
        if (useProfile && profile.HasSlot(slot)) {
            auto all = sheet->GetAnimation("");
            // Never fall through to the knight key on a custom sheet — the custom
            // SpriteSheet has no knight-named frames so it would silently return
            // empty, causing the engine to flash the frost-knight visuals.
            // Return what we got (may be empty if load failed, handled downstream).
            return all;
        }
        return sheet->GetAnimation(knightKey);
    };

    idleFrames  = getFrames(knightIdleSheet,  PlayerAnimSlot::Idle,  "0_Knight_Idle_");
    walkFrames  = getFrames(knightWalkSheet,  PlayerAnimSlot::Walk,  "0_Knight_Walking_");
    jumpFrames  = getFrames(knightJumpSheet,  PlayerAnimSlot::Jump,  "0_Knight_Jump Start_");
    hurtFrames  = getFrames(knightHurtSheet,  PlayerAnimSlot::Hurt,  "0_Knight_Hurt_");
    duckFrames  = getFrames(knightSlideSheet, PlayerAnimSlot::Crouch, "0_Knight_Sliding_");
    frontFrames = getFrames(knightFallSheet,  PlayerAnimSlot::Fall,  "0_Knight_Falling Down_");
    slashFrames = getFrames(knightSlashSheet, PlayerAnimSlot::Slash, "0_Knight_Slashing_");

    // When a custom profile is active, redirect any unfilled slot (empty frames)
    // to idle frames so the character holds its idle pose instead of flashing
    // to the frost-knight sprite set whenever an unassigned action triggers.
    // The sheet texture is fixed up in resolveSheet() below when building AnimationSet.
    if (useProfile && !idleFrames.empty()) {
        if (!profile.HasSlot(PlayerAnimSlot::Walk)   && walkFrames.empty())  walkFrames  = idleFrames;
        if (!profile.HasSlot(PlayerAnimSlot::Jump)   && jumpFrames.empty())  jumpFrames  = idleFrames;
        if (!profile.HasSlot(PlayerAnimSlot::Hurt)   && hurtFrames.empty())  hurtFrames  = idleFrames;
        if (!profile.HasSlot(PlayerAnimSlot::Crouch) && duckFrames.empty())  duckFrames  = idleFrames;
        if (!profile.HasSlot(PlayerAnimSlot::Fall)   && frontFrames.empty()) frontFrames = idleFrames;
        if (!profile.HasSlot(PlayerAnimSlot::Slash)  && slashFrames.empty()) slashFrames = idleFrames;
    }

    enemySheet = std::make_unique<SpriteSheet>(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");
    enemySheet->CreateTexture(ren);
    enemyWalkFrames = enemySheet->GetAnimation("slimeWalk");

    std::string bgPath = (!mLevelPath.empty() && !mLevel.background.empty())
                           ? mLevel.background
                           : "game_assets/backgrounds/deepspace_scene.png";
    background = std::make_unique<Image>(bgPath, FitModeFromString(mLevel.bgFitMode));
    locationText = std::make_unique<Text>("You are in space!!", 20, 20);
    actionText   = std::make_unique<Text>(
        "Level 1: Collect ALL the coins!", SDL_Color{255, 255, 255, 0}, 20, 80, 20);

    gameOverText = std::make_unique<Text>("Game Over!",
                                          SDL_Color{255, 0, 0, 255},
                                          window.GetWidth() / 2 - 100,
                                          window.GetHeight() / 2 - 60, 64);
    retryBtnText = std::make_unique<Text>("Retry",
                                          SDL_Color{0, 0, 0, 255},
                                          window.GetWidth() / 2 - 28,
                                          window.GetHeight() / 2 + 22, 32);
    retryKeyText = std::make_unique<Text>("Press R to Retry",
                                          SDL_Color{200, 200, 200, 255},
                                          window.GetWidth() / 2 - 100,
                                          window.GetHeight() / 2 + 110, 24);

    retryBtnRect = {window.GetWidth() / 2 - 75, window.GetHeight() / 2 + 10, 150, 55};
    retryButton  = std::make_unique<Rectangle>(retryBtnRect);
    retryButton->SetColor({255, 255, 255, 255});
    retryButton->SetHoverColor({180, 180, 180, 255});

    levelCompleteText = std::make_unique<Text>("Level Complete!",
                                               SDL_Color{255, 215, 0, 255},
                                               window.GetWidth() / 2 - 160,
                                               window.GetHeight() / 2 - 40, 64);
    Spawn();
}

void GameScene::Unload() {
    reg.clear();
    for (auto* t : tileScaledTextures) SDL_DestroyTexture(t);
    tileScaledTextures.clear();
    tileTextureCache.clear();  // non-owning refs — textures already freed above
    tileAnimFrameMap.clear();
    mSortedTileRenderList.clear();
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
        // Always return to the editor when launched via Play button,
        // regardless of whether there's a "next level" to go to.
        if (mFromEditor)
            return std::make_unique<LevelEditorScene>(mLevelPath, false, "", mProfilePath);
        return std::make_unique<LevelTwo>();
    }
    return nullptr;
}

bool GameScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) return false;

    if (mPaused) {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
            mPaused = false;
            return true;
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (mx >= mPauseResumeRect.x && mx < mPauseResumeRect.x + mPauseResumeRect.w &&
                my >= mPauseResumeRect.y && my < mPauseResumeRect.y + mPauseResumeRect.h) {
                mPaused = false; return true;
            }
            if (mx >= mPauseBackRect.x && mx < mPauseBackRect.x + mPauseBackRect.w &&
                my >= mPauseBackRect.y && my < mPauseBackRect.y + mPauseBackRect.h) {
                mGoBackFromPause = true; return true;
            }
        }
        if (mPauseResumeBtn) mPauseResumeBtn->HandleEvent(e);
        if (mPauseBackBtn)   mPauseBackBtn->HandleEvent(e);
        return true;
    }

    if (!gameOver) {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F1) {
            mDebugHitboxes = !mDebugHitboxes; return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_F11) {
            mWindow->ToggleFullscreen(); return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE && !levelComplete) {
            mPaused = true;
            if (mWindow) BuildPauseUI(mWindow->GetWidth(), mWindow->GetHeight());
            return true;
        }
        InputSystem(reg, e);
    } else {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_R) Respawn();
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
    if (mPaused) return;
    if (levelComplete) { levelCompleteTimer -= dt; return; }
    if (gameOver) return;

    MovingPlatformTick(reg, dt);
    FloatingResult floatResult = FloatingSystem(reg, dt);
    LadderSystem(reg, dt);
    PlayerStateSystem(reg);
    MovementSystem(reg, dt, mWindow->GetWidth());
    BoundsSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight(),
                 mLevel.gravityMode == GravityMode::WallRun,
                 mLevelW, mLevelH);
    AnimationSystem(reg, dt);

    // Advance animated tile frames
    for (auto& [ent, frames] : tileAnimFrameMap) {
        if (!reg.valid(ent) || frames.empty()) continue;
        auto* anim = reg.try_get<AnimationState>(ent);
        auto* rend = reg.try_get<Renderable>(ent);
        if (!anim || !rend) continue;
        float dur = (anim->fps > 0.0f) ? 1.0f / anim->fps : 0.125f;
        anim->timer += dt;
        while (anim->timer >= dur) {
            anim->timer -= dur;
            if (anim->looping) {
                anim->currentFrame = (anim->currentFrame + 1) % (int)frames.size();
            } else {
                if (anim->currentFrame < (int)frames.size() - 1) anim->currentFrame++;
                else anim->timer = 0.0f;
            }
        }
        SDL_Texture* cur = frames[anim->currentFrame];
        if (cur) rend->sheet = cur;
    }

    // Destroy action tiles whose death animation finished
    {
        std::vector<entt::entity> toDestroy;
        auto destroyView = reg.view<DestroyAnimTag, AnimationState>();
        destroyView.each([&](entt::entity e, DestroyAnimTag& dat, const AnimationState& anim) {
            if (!anim.looping && anim.currentFrame >= anim.totalFrames - 1) {
                if (dat.reachedEnd) toDestroy.push_back(e);
                else dat.reachedEnd = true;
            }
        });
        for (entt::entity e : toDestroy) {
            tileAnimFrameMap.erase(e);
            if (reg.valid(e)) {
                if (reg.all_of<Renderable>(e)) reg.remove<Renderable>(e);
                reg.destroy(e);
            }
        }
    }

    // Tick HitFlash timers
    {
        auto flashView = reg.view<HitFlash>();
        std::vector<entt::entity> expired;
        flashView.each([&](entt::entity e, HitFlash& hf) {
            hf.timer -= dt;
            if (hf.timer <= 0.0f) expired.push_back(e);
        });
        for (auto e : expired) reg.remove<HitFlash>(e);
    }

    CollisionResult collision = CollisionSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight());
    for (auto e : floatResult.actionTilesTriggered)
        collision.actionTilesTriggered.push_back(e);
    MovingPlatformCarry(reg);

    // ── Power-up pickup detection ──────────────────────────────────────────
    // AABB overlap test: player vs any tile with PowerUpTag.
    // On overlap, apply the power-up to the player and destroy the tile.
    {
        entt::entity playerEnt = entt::null;
        SDL_Rect     playerRect{};
        {
            auto pv = reg.view<PlayerTag, Transform, Collider>();
            pv.each([&](entt::entity e, const Transform& t, const Collider& c) {
                playerEnt = e;
                playerRect = {(int)t.x, (int)t.y, c.w, c.h};
            });
        }
        if (playerEnt != entt::null) {
            std::vector<entt::entity> toConsume;
            auto puv = reg.view<PowerUpTag, Transform, Collider>();
            puv.each([&](entt::entity e, const PowerUpTag& pu, const Transform& t, const Collider& c) {
                SDL_Rect pr = {(int)t.x, (int)t.y, c.w, c.h};
                bool overlap = (playerRect.x < pr.x + pr.w && playerRect.x + playerRect.w > pr.x &&
                                playerRect.y < pr.y + pr.h && playerRect.y + playerRect.h > pr.y);
                if (overlap) toConsume.push_back(e);
            });
            for (entt::entity e : toConsume) {
                if (!reg.valid(e)) continue;
                const PowerUpTag& pu = reg.get<PowerUpTag>(e);
                // Apply or refresh the power-up on the player
                if (reg.all_of<ActivePowerUp>(playerEnt)) {
                    auto& active = reg.get<ActivePowerUp>(playerEnt);
                    // Refresh/stack: take whichever duration is longer
                    if (active.type == pu.type) {
                        active.remaining = std::max(active.remaining, pu.duration);
                        active.duration  = active.remaining;
                    } else {
                        active.type      = pu.type;
                        active.remaining = pu.duration;
                        active.duration  = pu.duration;
                    }
                } else {
                    reg.emplace<ActivePowerUp>(playerEnt, pu.type, pu.duration, pu.duration);
                }
                // Consume the tile — remove from render list, destroy entity
                auto it2 = std::find(mSortedTileRenderList.begin(), mSortedTileRenderList.end(), e);
                if (it2 != mSortedTileRenderList.end()) mSortedTileRenderList.erase(it2);
                reg.destroy(e);
            }
        }
    }

    // ── Active power-up tick ────────────────────────────────────────────────
    // Counts down remaining time; removes component when expired.
    // Also applies per-frame effects (e.g. zero gravity).
    {
        auto apv = reg.view<PlayerTag, ActivePowerUp, GravityState>();
        apv.each([&](entt::entity e, ActivePowerUp& ap, GravityState& g) {
            ap.remaining -= dt;
            if (ap.remaining <= 0.0f) {
                // Power-up expired: restore gravity
                if (ap.type == PowerUpType::AntiGravity) {
                    g.active    = true;
                    g.velocity  = 0.0f;
                }
                reg.remove<ActivePowerUp>(e);
                return;
            }
            // Apply per-frame effect
            switch (ap.type) {
                case PowerUpType::AntiGravity:
                    // Suspend gravity: freeze Y velocity accumulation
                    g.active   = false; // MovementSystem free-float path handles movement
                    g.velocity = 0.0f;  // zero out any accumulated fall speed
                    break;
                default:
                    break;
            }
        });
    }

    coinCount  += collision.coinsCollected;
    stompCount += collision.enemiesStomped + collision.enemiesSlashed;
    if (collision.playerDied) gameOver = true;

    // Process triggered action tiles
    SDL_Renderer* ren = mWindow->GetRenderer();
    for (entt::entity e : collision.actionTilesTriggered) {
        if (!reg.valid(e)) continue;
        if (reg.all_of<TileTag>(e))  reg.remove<TileTag>(e);
        if (reg.all_of<Collider>(e)) reg.remove<Collider>(e);

        const ActionTag* atag = reg.try_get<ActionTag>(e);
        if (atag && !atag->destroyAnimPath.empty()) {
            AnimatedTileDef def;
            std::print("[DestroyAnim] tile triggered, path='{}' ", atag->destroyAnimPath);
            if (LoadAnimatedTileDef(atag->destroyAnimPath, def) && !def.framePaths.empty()) {
                std::print("loaded {} frames at {}fps\n", def.framePaths.size(), def.fps);
                int tw = 0, th = 0;
                if (reg.all_of<Renderable>(e)) {
                    const auto& rend = reg.get<Renderable>(e);
                    if (!rend.frames.empty()) { tw = rend.frames[0].w; th = rend.frames[0].h; }
                }
                if (tw <= 0 || th <= 0) { tw = 38; th = 38; }

                std::vector<SDL_Texture*> frameTex;
                for (const auto& fp : def.framePaths) {
                    SDL_Texture* t = LoadScaledTexture(ren, fp, tw, th);
                    frameTex.push_back(t);
                    if (t) tileScaledTextures.push_back(t);
                }

                if (!frameTex.empty() && frameTex[0]) {
                    tileAnimFrameMap[e] = std::move(frameTex);
                    auto& fvec = tileAnimFrameMap[e];
                    std::vector<SDL_Rect> animRects((int)fvec.size(), SDL_Rect{0, 0, tw, th});
                    if (reg.all_of<Renderable>(e)) reg.remove<Renderable>(e);
                    reg.emplace<Renderable>(e, fvec[0], std::move(animRects), false);
                    if (reg.all_of<AnimationState>(e)) reg.remove<AnimationState>(e);
                    reg.emplace<AnimationState>(e, 0, (int)fvec.size(), 0.0f, def.fps, false);
                    if (!reg.all_of<TileAnimTag>(e)) reg.emplace<TileAnimTag>(e);
                    if (!reg.all_of<DestroyAnimTag>(e))
                        reg.emplace<DestroyAnimTag>(e, (int)fvec.size(), def.fps, false);
                    continue;
                } else {
                    std::print("[DestroyAnim] frame texture load failed\n");
                    for (auto* t : frameTex) if (t) SDL_DestroyTexture(t);
                }
            } else {
                std::print("def load failed or empty\n");
            }
        }
        if (reg.all_of<Renderable>(e)) reg.remove<Renderable>(e);
    }

    // Hazard damage
    {
        auto hView = reg.view<PlayerTag, Health, HazardState, AnimationState,
                              Renderable, AnimationSet>();
        hView.each([&](entt::entity playerEnt, Health& hp, HazardState& hz,
                       AnimationState& anim, Renderable& r, const AnimationSet& set) {
            hz.active = collision.onHazard;
            if (hz.active) {
                hp.current -= HAZARD_DAMAGE_PER_SEC * dt;
                if (hp.current <= 0.0f) { hp.current = 0.0f; gameOver = true; }
                hz.flashTimer += dt;
                // Attack always takes priority — never stomp it while in lava.
                bool isAttacking = false;
                if (auto* atk = reg.try_get<AttackState>(playerEnt))
                    isAttacking = atk->isAttacking;
                if (!isAttacking && !set.hurt.empty()) {
                    // Restart hurt from frame 0 whenever:
                    //   - we just entered lava (currentAnim != HURT), OR
                    //   - the previous hurt cycle finished (last frame reached)
                    bool justEntered  = (anim.currentAnim != AnimationID::HURT);
                    bool cycleFinished = (anim.currentAnim == AnimationID::HURT
                                          && !anim.looping
                                          && anim.currentFrame >= anim.totalFrames - 1);
                    if (justEntered || cycleFinished) {
                        r.sheet = set.hurtSheet; r.frames = set.hurt;
                        anim.currentFrame = 0; anim.timer = 0.0f;
                        anim.fps = (set.hurtFps > 0.0f) ? set.hurtFps : 12.0f;
                        anim.looping = false;  // play once; re-triggers next frame if still in lava
                        anim.totalFrames = (int)set.hurt.size();
                        anim.currentAnim = AnimationID::HURT;
                    }
                }
            } else {
                hz.flashTimer = 0.0f;
                if (anim.currentAnim == AnimationID::HURT)
                    anim.currentAnim = AnimationID::NONE;
            }
        });
    }

    if (mLevel.gravityMode != GravityMode::OpenWorld) {
        auto jumpView = reg.view<PlayerTag, GravityState, AnimationSet>();
        jumpView.each([](GravityState& g, const AnimationSet& set) {
            // Respect slot capability: no jump frames = jumping disabled.
            if (g.active && g.jumpHeld && g.isGrounded && !set.jump.empty()) {
                g.velocity = -JUMP_FORCE; g.isGrounded = false; g.jumpHeld = false;
            } else if (set.jump.empty()) {
                g.jumpHeld = false; // drain the held flag so it doesn't queue
            }
        });
    }

    if (totalCoins > 0 && coinCount >= totalCoins) levelComplete = true;

    {
        auto pView = reg.view<PlayerTag, Transform, Collider>();
        pView.each([&](const Transform& pt, const Collider& pc) {
            float cx = pt.x + pc.w * 0.5f, cy = pt.y + pc.h * 0.5f;
            mCamera.Update(cx, cy, mWindow->GetWidth(), mWindow->GetHeight(), mLevelW, mLevelH, dt);
        });
    }
}

void GameScene::Render(Window& window) {
    SDL_Renderer* ren = window.GetRenderer();
    window.Render(); // clear
    if (background->GetFitMode() == FitMode::SCROLL)
        background->RenderScrolling(ren, mCamera.x, (float)mLevelW);
    else
        background->Render(ren);

    const int W = window.GetWidth();
    const int H = window.GetHeight();
    if (levelComplete) {
        RenderSystem(reg, ren, mCamera.x, mCamera.y, W, H, &mSortedTileRenderList);
        HUDSystem(reg, ren, W,
                  healthText.get(), gravityText.get(),
                  coinText.get(), coinCount, stompText.get(), stompCount);
        if (levelCompleteText) levelCompleteText->Render(ren);
    } else if (gameOver) {
        gameOverText->Render(ren);
        retryButton->Render(ren);
        retryBtnText->Render(ren);
        retryKeyText->Render(ren);
    } else {
        locationText->Render(ren);
        actionText->Render(ren);
        RenderSystem(reg, ren, mCamera.x, mCamera.y, W, H, &mSortedTileRenderList);

        // ── Debug hitbox overlay (F1) ─────────────────────────────────────
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
                auto pv = reg.view<PlayerTag, Transform, Collider>();
                pv.each([&](const Transform& t, const Collider& c) {
                    int sx = (int)(t.x - mCamera.x), sy = (int)(t.y - mCamera.y);
                    SDL_Rect r = {sx, sy, c.w, c.h};
                    fill(r, 0, 255, 255, 50); outline(r, 0, 255, 255);
                    Text lbl(std::to_string(c.w) + "x" + std::to_string(c.h),
                             SDL_Color{0,255,255,255}, sx+2, sy+2, 10);
                    lbl.Render(ren);
                });
            }
            {
                auto tv = reg.view<TileTag, Transform, Collider>();
                tv.each([&](entt::entity te, const Transform& t, const Collider& c) {
                    float tx = t.x, ty = t.y;
                    if (const auto* off = reg.try_get<ColliderOffset>(te)) { tx += off->x; ty += off->y; }
                    SDL_Rect r = {(int)(tx-mCamera.x), (int)(ty-mCamera.y), c.w, c.h};
                    fill(r, 255,255,255,18); outline(r, 160,160,255);
                });
            }
            {
                auto hv = reg.view<HazardTag, Transform, Collider>();
                hv.each([&](entt::entity he, const Transform& t, const Collider& c) {
                    float hx = t.x, hy = t.y;
                    if (const auto* off = reg.try_get<ColliderOffset>(he)) { hx += off->x; hy += off->y; }
                    SDL_Rect r = {(int)(hx-mCamera.x), (int)(hy-mCamera.y), c.w, c.h};
                    fill(r,255,40,40,60); outline(r,255,40,40);
                });
            }
            {
                auto lv = reg.view<LadderTag, Transform, Collider>();
                lv.each([&](const Transform& t, const Collider& c) {
                    SDL_Rect r = {(int)(t.x-mCamera.x),(int)(t.y-mCamera.y),c.w,c.h};
                    outline(r,60,255,60);
                });
            }
            {
                auto ev = reg.view<EnemyTag, Transform, Collider>(entt::exclude<DeadTag>);
                ev.each([&](const Transform& t, const Collider& c) {
                    SDL_Rect r = {(int)(t.x-mCamera.x),(int)(t.y-mCamera.y),c.w,c.h};
                    fill(r,255,140,0,45); outline(r,255,140,0);
                });
            }

            SDL_FRect hintBg = {0,(float)(window.GetHeight()-20),(float)window.GetWidth(),20};
            SDL_SetRenderDrawColor(ren,0,0,0,140);
            SDL_RenderFillRect(ren, &hintBg);
            Text hint("[F1] Hitboxes  Cyan=Player  White=Solid  Red=Hazard  Green=Ladder  Orange=Enemy",
                      SDL_Color{220,220,220,255}, 8, window.GetHeight()-16, 11);
            hint.Render(ren);
        }

        HUDSystem(reg, ren, W,
                  healthText.get(), gravityText.get(),
                  coinText.get(), coinCount, stompText.get(), stompCount);
    }

    if (mPaused) RenderPauseOverlay(window);
    window.Update();
}

// ─────────────────────────────────────────────────────────────────────────────
// Pause overlay
// ─────────────────────────────────────────────────────────────────────────────
void GameScene::BuildPauseUI(int W, int H) {
    int cx = W/2, cy = H/2;
    SDL_Rect titleRect = {cx-160, cy-145, 320, 50};
    auto [tx,ty] = Text::CenterInRect("PAUSED", 36, titleRect);
    mPauseTitleLbl = std::make_unique<Text>("PAUSED", SDL_Color{255,215,0,255}, tx, ty, 36);

    mPauseResumeRect = {cx-130, cy-60, 260, 55};
    mPauseResumeBtn  = std::make_unique<Rectangle>(mPauseResumeRect);
    mPauseResumeBtn->SetColor({40,160,80,255});
    mPauseResumeBtn->SetHoverColor({60,200,100,255});
    auto [rx,ry] = Text::CenterInRect("Resume", 28, mPauseResumeRect);
    mPauseResumeLbl = std::make_unique<Text>("Resume", SDL_Color{255,255,255,255}, rx, ry, 28);

    std::string backLabel = mFromEditor ? "Back to Editor" : "Back to Title";
    mPauseBackRect = {cx-130, cy+20, 260, 55};
    mPauseBackBtn  = std::make_unique<Rectangle>(mPauseBackRect);
    mPauseBackBtn->SetColor({120,50,50,255});
    mPauseBackBtn->SetHoverColor({180,70,70,255});
    auto [bx,by] = Text::CenterInRect(backLabel, 22, mPauseBackRect);
    mPauseBackLbl = std::make_unique<Text>(backLabel, SDL_Color{255,220,220,255}, bx, by, 22);
    mPauseHintLbl = std::make_unique<Text>("ESC to resume", SDL_Color{100,100,120,255},
                                           cx-70, cy+100, 14);
}

void GameScene::RenderPauseOverlay(Window& window) {
    SDL_Renderer* ren = window.GetRenderer();
    int W = window.GetWidth(), H = window.GetHeight();

    // Dim overlay
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 160);
    SDL_FRect full = {0,0,(float)W,(float)H};
    SDL_RenderFillRect(ren, &full);

    // Panel background
    SDL_Rect panel = {W/2-180, H/2-160, 360, 320};
    SDL_SetRenderDrawColor(ren, 18, 20, 32, 230);
    SDL_FRect fp = {(float)panel.x,(float)panel.y,(float)panel.w,(float)panel.h};
    SDL_RenderFillRect(ren, &fp);

    // Panel border
    SDL_SetRenderDrawColor(ren, 80, 120, 220, 255);
    SDL_RenderRect(ren, &fp);

    if (mPauseTitleLbl)  mPauseTitleLbl->Render(ren);
    if (mPauseResumeBtn) mPauseResumeBtn->Render(ren);
    if (mPauseResumeLbl) mPauseResumeLbl->Render(ren);
    if (mPauseBackBtn)   mPauseBackBtn->Render(ren);
    if (mPauseBackLbl)   mPauseBackLbl->Render(ren);
    if (mPauseHintLbl)   mPauseHintLbl->Render(ren);
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────
void GameScene::Spawn() {
    SDL_Renderer* ren = mWindow->GetRenderer();

    healthText  = std::make_unique<Text>("100", SDL_Color{255,255,255,255}, 0, 0, 16);
    gravityText = std::make_unique<Text>("",    SDL_Color{100,200,255,255}, 0, 0, 20);
    coinText    = std::make_unique<Text>("Gold Collected: 0",   SDL_Color{255,215,0,255},   0, 0, 16);
    stompText   = std::make_unique<Text>("Enemies Stomped: 0",  SDL_Color{255,100,100,255}, 0, 0, 16);

    coinSheet = std::make_unique<SpriteSheet>("game_assets/gold_coins/", "Gold_", 30, 40, 40);
    coinSheet->CreateTexture(ren);
    std::vector<SDL_Rect> coinFrames = coinSheet->GetAnimation("Gold_");

    auto spawnCoin = [&](float cx, float cy) {
        auto coin = reg.create();
        reg.emplace<Transform>(coin, cx, cy);
        reg.emplace<Renderable>(coin, coinSheet->GetTexture(), coinFrames, false);
        reg.emplace<AnimationState>(coin, 0, (int)coinFrames.size(), 0.0f, 15.0f, true);
        reg.emplace<Collider>(coin, COIN_SIZE, COIN_SIZE);
        reg.emplace<CoinTag>(coin);
    };

    if (!mLevelPath.empty()) {
        for (const auto& c : mLevel.coins) spawnCoin(c.x, c.y);
    } else {
        for (int i = 0; i < COIN_COUNT; i++) {
            int wall = rand() % 3; float cx=0,cy=0; int pad = COIN_SIZE+10;
            switch(wall){
                case 0: cx=5;cy=(float)(pad+rand()%(mWindow->GetHeight()-pad*2)); break;
                case 1: cx=(float)(pad+rand()%(mWindow->GetWidth()-pad*2));cy=5; break;
                case 2: cx=(float)(mWindow->GetWidth()-COIN_SIZE-5);cy=(float)(pad+rand()%(mWindow->GetHeight()-pad*2)); break;
            }
            spawnCoin(cx,cy);
        }
    }
    totalCoins = (int)reg.view<CoinTag>().size();

    mLevelW = (float)mWindow->GetWidth();
    mLevelH = (float)mWindow->GetHeight();
    for (const auto& ts : mLevel.tiles) {
        float right = ts.x + ts.w, bottom = ts.y + ts.h;
        if (ts.moving) {
            if (ts.moveHoriz) right  = std::max(right,  ts.x + ts.moveRange + ts.w);
            else               bottom = std::max(bottom, ts.y + ts.moveRange + ts.h);
        }
        if (right  > mLevelW) mLevelW = right;
        if (bottom > mLevelH) mLevelH = bottom;
    }
    mLevelW += (float)mWindow->GetWidth()  * 0.25f;
    mLevelH += (float)mWindow->GetHeight() * 0.25f;

    {
        float px = mLevel.player.x + PLAYER_STAND_WIDTH  * 0.5f;
        float py = mLevel.player.y + PLAYER_STAND_HEIGHT * 0.5f;
        mCamera.x = px - mWindow->GetWidth()  * 0.5f;
        mCamera.y = py - mWindow->GetHeight() * 0.5f;
        if (mCamera.x < 0) mCamera.x = 0;
        if (mCamera.y < 0) mCamera.y = 0;
        if (mCamera.x + mWindow->GetWidth()  > mLevelW) mCamera.x = mLevelW - mWindow->GetWidth();
        if (mCamera.y + mWindow->GetHeight() > mLevelH) mCamera.y = mLevelH - mWindow->GetHeight();
        if (mCamera.x < 0) mCamera.x = 0;
        if (mCamera.y < 0) mCamera.y = 0;
    }

    // ── Player spawn ──────────────────────────────────────────────────────────
    int pColW, pColH, pROffX, pROffY;
    {
        PlayerProfile tmpProfile;
        bool hasProfile = !mProfilePath.empty() && LoadPlayerProfile(mProfilePath, tmpProfile);
        const AnimHitbox& idleHB = hasProfile ? tmpProfile.Slot(PlayerAnimSlot::Idle).hitbox : AnimHitbox{};
        if (!idleHB.IsDefault()) {
            pColW = idleHB.w; pColH = idleHB.h;
            pROffX = -idleHB.x; pROffY = -idleHB.y;
        } else {
            const float sx = (float)mPlayerSpriteW / PLAYER_SPRITE_WIDTH;
            const float sy = (float)mPlayerSpriteH / PLAYER_SPRITE_HEIGHT;
            int insetX = (int)(PLAYER_BODY_INSET_X * sx);
            int insetT = (int)(PLAYER_BODY_INSET_TOP * sy);
            int insetB = (int)(PLAYER_BODY_INSET_BOTTOM * sy);
            pColW = mPlayerSpriteW - insetX*2; pColH = mPlayerSpriteH - insetT - insetB;
            pROffX = -insetX; pROffY = -insetT;
        }
    }

    float playerX = mLevelPath.empty() ? (float)(mWindow->GetWidth()/2 - pColW/2)  : mLevel.player.x;
    float playerY = mLevelPath.empty() ? (float)(mWindow->GetHeight() - pColH)      : mLevel.player.y;

    auto player = reg.create();
    reg.emplace<Transform>(player, playerX, playerY);
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
    reg.emplace<Renderable>(player, knightIdleSheet->GetTexture(), idleFrames, false);
    reg.emplace<PlayerTag>(player);
    reg.emplace<Health>(player);
    reg.emplace<Collider>(player, pColW, pColH);
    reg.emplace<RenderOffset>(player, pROffX, pROffY);
    {
        PlayerBaseCollider base;
        base.standW = pColW; base.standH = pColH;
        base.standRoffX = pROffX; base.standRoffY = pROffY;
        base.duckW = pColW; base.duckH = pColH/2;
        base.duckRoffX = pROffX; base.duckRoffY = -(mPlayerSpriteH - base.duckH);
        reg.emplace<PlayerBaseCollider>(player, base);
    }
    reg.emplace<InvincibilityTimer>(player);
    {
        GravityState gs;
        if (mLevel.gravityMode == GravityMode::OpenWorld) { gs.active = false; gs.isGrounded = true; }
        reg.emplace<GravityState>(player, gs);
    }
    if (mLevel.gravityMode == GravityMode::OpenWorld) reg.emplace<OpenWorldTag>(player);
    reg.emplace<ClimbState>(player);
    reg.emplace<HazardState>(player);
    reg.emplace<AttackState>(player);
    auto slotFps = [&](PlayerAnimSlot slot) -> float {
        return mSlotFps[static_cast<int>(slot)];
    };
    // For custom profiles, any slot that ended up with idleFrames (because it
    // was unfilled) must also use the idle sheet texture, not the frost-knight
    // sheet that was loaded as a fallback. Otherwise sheet and frames mismatch.
    SDL_Texture* idleT  = knightIdleSheet->GetTexture();
    auto resolveSheet = [&](SDL_Texture* slotTex,
                            const std::vector<SDL_Rect>& slotFrames) -> SDL_Texture* {
        // If frames were patched to idleFrames, the slot texture must also be
        // the idle texture so sheet+frames always refer to the same atlas.
        if (mHasProfile && &slotFrames == &idleFrames) return idleT;
        return slotTex;
    };
    reg.emplace<AnimationSet>(player, AnimationSet{
        .idle       = idleFrames,  .idleSheet  = idleT,
                                   .idleFps    = slotFps(PlayerAnimSlot::Idle),
        .walk       = walkFrames,  .walkSheet  = resolveSheet(knightWalkSheet->GetTexture(),  walkFrames),
                                   .walkFps    = slotFps(PlayerAnimSlot::Walk),
        .jump       = jumpFrames,  .jumpSheet  = resolveSheet(knightJumpSheet->GetTexture(),  jumpFrames),
                                   .jumpFps    = slotFps(PlayerAnimSlot::Jump),
        .hurt       = hurtFrames,  .hurtSheet  = resolveSheet(knightHurtSheet->GetTexture(),  hurtFrames),
                                   .hurtFps    = slotFps(PlayerAnimSlot::Hurt),
        .duck       = duckFrames,  .duckSheet  = resolveSheet(knightSlideSheet->GetTexture(), duckFrames),
                                   .duckFps    = slotFps(PlayerAnimSlot::Crouch),
        .front      = frontFrames, .frontSheet = resolveSheet(knightFallSheet->GetTexture(),  frontFrames),
                                   .frontFps   = slotFps(PlayerAnimSlot::Fall),
        .slash      = slashFrames, .slashSheet = resolveSheet(knightSlashSheet->GetTexture(), slashFrames),
                                   .slashFps   = slotFps(PlayerAnimSlot::Slash),
    });

    // Cache-aware tile texture helper.
    // On first Spawn: loads from disk, uploads to GPU, caches the pointer.
    // On subsequent Spawn() calls (Respawn): returns the cached GPU texture instantly.
    auto getCachedTex = [&](const std::string& path, int w, int h,
                             int rot = 0) -> SDL_Texture* {
        std::string key = TileCacheKey(path, w, h, rot);
        auto it = tileTextureCache.find(key);
        if (it != tileTextureCache.end()) return it->second;
        SDL_Texture* tex = LoadScaledTexture(ren, path, w, h, rot);
        if (tex) {
            tileScaledTextures.push_back(tex);
            tileTextureCache[key] = tex;
        }
        return tex;
    };

    // ── Spawn tiles ───────────────────────────────────────────────────────────
    for (const auto& ts : mLevel.tiles) {
        if (IsAnimatedTile(ts.imagePath)) {
            AnimatedTileDef def;
            if (!LoadAnimatedTileDef(ts.imagePath, def) || def.framePaths.empty()) {
                std::print("Failed to load animated tile def: {}\n", ts.imagePath);
                continue;
            }

            // Each animation frame gets its own cache entry so they're reused
            // across Respawn() calls without hitting disk again.
            std::vector<SDL_Texture*> frameTex;
            for (const auto& fp : def.framePaths)
                frameTex.push_back(getCachedTex(fp, ts.w, ts.h, ts.rotation));
            if (frameTex.empty() || !frameTex[0]) continue;

            std::vector<SDL_Rect> frameRects((int)frameTex.size(), SDL_Rect{0, 0, ts.w, ts.h});
            auto tile = reg.create();
            reg.emplace<Transform>(tile, ts.x, ts.y);

            bool hasCustomHitbox = (ts.hitboxW > 0 || ts.hitboxH > 0);
            int colW = hasCustomHitbox ? (ts.hitboxW > 0 ? ts.hitboxW : ts.w) : ts.w;
            int colH = hasCustomHitbox ? (ts.hitboxH > 0 ? ts.hitboxH : ts.h) : ts.h;

            if (ts.ladder) reg.emplace<LadderTag>(tile);
            else if (ts.slope != SlopeType::None) { reg.emplace<TileTag>(tile); reg.emplace<SlopeCollider>(tile, ts.slope, ts.slopeHeightFrac); }
            else if (ts.hazard)  { reg.emplace<HazardTag>(tile); if (!ts.prop) reg.emplace<TileTag>(tile); }
            else if (!ts.prop)     reg.emplace<TileTag>(tile);
            if (ts.prop)           reg.emplace<PropTag>(tile);
            if (ts.action)         reg.emplace<ActionTag>(tile, ts.actionGroup, ts.actionHits, ts.actionHits, ts.actionDestroyAnim);
            if (!ts.prop || ts.hazard) reg.emplace<Collider>(tile, colW, colH);
            if (hasCustomHitbox)   reg.emplace<ColliderOffset>(tile, ts.hitboxOffX, ts.hitboxOffY);

            reg.emplace<TileAnimTag>(tile);
            reg.emplace<Renderable>(tile, frameTex[0], std::move(frameRects), false);
            reg.emplace<AnimationState>(tile, 0, (int)frameTex.size(), 0.0f, def.fps, true);
            tileAnimFrameMap[tile] = std::move(frameTex);
            continue;
        }

        // ── Normal PNG tile ────────────────────────────────────────────────
        // Use cache: on Respawn this returns the already-uploaded GPU texture
        // without touching disk or the CPU scaling pipeline.
        SDL_Texture* tex = getCachedTex(ts.imagePath, ts.w, ts.h, ts.rotation);
        if (!tex) continue;

        auto tile = reg.create();
        reg.emplace<Transform>(tile, ts.x, ts.y);

        bool hasCustomHitbox = (ts.hitboxW > 0 || ts.hitboxH > 0);
        int colW = hasCustomHitbox ? (ts.hitboxW > 0 ? ts.hitboxW : ts.w) : ts.w;
        int colH = hasCustomHitbox ? (ts.hitboxH > 0 ? ts.hitboxH : ts.h) : ts.h;

        if (ts.ladder) {
            reg.emplace<LadderTag>(tile); reg.emplace<Collider>(tile, colW, colH);
        } else if (ts.slope != SlopeType::None) {
            reg.emplace<TileTag>(tile); reg.emplace<Collider>(tile, colW, colH);
            reg.emplace<SlopeCollider>(tile, ts.slope, ts.slopeHeightFrac);
        } else if (ts.hazard) {
            reg.emplace<HazardTag>(tile); reg.emplace<Collider>(tile, colW, colH);
            if (!ts.prop) reg.emplace<TileTag>(tile);
        } else {
            reg.emplace<Collider>(tile, colW, colH);
            if (!ts.prop) reg.emplace<TileTag>(tile);
        }
        if (ts.prop)   reg.emplace<PropTag>(tile);
        if (ts.action) reg.emplace<ActionTag>(tile, ts.actionGroup, ts.actionHits, ts.actionHits, ts.actionDestroyAnim);

        if (ts.antiGravity) {
            reg.emplace<FloatTag>(tile);
            FloatState fs; fs.baseY=ts.y;
            fs.bobAmp=4.0f+(rand()%50)*0.08f; fs.bobSpeed=1.4f+(rand()%80)*0.01f;
            fs.bobPhase=(rand()%628)*0.01f;
            reg.emplace<FloatState>(tile, fs);
        }
        if (ts.moving) {
            reg.emplace<MovingPlatformTag>(tile);
            MovingPlatformState mps;
            mps.horiz=ts.moveHoriz; mps.range=ts.moveRange; mps.speed=ts.moveSpeed;
            mps.groupId=ts.moveGroupId; mps.originX=ts.x; mps.originY=ts.y;
            mps.loop=ts.moveLoop; mps.trigger=ts.moveTrigger; mps.triggered=false;
            if (ts.moveLoop) {
                mps.phase=ts.movePhase*ts.moveRange; mps.loopDir=ts.moveLoopDir;
                if (ts.moveHoriz) reg.get<Transform>(tile).x = ts.x + mps.phase;
            } else {
                mps.phase=ts.movePhase*6.28318f; mps.loopDir=1;
            }
            reg.emplace<MovingPlatformState>(tile, mps);
        }
        if (hasCustomHitbox) reg.emplace<ColliderOffset>(tile, ts.hitboxOffX, ts.hitboxOffY);
        if (ts.powerUp && !ts.powerUpType.empty()) {
            PowerUpType puType = PowerUpType::None;
            if (ts.powerUpType == "antigravity") puType = PowerUpType::AntiGravity;
            // Future: else if (ts.powerUpType == "speedboost") puType = PowerUpType::SpeedBoost;
            if (puType != PowerUpType::None)
                reg.emplace<PowerUpTag>(tile, puType, ts.powerUpDuration);
        }

        std::vector<SDL_Rect> tileFrame = {{0, 0, ts.w, ts.h}};
        reg.emplace<Renderable>(tile, tex, tileFrame, false);
        reg.emplace<AnimationState>(tile, 0, 1, 0.0f, 1.0f, false);
    }

    // ── Spawn enemies ─────────────────────────────────────────────────────────
    auto spawnEnemy = [&](float x, float y, float speed, bool antiGrav = false) {
        float dx = (rand()%2==0) ? speed : -speed;
        auto  enemy = reg.create();
        reg.emplace<Transform>(enemy, x, y);
        reg.emplace<Velocity>(enemy, dx, 0.0f, speed);
        reg.emplace<AnimationState>(enemy, 0, (int)enemyWalkFrames.size(), 0.0f, 7.0f, true);
        reg.emplace<Renderable>(enemy, enemySheet->GetTexture(), enemyWalkFrames, false);
        reg.emplace<Collider>(enemy, SLIME_SPRITE_WIDTH, SLIME_SPRITE_HEIGHT);
        reg.emplace<EnemyTag>(enemy);
        Health eh; eh.current = SLIME_MAX_HEALTH; eh.max = SLIME_MAX_HEALTH;
        reg.emplace<Health>(enemy, eh);
        if (antiGrav) {
            reg.emplace<FloatTag>(enemy);
            FloatState fs; fs.baseY=y;
            fs.bobAmp=5.0f+(rand()%40)*0.1f; fs.bobSpeed=1.6f+(rand()%60)*0.01f;
            fs.bobPhase=(rand()%628)*0.01f;
            reg.emplace<FloatState>(enemy, fs);
        }
    };

    if (!mLevelPath.empty()) {
        for (const auto& e : mLevel.enemies) spawnEnemy(e.x, e.y, e.speed, e.antiGravity);
    } else {
        for (int i = 0; i < GRAVITYSLUGSCOUNT; ++i) {
            float x = (float)(rand() % (mWindow->GetWidth()-100));
            float y = (float)(rand() % (mWindow->GetHeight()-SLIME_SPRITE_HEIGHT));
            float speed = 60.0f + (float)(rand()%120);
            spawnEnemy(x, y, speed);
        }
    }

    // Build the pre-sorted tile render list used by RenderSystem Pass 1.
    // Tile entity IDs are stable for the lifetime of Spawn() -- they are never
    // re-created mid-level, only destroyed (action tiles). RenderSystem uses
    // this list instead of building+sorting a vector on every frame.
    // We rebuild it here on each Spawn() because Respawn() clears the registry.
    {
        mSortedTileRenderList.clear();
        auto tv = reg.view<TileTag,   AnimationState, Renderable>();
        auto lv = reg.view<LadderTag, AnimationState, Renderable>();
        auto pv = reg.view<PropTag,   AnimationState, Renderable>();
        mSortedTileRenderList.reserve(tv.size_hint() + lv.size_hint() + pv.size_hint());
        for (auto e : tv) mSortedTileRenderList.push_back(e);
        for (auto e : lv) mSortedTileRenderList.push_back(e);
        for (auto e : pv) mSortedTileRenderList.push_back(e);
        std::sort(mSortedTileRenderList.begin(), mSortedTileRenderList.end());
    }
}

void GameScene::Respawn() {
    reg.clear();
    tileAnimFrameMap.clear();
    mSortedTileRenderList.clear();
    // tileScaledTextures and tileTextureCache are intentionally NOT cleared here.
    // All tile textures are already uploaded to the GPU and can be reused as-is.
    // They are only freed in Unload() when the scene is torn down entirely.
    gameOver = false; levelComplete = false;
    levelCompleteTimer = 2.0f; coinCount = 0; stompCount = 0;
    mCamera = Camera{};
    Spawn();
}
