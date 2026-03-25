#include "LevelEditorScene.hpp"
#include "AnimatedTile.hpp"
#include "EditorCanvasRenderer.hpp"
#include "EditorFileOps.hpp"
#include "EditorPopups.hpp"
#include "EditorUIRenderer.hpp"
#include "GameScene.hpp"
#include "SurfaceUtils.hpp"
#include "TitleScene.hpp"
#include <SDL3_ttf/SDL_ttf.h>
#include <climits>
#include <cstdio>
#include <print>

namespace fs = std::filesystem;

// Shims kept for callers inside Load() / ImportPath() that use MakeThumb/LoadPNG
// directly (folder icon, bg import). These simply forward to the cache statics.
static SDL_Surface* MakeThumb(SDL_Surface* src, int w, int h) {
    return EditorSurfaceCache::MakeThumb(src, w, h);
}
static SDL_Surface* LoadPNG(const fs::path& p) {
    return EditorSurfaceCache::LoadPNG(p);
}

// --- MakePopupCtx ----------------------------------------------------------
EditorPopups::Ctx LevelEditorScene::MakePopupCtx() {
    return EditorPopups::Ctx{
        .level            = mLevel,
        .palette          = mPalette,
        .setStatus        = [this](const std::string& msg) { SetStatus(msg); },
        .refreshTileView  = [this]() { LoadTileView(mPalette.CurrentDir()); },
        .refreshBgPalette = [this]() { LoadBgPalette(); },
        .importPath       = [this](const std::string& p) { return ImportPath(p); },
        .getAnimThumb =
            [this](const std::string& p) { return mSurfaceCache.GetDestroyAnimThumb(p); },
        .sdlWindow = mWindow ? mWindow->GetRaw() : nullptr,
        .tileRoot  = TILE_ROOT,
        .bgRoot    = BG_ROOT,
    };
}

// --- ImportPath (delegate to EditorFileOps) --------------------------------
bool LevelEditorScene::ImportPath(const std::string& srcPath) {
    EditorFileOps::Ctx ctx{
        .palette          = mPalette,
        .cache            = mSurfaceCache,
        .level            = mLevel,
        .setStatus        = [this](const std::string& msg) { SetStatus(msg); },
        .refreshTileView  = [this]() { LoadTileView(mPalette.CurrentDir()); },
        .refreshBgPalette = [this]() { LoadBgPalette(); },
        .applyBackground  = [this](int idx) { ApplyBackground(idx); },
        .switchToTileTool =
            [this]() {
                SwitchTool(ToolId::Tile);
                if (lblTool)
                    lblTool->CreateSurface("Tile");
            },
        .palIcon  = PAL_ICON,
        .palCols  = PAL_COLS,
        .palW     = PALETTE_W,
        .tileRoot = TILE_ROOT,
        .bgRoot   = BG_ROOT,
    };
    return EditorFileOps::ImportPath(srcPath, ctx);
}

// --- OpenAnimPicker / CloseAnimPicker (shims into mPopups) ----------------
void LevelEditorScene::OpenAnimPicker(int tileIdx) {
    auto ctx = MakePopupCtx();
    mPopups.OpenAnimPicker(tileIdx, ctx);
}
void LevelEditorScene::CloseAnimPicker() {
    mPopups.CloseAnimPicker();
}

// --- GetPowerUpRegistry ----------------------------------------------------
// The single authoritative list of power-up types for both the editor UI and
// the game runtime. To add a new power-up:
//   1. Add an entry here (id, label, duration)
//   2. Add a PowerUpType enum value in Components.hpp
//   3. Handle the id string in GameScene::Spawn() -> PowerUpTag
//   4. Handle the PowerUpType in MovementSystem / GameScene::Update
const std::vector<EditorPopups::PowerUpEntry>& LevelEditorScene::GetPowerUpRegistry() {
    static const std::vector<EditorPopups::PowerUpEntry> kRegistry = {
        {"antigravity", "Anti-Gravity (15s)", 15.0f},
        // Add future power-ups here:
        // {"speedboost",  "Speed Boost (10s)",  10.0f},
        // {"invincible",  "Invincibility (8s)",  8.0f},
    };
    return kRegistry;
}

// --- LoadTileView / LoadBgPalette / ApplyBackground -----------------------
// Implementations moved to EditorPalette.cpp. LevelEditorScene delegates
// via inline wrappers in the header. The old ApplyBackground is replaced below.
void LevelEditorScene::ApplyBackground(int idx) {
    mPalette.ApplyBackground(idx, mLevel, [this](const std::string& bgPath) {
        background = std::make_unique<Image>(bgPath, FitModeFromString(mLevel.bgFitMode));
        background->SetRepeat(mLevel.bgRepeat);
        auto&       items = mPalette.BgItems();
        int         i     = mPalette.SelectedBg();
        std::string label = (i >= 0 && i < (int)items.size()) ? items[i].label : bgPath;
        SetStatus("Background: " + label + "  [" + mLevel.bgFitMode + "]");
    });
}

void LevelEditorScene::RebuildParallaxImages() {
    mParallaxImages.clear();
    for (const auto& pl : mLevel.parallaxLayers) {
        if (pl.imagePath.empty()) { mParallaxImages.push_back(nullptr); continue; }
        auto img = std::make_unique<Image>(pl.imagePath, FitMode::SCROLL);
        img->SetRepeat(true);
        mParallaxImages.push_back(std::move(img));
    }
}

// ─── Load ─────────────────────────────────────────────────────────────────────
void LevelEditorScene::Load(Window& window) {
    mWindow     = &window;
    mLaunchGame = false;
    // Disable SDL motion event coalescing so every mouse move is delivered
    // immediately — without this macOS batches motion events causing pan lag.
    SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
    SDL_SetHint("SDL_MOUSE_TOUCH_EVENTS", "0");

    background = std::make_unique<Image>("game_assets/backgrounds/deepspace_scene.png",
                                         FitModeFromString(mLevel.bgFitMode));
    enemySheet = std::make_unique<SpriteSheet>(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");

    // Determine which level file to load on startup.
    // Three cases:
    //   mForceNew == true          → skip all loading, start blank
    //   mOpenPath non-empty        → load that specific file
    //   mOpenPath empty, no force  → auto-resume levels/level1.json if it exists
    if (!mForceNew && mLevel.enemies.empty() &&
        mLevel.tiles.empty()) {
        std::string autoPath;
        if (!mOpenPath.empty()) {
            autoPath = mOpenPath;
            fs::path p(mOpenPath);
            mLevelName = p.stem().string();
        } else {
            autoPath = "levels/" + mLevelName + ".json";
        }
        if (fs::exists(autoPath)) {
            LoadLevel(autoPath, mLevel);
            SetStatus("Resumed: " + autoPath);
            if (!mLevel.background.empty()) {
                background = std::make_unique<Image>(mLevel.background,
                                                     FitModeFromString(mLevel.bgFitMode));
                background->SetRepeat(mLevel.bgRepeat);
            }
            RebuildParallaxImages();
        } else if (!mOpenPath.empty()) {
            // Path given but file doesn't exist yet — new level with that name
            SetStatus("New level: " + mLevelName);
        }
    } else if (mForceNew) {
        // Apply the name chosen in the title-screen modal, if one was given
        if (!mPresetName.empty())
            mLevelName = mPresetName;
        SetStatus("New level: " + mLevelName);
    }

    if (mLevel.player.x == 0 && mLevel.player.y == 0) {
        mLevel.player.x = static_cast<float>(CanvasW() / 2 - PLAYER_STAND_WIDTH / 2);
        mLevel.player.y =
            static_cast<float>(window.GetHeight() - PLAYER_STAND_HEIGHT - GRID * 2);
    }

    // Load the generic folder icon once — shared by all folder palette cells.
    // We keep it alive for the lifetime of the editor scene.
    if (!mFolderIcon) {
        SDL_Surface* raw = IMG_Load("game_assets/generic_folder.png");
        if (raw) {
            SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
            SDL_DestroySurface(raw);
            if (conv) {
                mFolderIcon = MakeThumb(conv, PAL_ICON, PAL_ICON);
                SDL_DestroySurface(conv);
            }
        }
    }

    // Initialize the palette subsystem with the shared surface cache and
    // folder icon, then load tile and background palettes.
    mPalette.Init(mSurfaceCache, mFolderIcon);
    LoadTileView(TILE_ROOT);
    LoadBgPalette();

    // ── Toolbar layout ────────────────────────────────────────────────────────
    mToolbar.RebuildLayout();
    mToolbar.CreateLabels();

    // Set gravity label to match current level mode
    {
        std::string gLbl = (mLevel.gravityMode == GravityMode::WallRun)     ? "Wall Run"
                           : (mLevel.gravityMode == GravityMode::OpenWorld) ? "Open World"
                                                                            : "Platform";
        mToolbar.SetGravityLabel(gLbl);
    }

    lblStatus = std::make_unique<Text>(
        mStatusMsg, SDL_Color{180, 180, 200, 255}, BTN_GAP, TOOLBAR_H + 4, 12);
    lblTool = std::make_unique<Text>(
        "Pan", SDL_Color{255, 215, 0, 255}, window.GetWidth() - PALETTE_W - 120, 22, 13);

    // Initialize popup subsystem
    mPopups.powerUpRegistry = &GetPowerUpRegistry();
    mPopups.movPlatGroupId  = mMovPlatCurGroupId;

    // ── Centre camera on player spawn ─────────────────────────────────────
    // Position the camera so the player spawn point sits 100px from the
    // left edge of the canvas. Clamp to (0,0) so we never go negative.
    {
        float spawnCamX = mLevel.player.x - 100.0f;
        float spawnCamY = mLevel.player.y - window.GetHeight() * 0.5f;
        if (spawnCamX < 0.0f) spawnCamX = 0.0f;
        if (spawnCamY < 0.0f) spawnCamY = 0.0f;
        mCamera.SetPosition(spawnCamX, spawnCamY);
    }

    // Initialize the default tool
    SwitchTool(ToolId::MoveCam);
}

// --- Unload ----------------------------------------------------------------
void LevelEditorScene::Unload() {
    // Deactivate the current tool before teardown
    if (mTool) {
        auto ctx = MakeToolCtx();
        mTool->OnDeactivate(ctx);
        mTool.reset();
    }

    // Palette owns all tile/bg item surfaces -- free them in one call.
    mPalette.Clear();

    if (mFolderIcon) {
        SDL_DestroySurface(mFolderIcon);
        mFolderIcon = nullptr;
    }

    // Free all cached surfaces (rotation, badge, destroy-anim, tile, extra)
    mSurfaceCache.Clear();

    mWindow = nullptr;
}

// --- HandleEvent -----------------------------------------------------------
bool LevelEditorScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT)
        return false;

    // ── File / folder drop ────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_DROP_BEGIN) {
        mDropActive = true;
        SetStatus("Drop a .png or folder...");
        return true;
    }
    if (e.type == SDL_EVENT_DROP_COMPLETE) {
        mDropActive = false;
        return true;
    }
    if (e.type == SDL_EVENT_DROP_FILE) {
        mDropActive      = false;
        std::string path = e.drop.data ? std::string(e.drop.data) : "";
        if (!path.empty()) {
            // If Action tool is active and the dropped file is an animated tile JSON,
            // assign it as the destroy animation for whatever action tile the cursor
            // is over. Don't import it into the palette.
            if (mActiveToolId == ToolId::Action && IsAnimatedTile(path)) {
                float fmx, fmy;
                SDL_GetMouseState(&fmx, &fmy);
                int mx = (int)fmx, my = (int)fmy;
                int ti = (my >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, my) : -1;
                if (ti >= 0 && mLevel.tiles[ti].HasAction()) {
                    mLevel.tiles[ti].action->destroyAnimPath = path;
                    // Preload the thumbnail now so it's ready to render immediately
                    GetDestroyAnimThumb(path);
                    fs::path p(path);
                    SetStatus("Tile " + std::to_string(ti) + ": death anim \"" +
                              p.stem().string() + "\" assigned");
                } else {
                    SetStatus("Drop a .json onto an Action tile to assign its death anim");
                }
                mActionAnimDropHover = -1;
                return true;
            }
            ImportPath(path);
        }
        return true;
    }

    // Track which action tile the cursor is hovering during an active file drag
    // (SDL sends DROP_BEGIN but not continuous position events, so we do this
    // in motion events below; this just resets when the drop window closes)
    if (e.type == SDL_EVENT_DROP_COMPLETE) {
        mActionAnimDropHover = -1;
    }

    // ── Moving-platform popup: text input for speed field ─────────────────────
    // -- Popup subsystem: movplat speed field, import input, delete confirm,
    //    anim picker, power-up picker, movplat config clicks
    {
        auto pctx = MakePopupCtx();
        if (mPopups.HandleEvent(e, pctx, mMovPlatIndices))
            return true;
    }

    // (Import input handled by mPopups.HandleEvent above)

    // ── Delete confirmation popup ──────────────────────────────────────────
    // (Delete confirm handled by mPopups.HandleEvent above)

    // EnemyTool speed popup text input
    if (mActiveToolId == ToolId::Enemy && mTool) {
        auto* et = dynamic_cast<EnemyTool*>(mTool.get());
        if (et && et->speedInputActive) {
            if (e.type == SDL_EVENT_TEXT_INPUT) {
                for (char c : std::string(e.text.text))
                    if (std::isdigit((unsigned char)c) && et->speedStr.size() < 6)
                        et->speedStr += c;
                return true;
            }
            if (e.type == SDL_EVENT_KEY_DOWN) {
                auto ctx = MakeToolCtx();
                if (mTool->OnKeyDown(ctx, e.key.key, SDL_GetModState()) ==
                    ToolResult::Consumed)
                    return true;
            }
        }
    }
    // ── Pan: middle-mouse drag OR Ctrl + left-mouse drag ────────────────────
    auto startPan = [&](int mx, int my) { mCamera.StartPan(mx, my); };

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_MIDDLE) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (mx < CanvasW() && my >= TOOLBAR_H) {
            startPan(mx, my);
            return true;
        }
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (SDL_GetModState() & SDL_KMOD_CTRL) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (mx < CanvasW() && my >= TOOLBAR_H) {
                startPan(mx, my);
                return true;
            }
        }
    }
    // Left-drag with the MoveCam tool pans the camera.
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT &&
        !(SDL_GetModState() & SDL_KMOD_CTRL) && mActiveToolId == ToolId::MoveCam) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (mx < CanvasW() && my >= TOOLBAR_H) {
            startPan(mx, my);
            return true;
        }
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP &&
        (e.button.button == SDL_BUTTON_MIDDLE || e.button.button == SDL_BUTTON_LEFT)) {
        if (mCamera.IsPanning()) {
            mCamera.StopPan();
            return true;
        }
    }
    // Pan motion is handled in Update() by polling SDL_GetMouseState every frame
    // for smoothness — no event handler needed here.

    // ── Mouse wheel ───────────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        float fmx, fmy;
        SDL_GetMouseState(&fmx, &fmy);
        int mx = (int)fmx, my = (int)fmy;

        // When MovingPlat tool is active, Ctrl+scroll adjusts range instead of zooming.
        // Adjusts the hovered tile's group, OR the current session group if no hover.
        // Start position stays fixed; only the end (range) moves.
        if ((SDL_GetModState() & SDL_KMOD_CTRL) && mActiveToolId == ToolId::MovingPlat &&
            mx < CanvasW()) {
            mMovPlatRange = std::max(GRID * 1.0f, mMovPlatRange + e.wheel.y * GRID);
            // Update current session tiles
            for (int idx : mMovPlatIndices)
                mLevel.tiles[idx].moving->range = mMovPlatRange;
            // Also update the hovered tile's group (handles already-placed platforms)
            int hovTi = (my >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, my) : -1;
            if (hovTi >= 0 && mLevel.tiles[hovTi].HasMoving()) {
                int grp = mLevel.tiles[hovTi].moving->groupId;
                for (auto& t : mLevel.tiles) {
                    if (!t.HasMoving())
                        continue;
                    if (grp != 0 ? (t.moving->groupId == grp) : (&t == &mLevel.tiles[hovTi]))
                        t.moving->range = mMovPlatRange;
                }
            }
            SetStatus("MovePlat range=" + std::to_string((int)mMovPlatRange));
            return true;
        }

        // Ctrl+scroll = zoom in/out, anchored to mouse position
        if ((SDL_GetModState() & SDL_KMOD_CTRL) && mx < CanvasW()) {
            if (mCamera.ApplyZoom(e.wheel.y, mx, my)) {
                SetStatus("Zoom: " + std::to_string(mCamera.ZoomPercent()) +
                          "%  (Ctrl+scroll)");
            }
            return true;
        }

        if (mx >= CanvasW()) {
            if (mPalette.ActiveTab() == EditorPalette::Tab::Tiles) {
                int scroll = std::max(0, mPalette.TileScroll() - (int)e.wheel.y);
                int rows   = ((int)mPalette.Items().size() + PAL_COLS - 1) / PAL_COLS;
                scroll     = std::min(scroll, std::max(0, rows - 1));
                mPalette.SetTileScroll(scroll);
            } else {
                int scroll = std::max(0, mPalette.BgScroll() - (int)e.wheel.y);
                scroll = std::min(scroll, std::max(0, (int)mPalette.BgItems().size() - 1));
                mPalette.SetBgScroll(scroll);
            }
        } else if (mActiveToolId == ToolId::Tile && mTool) {
            auto ctx = MakeToolCtx();
            mTool->OnScroll(ctx, e.wheel.y, mx, my, SDL_GetModState());
        } else if (mActiveToolId == ToolId::Action) {
            // Accumulate fractional SDL3 scroll ticks, step by 1 hit per full tick
            float fmya;
            SDL_GetMouseState(nullptr, &fmya);
            int mya       = (int)fmya;
            int hovAction = (mya >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, mya) : -1;
            if (hovAction >= 0 && mLevel.tiles[hovAction].HasAction()) {
                mScrollAccum += e.wheel.y;
                int steps = (int)mScrollAccum;
                if (steps != 0) {
                    mScrollAccum -= steps;
                    int& hits = mLevel.tiles[hovAction].action->hitsRequired;
                    hits      = std::clamp(hits + steps, 1, 99);
                    SetStatus("Action tile hits: " + std::to_string(hits));
                }
            } else {
                mScrollAccum = 0.0f; // not hovering a tile, discard accumulation
            }
        } else if (mActiveToolId == ToolId::Slope && mTool) {
            auto ctx = MakeToolCtx();
            mTool->OnScroll(ctx, e.wheel.y, mx, my, SDL_GetModState());
        } else if (mActiveToolId == ToolId::Enemy && mTool) {
            auto ctx = MakeToolCtx();
            mTool->OnScroll(ctx, e.wheel.y, mx, my, SDL_GetModState());
        } else if (mActiveToolId == ToolId::MovingPlat) {
            float fmx2, fmy2;
            SDL_GetMouseState(&fmx2, &fmy2);
            int  mxw = (int)fmx2, myw = (int)fmy2;
            int  hovTi = (myw >= TOOLBAR_H && mxw < CanvasW()) ? HitTile(mxw, myw) : -1;
            bool ctrl  = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
            bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
            if (ctrl && hovTi >= 0 && mLevel.tiles[hovTi].HasMoving()) {
                // Ctrl+scroll: adjust starting phase for the tile AND its whole group
                auto& ht       = mLevel.tiles[hovTi];
                float newPhase = ht.moving->phase + e.wheel.y * 0.05f;
                // Wrap instead of clamp so you can scroll continuously
                if (newPhase < 0.0f)
                    newPhase += 1.0f;
                if (newPhase > 1.0f)
                    newPhase -= 1.0f;
                int grp = ht.moving->groupId;
                for (auto& t : mLevel.tiles) {
                    if (!t.HasMoving())
                        continue;
                    if (grp != 0 ? (t.moving->groupId == grp) : (&t == &ht))
                        t.moving->phase = newPhase;
                }
                SetStatus((grp != 0 ? "Group " + std::to_string(grp)
                                    : "Tile " + std::to_string(hovTi)) +
                          "  phase=" + std::to_string(newPhase).substr(0, 4) +
                          "  dir=" + (ht.moving->loopDir > 0 ? "+1(right)" : "-1(left)"));
            } else if (shift && hovTi >= 0 && mLevel.tiles[hovTi].HasMoving()) {
                // Shift+scroll: flip start direction for the tile AND its whole group
                int newDir = (e.wheel.y > 0) ? 1 : -1;
                int grp    = mLevel.tiles[hovTi].moving->groupId;
                for (auto& t : mLevel.tiles) {
                    if (!t.HasMoving())
                        continue;
                    if (grp != 0 ? (t.moving->groupId == grp) : (&t == &mLevel.tiles[hovTi]))
                        t.moving->loopDir = newDir;
                }
                SetStatus(
                    (grp != 0 ? "Group " + std::to_string(grp)
                              : "Tile " + std::to_string(hovTi)) +
                    "  dir=" + (newDir > 0 ? "+1 (starts right)" : "-1 (starts left)"));
            } else {
                // Plain scroll: adjust range for current group
                mMovPlatRange = std::max(48.0f, mMovPlatRange + (int)e.wheel.y * GRID);
                for (int idx : mMovPlatIndices)
                    mLevel.tiles[idx].moving->range = mMovPlatRange;
                SetStatus("MovePlat range=" + std::to_string((int)mMovPlatRange) +
                          "  spd=" + std::to_string((int)mPopups.movPlatSpeed) +
                          (mPopups.movPlatLoop ? "  LOOP" : "") +
                          (mPopups.movPlatTrigger ? "  TRIGGER" : ""));
            }
        }
    }

    // ── Key down ──────────────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
            case SDLK_Q:
                SwitchTool(ToolId::Select);
                lblTool->CreateSurface("Select");
                break;
            case SDLK_1:
                SwitchTool(ToolId::Goal);
                lblTool->CreateSurface("Goal");
                break;
            case SDLK_2:
                SwitchTool(ToolId::Enemy);
                lblTool->CreateSurface("Enemy");
                break;
            case SDLK_F:
                if (mActiveToolId == ToolId::Enemy && mTool) {
                    auto* et = dynamic_cast<EnemyTool*>(mTool.get());
                    if (et) {
                        et->placementStartLeft = !et->placementStartLeft;
                        SetStatus(std::string("Enemy starts: ") +
                                  (et->placementStartLeft ? "LEFT" : "RIGHT") +
                                  "  (F to flip)");
                    }
                }
                break;
            case SDLK_3:
                SwitchTool(ToolId::Tile);
                lblTool->CreateSurface("Tile");
                mPalette.SetActiveTab(EditorPalette::Tab::Tiles);
                break;
            case SDLK_4:
                SwitchTool(ToolId::Erase);
                lblTool->CreateSurface("Erase");
                break;
            case SDLK_5:
                SwitchTool(ToolId::PlayerStart);
                lblTool->CreateSurface("Player");
                break;
            case SDLK_6:
                mPalette.SetActiveTab(EditorPalette::Tab::Backgrounds);
                lblTool->CreateSurface("Backgrounds");
                break;
            case SDLK_7:
            case SDLK_R:
                SwitchTool(ToolId::Resize);
                lblTool->CreateSurface("Resize");
                break;
            case SDLK_8:
            case SDLK_P:
                SwitchTool(ToolId::Prop);
                lblTool->CreateSurface("Prop");
                break;
            case SDLK_9:
            case SDLK_L:
                SwitchTool(ToolId::Ladder);
                lblTool->CreateSurface("Ladder");
                break;
            case SDLK_0:
                SwitchTool(ToolId::Action);
                lblTool->CreateSurface("Action");
                CloseAnimPicker();
                break;
            case SDLK_T:
                SwitchTool(ToolId::MoveCam);
                lblTool->CreateSurface("Pan");
                break;
            case SDLK_MINUS:
                SwitchTool(ToolId::Slope);
                lblTool->CreateSurface("Slope");
                break;
            case SDLK_G: {
                if (mLevel.gravityMode == GravityMode::Platformer)
                    mLevel.gravityMode = GravityMode::WallRun;
                else if (mLevel.gravityMode == GravityMode::WallRun)
                    mLevel.gravityMode = GravityMode::OpenWorld;
                else
                    mLevel.gravityMode = GravityMode::Platformer;
                std::string gLbl = (mLevel.gravityMode == GravityMode::WallRun) ? "Wall Run"
                                   : (mLevel.gravityMode == GravityMode::OpenWorld)
                                       ? "Open World"
                                       : "Platform";
                std::string gStatus = (mLevel.gravityMode == GravityMode::WallRun)
                                          ? "Mode: Wall Run"
                                      : (mLevel.gravityMode == GravityMode::OpenWorld)
                                          ? "Mode: Open World (top-down)"
                                          : "Mode: Platformer";
                mToolbar.SetGravityLabel(gLbl);
                SetStatus(gStatus);
                break;
            }

            case SDLK_I: {
                auto pctx2 = MakePopupCtx();
                mPopups.OpenImportInput(
                    mPalette.ActiveTab() == EditorPalette::Tab::Backgrounds, pctx2);
                break;
            }
            case SDLK_S:
                if (e.key.mod & SDL_KMOD_CTRL) {
                    fs::create_directories("levels");
                    std::string path = "levels/" + mLevelName + ".json";
                    mLevel.name      = mLevelName;
                    SaveLevel(mLevel, path);
                    SetStatus("Saved: " + path);
                }
                break;
            case SDLK_Z:
                if (e.key.mod & SDL_KMOD_CTRL) {
                    if (!mLevel.tiles.empty()) {
                        mLevel.tiles.pop_back();
                        SetStatus("Undo tile");
                    } else if (!mLevel.enemies.empty()) {
                        mLevel.enemies.pop_back();
                        SetStatus("Undo enemy");
                    }
                }
                break;
            case SDLK_DELETE:
            case SDLK_BACKSPACE:
                // Delegate to SelectTool if active
                if (mTool) {
                    auto ctx = MakeToolCtx();
                    mTool->OnKeyDown(ctx, e.key.key, (SDL_Keymod)e.key.mod);
                }
                break;
            case SDLK_TAB:
                mPalette.ToggleCollapsed();
                SetStatus(mPalette.IsCollapsed() ? "Palette hidden (Tab to show)"
                                                 : "Palette visible (Tab to hide)");
                break;
            case SDLK_ESCAPE:
                if (mPopups.animPickerTile >= 0) {
                    CloseAnimPicker();
                    break;
                }
                if (mActiveToolId == ToolId::Select && mTool) {
                    auto ctx = MakeToolCtx();
                    if (mTool->OnKeyDown(ctx, e.key.key, (SDL_Keymod)e.key.mod) ==
                        ToolResult::Consumed)
                        break; // don't fall through to the tile-browser Esc handler
                }
                // Navigate back up in tile browser
                if (mPalette.ActiveTab() == EditorPalette::Tab::Tiles &&
                    mPalette.CurrentDir() != TILE_ROOT) {
                    fs::path    parent = fs::path(mPalette.CurrentDir()).parent_path();
                    std::string up     = parent.string();
                    if (up.rfind(TILE_ROOT, 0) != 0)
                        up = TILE_ROOT;
                    LoadTileView(up);
                }
                break;
            default:
                break;
        }
    }

    // ── Mouse button down ─────────────────────────────────────────────────────
    // ── Right-click: group cycling for action tiles ──────────────────────────
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (mActiveToolId == ToolId::Action && my >= TOOLBAR_H && mx < CanvasW()) {
            int ti = HitTile(mx, my);
            if (ti >= 0 && mLevel.tiles[ti].HasAction()) {
                // Right-click cycles through available death animations:
                // None -> anim1 -> anim2 -> ... -> last -> None -> ...
                // The thumbnail badge updates immediately on each click.
                {
                    auto  manifests = ScanAnimatedTiles();
                    auto& cur       = mLevel.tiles[ti].action->destroyAnimPath;
                    if (manifests.empty()) {
                        cur.clear();
                        SetStatus("No death anims in animated_tiles/");
                    } else {
                        int idx = -1; // -1 = currently None
                        for (int m = 0; m < (int)manifests.size(); m++)
                            if (manifests[m].string() == cur) {
                                idx = m;
                                break;
                            }
                        idx++; // advance: None->0, 0->1, ..., last->None
                        if (idx >= (int)manifests.size()) {
                            cur.clear();
                            SetStatus("Tile " + std::to_string(ti) + ": death anim -> None");
                        } else {
                            cur = manifests[idx].string();
                            GetDestroyAnimThumb(cur); // preload thumbnail
                            SetStatus("Tile " + std::to_string(ti) + ": death anim -> " +
                                      manifests[idx].stem().string());
                        }
                    }
                }
                return true;
            }
        }
    }

    // Right-click on canvas: cycle tile rotation / action group / moving-plat params
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (mActiveToolId == ToolId::MovingPlat && my >= TOOLBAR_H && mx < CanvasW()) {
            // Preset cycle:
            //  H  96  60  (default horiz, medium)
            //  H  48  40  (horiz, short)
            //  H 192  80  (horiz, long)
            //  V  96  60  (vert, medium)
            //  V 192  50  (vert, long)
            //  V  48  40  (vert, short)
            //  → back to H 96 60
            if (mPopups.movPlatHoriz && mMovPlatRange == 96 && mPopups.movPlatSpeed == 60 &&
                !mPopups.movPlatLoop) {
                mPopups.movPlatHoriz = true;
                mMovPlatRange        = 48;
                mPopups.movPlatSpeed = 40;
                mPopups.movPlatLoop  = false;
            } else if (mPopups.movPlatHoriz && mMovPlatRange == 48 && !mPopups.movPlatLoop) {
                mPopups.movPlatHoriz = true;
                mMovPlatRange        = 192;
                mPopups.movPlatSpeed = 80;
                mPopups.movPlatLoop  = false;
            } else if (mPopups.movPlatHoriz && mMovPlatRange == 192 &&
                       !mPopups.movPlatLoop) {
                mPopups.movPlatHoriz = false;
                mMovPlatRange        = 96;
                mPopups.movPlatSpeed = 60;
                mPopups.movPlatLoop  = false;
            } else if (!mPopups.movPlatHoriz && mMovPlatRange == 96 &&
                       !mPopups.movPlatLoop) {
                mPopups.movPlatHoriz = false;
                mMovPlatRange        = 192;
                mPopups.movPlatSpeed = 50;
                mPopups.movPlatLoop  = false;
            } else if (!mPopups.movPlatHoriz && mMovPlatRange == 192 &&
                       !mPopups.movPlatLoop) {
                mPopups.movPlatHoriz = false;
                mMovPlatRange        = 48;
                mPopups.movPlatSpeed = 40;
                mPopups.movPlatLoop  = false;
            } else if (!mPopups.movPlatLoop) {
                mPopups.movPlatHoriz   = true;
                mMovPlatRange          = 1800;
                mPopups.movPlatSpeed   = 150;
                mPopups.movPlatLoop    = true;
                mPopups.movPlatTrigger = true;
            } else {
                mPopups.movPlatHoriz   = true;
                mMovPlatRange          = 96;
                mPopups.movPlatSpeed   = 60;
                mPopups.movPlatLoop    = false;
                mPopups.movPlatTrigger = false;
            }
            // Update all tiles already in the group
            for (int idx : mMovPlatIndices) {
                mLevel.tiles[idx].moving->horiz   = mPopups.movPlatHoriz;
                mLevel.tiles[idx].moving->range   = mMovPlatRange;
                mLevel.tiles[idx].moving->speed   = mPopups.movPlatSpeed;
                mLevel.tiles[idx].moving->loop    = mPopups.movPlatLoop;
                mLevel.tiles[idx].moving->trigger = mPopups.movPlatTrigger;
            }
            SetStatus(std::string(mPopups.movPlatHoriz ? "H" : "V") +
                      "  range=" + std::to_string((int)mMovPlatRange) +
                      "  spd=" + std::to_string((int)mPopups.movPlatSpeed) +
                      "  (RClick cycles presets)");
            return true;
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (my >= TOOLBAR_H && mx < CanvasW()) {
            int ti = HitTile(mx, my);
            if (ti >= 0) {
                if (mActiveToolId == ToolId::Action && mLevel.tiles[ti].HasAction()) {
                    int& grp = mLevel.tiles[ti].action->group;
                    grp      = (grp + 1) % 10;
                    SetStatus("Tile " + std::to_string(ti) + " group -> " +
                              (grp == 0 ? "standalone" : std::to_string(grp)));
                } else {
                    int& rot = mLevel.tiles[ti].rotation;
                    rot      = (rot + 90) % 360;
                    SetStatus("Tile " + std::to_string(ti) + " rotated to " +
                              std::to_string(rot) + "deg");
                }
                return true;
            } else if (mActiveToolId == ToolId::Tile && mTool) {
                // Delegate right-click to TileTool (cycles ghost rotation)
                auto ctx = MakeToolCtx();
                mTool->OnMouseDown(ctx, mx, my, SDL_BUTTON_RIGHT, SDL_GetModState());
                return true;
            }
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = (int)e.button.x, my = (int)e.button.y;

        // ── Destroy-anim picker — handle clicks inside the popup ──────────────────
        // Anim-picker clicks handled by mPopups.HandleEvent fired above.
        if (false && !mPopups.animPickerEntries.empty()) {
            if (HitTest(mPopups.animPickerRect, mx, my)) {
                const int THUMB    = 48;
                const int ROW_H    = THUMB + 10;
                const int PAD      = 8;
                const int COL_W    = THUMB + PAD * 2;
                const int COLS     = 4;
                const int TITLE_H  = 28;
                int       px       = mPopups.animPickerRect.x;
                int       py       = mPopups.animPickerRect.y;
                int       ey       = py + TITLE_H;
                int       nEntries = (int)mPopups.animPickerEntries.size();
                for (int i = 0; i < nEntries; i++) {
                    int      col  = i % COLS;
                    int      row  = i / COLS;
                    int      ex   = px + PAD + col * COL_W;
                    int      ey2  = ey + PAD + row * (ROW_H + PAD);
                    SDL_Rect cell = {ex, ey2, COL_W - PAD, ROW_H};
                    if (HitTest(cell, mx, my)) {
                        const auto& entry = mPopups.animPickerEntries[i];
                        if (mPopups.animPickerTile < (int)mLevel.tiles.size()) {
                            mLevel.tiles[mPopups.animPickerTile].action->destroyAnimPath =
                                entry.path;
                            if (!entry.path.empty())
                                GetDestroyAnimThumb(entry.path);
                            SetStatus("Tile " + std::to_string(mPopups.animPickerTile) +
                                      ": death anim → " +
                                      (entry.path.empty() ? "None" : entry.name));
                        }
                        CloseAnimPicker();
                        return true;
                    }
                }
                return true; // click inside popup but not on a cell — absorb
            } else {
                // Click outside popup — close without changing anything
                CloseAnimPicker();
                // Fall through so the click still acts on the canvas
            }
        }

        // Palette collapse toggle tab — a narrow strip at the left edge of the panel
        {
            int      cw          = CanvasW();
            SDL_Rect collapseBtn = {cw, TOOLBAR_H, PALETTE_TAB_W, 28};
            if (HitTest(collapseBtn, mx, my)) {
                mPalette.ToggleCollapsed();
                return true;
            }
        }

        // Tab bar (only when expanded)
        if (!mPalette.IsCollapsed() && mx >= CanvasW() + PALETTE_TAB_W && my >= TOOLBAR_H &&
            my < TOOLBAR_H + TAB_H) {
            int hw = (PALETTE_W - PALETTE_TAB_W) / 2;
            mPalette.SetActiveTab((mx < CanvasW() + PALETTE_TAB_W + hw)
                                      ? EditorPalette::Tab::Tiles
                                      : EditorPalette::Tab::Backgrounds);
            return true;
        }

        // Toolbar group collapse pills (strip lives below the buttons)
        for (int gi = 0; gi < static_cast<int>(TBGrp::COUNT); ++gi) {
            auto grp = static_cast<TBGrp>(gi);
            if (HitTest(mToolbar.PillRect(grp), mx, my)) {
                mToolbar.ToggleGroup(grp);
                static const char* kGrpNames[] = {"Place", "Modifier", "Actions"};
                SetStatus(std::string(kGrpNames[gi]) + (mToolbar.IsCollapsed(grp)
                                                            ? " group collapsed"
                                                            : " group expanded"));
                return true;
            }
        }

        // Toolbar
        // Any toolbar click closes the anim picker
        if (mPopups.animPickerTile >= 0 && my < TOOLBAR_H)
            CloseAnimPicker();

        // Toolbar button dispatch via EditorToolbar::HandleClick
        if (my < TOOLBAR_H) {
            auto click = mToolbar.HandleClick(mx, my);
            if (click.kind == EditorToolbar::ClickResult::Kind::Button) {
                CloseAnimPicker();
                switch (click.button) {
                    case TBBtn::Goal:
                        SwitchTool(ToolId::Goal);
                        lblTool->CreateSurface("Goal");
                        return true;
                    case TBBtn::Enemy:
                        SwitchTool(ToolId::Enemy);
                        lblTool->CreateSurface("Enemy");
                        return true;
                    case TBBtn::Tile:
                        SwitchTool(ToolId::Tile);
                        lblTool->CreateSurface("Tile");
                        return true;
                    case TBBtn::Erase:
                        SwitchTool(ToolId::Erase);
                        lblTool->CreateSurface("Erase");
                        return true;
                    case TBBtn::PlayerStart:
                        SwitchTool(ToolId::PlayerStart);
                        lblTool->CreateSurface("Player");
                        return true;
                    case TBBtn::Select:
                        SwitchTool(ToolId::Select);
                        lblTool->CreateSurface("Select");
                        return true;
                    case TBBtn::MoveCam:
                        SwitchTool(ToolId::MoveCam);
                        lblTool->CreateSurface("Pan");
                        return true;
                    case TBBtn::Prop:
                        SwitchTool(ToolId::Prop);
                        lblTool->CreateSurface("Prop");
                        return true;
                    case TBBtn::Ladder:
                        SwitchTool(ToolId::Ladder);
                        lblTool->CreateSurface("Ladder");
                        return true;
                    case TBBtn::Action:
                        SwitchTool(ToolId::Action);
                        lblTool->CreateSurface("Action");
                        return true;
                    case TBBtn::Slope:
                        SwitchTool(ToolId::Slope);
                        lblTool->CreateSurface("Slope");
                        return true;
                    case TBBtn::Resize:
                        SwitchTool(ToolId::Resize);
                        lblTool->CreateSurface("Resize");
                        return true;
                    case TBBtn::Hitbox:
                        SwitchTool(ToolId::Hitbox);
                        lblTool->CreateSurface("Hitbox");
                        return true;
                    case TBBtn::Hazard:
                        SwitchTool(ToolId::Hazard);
                        lblTool->CreateSurface("Hazard");
                        return true;
                    case TBBtn::AntiGrav:
                        SwitchTool(ToolId::AntiGrav);
                        lblTool->CreateSurface("Float");
                        return true;
                    case TBBtn::PowerUp:
                        SwitchTool(ToolId::PowerUp);
                        mPopups.powerUpOpen     = false;
                        mPopups.powerUpTileIdx  = -1;
                        mPopups.powerUpRegistry = &GetPowerUpRegistry();
                        lblTool->CreateSurface("PowerUp");
                        SetStatus("PowerUp: click a tile to assign a power-up pickup");
                        return true;
                    case TBBtn::MovingPlat: {
                        SwitchTool(ToolId::MovingPlat);
                        lblTool->CreateSurface("MovingPlat");
                        int maxUsed = 0;
                        for (const auto& ts : mLevel.tiles)
                            if (ts.HasMoving() && ts.moving->groupId > maxUsed)
                                maxUsed = ts.moving->groupId;
                        mMovPlatNextGroupId    = maxUsed + 1;
                        mMovPlatCurGroupId     = mMovPlatNextGroupId++;
                        mPopups.movPlatGroupId = mMovPlatCurGroupId;
                        mMovPlatIndices.clear();
                        for (int i = 0; i < (int)mLevel.tiles.size(); i++) {
                            if (!mLevel.tiles[i].HasMoving())
                                continue;
                            const auto& mp         = *mLevel.tiles[i].moving;
                            mPopups.movPlatHoriz   = mp.horiz;
                            mMovPlatRange          = mp.range;
                            mPopups.movPlatSpeed   = mp.speed;
                            mPopups.movPlatLoop    = mp.loop;
                            mPopups.movPlatTrigger = mp.trigger;
                            break;
                        }
                        mPopups.movPlatOpen       = true;
                        mPopups.movPlatSpeedInput = false;
                        mPopups.movPlatSpeedStr = std::to_string((int)mPopups.movPlatSpeed);
                        SetStatus(
                            "MovingPlat: click tiles to add. RClick=axis/range. New group "
                            "ID=" +
                            std::to_string(mMovPlatCurGroupId));
                        return true;
                    }
                    case TBBtn::Save: {
                        fs::create_directories("levels");
                        std::string path = "levels/" + mLevelName + ".json";
                        mLevel.name      = mLevelName;
                        SaveLevel(mLevel, path);
                        SetStatus("Saved: " + path);
                        return true;
                    }
                    case TBBtn::Load: {
                        std::string path = "levels/" + mLevelName + ".json";
                        if (LoadLevel(path, mLevel)) {
                            SetStatus("Loaded: " + path);
                            if (!mLevel.background.empty())
                                background = std::make_unique<Image>(
                                    mLevel.background, FitModeFromString(mLevel.bgFitMode));
                            RebuildParallaxImages();
                            LoadBgPalette();
                            mCamera.SetPosition(0.0f, 0.0f);
                        } else
                            SetStatus("No file: " + path);
                        return true;
                    }
                    case TBBtn::Clear:
                        mLevel.enemies.clear();
                        mLevel.tiles.clear();
                        SetStatus("Cleared");
                        return true;
                    case TBBtn::Play: {
                        fs::create_directories("levels");
                        std::string path = "levels/" + mLevelName + ".json";
                        mLevel.name      = mLevelName;
                        SaveLevel(mLevel, path);
                        mLaunchGame = true;
                        return true;
                    }
                    case TBBtn::Back: {
                        fs::create_directories("levels");
                        std::string path = "levels/" + mLevelName + ".json";
                        mLevel.name      = mLevelName;
                        SaveLevel(mLevel, path);
                        mGoBack = true;
                        return true;
                    }
                    case TBBtn::Gravity: {
                        if (mLevel.gravityMode == GravityMode::Platformer)
                            mLevel.gravityMode = GravityMode::WallRun;
                        else if (mLevel.gravityMode == GravityMode::WallRun)
                            mLevel.gravityMode = GravityMode::OpenWorld;
                        else
                            mLevel.gravityMode = GravityMode::Platformer;
                        std::string gLbl =
                            (mLevel.gravityMode == GravityMode::WallRun)     ? "Wall Run"
                            : (mLevel.gravityMode == GravityMode::OpenWorld) ? "Open World"
                                                                             : "Platform";
                        std::string gStatus =
                            (mLevel.gravityMode == GravityMode::WallRun) ? "Mode: Wall Run"
                            : (mLevel.gravityMode == GravityMode::OpenWorld)
                                ? "Mode: Open World (top-down)"
                                : "Mode: Platformer";
                        mToolbar.SetGravityLabel(gLbl);
                        SetStatus(gStatus);
                        return true;
                    }
                    default:
                        break;
                } // switch
            } // if Button
        } // if toolbar

        // ── Palette panel ──────────────────────────────────────────────────────
        if (mx >= CanvasW() && my >= TOOLBAR_H + TAB_H) {
            if (mPalette.ActiveTab() == EditorPalette::Tab::Tiles) {
                // Resolve which palette entry was clicked (same grid as render)
                constexpr int PAD = 4, LBL_H = 14;
                const int     cellW = (PALETTE_W - PAD * (PAL_COLS + 1)) / PAL_COLS;
                const int     cellH = cellW + LBL_H;
                const int     itemH = cellH + PAD;
                int           relX  = mx - CanvasW() - PAD;
                int           relY  = my - TOOLBAR_H - TAB_H - PAD;

                // header strip (44px)
                if (relY < 44)
                    return true;
                relY -= 44;

                int col = relX / (cellW + PAD);
                int row = relY / itemH;
                if (col < 0 || col >= PAL_COLS)
                    return true;

                int idx = (mPalette.TileScroll() + row) * PAL_COLS + col;
                if (idx < 0 || idx >= (int)mPalette.Items().size())
                    return true;

                const auto& item = mPalette.Items()[idx];

                // Delete button hit? open confirm popup
                if (item.delBtn.x >= 0 && HitTest(item.delBtn, mx, my)) {
                    mPopups.OpenDeleteConfirm(
                        item.path, item.isFolder, fs::path(item.path).filename().string());
                    return true;
                }

                if (item.isFolder) {
                    // Copy the path BEFORE calling LoadTileView — LoadTileView clears
                    // and rebuilds mPaletteItems, which invalidates the 'item' reference.
                    std::string folderPath = item.path;
                    std::string folderName = fs::path(folderPath).filename().string();
                    LoadTileView(folderPath);
                    SetStatus("Opened: " + folderName);
                } else {
                    // ── Double-click detection ────────────────────────────────
                    bool isDouble = mPalette.CheckDoubleClick(idx);

                    mPalette.SetSelectedTile(idx);
                    mGhostRotation = 0; // reset rotation when a new tile is picked
                    SwitchTool(ToolId::Tile);
                    lblTool->CreateSurface("Tool: Tile");
                    SetStatus("Selected: " + item.label + (isDouble ? " (double)" : ""));
                }
            } else {
                // Backgrounds — single column
                constexpr int PAD = 4, LBL_H = 16;
                const int     thumbW = PALETTE_W - PAD * 2;
                const int     thumbH = thumbW / 2;
                const int     itemH  = thumbH + LBL_H + PAD;

                // Fit-mode cycle button in the header strip (same geometry as Render)
                {
                    int bw = 54, bh = 16;
                    int bx  = CanvasW() + PALETTE_W - bw - 4;
                    int by2 = TOOLBAR_H + TAB_H + (24 - bh) / 2;
                    if (mx >= bx && mx < bx + bw && my >= by2 && my < by2 + bh) {
                        // Cycle: fill -> cover -> contain -> stretch -> tile -> scroll ->
                        // fill
                        auto& fm = mLevel.bgFitMode;
                        if (fm == "fill")
                            fm = "cover";
                        else if (fm == "cover")
                            fm = "contain";
                        else if (fm == "contain")
                            fm = "stretch";
                        else if (fm == "stretch")
                            fm = "tile";
                        else if (fm == "tile")
                            fm = "scroll";
                        else if (fm == "scroll")
                            fm = "scroll_wide";
                        else
                            fm = "fill";
                        // Rebuild background image with new fit mode
                        if (!mLevel.background.empty()) {
                            background = std::make_unique<Image>(mLevel.background,
                                                                 FitModeFromString(fm));
                            background->SetRepeat(mLevel.bgRepeat);
                        }
                        // Force badge cache rebuild for the new label
                        lblBgHeader.reset();
                        SetStatus("Background fit: " + fm);
                        return true;
                    }
                }

                // Repeat toggle button — right of the fit-mode button
                {
                    int rw = 50, rh = 16;
                    int rx = CanvasW() + PALETTE_W - 54 - 4 - rw - 4;
                    int ry = TOOLBAR_H + TAB_H + (24 - rh) / 2;
                    if (mx >= rx && mx < rx + rw && my >= ry && my < ry + rh) {
                        mLevel.bgRepeat = !mLevel.bgRepeat;
                        if (background)
                            background->SetRepeat(mLevel.bgRepeat);
                        lblBgHeader.reset();
                        SetStatus(std::string("Background repeat: ") +
                                  (mLevel.bgRepeat ? "ON" : "OFF"));
                        return true;
                    }
                }

                // ── Parallax layer controls ──────────────────────────────────
                constexpr int PLX_ROW_H = 34;
                constexpr int PLX_HDR_H = 28;
                constexpr int BTN_SZ    = 20;
                constexpr int BTN_GAP   = 4;
                int plxCount = (int)mLevel.parallaxLayers.size();
                int plxAreaH = PLX_HDR_H + std::max(plxCount, 1) * PLX_ROW_H + 6;
                int plxY     = mWindow->GetHeight() - plxAreaH + PLX_HDR_H;

                for (int li = 0; li < plxCount; ++li) {
                    int ry   = plxY + li * PLX_ROW_H;
                    int btnY = ry + (PLX_ROW_H - BTN_SZ) / 2;
                    int minusX = CanvasW() + PALETTE_W - 3 * (BTN_SZ + BTN_GAP);
                    int plusX  = CanvasW() + PALETTE_W - 2 * (BTN_SZ + BTN_GAP);
                    int delX   = CanvasW() + PALETTE_W - 1 * (BTN_SZ + BTN_GAP);

                    if (mx >= delX && mx < delX + BTN_SZ && my >= btnY && my < btnY + BTN_SZ) {
                        std::string name = fs::path(mLevel.parallaxLayers[li].imagePath).filename().string();
                        mLevel.parallaxLayers.erase(mLevel.parallaxLayers.begin() + li);
                        RebuildParallaxImages();
                        SetStatus("Removed parallax layer: " + name);
                        return true;
                    }
                    if (mx >= minusX && mx < minusX + BTN_SZ && my >= btnY && my < btnY + BTN_SZ) {
                        auto& f = mLevel.parallaxLayers[li].scrollFactor;
                        f = std::max(0.0f, f - 0.05f);
                        char buf[32]; std::snprintf(buf, sizeof(buf), "%.2f", f);
                        SetStatus("Parallax scroll: " + std::string(buf));
                        return true;
                    }
                    if (mx >= plusX && mx < plusX + BTN_SZ && my >= btnY && my < btnY + BTN_SZ) {
                        auto& f = mLevel.parallaxLayers[li].scrollFactor;
                        f = std::min(2.0f, f + 0.05f);
                        char buf[32]; std::snprintf(buf, sizeof(buf), "%.2f", f);
                        SetStatus("Parallax scroll: " + std::string(buf));
                        return true;
                    }
                }

                // ── Bg item click ───────────────────────────────────────────
                int relY = my - TOOLBAR_H - TAB_H - 24 - PAD; // 24 = bg header strip
                int row  = relY / itemH;
                int idx  = mPalette.BgScroll() + row;
                if (idx >= 0 && idx < (int)mPalette.BgItems().size()) {
                    if (mPalette.BgItems()[idx].delBtn.x >= 0 &&
                        HitTest(mPalette.BgItems()[idx].delBtn, mx, my)) {
                        mPopups.OpenDeleteConfirm(
                            mPalette.BgItems()[idx].path,
                            false,
                            fs::path(mPalette.BgItems()[idx].path).filename().string());
                    } else {
                        bool shiftHeld = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
                        if (shiftHeld) {
                            ParallaxLayer pl;
                            pl.imagePath    = mPalette.BgItems()[idx].path;
                            pl.scrollFactor = 0.5f;
                            pl.yOffset      = 0.0f;
                            mLevel.parallaxLayers.push_back(std::move(pl));
                            RebuildParallaxImages();
                            SetStatus("Added parallax layer: " +
                                      mPalette.BgItems()[idx].label + " (scroll: 0.50)");
                        } else {
                            ApplyBackground(idx);
                        }
                    }
                }
            }
            return true;
        }

        // ── Canvas ─────────────────────────────────────────────────────────────
        // MovingPlat config popup clicks handled by mPopups.HandleEvent above.

        if (my < TOOLBAR_H || mx >= CanvasW())
            return true;
        auto [sx, sy] = SnapToGrid(mx, my);

        // Dispatch to extracted tools first
        if (mTool) {
            // Populate TileTool's placement info if active
            if (mActiveToolId == ToolId::Tile) {
                if (auto* tt = dynamic_cast<TileTool*>(mTool.get())) {
                    const auto* selItem = mPalette.SelectedItem();
                    tt->placementInfo   = selItem ? TilePlacementInfo{true,
                                                                    selItem->isFolder,
                                                                    selItem->path,
                                                                    selItem->label}
                                                  : TilePlacementInfo{};
                }
            }
            auto ctx = MakeToolCtx();
            auto res = mTool->OnMouseDown(ctx, mx, my, SDL_BUTTON_LEFT, SDL_GetModState());
            if (res == ToolResult::Consumed)
                return true;
        }

        switch (mActiveToolId) {
            // Coin, Enemy, Tile, Erase, PlayerStart, MoveCam handled by mTool above
            default:
                break;
            case ToolId::Action: {
                // Picker clicks handled by mPopups.HandleEvent above.
                // If picker open but click was outside, it's already closed.
                if (mPopups.animPickerTile >= 0) {
                    if (HitTest(mPopups.animPickerRect, mx, my))
                        return true; // consumed by HandleEvent
                    CloseAnimPicker();
                }
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    if (mLevel.tiles[ti].HasAction()) {
                        // Tile is already an action tile — open the anim picker
                        OpenAnimPicker(ti);
                        SetStatus("Tile " + std::to_string(ti) + ": choose death animation");
                    } else {
                        // Not an action tile yet — make it one
                        mLevel.tiles[ti].action = ActionData{};
                        mLevel.tiles[ti].prop   = false;
                        mLevel.tiles[ti].ladder = false;
                        mLevel.tiles[ti].slope.reset();
                        SetStatus("Tile " + std::to_string(ti) +
                                  " → action  (click again to assign death anim)");
                    }
                } else {
                    CloseAnimPicker();
                }
                return true;
            }
            case ToolId::PowerUp: {
                // PowerUp popup clicks are handled by mPopups.HandleEvent.
                // Here we handle opening the popup on tile click.
                // If popup is open, HandleEvent consumed the click already.
                if (mPopups.powerUpOpen && mPopups.powerUpTileIdx >= 0)
                    return true; // absorbed by HandleEvent
                // Open/reopen on tile click
                mPopups.powerUpOpen    = false;
                mPopups.powerUpTileIdx = -1;
                int ti                 = HitTile(mx, my);
                if (ti >= 0) {
                    auto [wsx, wsy] = WorldToScreen(mLevel.tiles[ti].x, mLevel.tiles[ti].y);
                    int winW        = mWindow ? mWindow->GetWidth() : 800;
                    int winH        = mWindow ? mWindow->GetHeight() : 600;
                    mPopups.OpenPowerUpPicker(
                        ti, wsx, wsy + mLevel.tiles[ti].h, winW, winH, TOOLBAR_H);
                    SetStatus("Tile " + std::to_string(ti) + ": choose power-up type");
                } else {
                    SetStatus("PowerUp: click a tile to assign a power-up pickup");
                }
                return true;
            }
            case ToolId::MovingPlat: {
                // Left-click: toggle tile into/out of the current moving group
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    auto& t = mLevel.tiles[ti];
                    // If the tile is already moving but from a *different* session group,
                    // adopt its group so the speed popup edits the right group.
                    if (t.HasMoving() &&
                        std::find(mMovPlatIndices.begin(), mMovPlatIndices.end(), ti) ==
                            mMovPlatIndices.end()) {
                        // Clicking an existing platform from a previous session: adopt its
                        // group
                        const auto& mp = *t.moving;
                        mMovPlatCurGroupId =
                            (mp.groupId != 0) ? mp.groupId : mMovPlatNextGroupId++;
                        mPopups.movPlatGroupId  = mMovPlatCurGroupId;
                        mPopups.movPlatHoriz    = mp.horiz;
                        mMovPlatRange           = mp.range;
                        mPopups.movPlatSpeed    = mp.speed;
                        mPopups.movPlatLoop     = mp.loop;
                        mPopups.movPlatTrigger  = mp.trigger;
                        mPopups.movPlatSpeedStr = std::to_string((int)mPopups.movPlatSpeed);
                        // Collect all tiles in that group into mMovPlatIndices
                        mMovPlatIndices.clear();
                        for (int i = 0; i < (int)mLevel.tiles.size(); i++) {
                            if (!mLevel.tiles[i].HasMoving())
                                continue;
                            bool inGrp =
                                (mp.groupId != 0)
                                    ? (mLevel.tiles[i].moving->groupId == mp.groupId)
                                    : (i == ti);
                            if (inGrp)
                                mMovPlatIndices.push_back(i);
                        }
                        SetStatus("Adopted platform group " +
                                  std::to_string(mMovPlatCurGroupId) +
                                  "  spd=" + std::to_string((int)mPopups.movPlatSpeed) +
                                  "  tiles=" + std::to_string(mMovPlatIndices.size()));
                        return true;
                    }
                    // If already in this group, remove it
                    auto it = std::find(mMovPlatIndices.begin(), mMovPlatIndices.end(), ti);
                    if (it != mMovPlatIndices.end()) {
                        mMovPlatIndices.erase(it);
                        t.moving.reset();
                        SetStatus("Tile " + std::to_string(ti) +
                                  " removed from platform group " +
                                  std::to_string(mMovPlatCurGroupId));
                    } else {
                        // Add to current group
                        mMovPlatIndices.push_back(ti);
                        t.moving          = MovingPlatformData{};
                        t.moving->horiz   = mPopups.movPlatHoriz;
                        t.moving->range   = mMovPlatRange;
                        t.moving->speed   = mPopups.movPlatSpeed;
                        t.moving->loop    = mPopups.movPlatLoop;
                        t.moving->trigger = mPopups.movPlatTrigger;
                        t.moving->phase   = 0.0f; // set per-tile via Ctrl+scroll in editor
                        t.moving->loopDir = 1;    // set per-tile via Shift+scroll in editor
                        t.moving->groupId =
                            (mMovPlatIndices.size() > 1) ? mMovPlatCurGroupId : 0;
                        // Re-apply group id to all tiles in group
                        if (mMovPlatIndices.size() > 1) {
                            for (int idx : mMovPlatIndices)
                                mLevel.tiles[idx].moving->groupId = mMovPlatCurGroupId;
                        }
                        SetStatus("Tile " + std::to_string(ti) +
                                  " added to platform group " +
                                  std::to_string(mMovPlatCurGroupId) + "  " +
                                  (mPopups.movPlatHoriz ? "H" : "V") +
                                  "  range=" + std::to_string((int)mMovPlatRange) +
                                  "  spd=" + std::to_string((int)mPopups.movPlatSpeed));
                    }
                }
                return true;
            }
        }

        // Only start drag for inline tools that don't have their own entity-drag.
        // Extracted tools (Prop, Ladder, Slope, Hazard, AntiGrav, Select, Resize,
        // Hitbox) handle drag via mTool->OnMouseDown()/OnMouseMove()/OnMouseUp().
        if (mActiveToolId != ToolId::Erase && mActiveToolId != ToolId::Goal &&
            mActiveToolId != ToolId::Enemy && mActiveToolId != ToolId::Tile &&
            mActiveToolId != ToolId::MoveCam && mActiveToolId != ToolId::PlayerStart &&
            mActiveToolId != ToolId::Prop && mActiveToolId != ToolId::Ladder &&
            mActiveToolId != ToolId::Slope && mActiveToolId != ToolId::Hazard &&
            mActiveToolId != ToolId::AntiGrav && mActiveToolId != ToolId::Select &&
            mActiveToolId != ToolId::Resize && mActiveToolId != ToolId::Hitbox) {
            int ti = HitTile(mx, my);
            if (ti >= 0) {
                mIsDragging = true;
                mDragIndex  = ti;
                mDragIsTile = true;
                return true;
            }
            int ei = HitEnemy(mx, my);
            if (ei >= 0) {
                mIsDragging = true;
                mDragIndex  = ei;
                mDragIsTile = false;
                return true;
            }
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        // Dispatch to active tool first (handles entity drag stop for modifier tools)
        if (mTool) {
            auto ctx = MakeToolCtx();
            mTool->OnMouseUp(
                ctx, (int)e.button.x, (int)e.button.y, e.button.button, SDL_GetModState());
        }
        mIsDragging = false;
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        int mx = (int)e.motion.x, my = (int)e.motion.y;

        // ── Pan: derive delta from absolute position vs. recorded start ─────────
        // SDL coalesces MOUSE_MOTION on macOS so xrel/yrel can skip frames.
        // Using absolute position minus the recorded start position means we
        // always land exactly where the mouse is right now, regardless of how
        // many motion events were coalesced into this one.
        if (mCamera.IsPanning()) {
            mCamera.UpdatePan(mx, my);
            return true;
        }

        // Track which action tile is under the cursor during an active drag-drop
        // so Render can show the "drop here" highlight.
        if (mDropActive && mActiveToolId == ToolId::Action) {
            int ti = (my >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, my) : -1;
            mActionAnimDropHover = (ti >= 0 && mLevel.tiles[ti].HasAction()) ? ti : -1;
        } else {
            mActionAnimDropHover = -1;
        }

        // Delegate motion to active tool (select, resize, hitbox, entity drag)
        if (mTool) {
            auto ctx = MakeToolCtx();
            if (mTool->OnMouseMove(ctx, mx, my) == ToolResult::Consumed)
                return true;
        }

        // ── Entity drag ───────────────────────────────────────────────────────
        if (mIsDragging && mDragIndex >= 0 && my >= TOOLBAR_H && mx < CanvasW()) {
            auto [sx, sy] = SnapToGrid(mx, my);
            if (mDragIsTile && mDragIndex < (int)mLevel.tiles.size()) {
                mLevel.tiles[mDragIndex].x = (float)sx;
                mLevel.tiles[mDragIndex].y = (float)sy;
            } else if (!mDragIsTile &&
                       mDragIndex < (int)mLevel.enemies.size()) {
                mLevel.enemies[mDragIndex].x = (float)sx;
                mLevel.enemies[mDragIndex].y = (float)sy;
            }
        }
    }

    return true;
}

// --- Update ----------------------------------------------------------------
void LevelEditorScene::Update(float /*dt*/) {
    // Pan is driven entirely by SDL_EVENT_MOUSE_MOTION in HandleEvent using
    // absolute position delta from the recorded start point. SDL_CaptureMouse
    // ensures motion events are delivered reliably, so no polling catch-up is
    // needed here. Having two writers to mCamX/Y caused jitter, especially
    // under thermal throttling where frame timing is inconsistent.
}

// --- Render ----------------------------------------------------------------
void LevelEditorScene::Render(Window& window, float /*alpha*/) {
    window.Render();
    SDL_Renderer* ren = window.GetRenderer();

    int          W = mWindow->GetWidth(), H = mWindow->GetHeight();
    SDL_Surface* screen = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_ARGB8888);
    if (!screen) {
        window.Update();
        return;
    }
    SDL_SetSurfaceBlendMode(screen, SDL_BLENDMODE_BLEND);
    SDL_FillSurfaceRect(
        screen,
        nullptr,
        SDL_MapRGBA(SDL_GetPixelFormatDetails(screen->format), nullptr, 0, 0, 0, 0));
    int cw = CanvasW();

    // ── Canvas pass ─────────────────────────────────────────────────────────
    EditorCanvasRenderer::MovPlatState mp;
    mp.indices    = &mMovPlatIndices;
    mp.curGroupId = mMovPlatCurGroupId;
    mp.horiz      = mPopups.movPlatHoriz;
    mp.range      = mMovPlatRange;
    mp.speed      = mPopups.movPlatSpeed;
    mp.loop       = mPopups.movPlatLoop;
    mp.trigger    = mPopups.movPlatTrigger;
    mp.popupOpen  = mPopups.movPlatOpen;
    mp.speedInput = mPopups.movPlatSpeedInput;
    mp.speedStr   = mPopups.movPlatSpeedStr;
    mp.popupRect  = mPopups.movPlatRect;

    // Build parallax scroll factors from level data for the canvas renderer
    std::vector<float> plxFactors;
    plxFactors.reserve(mLevel.parallaxLayers.size());
    for (const auto& pl : mLevel.parallaxLayers)
        plxFactors.push_back(pl.scrollFactor);

    mCanvasRenderer.Render(window,
                           screen,
                           cw,
                           TOOLBAR_H,
                           GRID,
                           mLevel,
                           mCamera,
                           mSurfaceCache,
                           mPalette,
                           background.get(),
                           enemySheet.get(),
                           mActiveToolId,
                           mTool.get(),
                           MakeToolCtx(),
                           mActionAnimDropHover,
                           mp,
                           &mParallaxImages,
                           &plxFactors);

    // Sync back the popup rect (canvas renderer computes it when popup is open)
    mPopups.movPlatRect = mCanvasRenderer.MovPlatPopupRect();

    // ── UI pass ─────────────────────────────────────────────────────────────
    // Bridge AnimPickerEntry types (LevelEditorScene::AnimPickerEntry ->
    // EditorUIRenderer::AnimPickerEntry — same fields, different type).
    // Bridge: EditorPopups::AnimPickerEntry -> EditorUIRenderer::AnimPickerEntry
    std::vector<EditorUIRenderer::AnimPickerEntry> uiPickerEntries;
    uiPickerEntries.reserve(mPopups.animPickerEntries.size());
    for (const auto& e : mPopups.animPickerEntries)
        uiPickerEntries.push_back({e.path, e.name, e.thumb});

    // Bridge PowerUpEntry types
    const auto&                                 reg = GetPowerUpRegistry();
    std::vector<EditorUIRenderer::PowerUpEntry> uiPowerUpReg;
    uiPowerUpReg.reserve(reg.size());
    for (const auto& r : reg)
        uiPowerUpReg.push_back({r.id, r.label, r.defaultDuration});

    EditorUIRenderer::PowerUpPopupState puState;
    puState.open     = mPopups.powerUpOpen;
    puState.tileIdx  = mPopups.powerUpTileIdx;
    puState.rect     = mPopups.powerUpRect;
    puState.registry = &uiPowerUpReg;

    EditorUIRenderer::DelConfirmState dcState;
    dcState.active  = mPopups.delActive;
    dcState.isDir   = mPopups.delIsDir;
    dcState.name    = mPopups.delName;
    dcState.yesRect = mPopups.delYes;
    dcState.noRect  = mPopups.delNo;

    EditorUIRenderer::ImportInputState impState;
    impState.active = mPopups.importActive;
    impState.text   = mPopups.importText;

    EditorUIRenderer::MovPlatPopupState mpuState;
    mpuState.open       = mPopups.movPlatOpen;
    mpuState.speedInput = mPopups.movPlatSpeedInput;
    mpuState.speedStr   = mPopups.movPlatSpeedStr;
    mpuState.speed      = mPopups.movPlatSpeed;
    mpuState.horiz      = mPopups.movPlatHoriz;
    mpuState.loop       = mPopups.movPlatLoop;
    mpuState.trigger    = mPopups.movPlatTrigger;
    mpuState.curGroupId = mMovPlatCurGroupId;
    mpuState.rect       = mPopups.movPlatRect;

    mUIRenderer.Render(window,
                       screen,
                       cw,
                       TOOLBAR_H,
                       GRID,
                       mActiveToolId,
                       mCamera,
                       mLevel,
                       mSurfaceCache,
                       mToolbar,
                       mPalette,
                       lblStatus.get(),
                       lblTool.get(),
                       lblToolPrefix.get(),
                       mPopups.animPickerTile,
                       uiPickerEntries,
                       mPopups.animPickerRect,
                       puState,
                       dcState,
                       impState,
                       mpuState,
                       mDropActive,
                       lblPalHeader,
                       lblPalHint1,
                       lblPalHint2,
                       lblBgHeader,
                       lblStatusBar,
                       lblCamPos,
                       lblBottomHint,
                       mLastTileCount,
                       mLastEnemyCount,
                       mLastCamX,
                       mLastCamY,
                       mLastTileSizeW,
                       mLastPalHeaderPath,
                       GetTileW());

    // Sync back rects that HandleEvent needs
    mPopups.delYes         = mUIRenderer.DelConfirmYesRect();
    mPopups.delNo          = mUIRenderer.DelConfirmNoRect();
    mPopups.animPickerRect       = mUIRenderer.AnimPickerRect();
    mPopups.camShakeToggleRect   = mUIRenderer.CamShakeToggleRect();

    // Upload completed surface to GPU and present.
    // Use LINEAR filtering — the surface contains anti-aliased text from
    // TTF_RenderText_Blended. NEAREST would make text blocky, especially
    // on Retina/HiDPI where the logical surface is upscaled 2x.
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, screen);
    SDL_DestroySurface(screen);
    if (tex) {
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
        SDL_RenderTexture(ren, tex, nullptr, nullptr);
        SDL_DestroyTexture(tex);
    }
    window.Update();
}

// --- NextScene -------------------------------------------------------------
std::unique_ptr<Scene> LevelEditorScene::NextScene() {
    if (mLaunchGame) {
        mLaunchGame = false;
        return std::make_unique<GameScene>(
            "levels/" + mLevelName + ".json", true, mProfilePath);
    }
    if (mGoBack) {
        mGoBack = false;
        return std::make_unique<TitleScene>();
    }
    return nullptr;
}
// --- Tile tool accessors ---------------------------------------------------
int LevelEditorScene::GetTileW() const {
    if (mActiveToolId == ToolId::Tile && mTool)
        if (auto* tt = dynamic_cast<const TileTool*>(mTool.get()))
            return tt->tileW;
    return mTileW;
}
int LevelEditorScene::GetTileH() const {
    if (mActiveToolId == ToolId::Tile && mTool)
        if (auto* tt = dynamic_cast<const TileTool*>(mTool.get()))
            return tt->tileH;
    return mTileH;
}
int LevelEditorScene::GetGhostRotation() const {
    if (mActiveToolId == ToolId::Tile && mTool)
        if (auto* tt = dynamic_cast<const TileTool*>(mTool.get()))
            return tt->ghostRotation;
    return mGhostRotation;
}
