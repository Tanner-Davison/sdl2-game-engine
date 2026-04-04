#include "LevelEditorScene.hpp"
#include "AnimatedTile.hpp"
#include "EditorCanvasRenderer.hpp"
#include "EditorFileOps.hpp"
#include "EditorPopups.hpp"
#include "EditorUIRenderer.hpp"
#include "GameScene.hpp"
#include "LevelBinary.hpp"
#include "SurfaceUtils.hpp"
#include "TitleScene.hpp"
#include <SDL3_ttf/SDL_ttf.h>
#include <climits>
#include <cstdio>
#include <print>

namespace fs = std::filesystem;

SDL_Surface* LevelEditorScene::mFolderIcon   = nullptr;
Level        LevelEditorScene::sLevelStash;
std::string  LevelEditorScene::sLevelStashName;
bool         LevelEditorScene::sHasLevelStash = false;

static SDL_Surface* MakeThumb(SDL_Surface* src, int w, int h) {
    return EditorSurfaceCache::MakeThumb(src, w, h);
}
static SDL_Surface* LoadPNG(const fs::path& p) {
    return EditorSurfaceCache::LoadPNG(p);
}

// --- MakePopupCtx ---
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

// --- ImportPath ---
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

// --- OpenAnimPicker / CloseAnimPicker ---
void LevelEditorScene::OpenAnimPicker(int tileIdx) {
    auto ctx = MakePopupCtx();
    mPopups.OpenAnimPicker(tileIdx, ctx);
}
void LevelEditorScene::CloseAnimPicker() {
    mPopups.CloseAnimPicker();
}

// --- GetPowerUpRegistry ---
// The single authoritative list of power-up types for both the editor UI and
// the game runtime. To add a new power-up:
//   1. Add an entry here (id, label, duration)
//   2. Add a PowerUpType enum value in Components.hpp
//   3. Handle the id string in GameScene::Spawn() -> PowerUpTag
//   4. Handle the PowerUpType in MovementSystem / GameScene::Update
const std::vector<EditorPopups::PowerUpEntry>& LevelEditorScene::GetPowerUpRegistry() {
    static const std::vector<EditorPopups::PowerUpEntry> kRegistry = {
        {"antigravity",  "Anti-Gravity (15s)",     15.0f},
        {"turret",       "Orbiting Turret (15s)",  15.0f},
        {"healthboost",  "Health Boost (+25%)",     0.0f},
        {"teleport",     "Teleport (entrance)",     0.0f},
        {"shooter",      "Turret (static)",         0.0f},
    };
    return kRegistry;
}

// --- ApplyBackground ---
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

// --- Load ---
void LevelEditorScene::Load(Window& window) {
    mWindow     = &window;
    mLaunchGame = false;
    SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
    SDL_SetHint("SDL_MOUSE_TOUCH_EVENTS", "0");

    enemySheet = std::make_unique<SpriteSheet>(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");

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

        bool loaded = false;
        if (sHasLevelStash && sLevelStashName == mLevelName) {
            mLevel = std::move(sLevelStash);
            sHasLevelStash = false;
            sLevelStashName.clear();
            loaded = true;
        } else {
            if (fs::exists(autoPath)) {
                LoadLevel(autoPath, mLevel);
                loaded = true;
            }
        }

        if (loaded) {
            SetStatus("Resumed: " + autoPath);
            RebuildParallaxImages();
        } else if (!mOpenPath.empty()) {
            SetStatus("New level: " + mLevelName);
        }
    } else if (mForceNew) {
        if (!mPresetName.empty())
            mLevelName = mPresetName;
        SetStatus("New level: " + mLevelName);
    }

    if (!mLevel.background.empty())
        background = std::make_unique<Image>(mLevel.background,
                                             FitModeFromString(mLevel.bgFitMode));
    else
        background = std::make_unique<Image>("game_assets/backgrounds/deepspace_scene.png",
                                             FitModeFromString(mLevel.bgFitMode));
    background->SetRepeat(mLevel.bgRepeat);

    if (mLevel.player.x == 0 && mLevel.player.y == 0) {
        mLevel.player.x = static_cast<float>(CanvasW() / 2 - PLAYER_STAND_WIDTH / 2);
        mLevel.player.y =
            static_cast<float>(window.GetHeight() - PLAYER_STAND_HEIGHT - GRID * 2);
    }

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

    mPalette.Init(mSurfaceCache, mFolderIcon);
    if (!mPalette.RestoreTileItems(TILE_ROOT, mLevel))
        LoadTileView(TILE_ROOT);
    if (!mPalette.RestoreBgItems(mLevel))
        LoadBgPalette();

    mToolbar.RebuildLayout();
    mToolbar.CreateLabels();

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

    // Find the highest teleport group ID already in the level.
    mTeleportNextGroupId = 1;
    for (const auto& ts : mLevel.tiles)
        if (ts.HasPowerUp() && ts.powerUp->type == "teleport")
            mTeleportNextGroupId = std::max(mTeleportNextGroupId, ts.powerUp->teleportGroup + 1);
    mTeleportLinking = false;

    // Position camera so player spawn sits 100px from left edge, clamped to (0,0).
    {
        float spawnCamX = mLevel.player.x - 100.0f;
        float spawnCamY = mLevel.player.y - window.GetHeight() * 0.5f;
        if (spawnCamX < 0.0f) spawnCamX = 0.0f;
        if (spawnCamY < 0.0f) spawnCamY = 0.0f;
        mCamera.SetPosition(spawnCamX, spawnCamY);
    }

    SwitchTool(ToolId::MoveCam);
}

// --- Unload ---
void LevelEditorScene::Unload() {
    if (mTool) {
        auto ctx = MakeToolCtx();
        mTool->OnDeactivate(ctx);
        mTool.reset();
    }

    sLevelStash     = mLevel;
    sLevelStashName = mLevelName;
    sHasLevelStash  = true;

    mPalette.StashTileItems();
    mPalette.StashBgItems();
    mPalette.Clear();

    mSurfaceCache.StashSeededSurfaces();
    mSurfaceCache.Clear();

    mWindow = nullptr;
}

// --- HandleEvent ---
bool LevelEditorScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT)
        return false;

    // Hot-plug: open newly connected gamepads immediately.
    if (e.type == SDL_EVENT_GAMEPAD_ADDED) {
        SDL_OpenGamepad(e.gdevice.which);
        return true;
    }

    // --- Gamepad: SELECT button toggles toolbar focus (works for all tools) ---
    if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
        auto gbtn = e.gbutton.button;

        // SELECT (Back) — open / close the toolbar overlay.
        // On open: pre-select the button matching the currently active tool.
        if (gbtn == SDL_GAMEPAD_BUTTON_BACK) {
            if (mPadToolbarActive) {
                mPadToolbarActive = false;
                SetStatus("Returned to tool");
            } else {
                mPadToolbarActive    = true;
                mPadToolbarNavTimer  = 0.f;
                mPadToolbarNavRepeat = false;

                auto  visible    = VisibleToolbarButtons();
                TBBtn activeBtn  = ToolIdToBtn(mActiveToolId);
                int   startIdx   = 0;
                for (int i = 0; i < (int)visible.size(); ++i) {
                    if (visible[i] == activeBtn) { startIdx = i; break; }
                }
                mPadToolbarBtnIdx = startIdx;
                SetStatus("Toolbar  |  LS/\xe2\x86\x90\xe2\x86\x92=navigate  D-pad \xe2\x86\x93=groups/palette  A=select  B=close");
            }
            return true;
        }

        // While toolbar overlay is active, route A/B/D-pad here.
        // Sentinel layout:
        //   0 … visible.size()-1   → tool buttons (tool row)
        //   visible.size()         → group-collapse pills (LEFT/RIGHT picks Place/Mod/Actions)
        //   visible.size() + 1     → palette collapse tab
        if (mPadToolbarActive) {
            auto visible       = VisibleToolbarButtons();
            bool onTool        = (mPadToolbarBtnIdx < (int)visible.size());
            bool onGrpPills    = (mPadToolbarBtnIdx == (int)visible.size());
            bool onPalCollapse = (mPadToolbarBtnIdx == (int)visible.size() + 1);

            if (gbtn == SDL_GAMEPAD_BUTTON_SOUTH) {
                // A — activate focused item.
                if (onPalCollapse) {
                    mPalette.ToggleCollapsed();
                    SetStatus(mPalette.IsCollapsed() ? "Palette hidden  (press again to show)"
                                                     : "Palette shown");
                    mPadToolbarActive = false;
                } else if (onGrpPills) {
                    // Toggle the focused group's collapse pill.
                    auto grp = static_cast<TBGrp>(mPadToolbarGrpIdx);
                    mToolbar.ToggleGroup(grp);
                    const char* grpName = (grp == TBGrp::Place)    ? "Place"
                                        : (grp == TBGrp::Modifier)  ? "Modifier"
                                                                     : "Actions";
                    SetStatus(mToolbar.IsCollapsed(grp)
                        ? std::string(grpName) + " group hidden"
                        : std::string(grpName) + " group shown");
                    // Stay in overlay on the group row so user can toggle more groups.
                } else if (onTool && mPadToolbarBtnIdx >= 0 &&
                           mPadToolbarBtnIdx < (int)visible.size()) {
                    CloseAnimPicker();
                    ActivateToolbarButton(visible[mPadToolbarBtnIdx]);
                    mPadToolbarActive = false;
                }
                return true;
            }
            if (gbtn == SDL_GAMEPAD_BUTTON_EAST) {
                // B — close overlay without activating.
                mPadToolbarActive = false;
                SetStatus("Toolbar closed");
                return true;
            }
            // D-pad down: tool row → group pills → palette tab (stops at bottom).
            if (gbtn == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                if (onTool) {
                    // Pre-select the pill for the group the current tool belongs to.
                    if (!visible.empty() && mPadToolbarBtnIdx < (int)visible.size()) {
                        TBGrp g = EditorToolbar::GroupOf(visible[mPadToolbarBtnIdx]);
                        mPadToolbarGrpIdx = static_cast<int>(g);
                    }
                    mPadToolbarBtnIdx = (int)visible.size();        // → group pills
                } else if (onGrpPills) {
                    mPadToolbarBtnIdx = (int)visible.size() + 1;    // → palette collapse
                }
                // already at bottom — do nothing
                return true;
            }
            // D-pad up: palette tab → group pills → tool row.
            if (gbtn == SDL_GAMEPAD_BUTTON_DPAD_UP) {
                if (onPalCollapse)
                    mPadToolbarBtnIdx = (int)visible.size();        // → group pills
                else if (onGrpPills)
                    mPadToolbarBtnIdx = std::max(0, (int)visible.size() - 1); // → last tool
                // already at top — do nothing
                return true;
            }
            // D-pad left/right — navigate tool row OR cycle group pills.
            if (gbtn == SDL_GAMEPAD_BUTTON_DPAD_LEFT ||
                gbtn == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
                int dir = (gbtn == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) ? 1 : -1;
                if (onTool && !visible.empty()) {
                    mPadToolbarBtnIdx = std::clamp(
                        mPadToolbarBtnIdx + dir, 0, (int)visible.size() - 1);
                } else if (onGrpPills) {
                    constexpr int kGrpCount = EditorToolbar::kGroupCount;
                    mPadToolbarGrpIdx = (mPadToolbarGrpIdx + dir + kGrpCount) % kGrpCount;
                }
                return true;
            }
            return true; // consume all other buttons while overlay is open
        }
    }

    // --- PowerUp tool gamepad cursor ---
    if (mActiveToolId == ToolId::PowerUp && mEditorPad && !mPadToolbarActive) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            auto btn = e.gbutton.button;

            if (mPopups.powerUpOpen) {
                // ---- PICKER MODE: A selects, B cancels, D-pad navigates ----
                if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                    ApplyPowerUpPickerEntry(mPadPuPickerIdx);
                    return true;
                }
                if (btn == SDL_GAMEPAD_BUTTON_EAST) {
                    mPopups.ClosePowerUpPicker();
                    SetStatus("PowerUp: cancelled");
                    return true;
                }
                if (btn == SDL_GAMEPAD_BUTTON_DPAD_UP ||
                    btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                    int dir   = (btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN) ? 1 : -1;
                    int total = (int)(GetPowerUpRegistry().size() + 1); // +1 for None
                    mPadPuPickerIdx = std::clamp(mPadPuPickerIdx + dir, 0, total - 1);
                    return true;
                }
                return true; // consume everything while picker is open
            }

            // ---- CURSOR MODE: A = click tile to open picker ----
            if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                int ti = HitTile((int)mPadCursorX, (int)mPadCursorY);
                if (ti >= 0) {
                    // Mirror the mouse-click logic for the PowerUp tool.
                    mPopups.powerUpOpen    = false;
                    mPopups.powerUpTileIdx = -1;

                    if (mTeleportLinking) {
                        auto& t = mLevel.tiles[ti];
                        t.powerUp = PowerUpData{"teleport", 0.0f, 3.0f, 25.0f,
                                                mTeleportLinkGroup, true, ""};
                        t.prop       = true;
                        mTeleportLinking = false;
                        SetStatus("Tile " + std::to_string(ti) +
                                  " -> teleport DESTINATION (group " +
                                  std::to_string(mTeleportLinkGroup) + ")");
                    } else if (mLevel.tiles[ti].HasPowerUp() &&
                               mLevel.tiles[ti].powerUp->type == "teleport" &&
                               !mLevel.tiles[ti].powerUp->teleportDest) {
                        mTeleportLinking   = true;
                        mTeleportLinkGroup = mLevel.tiles[ti].powerUp->teleportGroup;
                        SetStatus("Teleport group " + std::to_string(mTeleportLinkGroup) +
                                  ": A on another tile to set DESTINATION");
                    } else {
                        // Open power-up picker popup at tile position.
                        auto [wsx, wsy] = WorldToScreen(mLevel.tiles[ti].x,
                                                        mLevel.tiles[ti].y);
                        int winW = mWindow ? mWindow->GetWidth() : 800;
                        int winH = mWindow ? mWindow->GetHeight() : 600;
                        mPopups.OpenPowerUpPicker(
                            ti, wsx, wsy + (int)(mLevel.tiles[ti].h * mCamera.Zoom()),
                            winW, winH, TOOLBAR_H);
                        mPadPuPickerIdx      = 0;
                        mPadPuPickerNavTimer  = 0.f;
                        mPadPuPickerNavRepeat = false;
                        SetStatus("Tile " + std::to_string(ti) +
                                  ": choose power-up  LS/D-pad=navigate  A=select  B=cancel");
                    }
                } else {
                    SetStatus("PowerUp: move cursor over a tile and press A to assign");
                }
                return true;
            }
        }
    }

    // --- Prop tool gamepad ---
    if (mActiveToolId == ToolId::Prop && mTool && mEditorPad && !mPadToolbarActive) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            auto  btn = e.gbutton.button;
            auto* pt  = dynamic_cast<PropTool*>(mTool.get());

            if (pt && pt->popupOpen) {
                // ---- POPUP OPEN: navigate Front/Back, A = apply, B = close ----
                if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                    // A: apply focused option
                    if (pt->popupIdx >= 0 && pt->popupIdx < (int)mLevel.tiles.size()) {
                        mLevel.tiles[pt->popupIdx].propBehind = !mPadPropFocusFront;
                        SetStatus("Tile " + std::to_string(pt->popupIdx) +
                                  " prop -> " + (mPadPropFocusFront ? "FRONT" : "BACK"));
                    }
                    pt->popupOpen = false;
                    pt->popupIdx  = -1;
                    return true;
                }
                if (btn == SDL_GAMEPAD_BUTTON_EAST) {
                    // B: close without changing
                    pt->popupOpen = false;
                    pt->popupIdx  = -1;
                    SetStatus("Prop: layer unchanged");
                    return true;
                }
                // Any D-pad direction toggles Front ↔ Back focus
                if (btn == SDL_GAMEPAD_BUTTON_DPAD_LEFT  ||
                    btn == SDL_GAMEPAD_BUTTON_DPAD_RIGHT ||
                    btn == SDL_GAMEPAD_BUTTON_DPAD_UP    ||
                    btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                    mPadPropFocusFront = !mPadPropFocusFront;
                    SetStatus(std::string("Prop focus: ") +
                              (mPadPropFocusFront ? "FRONT  A=apply  B=cancel"
                                                  : "BACK   A=apply  B=cancel"));
                    return true;
                }
                return true; // consume all other buttons while popup is open
            }

            // ---- CURSOR MODE: A = toggle prop flag, then open popup ----
            if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                int ti = HitTile((int)mPadCursorX, (int)mPadCursorY);
                if (ti >= 0) {
                    auto ctx = MakeToolCtx();
                    mTool->OnMouseDown(ctx, (int)mPadCursorX, (int)mPadCursorY,
                                       SDL_BUTTON_LEFT, SDL_GetModState());
                    // If tile is now a prop, immediately open the Front/Back popup.
                    if (pt && mLevel.tiles[ti].prop) {
                        int pw = 180, ph = 72;
                        int px = std::clamp((int)mPadCursorX - pw / 2, 4,
                                            CanvasW() - pw - 4);
                        int pyp = std::clamp((int)mPadCursorY - ph - 8,
                                             TOOLBAR_H + 4,
                                             mWindow->GetHeight() - ph - 4);
                        pt->popupIdx  = ti;
                        pt->popupOpen = true;
                        pt->popupRect = {px, pyp, pw, ph};
                        // Mirror the current state so focus matches existing value.
                        mPadPropFocusFront = !mLevel.tiles[ti].propBehind;
                        mPadPropNavTimer   = 0.f;
                        SetStatus("Tile " + std::to_string(ti) +
                                  ": choose prop layer  D-pad=toggle  A=apply  B=close");
                    }
                } else {
                    SetStatus("Prop: move cursor over a tile and press A");
                }
                return true;
            }
        }
    }

    // --- Ladder tool gamepad ---
    if (mActiveToolId == ToolId::Ladder && mTool && mEditorPad && !mPadToolbarActive) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
            e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
            int ti = HitTile((int)mPadCursorX, (int)mPadCursorY);
            if (ti >= 0) {
                auto ctx = MakeToolCtx();
                mTool->OnMouseDown(ctx, (int)mPadCursorX, (int)mPadCursorY,
                                   SDL_BUTTON_LEFT, SDL_GetModState());
            } else {
                SetStatus("Ladder: move cursor over a tile and press A");
            }
            return true;
        }
    }

    // --- Action tool gamepad ---
    if (mActiveToolId == ToolId::Action && mEditorPad && !mPadToolbarActive) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            auto btn = e.gbutton.button;

            // A — apply / select
            if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                if (mPopups.animPickerTile >= 0) {
                    // Picker is open: apply the highlighted entry.
                    int n = (int)mPopups.animPickerEntries.size();
                    if (n > 0) {
                        int idx = std::clamp(mPadActionPickerIdx, 0, n - 1);
                        const auto& entry = mPopups.animPickerEntries[idx];
                        if (mPopups.animPickerTile < (int)mLevel.tiles.size() &&
                            mLevel.tiles[mPopups.animPickerTile].HasAction()) {
                            mLevel.tiles[mPopups.animPickerTile].action->destroyAnimPath =
                                entry.path;
                            if (!entry.path.empty())
                                GetDestroyAnimThumb(entry.path);
                            SetStatus("Tile " + std::to_string(mPopups.animPickerTile) +
                                      ": death anim -> " +
                                      (entry.path.empty() ? "None" : entry.name));
                        }
                    }
                    CloseAnimPicker();
                } else {
                    // Cursor mode: toggle/open action on hovered tile.
                    int ti = HitTile((int)mPadCursorX, (int)mPadCursorY);
                    if (ti >= 0) {
                        if (mLevel.tiles[ti].HasAction()) {
                            // Already an action tile — open anim picker.
                            OpenAnimPicker(ti);
                            // Pre-select the entry that matches the current anim.
                            const std::string& cur = mLevel.tiles[ti].action->destroyAnimPath;
                            mPadActionPickerIdx = 0; // default to None
                            for (int i = 0; i < (int)mPopups.animPickerEntries.size(); ++i) {
                                if (mPopups.animPickerEntries[i].path == cur) {
                                    mPadActionPickerIdx = i;
                                    break;
                                }
                            }
                            mPadActionPickerTimer  = 0.f;
                            mPadActionPickerRepeat = false;
                            SetStatus("Tile " + std::to_string(ti) +
                                      ": choose death anim  LS/D-pad=navigate  A=apply  B=close");
                        } else {
                            // Make it an action tile.
                            mLevel.tiles[ti].action = ActionData{};
                            mLevel.tiles[ti].prop   = false;
                            mLevel.tiles[ti].ladder = false;
                            mLevel.tiles[ti].slope.reset();
                            SetStatus("Tile " + std::to_string(ti) +
                                      " -> action  (press A again to assign death anim)");
                        }
                    } else {
                        SetStatus("Action: move cursor over a tile and press A");
                    }
                }
                return true;
            }

            // B — close picker without applying
            if (btn == SDL_GAMEPAD_BUTTON_EAST) {
                if (mPopups.animPickerTile >= 0) {
                    CloseAnimPicker();
                    SetStatus("Action: death anim unchanged");
                    return true;
                }
            }

            // D-pad up/down — single-step nav in picker
            if (mPopups.animPickerTile >= 0) {
                if (btn == SDL_GAMEPAD_BUTTON_DPAD_UP || btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN) {
                    int dir = (btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN) ? 1 : -1;
                    int n   = (int)mPopups.animPickerEntries.size();
                    if (n > 0) {
                        mPadActionPickerIdx = std::clamp(mPadActionPickerIdx + dir, 0, n - 1);
                        SetStatus("Death anim: " +
                                  mPopups.animPickerEntries[mPadActionPickerIdx].name);
                    }
                    return true;
                }
            }
        }
    }

    // --- Slope tool gamepad ---
    if (mActiveToolId == ToolId::Slope && mTool && mEditorPad && !mPadToolbarActive) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
            e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
            // A: cycle slope type on the tile under the cursor.
            int ti = HitTile((int)mPadCursorX, (int)mPadCursorY);
            if (ti >= 0) {
                auto ctx = MakeToolCtx();
                mTool->OnMouseDown(ctx, (int)mPadCursorX, (int)mPadCursorY,
                                   SDL_BUTTON_LEFT, SDL_GetModState());
            } else {
                SetStatus("Slope: move cursor over a tile and press A to cycle slope type");
            }
            return true;
        }
    }

    // --- Hitbox tool gamepad ---
    if (mActiveToolId == ToolId::Hitbox && mTool && mEditorPad && !mPadToolbarActive) {
        // Ordered list of handles matching mPadHbHandleIdx 0-7.
        static constexpr HitboxTool::Handle kHbHandles[] = {
            HitboxTool::Handle::TopLeft,  HitboxTool::Handle::Top,
            HitboxTool::Handle::TopRight, HitboxTool::Handle::Left,
            HitboxTool::Handle::Right,    HitboxTool::Handle::BotLeft,
            HitboxTool::Handle::Bottom,   HitboxTool::Handle::BotRight
        };
        static const char* kHbNames[] = {
            "Top-Left","Top","Top-Right","Left","Right","Bot-Left","Bottom","Bot-Right"
        };

        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            const auto btn = e.gbutton.button;

            // A — mode-advance: cursor→node-select, node-select→node-edit, node-edit→node-select
            if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                if (mPadHbMode == 0) {
                    // Select tile under cursor.
                    int ti = HitTile((int)mPadCursorX, (int)mPadCursorY);
                    if (ti >= 0) {
                        auto& t = mLevel.tiles[ti];
                        if (!t.HasHitbox())
                            t.hitbox = HitboxData{0, 0, t.w, t.h};
                        mPadHbMode       = 1;
                        mPadHbHandleIdx  = 0;
                        mPadHbNavTimer   = 0.f;
                        mPadHbNavRepeat  = false;
                        auto* hbt = dynamic_cast<HitboxTool*>(mTool.get());
                        if (hbt) hbt->SetGamepadFocus(ti, kHbHandles[0]);
                        SetStatus(std::string("Hitbox: tile ") + std::to_string(ti) +
                                  "  LS/D-pad=navigate nodes  A=edit node  B=back to cursor");
                    } else {
                        SetStatus("Hitbox: move cursor over a tile and press A");
                    }
                } else if (mPadHbMode == 1) {
                    // Enter node-edit mode.
                    mPadHbMode       = 2;
                    mPadHbEditAccumX = 0.f;
                    mPadHbEditAccumY = 0.f;
                    SetStatus(std::string("Hitbox editing [") + kHbNames[mPadHbHandleIdx] +
                              "]  LS=adjust  A/B=done");
                } else {
                    // Return to node-select.
                    mPadHbMode = 1;
                    SetStatus(std::string("Hitbox: handle ") + kHbNames[mPadHbHandleIdx] +
                              "  LS/D-pad=navigate  A=edit  B=back to cursor");
                }
                return true;
            }

            // B — mode-retreat: node-edit→node-select, node-select→cursor
            if (btn == SDL_GAMEPAD_BUTTON_EAST) {
                if (mPadHbMode == 2) {
                    mPadHbMode = 1;
                    SetStatus(std::string("Hitbox: handle ") + kHbNames[mPadHbHandleIdx] +
                              "  LS/D-pad=navigate  A=edit  B=back to cursor");
                } else if (mPadHbMode == 1) {
                    mPadHbMode = 0;
                    auto* hbt = dynamic_cast<HitboxTool*>(mTool.get());
                    if (hbt) hbt->SetGamepadFocus(-1, HitboxTool::Handle::None);
                    SetStatus("Hitbox: LS=move  A=select tile  SELECT=tools");
                }
                return true;
            }

            // D-pad: 2D grid navigation in node-select mode.
            // Grid layout (col→, row↓):
            //   TopLeft(0)  Top(1)   TopRight(2)
            //   Left(3)     [empty]  Right(4)
            //   BotLeft(5)  Bottom(6) BotRight(7)
            // kNav[handle][0=up, 1=down, 2=left, 3=right], -1 = no neighbour
            if (mPadHbMode == 1 &&
                (btn == SDL_GAMEPAD_BUTTON_DPAD_LEFT  || btn == SDL_GAMEPAD_BUTTON_DPAD_RIGHT ||
                 btn == SDL_GAMEPAD_BUTTON_DPAD_UP    || btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN)) {
                static constexpr int kNav[8][4] = {
                    // up  down  left  right
                    {  -1,   3,   -1,   1 },  // 0 TopLeft
                    {  -1,   6,    0,   2 },  // 1 Top
                    {  -1,   4,    1,  -1 },  // 2 TopRight
                    {   0,   5,   -1,   4 },  // 3 Left
                    {   2,   7,    3,  -1 },  // 4 Right
                    {   3,  -1,   -1,   6 },  // 5 BotLeft
                    {   1,  -1,    5,   7 },  // 6 Bottom
                    {   4,  -1,    6,  -1 },  // 7 BotRight
                };
                int col = (btn == SDL_GAMEPAD_BUTTON_DPAD_UP)    ? 0 :
                          (btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN)   ? 1 :
                          (btn == SDL_GAMEPAD_BUTTON_DPAD_LEFT)   ? 2 : 3;
                int next = kNav[mPadHbHandleIdx][col];
                if (next >= 0) {
                    mPadHbHandleIdx = next;
                    auto* hbt = dynamic_cast<HitboxTool*>(mTool.get());
                    if (hbt) hbt->SetGamepadFocus(hbt->GetTileIdx(), kHbHandles[mPadHbHandleIdx]);
                    SetStatus(std::string("Hitbox: handle ") + kHbNames[mPadHbHandleIdx] +
                              "  A=edit  B=back to cursor");
                }
                return true;
            }
        }
    }

    // --- Float (AntiGrav) tool gamepad ---
    if (mActiveToolId == ToolId::AntiGrav && mTool && mEditorPad && !mPadToolbarActive) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
            e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
            int ti = HitTile((int)mPadCursorX, (int)mPadCursorY);
            int ei = HitEnemy((int)mPadCursorX, (int)mPadCursorY);
            if (ti >= 0 || ei >= 0) {
                auto ctx = MakeToolCtx();
                mTool->OnMouseDown(ctx, (int)mPadCursorX, (int)mPadCursorY,
                                   SDL_BUTTON_LEFT, SDL_GetModState());
            } else {
                SetStatus("Float: move cursor over a tile or enemy and press A");
            }
            return true;
        }
    }

    // --- Shield tool gamepad ---
    if (mActiveToolId == ToolId::Shield && mTool && mEditorPad && !mPadToolbarActive) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
            e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
            int ti = HitTile((int)mPadCursorX, (int)mPadCursorY);
            if (ti >= 0) {
                auto ctx = MakeToolCtx();
                mTool->OnMouseDown(ctx, (int)mPadCursorX, (int)mPadCursorY,
                                   SDL_BUTTON_LEFT, SDL_GetModState());
            } else {
                SetStatus("Shield: move cursor over a tile and press A");
            }
            return true;
        }
    }

    // --- Erase tool gamepad ---
    if (mActiveToolId == ToolId::Erase && mTool && mEditorPad && !mPadToolbarActive) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN &&
            e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
            // A down: erase whatever is at the cursor right now, then start drag.
            auto ctx = MakeToolCtx();
            mTool->OnMouseDown(ctx, (int)mPadCursorX, (int)mPadCursorY,
                               SDL_BUTTON_LEFT, SDL_GetModState());
            mPadEraseAHeld = true;
            return true;
        }
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
            e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
            mPadEraseAHeld = false;
            SetStatus("Erase: LS=move  A=erase  hold A + LS=drag erase");
            return true;
        }
    }

    // --- Select tool gamepad ---
    if (mActiveToolId == ToolId::Select && mTool && mEditorPad && !mPadToolbarActive) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            auto btn = e.gbutton.button;

            if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                // A down — synthesize a left mouse-down at the cursor position.
                // SelectTool decides: start rubber-band box OR begin drag of selection.
                auto ctx = MakeToolCtx();
                mTool->OnMouseDown(ctx, (int)mPadCursorX, (int)mPadCursorY,
                                   SDL_BUTTON_LEFT, 0);
                mPadSelAHeld = true;
                return true;
            }

            if (btn == SDL_GAMEPAD_BUTTON_EAST) {
                // B — delete selected objects if any, otherwise clear.
                auto ctx = MakeToolCtx();
                mTool->OnKeyDown(ctx, SDLK_DELETE, 0);
                return true;
            }
        }
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
            e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH) {
            // A up — commit the rubber-band or finish the drag.
            auto ctx = MakeToolCtx();
            mTool->OnMouseUp(ctx, (int)mPadCursorX, (int)mPadCursorY,
                             SDL_BUTTON_LEFT, 0);
            mPadSelAHeld = false;
            return true;
        }
    }

    // --- Tile-tool gamepad two-mode state machine ---
    if (mActiveToolId == ToolId::Tile && mTool && !mPadToolbarActive) {

        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            auto btn = e.gbutton.button;

            // ---- PALETTE MODE ----
            if (mPadPaletteActive) {
                // A → open folder OR confirm tile and switch to placement mode
                if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                    auto& items = mPalette.Items();
                    if (mPadPaletteIdx >= 0 && mPadPaletteIdx < (int)items.size()) {
                        const auto& item = items[mPadPaletteIdx];
                        if (item.isFolder) {
                            // Navigate into / out of the folder, stay in palette mode.
                            std::string folderPath = item.path;
                            std::string folderName = fs::path(folderPath).filename().string();
                            LoadTileView(folderPath);
                            mPadPaletteIdx   = 0;
                            mPadNavTimer     = 0.f;
                            mPadNavRepeating = false;
                            mPalette.SetSelectedTile(0);
                            SetStatus("Opened: " + folderName);
                            return true;
                        }
                    }
                    // It's a tile — confirm and enter placement mode.
                    mPadPaletteActive = false;
                    mPadNavTimer      = 0.f;
                    mPadNavRepeating  = false;
                    const auto* sel = mPalette.SelectedItem();
                    SetStatus(sel ? ("Tile: " + sel->label
                                     + "  |  LS=move  A=place  B/D-pad=pick tile")
                                  : "No tile selected");
                    return true;
                }
                // LB / RB → cycle rotation (preview before confirming)
                if (btn == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER ||
                    btn == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
                    if (auto* tt = dynamic_cast<TileTool*>(mTool.get())) {
                        int step = (btn == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ? 90 : -90;
                        tt->ghostRotation = ((tt->ghostRotation + step) % 360 + 360) % 360;
                        SetStatus("Rotation: " + std::to_string(tt->ghostRotation)
                                  + "\xc2\xb0");
                    }
                    return true;
                }
                return true; // consume all other buttons while palette is open
            }

            // ---- PLACEMENT MODE ----

            // B → back to palette
            if (btn == SDL_GAMEPAD_BUTTON_EAST) {
                if (mPadAHeld) {
                    auto ctx = MakeToolCtx();
                    mTool->OnMouseUp(ctx, (int)mPadCursorX, (int)mPadCursorY,
                                     SDL_BUTTON_LEFT, SDL_GetModState());
                    mPadAHeld = false;
                }
                mPadPaletteActive = true;
                SetStatus("Tile picker  |  LS=navigate  A=select");
                return true;
            }

            // D-pad any direction → back to palette
            if (btn == SDL_GAMEPAD_BUTTON_DPAD_UP   ||
                btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN  ||
                btn == SDL_GAMEPAD_BUTTON_DPAD_LEFT  ||
                btn == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) {
                if (mPadAHeld) {
                    auto ctx = MakeToolCtx();
                    mTool->OnMouseUp(ctx, (int)mPadCursorX, (int)mPadCursorY,
                                     SDL_BUTTON_LEFT, SDL_GetModState());
                    mPadAHeld = false;
                }
                mPadPaletteActive = true;
                SetStatus("Tile picker  |  LS=navigate  A=select");
                return true;
            }

            // A → place tile at virtual cursor
            if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                if (auto* tt = dynamic_cast<TileTool*>(mTool.get())) {
                    const auto* selItem = mPalette.SelectedItem();
                    tt->placementInfo   = selItem ? TilePlacementInfo{true,
                                                                      selItem->isFolder,
                                                                      selItem->path,
                                                                      selItem->label}
                                                  : TilePlacementInfo{};
                }
                auto ctx = MakeToolCtx();
                mTool->OnMouseDown(ctx, (int)mPadCursorX, (int)mPadCursorY,
                                   SDL_BUTTON_LEFT, SDL_GetModState());
                mPadAHeld = true;
                return true;
            }

            // LB / RB → cycle rotation in placement mode too
            if (btn == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER ||
                btn == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) {
                if (auto* tt = dynamic_cast<TileTool*>(mTool.get())) {
                    int step = (btn == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ? 90 : -90;
                    tt->ghostRotation = ((tt->ghostRotation + step) % 360 + 360) % 360;
                    SetStatus("Rotation: " + std::to_string(tt->ghostRotation)
                              + "\xc2\xb0");
                }
                return true;
            }
        }

        // A released → end drag
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_UP &&
            e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH && mPadAHeld) {
            auto ctx = MakeToolCtx();
            mTool->OnMouseUp(ctx, (int)mPadCursorX, (int)mPadCursorY,
                             SDL_BUTTON_LEFT, SDL_GetModState());
            mPadAHeld = false;
            return true;
        }
    }

    if (mMusicConfirmActive) {
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (HitTest(mMusicConfirmYes, mx, my)) {
                namespace fs = std::filesystem;
                mLevel.musicPath   = mMusicConfirmNewPath;
                mLevel.musicVolume = 1.0f;
                SetStatus("Level music: "
                    + fs::path(mMusicConfirmNewPath).filename().string()
                    + "  (drop another to replace, M to remove)");
                mMusicConfirmActive  = false;
                mMusicConfirmNewPath.clear();
                return true;
            }
            if (HitTest(mMusicConfirmNo, mx, my)) {
                mMusicConfirmActive  = false;
                mMusicConfirmNewPath.clear();
                SetStatus("Music change cancelled.");
                return true;
            }
            return true; // consume click while popup is open
        }
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
            mMusicConfirmActive  = false;
            mMusicConfirmNewPath.clear();
            SetStatus("Music change cancelled.");
            return true;
        }
        return true; // block all other events while confirm is open
    }

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
            // Audio file drop
            {
                fs::path p(path);
                auto ext = p.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac") {
                    float fmxA, fmyA;
                    SDL_GetMouseState(&fmxA, &fmyA);
                    int mxA = (int)fmxA, myA = (int)fmyA;
                    int ti = (myA >= TOOLBAR_H && mxA < CanvasW()) ? HitTile(mxA, myA) : -1;

                    // Tool-aware: PowerUp tool targets power-up or shooter tiles
                    if (mActiveToolId == ToolId::PowerUp || mActiveToolId == ToolId::Shooter) {
                        if (ti >= 0 && mLevel.tiles[ti].HasShooter()) {
                            mLevel.tiles[ti].shooter->sfxPath = path;
                            SetStatus("Turret " + std::to_string(ti) + " fire SFX: "
                                      + p.filename().string());
                        } else if (ti >= 0 && mLevel.tiles[ti].HasPowerUp()) {
                            mLevel.tiles[ti].powerUp->sfxPath = path;
                            SetStatus("Power-up " + std::to_string(ti) + " fire SFX: "
                                      + p.filename().string());
                        } else {
                            SetStatus("No power-up/turret tile under cursor");
                        }
                        return true;
                    }

                    // Fallback: try turret, then power-up, then level music
                    if (ti >= 0 && mLevel.tiles[ti].HasShooter()) {
                        mLevel.tiles[ti].shooter->sfxPath = path;
                        SetStatus("Turret " + std::to_string(ti) + " fire SFX: "
                                  + p.filename().string());
                        return true;
                    }
                    if (ti >= 0 && mLevel.tiles[ti].HasPowerUp()) {
                        mLevel.tiles[ti].powerUp->sfxPath = path;
                        SetStatus("Power-up " + std::to_string(ti) + " fire SFX: "
                                  + p.filename().string());
                        return true;
                    }
                    mMusicConfirmActive  = true;
                    mMusicConfirmNewPath = path;
                    SetStatus("Confirm music change...");
                    return true;
                }
            }

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

    if (e.type == SDL_EVENT_DROP_COMPLETE) {
        mActionAnimDropHover = -1;
    }

    {
        auto pctx = MakePopupCtx();
        int prevPuTile = mPopups.powerUpTileIdx;
        bool prevPuOpen = mPopups.powerUpOpen;
        if (mPopups.HandleEvent(e, pctx, mMovPlatIndices)) {
            // Detect: popup just closed after assigning teleport -> enter linking mode
            if (prevPuOpen && !mPopups.powerUpOpen && prevPuTile >= 0 &&
                prevPuTile < (int)mLevel.tiles.size()) {
                auto& t = mLevel.tiles[prevPuTile];
                std::print("[Teleport] Popup closed. tile={} hasPU={} type='{}' dest={}\n",
                           prevPuTile, t.HasPowerUp(),
                           t.HasPowerUp() ? t.powerUp->type : "(none)",
                           t.HasPowerUp() ? t.powerUp->teleportDest : false);
                if (t.HasPowerUp() && t.powerUp->type == "teleport" && !t.powerUp->teleportDest) {
                    int grp = mTeleportNextGroupId++;
                    t.powerUp->teleportGroup = grp;
                    mTeleportLinking   = true;
                    mTeleportLinkGroup = grp;
                    SetStatus("Teleport entrance set (group " + std::to_string(grp) +
                              "). Now click another tile to mark as DESTINATION.");
                    std::print("[Teleport] Linking mode ON, group={}\n", grp);
                }
            }
            return true;
        }
    }

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
    if (!mLevel.musicPath.empty() && mMusicVolSlider.w > 0) {
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (HitTest(mMusicVolSlider, mx, my)) {
                mMusicVolDragging = true;
                float t = std::clamp((float)(mx - mMusicVolSlider.x) / (float)mMusicVolSlider.w, 0.0f, 1.0f);
                mLevel.musicVolume = t;
                int pct = (int)(t * 100.0f + 0.5f);
                SetStatus("Music volume: " + std::to_string(pct) + "%");
                return true;
            }
        }
        if (e.type == SDL_EVENT_MOUSE_MOTION && mMusicVolDragging) {
            int mx = (int)e.motion.x;
            float t = std::clamp((float)(mx - mMusicVolSlider.x) / (float)mMusicVolSlider.w, 0.0f, 1.0f);
            mLevel.musicVolume = t;
            return true;
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT && mMusicVolDragging) {
            mMusicVolDragging = false;
            int pct = (int)(mLevel.musicVolume * 100.0f + 0.5f);
            SetStatus("Music volume: " + std::to_string(pct) + "%  (saved with level)");
            return true;
        }
    }

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
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        float fmx, fmy;
        SDL_GetMouseState(&fmx, &fmy);
        int mx = (int)fmx, my = (int)fmy;

        // Ctrl+scroll adjusts range for hovered group (or current session group).
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
        } else if (mActiveToolId == ToolId::PowerUp) {
            int hovTi = (my >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, my) : -1;
            if (hovTi >= 0 && mLevel.tiles[hovTi].HasShooter()) {
                // Scroll adjusts static turret fire rate
                auto& sd = *mLevel.tiles[hovTi].shooter;
                sd.fireRate = std::clamp(sd.fireRate + e.wheel.y * 0.5f, 0.5f, 20.0f);
                char buf[64];
                std::snprintf(buf, sizeof(buf), "Turret fire rate: %.1f shots/sec", sd.fireRate);
                SetStatus(buf);
            } else if (hovTi >= 0 && mLevel.tiles[hovTi].HasPowerUp()) {
                auto& pu = *mLevel.tiles[hovTi].powerUp;
                if (pu.type == "turret") {
                    pu.fireRate = std::clamp(pu.fireRate + e.wheel.y * 0.5f, 0.5f, 20.0f);
                    char buf[64];
                    std::snprintf(buf, sizeof(buf), "Turret PU fire rate: %.1f shots/sec", pu.fireRate);
                    SetStatus(buf);
                } else if (pu.type == "healthboost") {
                    pu.healthPct = std::clamp(pu.healthPct + e.wheel.y * 25.0f, 25.0f, 200.0f);
                    SetStatus("Health Boost: +" + std::to_string((int)pu.healthPct) + "% max HP");
                }
            }
        }
    }

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
                mToolbar.SetGravityLabel(gLbl);
                SetStatus("Gravity: " + gLbl);
                break;
            }
            case SDLK_M: {
                // Toggle level music: M clears music, status shows current state
                if (!mLevel.musicPath.empty()) {
                    std::string old = fs::path(mLevel.musicPath).filename().string();
                    mLevel.musicPath.clear();
                    mLevel.musicVolume = 1.0f;
                    SetStatus("Music removed (was: " + old + ")  Drop audio file to set new");
                } else {
                    SetStatus("No music set. Drop a .wav/.ogg/.mp3 file to assign level music");
                }
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
            if (mTool && mActiveToolId == ToolId::Prop) {
                auto ctx = MakeToolCtx();
                if (mTool->OnMouseDown(ctx, mx, my, SDL_BUTTON_RIGHT, SDL_GetModState())
                    == ToolResult::Consumed)
                    return true;
            }
            int ti = HitTile(mx, my);
            if (ti >= 0) {
                if (mActiveToolId == ToolId::Action && mLevel.tiles[ti].HasAction()) {
                    int& grp = mLevel.tiles[ti].action->group;
                    grp      = (grp + 1) % 10;
                    SetStatus("Tile " + std::to_string(ti) + " group -> " +
                              (grp == 0 ? "standalone" : std::to_string(grp)));
                } else if ((mActiveToolId == ToolId::Shooter || mActiveToolId == ToolId::PowerUp)
                           && mLevel.tiles[ti].HasShooter()) {
                    int s = (static_cast<int>(mLevel.tiles[ti].shooter->side) + 1) % 4;
                    mLevel.tiles[ti].shooter->side = static_cast<ShooterSide>(s);
                    static const char* kNames[] = {"Top","Right","Bottom","Left"};
                    SetStatus("Tile " + std::to_string(ti) + " shooter side -> " + kNames[s]);
                } else if (mActiveToolId != ToolId::Prop) {
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

        if (mPopups.animPickerTile >= 0 && my < TOOLBAR_H)
            CloseAnimPicker();

        if (my < TOOLBAR_H) {
            auto click = mToolbar.HandleClick(mx, my);
            if (click.kind == EditorToolbar::ClickResult::Kind::Button) {
                CloseAnimPicker();
                ActivateToolbarButton(click.button);
                return true;
            } // if Button
        } // if toolbar

        if (mx >= CanvasW() && my >= TOOLBAR_H + TAB_H) {
            if (mPalette.ActiveTab() == EditorPalette::Tab::Tiles) {
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
            default:
                break;
            case ToolId::Action: {
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
                if (mPopups.powerUpOpen && mPopups.powerUpTileIdx >= 0)
                    return true;
                mPopups.powerUpOpen    = false;
                mPopups.powerUpTileIdx = -1;
                int ti                 = HitTile(mx, my);
                if (ti < 0) {
                    mTeleportLinking = false;
                    SetStatus("PowerUp: click a tile to assign a power-up pickup");
                    return true;
                }

                // Teleport destination linking mode
                if (mTeleportLinking) {
                    std::print("[Teleport] Destination click on tile {}\n", ti);
                    auto& t = mLevel.tiles[ti];
                    t.powerUp = PowerUpData{"teleport", 0.0f, 3.0f, 25.0f, mTeleportLinkGroup, true, ""};
                    t.prop = true;
                    mTeleportLinking = false;
                    SetStatus("Tile " + std::to_string(ti) +
                              " -> teleport DESTINATION (group " +
                              std::to_string(mTeleportLinkGroup) + ")");
                    return true;
                }

                // If tile already has a teleport entrance, re-enter linking mode
                if (mLevel.tiles[ti].HasPowerUp() &&
                    mLevel.tiles[ti].powerUp->type == "teleport" &&
                    !mLevel.tiles[ti].powerUp->teleportDest) {
                    mTeleportLinking   = true;
                    mTeleportLinkGroup = mLevel.tiles[ti].powerUp->teleportGroup;
                    SetStatus("Teleport group " + std::to_string(mTeleportLinkGroup) +
                              ": click another tile to set DESTINATION");
                    return true;
                }

                // Normal: open the power-up picker popup
                auto [wsx, wsy] = WorldToScreen(mLevel.tiles[ti].x, mLevel.tiles[ti].y);
                int winW        = mWindow ? mWindow->GetWidth() : 800;
                int winH        = mWindow ? mWindow->GetHeight() : 600;
                mPopups.OpenPowerUpPicker(
                    ti, wsx, wsy + mLevel.tiles[ti].h, winW, winH, TOOLBAR_H);
                SetStatus("Tile " + std::to_string(ti) + ": choose power-up type");
                return true;
            }
            case ToolId::MovingPlat: {
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    auto& t = mLevel.tiles[ti];
                    // Adopt the group of a previously-placed platform so popup edits the right group.
                    if (t.HasMoving() &&
                        std::find(mMovPlatIndices.begin(), mMovPlatIndices.end(), ti) ==
                            mMovPlatIndices.end()) {
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
            mActiveToolId != ToolId::AntiGrav && mActiveToolId != ToolId::Shooter &&
            mActiveToolId != ToolId::Shield && mActiveToolId != ToolId::Select &&
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
        if (mTool) {
            auto ctx = MakeToolCtx();
            mTool->OnMouseUp(
                ctx, (int)e.button.x, (int)e.button.y, e.button.button, SDL_GetModState());
        }
        mIsDragging = false;
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        int mx = (int)e.motion.x, my = (int)e.motion.y;

        // Absolute position delta avoids jitter from coalesced motion events on macOS.
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

        if (mTool) {
            auto ctx = MakeToolCtx();
            if (mTool->OnMouseMove(ctx, mx, my) == ToolResult::Consumed)
                return true;
        }

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

// --- Update ---
void LevelEditorScene::Update(float dt) {
    // Refresh the cached gamepad (picks up hot-plugged pads each frame).
    {
        int count = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&count);
        mEditorPad = (ids && count > 0) ? SDL_GetGamepadFromID(ids[0]) : nullptr;
        if (ids) SDL_free(ids);
    }

    // ----------------------------------------------------------------
    //  Toolbar focus — LS X navigates buttons on the tool row.
    //  Sentinels: visible.size()   = palette collapse tab
    //             visible.size()+1 = toolbar collapse tab
    //  D-pad up/down cycles between all three rows (HandleEvent).
    // ----------------------------------------------------------------
    if (mEditorPad && mPadToolbarActive) {
        constexpr float TB_DEAD        = 0.18f;
        constexpr float TB_FIRST_DELAY = 0.28f;
        constexpr float TB_REPEAT_RATE = 0.09f;

        float lxT = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        float lyT = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
        if (std::abs(lxT) < TB_DEAD) lxT = 0.f;
        if (std::abs(lyT) < TB_DEAD) lyT = 0.f;

        auto visible = VisibleToolbarButtons();
        bool onTool  = (mPadToolbarBtnIdx < (int)visible.size());

        // LS X: key-repeat left/right navigation — only on the tool button row.
        // (D-pad up/down handles sentinel transitions in HandleEvent.)
        if (onTool) {
            if (lxT == 0.f) {
                mPadToolbarNavTimer  = 0.f;
                mPadToolbarNavRepeat = false;
            } else {
                mPadToolbarNavTimer -= dt;
                if (mPadToolbarNavTimer <= 0.f) {
                    if (!visible.empty()) {
                        int dir = (lxT > 0.f) ? 1 : -1;
                        mPadToolbarBtnIdx = std::clamp(
                            mPadToolbarBtnIdx + dir, 0, (int)visible.size() - 1);
                    }
                    mPadToolbarNavTimer  = mPadToolbarNavRepeat ? TB_REPEAT_RATE : TB_FIRST_DELAY;
                    mPadToolbarNavRepeat = true;
                }
            }
        }
        (void)lyT; // reserved — D-pad up/down handles sentinel transitions
        return; // toolbar has focus — don't run tile-tool logic
    }

    // ----------------------------------------------------------------
    //  PowerUp tool — cursor pan + picker nav + right-stick fire rate
    // ----------------------------------------------------------------
    if (mActiveToolId == ToolId::PowerUp && mEditorPad && !mPadToolbarActive) {
        constexpr float PU_DEAD    = 0.18f;
        constexpr float PU_PAN_SPD = 380.f;

        float lx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        float ly = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
        if (std::abs(lx) < PU_DEAD) lx = 0.f;
        if (std::abs(ly) < PU_DEAD) ly = 0.f;

        // Cursor is always pinned to canvas centre.
        int puH = mWindow ? mWindow->GetHeight() : 600;
        mPadCursorX = CanvasW() * 0.5f;
        mPadCursorY = (TOOLBAR_H + puH) * 0.5f;

        if (mPopups.powerUpOpen) {
            // ---- PICKER MODE: left-stick Y navigates the list ----
            if (std::abs(ly) < PU_DEAD) {
                mPadPuPickerNavTimer  = 0.f;
                mPadPuPickerNavRepeat = false;
            } else {
                constexpr float FIRST_DELAY = 0.28f;
                constexpr float REPEAT_RATE = 0.09f;
                mPadPuPickerNavTimer -= dt;
                if (mPadPuPickerNavTimer <= 0.f) {
                    int dir   = (ly > 0.f) ? 1 : -1;
                    int total = (int)(GetPowerUpRegistry().size() + 1);
                    mPadPuPickerIdx = std::clamp(mPadPuPickerIdx + dir, 0, total - 1);
                    mPadPuPickerNavTimer  = mPadPuPickerNavRepeat ? REPEAT_RATE : FIRST_DELAY;
                    mPadPuPickerNavRepeat = true;
                }
            }
        } else {
            // ---- CURSOR MODE: left stick pans camera ----
            if (lx != 0.f || ly != 0.f) {
                float z    = mCamera.Zoom();
                float newX = std::max(0.f, mCamera.X() + lx * PU_PAN_SPD * dt / z);
                float newY = std::max(0.f, mCamera.Y() + ly * PU_PAN_SPD * dt / z);
                mCamera.SetPosition(newX, newY);
            }

            // Right stick — up/left decreases fire rate, down/right increases.
            constexpr float RATE_SPD  = 8.0f;   // shots/sec per sec at full deflection
            constexpr float RATE_DEAD = 0.08f;
            float rx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.f;
            float ry = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.f;
            if (std::abs(rx) < RATE_DEAD) rx = 0.f;
            if (std::abs(ry) < RATE_DEAD) ry = 0.f;
            float rateInput = (rx + ry) * 0.5f; // down-right = positive = faster

            int hovTi = HitTile((int)mPadCursorX, (int)mPadCursorY);
            if (rateInput != 0.f && hovTi >= 0) {
                if (mLevel.tiles[hovTi].HasShooter()) {
                    auto& sd  = *mLevel.tiles[hovTi].shooter;
                    sd.fireRate = std::clamp(sd.fireRate + rateInput * RATE_SPD * dt,
                                             0.5f, 20.0f);
                    char buf[64];
                    std::snprintf(buf, sizeof(buf),
                                  "Turret fire rate: %.1f shots/sec", sd.fireRate);
                    SetStatus(buf);
                } else if (mLevel.tiles[hovTi].HasPowerUp()) {
                    auto& pu = *mLevel.tiles[hovTi].powerUp;
                    if (pu.type == "turret") {
                        pu.fireRate = std::clamp(pu.fireRate + rateInput * RATE_SPD * dt,
                                                  0.5f, 20.0f);
                        char buf[64];
                        std::snprintf(buf, sizeof(buf),
                                      "Turret PU fire rate: %.1f shots/sec", pu.fireRate);
                        SetStatus(buf);
                    } else if (pu.type == "healthboost") {
                        // Health boost uses discrete 25% steps — accumulate.
                        mPadPuRateAccum += rateInput * RATE_SPD * dt;
                        int steps = (int)mPadPuRateAccum;
                        if (steps != 0) {
                            mPadPuRateAccum -= (float)steps;
                            pu.healthPct = std::clamp(
                                pu.healthPct + steps * 25.0f, 25.0f, 200.0f);
                            SetStatus("Health Boost: +" +
                                      std::to_string((int)pu.healthPct) + "% max HP");
                        }
                    }
                }
            } else if (rateInput == 0.f) {
                mPadPuRateAccum = 0.f;
            }
        }
        return; // PowerUp tool handled — don't fall through to tile logic
    }

    // ----------------------------------------------------------------
    //  Prop / Ladder tools — shared cursor pan block
    // ----------------------------------------------------------------
    if ((mActiveToolId == ToolId::Prop || mActiveToolId == ToolId::Ladder) &&
        mTool && mEditorPad && !mPadToolbarActive) {

        constexpr float PL_DEAD    = 0.18f;
        constexpr float PL_PAN_SPD = 380.f;

        float lx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        float ly = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
        if (std::abs(lx) < PL_DEAD) lx = 0.f;
        if (std::abs(ly) < PL_DEAD) ly = 0.f;

        int plH = mWindow ? mWindow->GetHeight() : 600;
        mPadCursorX = CanvasW() * 0.5f;
        mPadCursorY = (TOOLBAR_H + plH) * 0.5f;

        // Prop: while popup is open, left stick X toggles Front/Back focus (debounced).
        if (mActiveToolId == ToolId::Prop) {
            auto* pt = dynamic_cast<PropTool*>(mTool.get());
            if (pt && pt->popupOpen) {
                constexpr float TOGGLE_DELAY = 0.35f;
                mPadPropNavTimer -= dt;
                if (std::abs(lx) > PL_DEAD && mPadPropNavTimer <= 0.f) {
                    mPadPropFocusFront = (lx < 0.f); // left = Front, right = Back
                    mPadPropNavTimer   = TOGGLE_DELAY;
                    SetStatus(std::string("Prop focus: ") +
                              (mPadPropFocusFront ? "FRONT" : "BACK") +
                              "  A=apply  B=cancel");
                } else if (std::abs(lx) <= PL_DEAD) {
                    mPadPropNavTimer = 0.f; // reset when stick released
                }
                return; // don't pan camera while popup is open
            }
        }

        // Camera pan
        if (lx != 0.f || ly != 0.f) {
            float z    = mCamera.Zoom();
            float newX = std::max(0.f, mCamera.X() + lx * PL_PAN_SPD * dt / z);
            float newY = std::max(0.f, mCamera.Y() + ly * PL_PAN_SPD * dt / z);
            mCamera.SetPosition(newX, newY);
        }
        return;
    }

    // ----------------------------------------------------------------
    //  Pan (MoveCam) tool — left stick pans, right stick zooms
    // ----------------------------------------------------------------
    if (mActiveToolId == ToolId::MoveCam && mEditorPad && !mPadToolbarActive) {
        constexpr float PAN_DEAD    = 0.12f;
        constexpr float ZOOM_DEAD   = 0.12f;
        constexpr float PAN_SPD     = 420.f;  // world units / sec at full deflection
        // ZOOM_RATE: zoom-wheel equivalents per second at full deflection.
        // ApplyZoom multiplies by ZOOM_STEP (0.1), so 10.0 → 1.0 zoom unit/sec.
        constexpr float ZOOM_RATE   = 10.0f;

        float lx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        float ly = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
        if (std::abs(lx) < PAN_DEAD) lx = 0.f;
        if (std::abs(ly) < PAN_DEAD) ly = 0.f;

        float ry = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.f;
        if (std::abs(ry) < ZOOM_DEAD) ry = 0.f;

        // Left stick: pan
        if (lx != 0.f || ly != 0.f) {
            float z    = mCamera.Zoom();
            float newX = std::max(0.f, mCamera.X() + lx * PAN_SPD * dt / z);
            float newY = std::max(0.f, mCamera.Y() + ly * PAN_SPD * dt / z);
            mCamera.SetPosition(newX, newY);
        }

        // Right stick Y: zoom (up = zoom in, down = zoom out), anchored to canvas centre.
        if (ry != 0.f) {
            int panH   = mWindow ? mWindow->GetHeight() : 600;
            int anchorX = CanvasW() / 2;
            int anchorY = (TOOLBAR_H + panH) / 2;
            // Negative ry (stick up) = positive wheelY = zoom in.
            float wheelEq = -ry * ZOOM_RATE * dt;
            if (mCamera.ApplyZoom(wheelEq, anchorX, anchorY))
                SetStatus("Zoom: " + std::to_string(mCamera.ZoomPercent()) + "%");
        }
        return;
    }

    // ----------------------------------------------------------------
    //  Action tool — crosshair pinned to centre; A applies/opens picker;
    //  right stick Y adjusts hitsRequired on hovered action tile.
    // ----------------------------------------------------------------
    if (mActiveToolId == ToolId::Action && mEditorPad && !mPadToolbarActive) {
        constexpr float PAN_DEAD  = 0.12f;
        constexpr float PAN_SPD   = 400.f;
        constexpr float HITS_DEAD = 0.15f;
        constexpr float HITS_SPD  = 4.0f;  // hits per second at full deflection

        int acH = mWindow ? mWindow->GetHeight() : 600;

        // Cursor pinned to canvas centre.
        mPadCursorX = CanvasW() * 0.5f;
        mPadCursorY = (TOOLBAR_H + acH) * 0.5f;

        float lx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        float ly = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
        if (std::abs(lx) < PAN_DEAD) lx = 0.f;
        if (std::abs(ly) < PAN_DEAD) ly = 0.f;

        // Left stick: key-repeat nav through picker entries when picker is open;
        // otherwise pan the camera.
        if (mPopups.animPickerTile >= 0) {
            // ---- PICKER MODE: left-stick Y navigates entries ----
            constexpr float FIRST_DELAY = 0.28f;
            constexpr float REPEAT_RATE = 0.09f;
            int dir = 0;
            if      (ly >  PAN_DEAD) dir =  1;
            else if (ly < -PAN_DEAD) dir = -1;

            if (dir == 0) {
                mPadActionPickerTimer  = 0.f;
                mPadActionPickerRepeat = false;
            } else {
                mPadActionPickerTimer -= dt;
                if (mPadActionPickerTimer <= 0.f) {
                    int n = (int)mPopups.animPickerEntries.size();
                    if (n > 0) {
                        mPadActionPickerIdx = std::clamp(mPadActionPickerIdx + dir, 0, n - 1);
                        SetStatus("Death anim: " +
                                  mPopups.animPickerEntries[mPadActionPickerIdx].name);
                    }
                    mPadActionPickerTimer  = mPadActionPickerRepeat ? REPEAT_RATE : FIRST_DELAY;
                    mPadActionPickerRepeat = true;
                }
            }
        } else {
            // ---- CURSOR MODE: pan camera ----
            if (lx != 0.f || ly != 0.f) {
                float z    = mCamera.Zoom();
                float newX = std::max(0.f, mCamera.X() + lx * PAN_SPD * dt / z);
                float newY = std::max(0.f, mCamera.Y() + ly * PAN_SPD * dt / z);
                mCamera.SetPosition(newX, newY);
            }

            // Right stick Y: adjust hitsRequired on hovered action tile.
            float ry = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.f;
            if (std::abs(ry) < HITS_DEAD) ry = 0.f;
            if (ry != 0.f) {
                int hovTi = HitTile((int)mPadCursorX, (int)mPadCursorY);
                if (hovTi >= 0 && mLevel.tiles[hovTi].HasAction()) {
                    // Positive ry = stick down = fewer hits; negative ry = up = more.
                    mPadActionHitsAccum -= ry * HITS_SPD * dt;
                    int steps = (int)mPadActionHitsAccum;
                    if (steps != 0) {
                        mPadActionHitsAccum -= (float)steps;
                        int& hits = mLevel.tiles[hovTi].action->hitsRequired;
                        hits = std::clamp(hits + steps, 1, 99);
                        SetStatus("Action tile hits: " + std::to_string(hits));
                    }
                } else {
                    mPadActionHitsAccum = 0.f; // reset when not over an action tile
                }
            }
        }
        return;
    }

    // ----------------------------------------------------------------
    //  Slope tool — crosshair pinned to centre; LS pans camera;
    //  A (via HandleEvent) cycles slope type; RS Y adjusts heightFrac.
    // ----------------------------------------------------------------
    if (mActiveToolId == ToolId::Slope && mTool && mEditorPad && !mPadToolbarActive) {
        constexpr float PAN_DEAD   = 0.12f;
        constexpr float PAN_SPD    = 400.f;
        constexpr float SLOPE_DEAD = 0.15f;
        constexpr float SLOPE_SPD  = 5.0f;  // steps/sec at full deflection (1 step = 0.05)

        int slH = mWindow ? mWindow->GetHeight() : 600;
        mPadCursorX = CanvasW() * 0.5f;
        mPadCursorY = (TOOLBAR_H + slH) * 0.5f;

        float lx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        float ly = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
        if (std::abs(lx) < PAN_DEAD) lx = 0.f;
        if (std::abs(ly) < PAN_DEAD) ly = 0.f;
        if (lx != 0.f || ly != 0.f) {
            float z    = mCamera.Zoom();
            float newX = std::max(0.f, mCamera.X() + lx * PAN_SPD * dt / z);
            float newY = std::max(0.f, mCamera.Y() + ly * PAN_SPD * dt / z);
            mCamera.SetPosition(newX, newY);
        }

        // RS Y: adjust heightFrac on the hovered slope tile.
        // Negative ry (stick up) = increase height; positive (stick down) = decrease.
        float ry = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.f;
        if (std::abs(ry) < SLOPE_DEAD) ry = 0.f;
        if (ry != 0.f) {
            int hovTi = HitTile((int)mPadCursorX, (int)mPadCursorY);
            if (hovTi >= 0 && mLevel.tiles[hovTi].HasSlope()) {
                mPadSlopeHeightAccum -= ry * SLOPE_SPD * dt;
                int steps = static_cast<int>(mPadSlopeHeightAccum);
                if (steps != 0) {
                    mPadSlopeHeightAccum -= static_cast<float>(steps);
                    float& frac = mLevel.tiles[hovTi].slope->heightFrac;
                    frac = std::clamp(frac + steps * 0.05f, 0.05f, 1.0f);
                    frac = std::round(frac * 20.0f) / 20.0f;
                    SetStatus("Slope height: " + std::to_string((int)(frac * 100)) +
                              "%  (RS up/down to adjust)");
                }
            } else {
                mPadSlopeHeightAccum = 0.f;
            }
        }
        return;
    }

    // ----------------------------------------------------------------
    //  Hitbox tool — three-mode state machine:
    //    Mode 0 (cursor)      : LS pans camera, A selects tile.
    //    Mode 1 (node-select) : LS X key-repeat cycles handles, A edits.
    //    Mode 2 (node-edit)   : LS adjusts selected handle in world space.
    // ----------------------------------------------------------------
    if (mActiveToolId == ToolId::Hitbox && mTool && mEditorPad && !mPadToolbarActive) {
        constexpr float PAN_DEAD    = 0.12f;
        constexpr float PAN_SPD     = 400.f;
        constexpr float NAV_DEAD    = 0.40f;
        constexpr float FIRST_DELAY = 0.28f;
        constexpr float REPEAT_RATE = 0.09f;
        constexpr float EDIT_SPD    = 35.f;   // world units/sec at full deflection

        static constexpr HitboxTool::Handle kHbHandles[] = {
            HitboxTool::Handle::TopLeft,  HitboxTool::Handle::Top,
            HitboxTool::Handle::TopRight, HitboxTool::Handle::Left,
            HitboxTool::Handle::Right,    HitboxTool::Handle::BotLeft,
            HitboxTool::Handle::Bottom,   HitboxTool::Handle::BotRight
        };
        static const char* kHbNames[] = {
            "Top-Left","Top","Top-Right","Left","Right","Bot-Left","Bottom","Bot-Right"
        };

        float lx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        float ly = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;

        if (mPadHbMode == 0) {
            // Cursor pinned to canvas centre; LS pans camera.
            int hbWinH = mWindow ? mWindow->GetHeight() : 600;
            mPadCursorX = CanvasW() * 0.5f;
            mPadCursorY = (TOOLBAR_H + hbWinH) * 0.5f;
            if (std::abs(lx) < PAN_DEAD) lx = 0.f;
            if (std::abs(ly) < PAN_DEAD) ly = 0.f;
            if (lx != 0.f || ly != 0.f) {
                float z    = mCamera.Zoom();
                float newX = std::max(0.f, mCamera.X() + lx * PAN_SPD * dt / z);
                float newY = std::max(0.f, mCamera.Y() + ly * PAN_SPD * dt / z);
                mCamera.SetPosition(newX, newY);
            }
        } else if (mPadHbMode == 1) {
            // LS: 2D grid navigation with key-repeat.
            // kNav[handle][0=up, 1=down, 2=left, 3=right], -1 = no neighbour
            static constexpr int kNav[8][4] = {
                {  -1,   3,   -1,   1 },  // 0 TopLeft
                {  -1,   6,    0,   2 },  // 1 Top
                {  -1,   4,    1,  -1 },  // 2 TopRight
                {   0,   5,   -1,   4 },  // 3 Left
                {   2,   7,    3,  -1 },  // 4 Right
                {   3,  -1,   -1,   6 },  // 5 BotLeft
                {   1,  -1,    5,   7 },  // 6 Bottom
                {   4,  -1,    6,  -1 },  // 7 BotRight
            };
            // Pick dominant axis, then map to up/down/left/right column index.
            int navDir = -1;
            float ax = std::abs(lx), ay = std::abs(ly);
            if (ay > NAV_DEAD && ay >= ax)      navDir = (ly < 0) ? 0 : 1;  // up / down
            else if (ax > NAV_DEAD && ax > ay)  navDir = (lx < 0) ? 2 : 3;  // left / right

            if (navDir < 0) {
                mPadHbNavTimer  = 0.f;
                mPadHbNavRepeat = false;
            } else {
                mPadHbNavTimer -= dt;
                if (mPadHbNavTimer <= 0.f) {
                    int next = kNav[mPadHbHandleIdx][navDir];
                    if (next >= 0) {
                        mPadHbHandleIdx = next;
                        auto* hbt = dynamic_cast<HitboxTool*>(mTool.get());
                        if (hbt) hbt->SetGamepadFocus(hbt->GetTileIdx(), kHbHandles[mPadHbHandleIdx]);
                        SetStatus(std::string("Hitbox: handle ") + kHbNames[mPadHbHandleIdx] +
                                  "  A=edit  B=back to cursor");
                    }
                    mPadHbNavTimer  = mPadHbNavRepeat ? REPEAT_RATE : FIRST_DELAY;
                    mPadHbNavRepeat = true;
                }
            }
        } else {
            // Mode 2 — LS adjusts the focused handle's edges in world units.
            if (std::abs(lx) < PAN_DEAD) lx = 0.f;
            if (std::abs(ly) < PAN_DEAD) ly = 0.f;
            if (lx != 0.f || ly != 0.f) {
                mPadHbEditAccumX += lx * EDIT_SPD * dt;
                mPadHbEditAccumY += ly * EDIT_SPD * dt;
                int dx = static_cast<int>(mPadHbEditAccumX);
                int dy = static_cast<int>(mPadHbEditAccumY);
                if (dx != 0 || dy != 0) {
                    mPadHbEditAccumX -= static_cast<float>(dx);
                    mPadHbEditAccumY -= static_cast<float>(dy);
                    auto* hbt = dynamic_cast<HitboxTool*>(mTool.get());
                    int ti = hbt ? hbt->GetTileIdx() : -1;
                    if (ti >= 0 && ti < (int)mLevel.tiles.size() &&
                        mLevel.tiles[ti].HasHitbox()) {
                        auto& t = mLevel.tiles[ti];
                        constexpr int MIN_SIDE = 4;
                        switch (kHbHandles[mPadHbHandleIdx]) {
                            case HitboxTool::Handle::Left:
                                t.hitbox->offX = std::max(0, t.hitbox->offX + dx);
                                t.hitbox->w    = std::max(MIN_SIDE, t.hitbox->w - dx);
                                break;
                            case HitboxTool::Handle::Right:
                                t.hitbox->w = std::clamp(t.hitbox->w + dx,
                                                         MIN_SIDE, t.w - t.hitbox->offX);
                                break;
                            case HitboxTool::Handle::Top:
                                t.hitbox->offY = std::max(0, t.hitbox->offY + dy);
                                t.hitbox->h    = std::max(MIN_SIDE, t.hitbox->h - dy);
                                break;
                            case HitboxTool::Handle::Bottom:
                                t.hitbox->h = std::clamp(t.hitbox->h + dy,
                                                         MIN_SIDE, t.h - t.hitbox->offY);
                                break;
                            case HitboxTool::Handle::TopLeft:
                                t.hitbox->offX = std::max(0, t.hitbox->offX + dx);
                                t.hitbox->w    = std::max(MIN_SIDE, t.hitbox->w - dx);
                                t.hitbox->offY = std::max(0, t.hitbox->offY + dy);
                                t.hitbox->h    = std::max(MIN_SIDE, t.hitbox->h - dy);
                                break;
                            case HitboxTool::Handle::TopRight:
                                t.hitbox->w    = std::max(MIN_SIDE, t.hitbox->w + dx);
                                t.hitbox->offY = std::max(0, t.hitbox->offY + dy);
                                t.hitbox->h    = std::max(MIN_SIDE, t.hitbox->h - dy);
                                break;
                            case HitboxTool::Handle::BotLeft:
                                t.hitbox->offX = std::max(0, t.hitbox->offX + dx);
                                t.hitbox->w    = std::max(MIN_SIDE, t.hitbox->w - dx);
                                t.hitbox->h    = std::max(MIN_SIDE, t.hitbox->h + dy);
                                break;
                            case HitboxTool::Handle::BotRight:
                                t.hitbox->w = std::max(MIN_SIDE, t.hitbox->w + dx);
                                t.hitbox->h = std::max(MIN_SIDE, t.hitbox->h + dy);
                                break;
                            default: break;
                        }
                        // Clamp offsets so they stay within the tile.
                        t.hitbox->offX = std::max(0, std::min(t.hitbox->offX, t.w - MIN_SIDE));
                        t.hitbox->offY = std::max(0, std::min(t.hitbox->offY, t.h - MIN_SIDE));
                        SetStatus("Hitbox: off(" + std::to_string(t.hitbox->offX) + "," +
                                  std::to_string(t.hitbox->offY) + ") size(" +
                                  std::to_string(t.hitbox->w) + "x" +
                                  std::to_string(t.hitbox->h) + ")");
                    }
                }
            }
        }
        return;
    }

    // ----------------------------------------------------------------
    //  Float (AntiGrav) tool — green crosshair pinned to centre;
    //  LS pans camera; A toggles anti-gravity on hovered tile/enemy.
    // ----------------------------------------------------------------
    if (mActiveToolId == ToolId::AntiGrav && mTool && mEditorPad && !mPadToolbarActive) {
        constexpr float PAN_DEAD = 0.12f;
        constexpr float PAN_SPD  = 400.f;

        int agH = mWindow ? mWindow->GetHeight() : 600;
        mPadCursorX = CanvasW() * 0.5f;
        mPadCursorY = (TOOLBAR_H + agH) * 0.5f;

        float lx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        float ly = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
        if (std::abs(lx) < PAN_DEAD) lx = 0.f;
        if (std::abs(ly) < PAN_DEAD) ly = 0.f;
        if (lx != 0.f || ly != 0.f) {
            float z    = mCamera.Zoom();
            float newX = std::max(0.f, mCamera.X() + lx * PAN_SPD * dt / z);
            float newY = std::max(0.f, mCamera.Y() + ly * PAN_SPD * dt / z);
            mCamera.SetPosition(newX, newY);
        }
        return;
    }

    // ----------------------------------------------------------------
    //  Shield tool — purple crosshair pinned to centre;
    //  LS pans camera; A toggles shield pickup on hovered tile.
    // ----------------------------------------------------------------
    if (mActiveToolId == ToolId::Shield && mTool && mEditorPad && !mPadToolbarActive) {
        constexpr float PAN_DEAD = 0.12f;
        constexpr float PAN_SPD  = 400.f;

        int shH = mWindow ? mWindow->GetHeight() : 600;
        mPadCursorX = CanvasW() * 0.5f;
        mPadCursorY = (TOOLBAR_H + shH) * 0.5f;

        float lx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        float ly = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
        if (std::abs(lx) < PAN_DEAD) lx = 0.f;
        if (std::abs(ly) < PAN_DEAD) ly = 0.f;
        if (lx != 0.f || ly != 0.f) {
            float z    = mCamera.Zoom();
            float newX = std::max(0.f, mCamera.X() + lx * PAN_SPD * dt / z);
            float newY = std::max(0.f, mCamera.Y() + ly * PAN_SPD * dt / z);
            mCamera.SetPosition(newX, newY);
        }
        return;
    }

    // ----------------------------------------------------------------
    // ----------------------------------------------------------------
    //  Select tool — white crosshair pinned to centre; LS pans camera.
    //  Hold A = rubber-band box OR drag selection (SelectTool decides).
    //  B = delete selected entities.
    // ----------------------------------------------------------------
    if (mActiveToolId == ToolId::Select && mTool && mEditorPad && !mPadToolbarActive) {
        constexpr float PAN_DEAD = 0.12f;
        constexpr float PAN_SPD  = 400.f;

        int selH = mWindow ? mWindow->GetHeight() : 600;

        // Cursor always pinned to canvas centre.
        mPadCursorX = CanvasW() * 0.5f;
        mPadCursorY = (TOOLBAR_H + selH) * 0.5f;

        float lx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        float ly = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
        if (std::abs(lx) < PAN_DEAD) lx = 0.f;
        if (std::abs(ly) < PAN_DEAD) ly = 0.f;

        // LS always pans the camera (cursor world position shifts, driving the
        // rubber-band corner or drag offset automatically via OnMouseMove below).
        if (lx != 0.f || ly != 0.f) {
            float z    = mCamera.Zoom();
            float newX = std::max(0.f, mCamera.X() + lx * PAN_SPD * dt / z);
            float newY = std::max(0.f, mCamera.Y() + ly * PAN_SPD * dt / z);
            mCamera.SetPosition(newX, newY);
        }

        // Drive the SelectTool's move logic every frame while A is held
        // (both rubber-band extension and entity drag work this way).
        if (mPadSelAHeld) {
            auto ctx = MakeToolCtx();
            mTool->OnMouseMove(ctx, (int)mPadCursorX, (int)mPadCursorY);
        }
        return;
    }

    //  Erase tool — crosshair pinned to centre; A erases; hold A + LS
    //  drags continuously (camera pans under the fixed cursor).
    // ----------------------------------------------------------------
    if (mActiveToolId == ToolId::Erase && mTool && mEditorPad && !mPadToolbarActive) {
        constexpr float PAN_DEAD = 0.12f;
        constexpr float PAN_SPD  = 400.f;

        int erH = mWindow ? mWindow->GetHeight() : 600;

        // Cursor always pinned to canvas centre.
        mPadCursorX = CanvasW() * 0.5f;
        mPadCursorY = (TOOLBAR_H + erH) * 0.5f;

        float lx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
        float ly = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
        if (std::abs(lx) < PAN_DEAD) lx = 0.f;
        if (std::abs(ly) < PAN_DEAD) ly = 0.f;

        // Left stick always pans the camera.
        if (lx != 0.f || ly != 0.f) {
            float z    = mCamera.Zoom();
            float newX = std::max(0.f, mCamera.X() + lx * PAN_SPD * dt / z);
            float newY = std::max(0.f, mCamera.Y() + ly * PAN_SPD * dt / z);
            mCamera.SetPosition(newX, newY);
        }

        // While A is held and the stick is moving, erase whatever is now
        // under the (world-scrolled) cursor each frame.
        if (mPadEraseAHeld && (lx != 0.f || ly != 0.f)) {
            auto ctx = MakeToolCtx();
            mTool->OnMouseDown(ctx, (int)mPadCursorX, (int)mPadCursorY,
                               SDL_BUTTON_LEFT, SDL_GetModState());
        }
        return;
    }

    // Only drive the gamepad tile-tool logic when the tile tool is active.
    if (mActiveToolId != ToolId::Tile || !mTool || !mEditorPad)
        return;

    constexpr float DEAD    = 0.18f;
    constexpr float PAN_SPD = 380.f;   // world units/sec for right-stick camera pan

    int W = mWindow->GetWidth();
    int H = mWindow->GetHeight();

    float lx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTX) / 32767.f;
    float ly = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_LEFTY) / 32767.f;
    if (std::abs(lx) < DEAD) lx = 0.f;
    if (std::abs(ly) < DEAD) ly = 0.f;

    // ================================================================
    //  PALETTE MODE — left stick navigates the tile list
    // ================================================================
    if (mPadPaletteActive) {
        // Cardinal-only: pick the dominant axis; diagonals never fire both.
        float ax = std::abs(lx), ay = std::abs(ly);
        int dirX = 0, dirY = 0;
        if (ax > DEAD || ay > DEAD) {
            if (ay >= ax) dirY = (ly > 0.f ? 1 : -1);
            else          dirX = (lx > 0.f ? 1 : -1);
        }
        bool anyNav = (dirX != 0 || dirY != 0);

        if (!anyNav) {
            mPadNavTimer     = 0.f;
            mPadNavRepeating = false;
        } else {
            constexpr float FIRST_DELAY = 0.28f;
            constexpr float REPEAT_RATE = 0.09f;
            mPadNavTimer -= dt;
            if (mPadNavTimer <= 0.f) {
                int maxIdx = std::max(0, (int)mPalette.Items().size() - 1);
                int delta  = dirY * PAL_COLS + dirX;
                int newIdx = std::clamp(mPadPaletteIdx + delta, 0, maxIdx);
                mPadPaletteIdx = newIdx;
                mPalette.SetSelectedTile(newIdx);
                mPadNavTimer     = mPadNavRepeating ? REPEAT_RATE : FIRST_DELAY;
                mPadNavRepeating = true;
            }
        }

        // Keep highlighted item scrolled into view.
        {
            constexpr int PAD   = 4, LBL_H = 14;
            const int cellW     = (PALETTE_W - PAD * (PAL_COLS + 1)) / PAL_COLS;
            const int itemH     = cellW + LBL_H + PAD;
            const int palYStart = TOOLBAR_H + TAB_H + 44;
            const int visRows   = std::max(1, (H - palYStart) / itemH);
            const int targetRow = mPadPaletteIdx / PAL_COLS;
            const int curScroll = mPalette.TileScroll();
            if (targetRow < curScroll)
                mPalette.SetTileScroll(targetRow);
            else if (targetRow >= curScroll + visRows)
                mPalette.SetTileScroll(targetRow - visRows + 1);
        }
        return; // palette has focus — don't move cursor
    }

    // ================================================================
    //  PLACEMENT MODE — left stick moves the tile ghost cursor;
    //                   right stick pans the camera.
    //                   Camera auto-follows the cursor to keep it centred.
    // ================================================================
    // Ghost is always pinned to canvas centre — the camera pans under it.
    // This means what the user sees as "moving the tile" is actually the
    // world scrolling; the ghost never drifts to an edge.
    mPadCursorX = CanvasW() * 0.5f;
    mPadCursorY = (TOOLBAR_H + H) * 0.5f;

    // Left stick pans the camera (world moves under the centred ghost).
    if (lx != 0.f || ly != 0.f) {
        float z    = mCamera.Zoom();
        float newX = std::max(0.f, mCamera.X() + lx * PAN_SPD * dt / z);
        float newY = std::max(0.f, mCamera.Y() + ly * PAN_SPD * dt / z);
        mCamera.SetPosition(newX, newY);
    }

    // Right stick — down/right increases tile size, up/left decreases.
    // Both axes contribute so any direction in the down-right quadrant grows.
    constexpr float SIZE_SPD   = 14.0f; // grid steps/sec at full deflection
    constexpr float SIZE_DEAD  = 0.08f; // lower dead zone so light touches register
    float rx = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.f;
    float ry = SDL_GetGamepadAxis(mEditorPad, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.f;
    if (std::abs(rx) < SIZE_DEAD) rx = 0.f;
    if (std::abs(ry) < SIZE_DEAD) ry = 0.f;
    float sizeInput = (rx + ry) * 0.5f; // average; diagonal ≈ same rate as pure axis
    if (sizeInput != 0.f) {
        mPadSizeAccum += sizeInput * SIZE_SPD * dt;
        int steps = (int)mPadSizeAccum;
        if (steps != 0) {
            mPadSizeAccum -= steps;
            auto ctx = MakeToolCtx();
            mTool->OnScroll(ctx, (float)-steps,  // negate: down = grow (positive scroll)
                            (int)mPadCursorX, (int)mPadCursorY, SDL_GetModState());
        }
    } else {
        mPadSizeAccum = 0.f; // reset when stick released
    }

    // A held — continuous drag-placement as the cursor moves.
    if (mPadAHeld) {
        if (auto* tt = dynamic_cast<TileTool*>(mTool.get())) {
            const auto* selItem = mPalette.SelectedItem();
            tt->placementInfo   = selItem ? TilePlacementInfo{true,
                                                               selItem->isFolder,
                                                               selItem->path,
                                                               selItem->label}
                                          : TilePlacementInfo{};
        }
        auto ctx = MakeToolCtx();
        mTool->OnMouseMove(ctx, (int)mPadCursorX, (int)mPadCursorY);
    }
}

// --- Render ---
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

    std::vector<float> plxFactors;
    plxFactors.reserve(mLevel.parallaxLayers.size());
    for (const auto& pl : mLevel.parallaxLayers)
        plxFactors.push_back(pl.scrollFactor);

    // Placement mode: ghost follows the virtual stick cursor.
    // Palette mode:   ghost follows the mouse as normal (override disabled).
    if (mActiveToolId == ToolId::Tile && mEditorPad && !mPadPaletteActive) {
        mCanvasRenderer.padCursorOverrideX = mPadCursorX;
        mCanvasRenderer.padCursorOverrideY = mPadCursorY;
    } else {
        mCanvasRenderer.padCursorOverrideX = -1.f;
        mCanvasRenderer.padCursorOverrideY = -1.f;
    }

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

    mPopups.movPlatRect = mCanvasRenderer.MovPlatPopupRect();

    // Bridge EditorPopups::AnimPickerEntry -> EditorUIRenderer::AnimPickerEntry
    std::vector<EditorUIRenderer::AnimPickerEntry> uiPickerEntries;
    uiPickerEntries.reserve(mPopups.animPickerEntries.size());
    for (const auto& e : mPopups.animPickerEntries)
        uiPickerEntries.push_back({e.path, e.name, e.thumb});

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

    EditorUIRenderer::MusicConfirmState mcState;
    mcState.active  = mMusicConfirmActive;
    {
        namespace fs = std::filesystem;
        mcState.oldName = mLevel.musicPath.empty()
            ? "(none)" : fs::path(mLevel.musicPath).filename().string();
        mcState.newName = mMusicConfirmNewPath.empty()
            ? "" : fs::path(mMusicConfirmNewPath).filename().string();
    }

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
                       mcState,
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

    mPopups.delYes         = mUIRenderer.DelConfirmYesRect();
    mPopups.delNo          = mUIRenderer.DelConfirmNoRect();
    mMusicConfirmYes       = mUIRenderer.MusicConfirmYesRect();
    mMusicConfirmNo        = mUIRenderer.MusicConfirmNoRect();
    mPopups.animPickerRect       = mUIRenderer.AnimPickerRect();
    mPopups.camShakeToggleRect   = mUIRenderer.CamShakeToggleRect();

    if (!mLevel.musicPath.empty()) {
        auto* fmt = SDL_GetPixelFormatDetails(screen->format);
        auto mapC = [&](Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
            return SDL_MapRGBA(fmt, nullptr, r, g, b, a);
        };

        int sliderW = 120, sliderH = 12;
        int sliderX = cw - sliderW - 12;
        int sliderY = TOOLBAR_H - sliderH - 6;
        mMusicVolSlider = {sliderX, sliderY, sliderW, sliderH};

        int pct = (int)(mLevel.musicVolume * 100.0f + 0.5f);
        std::string musicName = fs::path(mLevel.musicPath).filename().string();
        if ((int)musicName.size() > 16) musicName = musicName.substr(0, 14) + "..";
        std::string label = musicName + " " + std::to_string(pct) + "%";
        SDL_Surface* badge = mSurfaceCache.GetBadge(label, {180, 200, 220, 255});
        if (badge) {
            SDL_Rect dst = {sliderX, sliderY - badge->h - 2, badge->w, badge->h};
            SDL_Rect bg  = {dst.x - 2, dst.y - 1, dst.w + 4, dst.h + 2};
            SDL_FillSurfaceRect(screen, &bg, mapC(18, 20, 30, 220));
            SDL_BlitSurface(badge, nullptr, screen, &dst);
        }

        SDL_FillSurfaceRect(screen, &mMusicVolSlider, mapC(20, 22, 36, 255));
        for (int side = 0; side < 4; ++side) {
            SDL_Rect edge{};
            if (side == 0) edge = {mMusicVolSlider.x, mMusicVolSlider.y, mMusicVolSlider.w, 1};
            if (side == 1) edge = {mMusicVolSlider.x, mMusicVolSlider.y + sliderH - 1, mMusicVolSlider.w, 1};
            if (side == 2) edge = {mMusicVolSlider.x, mMusicVolSlider.y, 1, sliderH};
            if (side == 3) edge = {mMusicVolSlider.x + sliderW - 1, mMusicVolSlider.y, 1, sliderH};
            SDL_FillSurfaceRect(screen, &edge, mapC(50, 55, 80, 255));
        }

        int fillW = (int)(mLevel.musicVolume * (float)(sliderW - 2));
        if (fillW > 0) {
            SDL_Rect filled = {sliderX + 1, sliderY + 1, fillW, sliderH - 2};
            SDL_FillSurfaceRect(screen, &filled, mapC(50, 120, 200, 255));
        }

        int knobX = sliderX + 1 + fillW;
        SDL_Rect knob = {knobX - 2, sliderY, 4, sliderH};
        SDL_FillSurfaceRect(screen, &knob, mapC(180, 200, 255, 255));
    }

    // LINEAR filtering: NEAREST would make blended TTF text blocky on HiDPI.
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, screen);
    SDL_DestroySurface(screen);
    if (tex) {
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
        SDL_RenderTexture(ren, tex, nullptr, nullptr);
        SDL_DestroyTexture(tex);
    }

    // --- Gamepad PowerUp tool overlay ---
    if (mActiveToolId == ToolId::PowerUp && mEditorPad && !mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        int cx = (int)mPadCursorX;
        int cy = (int)mPadCursorY;

        // --- Orange crosshair at cursor ---
        constexpr int CS = 12; // arm length
        SDL_SetRenderDrawColor(ren, 255, 120, 40, 230);
        SDL_RenderLine(ren, cx - CS, cy, cx + CS, cy);
        SDL_RenderLine(ren, cx, cy - CS, cx, cy + CS);
        SDL_SetRenderDrawColor(ren, 255, 180, 80, 255);
        SDL_FRect dot = {(float)(cx - 2), (float)(cy - 2), 5.f, 5.f};
        SDL_RenderFillRect(ren, &dot);

        // --- Highlight tile under cursor ---
        if (cy >= TOOLBAR_H && cx < CanvasW()) {
            int hovTi = HitTile(cx, cy);
            if (hovTi >= 0) {
                const auto& ht = mLevel.tiles[hovTi];
                auto [tsx, tsy] = WorldToScreen(ht.x, ht.y);
                float tsw = ht.w * mCamera.Zoom();
                float tsh = ht.h * mCamera.Zoom();

                // Warm fill + outline
                SDL_SetRenderDrawColor(ren, 255, 130, 50, 55);
                SDL_FRect fillR = {(float)tsx, (float)tsy, tsw, tsh};
                SDL_RenderFillRect(ren, &fillR);
                SDL_SetRenderDrawColor(ren, 255, 165, 60, 240);
                SDL_FRect outR = {(float)(tsx - 1), (float)(tsy - 1), tsw + 2, tsh + 2};
                SDL_RenderRect(ren, &outR);
            }
        }

        // --- Power-up picker: highlight focused entry ---
        if (mPopups.powerUpOpen) {
            const auto& reg     = GetPowerUpRegistry();
            const int   PAD     = 8;
            const int   ROW_H   = 28;
            const int   TITLE_H = 32;
            int         py      = mPopups.powerUpRect.y + TITLE_H;
            int         total   = (int)reg.size() + 1; // +1 for None
            int         idx     = std::min(mPadPuPickerIdx, total - 1);

            SDL_Rect row;
            if (idx < (int)reg.size()) {
                row = {mPopups.powerUpRect.x + PAD,
                       py + idx * (ROW_H + 2),
                       mPopups.powerUpRect.w - PAD * 2, ROW_H};
            } else {
                row = {mPopups.powerUpRect.x + PAD,
                       py + (int)reg.size() * (ROW_H + 2),
                       mPopups.powerUpRect.w - PAD * 2, ROW_H};
            }

            SDL_SetRenderDrawColor(ren, 255, 220, 50, 55);
            SDL_FRect selFill = {(float)row.x, (float)row.y,
                                 (float)row.w, (float)row.h};
            SDL_RenderFillRect(ren, &selFill);
            SDL_SetRenderDrawColor(ren, 255, 220, 50, 230);
            SDL_RenderRect(ren, &selFill);
        }

        // --- Hint bar ---
        const char* hint = mPopups.powerUpOpen
            ? "LS/D-pad=navigate  A=select  B=cancel"
            : "LS=move  A=assign power-up  RS=fire rate  SELECT=tools";
        Text puHint(hint, {255, 160, 60, 215}, 6, H - 18, 11);
        puHint.Render(ren);
    }

    // --- Gamepad Prop tool overlay ---
    if (mActiveToolId == ToolId::Prop && mTool && mEditorPad && !mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        int cx = (int)mPadCursorX;
        int cy = (int)mPadCursorY;

        // Cyan crosshair
        constexpr int CS = 12;
        SDL_SetRenderDrawColor(ren, 80, 180, 255, 230);
        SDL_RenderLine(ren, cx - CS, cy, cx + CS, cy);
        SDL_RenderLine(ren, cx, cy - CS, cx, cy + CS);
        SDL_SetRenderDrawColor(ren, 140, 220, 255, 255);
        SDL_FRect dot = {(float)(cx - 2), (float)(cy - 2), 5.f, 5.f};
        SDL_RenderFillRect(ren, &dot);

        // Tile hover highlight (cyan)
        if (cy >= TOOLBAR_H && cx < CanvasW()) {
            int hovTi = HitTile(cx, cy);
            if (hovTi >= 0) {
                const auto& ht = mLevel.tiles[hovTi];
                auto [tsx, tsy] = WorldToScreen(ht.x, ht.y);
                float tsw = ht.w * mCamera.Zoom();
                float tsh = ht.h * mCamera.Zoom();
                SDL_SetRenderDrawColor(ren, 80, 180, 255, 45);
                SDL_FRect fillR = {(float)tsx, (float)tsy, tsw, tsh};
                SDL_RenderFillRect(ren, &fillR);
                SDL_SetRenderDrawColor(ren, 100, 200, 255, 240);
                SDL_FRect outR = {(float)(tsx - 1), (float)(tsy - 1), tsw + 2.f, tsh + 2.f};
                SDL_RenderRect(ren, &outR);
            }
        }

        // Prop popup: highlight focused Front or Back button
        auto* pt = dynamic_cast<PropTool*>(mTool.get());
        if (pt && pt->popupOpen) {
            const SDL_Rect& focusR = mPadPropFocusFront ? pt->frontRect : pt->backRect;
            SDL_SetRenderDrawColor(ren, 255, 220, 50, 60);
            SDL_FRect selFill = {(float)focusR.x, (float)focusR.y,
                                 (float)focusR.w, (float)focusR.h};
            SDL_RenderFillRect(ren, &selFill);
            SDL_SetRenderDrawColor(ren, 255, 220, 50, 230);
            SDL_RenderRect(ren, &selFill);
        }

        // Hint bar
        const char* hint = (pt && pt->popupOpen)
            ? "LS/D-pad=Front\xe2\x86\x94" "Back  A=apply  B=cancel"
            : "LS=move  A=toggle prop  SELECT=tools";
        Text propHint(hint, {100, 200, 255, 215}, 6, H - 18, 11);
        propHint.Render(ren);
    }

    // --- Gamepad Ladder tool overlay ---
    if (mActiveToolId == ToolId::Ladder && mTool && mEditorPad && !mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        int cx = (int)mPadCursorX;
        int cy = (int)mPadCursorY;

        // Green crosshair
        constexpr int CS = 12;
        SDL_SetRenderDrawColor(ren, 60, 220, 100, 230);
        SDL_RenderLine(ren, cx - CS, cy, cx + CS, cy);
        SDL_RenderLine(ren, cx, cy - CS, cx, cy + CS);
        SDL_SetRenderDrawColor(ren, 120, 255, 140, 255);
        SDL_FRect dot = {(float)(cx - 2), (float)(cy - 2), 5.f, 5.f};
        SDL_RenderFillRect(ren, &dot);

        // Tile hover highlight (green)
        if (cy >= TOOLBAR_H && cx < CanvasW()) {
            int hovTi = HitTile(cx, cy);
            if (hovTi >= 0) {
                const auto& ht = mLevel.tiles[hovTi];
                auto [tsx, tsy] = WorldToScreen(ht.x, ht.y);
                float tsw = ht.w * mCamera.Zoom();
                float tsh = ht.h * mCamera.Zoom();
                SDL_SetRenderDrawColor(ren, 60, 220, 100, 45);
                SDL_FRect fillR = {(float)tsx, (float)tsy, tsw, tsh};
                SDL_RenderFillRect(ren, &fillR);
                SDL_SetRenderDrawColor(ren, 80, 240, 120, 240);
                SDL_FRect outR = {(float)(tsx - 1), (float)(tsy - 1), tsw + 2.f, tsh + 2.f};
                SDL_RenderRect(ren, &outR);
            }
        }

        // Hint bar
        Text ladderHint("LS=move  A=toggle ladder  SELECT=tools",
                        {100, 255, 140, 215}, 6, H - 18, 11);
        ladderHint.Render(ren);
    }

    // --- Gamepad Action tool overlay ---
    if (mActiveToolId == ToolId::Action && mEditorPad && !mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        int cx = (int)mPadCursorX;
        int cy = (int)mPadCursorY;

        // Orange crosshair (distinct from PowerUp's lighter orange)
        constexpr int CS = 12;
        SDL_SetRenderDrawColor(ren, 255, 130, 0, 230);
        SDL_RenderLine(ren, cx - CS, cy, cx + CS, cy);
        SDL_RenderLine(ren, cx, cy - CS, cx, cy + CS);
        SDL_SetRenderDrawColor(ren, 255, 200, 60, 255);
        SDL_FRect dot = {(float)(cx - 2), (float)(cy - 2), 5.f, 5.f};
        SDL_RenderFillRect(ren, &dot);

        // Tile hover highlight (orange)
        if (cy >= TOOLBAR_H && cx < CanvasW()) {
            int hovTi = HitTile(cx, cy);
            if (hovTi >= 0) {
                const auto& ht  = mLevel.tiles[hovTi];
                auto [tsx, tsy] = WorldToScreen(ht.x, ht.y);
                float tsw = ht.w * mCamera.Zoom();
                float tsh = ht.h * mCamera.Zoom();

                // Action tiles get a stronger tint; plain tiles get a subtle preview.
                bool hasAction = ht.HasAction();
                SDL_SetRenderDrawColor(ren, 255, 130, 0, hasAction ? 60 : 30);
                SDL_FRect fillR = {(float)tsx, (float)tsy, tsw, tsh};
                SDL_RenderFillRect(ren, &fillR);
                SDL_SetRenderDrawColor(ren, 255, 170, 40, 240);
                SDL_FRect outR = {(float)(tsx - 1), (float)(tsy - 1), tsw + 2.f, tsh + 2.f};
                SDL_RenderRect(ren, &outR);

                // Show hits badge on action tiles
                if (hasAction) {
                    std::string hitsStr = "x" + std::to_string(ht.action->hitsRequired);
                    Text hitsLbl(hitsStr, {255, 210, 80, 240},
                                 (int)tsx + 2, (int)tsy + 2, 10);
                    hitsLbl.Render(ren);
                }
            }
        }

        // Anim picker gamepad highlight — gold outline on focused entry
        if (mPopups.animPickerTile >= 0) {
            int n = (int)mPopups.animPickerEntries.size();
            if (n > 0) {
                constexpr int THUMB   = 48, PAD = 8, COL_W = THUMB + PAD * 2, COLS = 4;
                constexpr int TITLE_H = 28, ROW_H = THUMB + 10;
                int idx = std::clamp(mPadActionPickerIdx, 0, n - 1);
                int col = idx % COLS;
                int row = idx / COLS;
                int px  = mPopups.animPickerRect.x;
                int py  = mPopups.animPickerRect.y + TITLE_H;
                int ex  = px + PAD + col * COL_W;
                int ey  = py + PAD + row * (ROW_H + PAD);

                SDL_SetRenderDrawColor(ren, 255, 220, 50, 255);
                SDL_FRect sel1 = {(float)(ex - 2), (float)(ey - 2),
                                  (float)(COL_W - PAD + 4), (float)(ROW_H + 4)};
                SDL_RenderRect(ren, &sel1);
                SDL_SetRenderDrawColor(ren, 255, 180, 0, 140);
                SDL_FRect sel2 = {(float)(ex - 4), (float)(ey - 4),
                                  (float)(COL_W - PAD + 8), (float)(ROW_H + 8)};
                SDL_RenderRect(ren, &sel2);
            }
        }

        // Hint bar — changes based on picker state
        const char* actionHint = (mPopups.animPickerTile >= 0)
            ? "LS=navigate  A=apply anim  B=cancel"
            : "LS=move  A=toggle action / pick anim  RS up=more hits  RS down=fewer  SELECT=tools";
        Text acHint(actionHint, {255, 160, 40, 215}, 6, H - 18, 11);
        acHint.Render(ren);
    }

    // --- Gamepad Slope tool overlay ---
    if (mActiveToolId == ToolId::Slope && mTool && mEditorPad && !mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        int cx = (int)mPadCursorX;
        int cy = (int)mPadCursorY;

        // Yellow crosshair
        constexpr int CS = 12;
        SDL_SetRenderDrawColor(ren, 230, 210, 0, 230);
        SDL_RenderLine(ren, cx - CS, cy, cx + CS, cy);
        SDL_RenderLine(ren, cx, cy - CS, cx, cy + CS);
        SDL_SetRenderDrawColor(ren, 255, 240, 60, 255);
        SDL_FRect slDot = {(float)(cx - 2), (float)(cy - 2), 5.f, 5.f};
        SDL_RenderFillRect(ren, &slDot);

        // Tile hover highlight
        if (cy >= TOOLBAR_H && cx < CanvasW()) {
            int hovTi = HitTile(cx, cy);
            if (hovTi >= 0) {
                const auto& ht  = mLevel.tiles[hovTi];
                auto [tsx, tsy] = WorldToScreen(ht.x, ht.y);
                float tsw = ht.w * mCamera.Zoom();
                float tsh = ht.h * mCamera.Zoom();
                bool hasSlope = ht.HasSlope();
                SDL_SetRenderDrawColor(ren, 230, 210, 0, hasSlope ? 55 : 25);
                SDL_FRect slFill = {(float)tsx, (float)tsy, tsw, tsh};
                SDL_RenderFillRect(ren, &slFill);
                SDL_SetRenderDrawColor(ren, 255, 240, 60, 220);
                SDL_FRect slOut = {(float)(tsx - 1), (float)(tsy - 1), tsw + 2.f, tsh + 2.f};
                SDL_RenderRect(ren, &slOut);

                // Height % badge on slope tiles
                if (hasSlope) {
                    std::string pctStr = std::to_string((int)(ht.slope->heightFrac * 100)) + "%";
                    Text slPct(pctStr, {255, 240, 60, 240}, (int)tsx + 2, (int)tsy + 2, 10);
                    slPct.Render(ren);
                }
            }
        }

        // Hint bar
        Text slHint("LS=move  A=cycle slope type  RS up=more height  RS down=less  SELECT=tools",
                    {230, 210, 60, 215}, 6, H - 18, 11);
        slHint.Render(ren);
    }

    // --- Gamepad Hitbox tool overlay ---
    if (mActiveToolId == ToolId::Hitbox && mTool && mEditorPad && !mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        static const char* kHbNames[] = {
            "Top-Left","Top","Top-Right","Left","Right","Bot-Left","Bottom","Bot-Right"
        };

        if (mPadHbMode == 0) {
            // Cursor mode: cyan crosshair + hover highlight.
            int cx = (int)mPadCursorX;
            int cy = (int)mPadCursorY;
            constexpr int CS = 12;
            SDL_SetRenderDrawColor(ren, 60, 190, 255, 230);
            SDL_RenderLine(ren, cx - CS, cy, cx + CS, cy);
            SDL_RenderLine(ren, cx, cy - CS, cx, cy + CS);
            SDL_SetRenderDrawColor(ren, 160, 230, 255, 255);
            SDL_FRect hbDot = {(float)(cx - 2), (float)(cy - 2), 5.f, 5.f};
            SDL_RenderFillRect(ren, &hbDot);

            if (cy >= TOOLBAR_H && cx < CanvasW()) {
                int hovTi = HitTile(cx, cy);
                if (hovTi >= 0) {
                    const auto& ht  = mLevel.tiles[hovTi];
                    auto [tsx, tsy] = WorldToScreen(ht.x, ht.y);
                    float tsw = ht.w * mCamera.Zoom();
                    float tsh = ht.h * mCamera.Zoom();
                    SDL_SetRenderDrawColor(ren, 60, 190, 255, 35);
                    SDL_FRect hbFill = {(float)tsx, (float)tsy, tsw, tsh};
                    SDL_RenderFillRect(ren, &hbFill);
                    SDL_SetRenderDrawColor(ren, 80, 200, 255, 200);
                    SDL_FRect hbOut = {(float)(tsx - 1), (float)(tsy - 1), tsw + 2.f, tsh + 2.f};
                    SDL_RenderRect(ren, &hbOut);
                }
            }
            Text hbHint0("LS=move  A=select tile  SELECT=tools",
                         {80, 200, 255, 215}, 6, H - 18, 11);
            hbHint0.Render(ren);
        } else {
            // Modes 1 & 2: show a subtle tint over the focused tile.
            auto* hbt = dynamic_cast<HitboxTool*>(mTool.get());
            int ti = hbt ? hbt->GetTileIdx() : -1;
            if (ti >= 0 && ti < (int)mLevel.tiles.size()) {
                const auto& t   = mLevel.tiles[ti];
                auto [tsx, tsy] = WorldToScreen(t.x, t.y);
                float tsw = t.w * mCamera.Zoom();
                float tsh = t.h * mCamera.Zoom();
                // Mode 1: blue tint; mode 2: gold tint (editing)
                if (mPadHbMode == 1) {
                    SDL_SetRenderDrawColor(ren, 60, 190, 255, 20);
                } else {
                    SDL_SetRenderDrawColor(ren, 255, 215, 50, 25);
                }
                SDL_FRect hbTile = {(float)tsx, (float)tsy, tsw, tsh};
                SDL_RenderFillRect(ren, &hbTile);
            }

            if (mPadHbMode == 1) {
                std::string hbStr1 = std::string("Hitbox: [") + kHbNames[mPadHbHandleIdx] +
                    "]  LS/D-pad=navigate nodes  A=edit node  B=back to cursor  SELECT=tools";
                Text hbHint1(hbStr1, {80, 200, 255, 215}, 6, H - 18, 11);
                hbHint1.Render(ren);
            } else {
                std::string hbStr2 = std::string("Editing [") + kHbNames[mPadHbHandleIdx] +
                    "]  LS=adjust  A/B=done  SELECT=tools";
                Text hbHint2(hbStr2, {255, 220, 60, 215}, 6, H - 18, 11);
                hbHint2.Render(ren);
            }
        }
    }

    // --- Gamepad Float (AntiGrav) tool overlay ---
    if (mActiveToolId == ToolId::AntiGrav && mTool && mEditorPad && !mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        int cx = (int)mPadCursorX;
        int cy = (int)mPadCursorY;

        // Green crosshair
        constexpr int CS = 12;
        SDL_SetRenderDrawColor(ren, 60, 210, 100, 230);
        SDL_RenderLine(ren, cx - CS, cy, cx + CS, cy);
        SDL_RenderLine(ren, cx, cy - CS, cx, cy + CS);
        SDL_SetRenderDrawColor(ren, 120, 255, 160, 255);
        SDL_FRect agDot = {(float)(cx - 2), (float)(cy - 2), 5.f, 5.f};
        SDL_RenderFillRect(ren, &agDot);

        // Tile / enemy hover highlight
        if (cy >= TOOLBAR_H && cx < CanvasW()) {
            int hovTi = HitTile(cx, cy);
            if (hovTi >= 0) {
                const auto& ht  = mLevel.tiles[hovTi];
                auto [tsx, tsy] = WorldToScreen(ht.x, ht.y);
                float tsw = ht.w * mCamera.Zoom();
                float tsh = ht.h * mCamera.Zoom();
                bool hasAg = ht.antiGravity;
                SDL_SetRenderDrawColor(ren, 60, 210, 100, hasAg ? 60 : 25);
                SDL_FRect agFill = {(float)tsx, (float)tsy, tsw, tsh};
                SDL_RenderFillRect(ren, &agFill);
                SDL_SetRenderDrawColor(ren, 80, 240, 120, 220);
                SDL_FRect agOut = {(float)(tsx-1), (float)(tsy-1), tsw+2.f, tsh+2.f};
                SDL_RenderRect(ren, &agOut);
                if (hasAg) {
                    Text agLbl("float", {120, 255, 160, 240}, (int)tsx+2, (int)tsy+2, 10);
                    agLbl.Render(ren);
                }
            }
        }

        Text agHint("LS=move  A=toggle anti-gravity  SELECT=tools",
                    {80, 220, 120, 215}, 6, H - 18, 11);
        agHint.Render(ren);
    }

    // --- Gamepad Shield tool overlay ---
    if (mActiveToolId == ToolId::Shield && mTool && mEditorPad && !mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        int cx = (int)mPadCursorX;
        int cy = (int)mPadCursorY;

        // Purple crosshair
        constexpr int CS = 12;
        SDL_SetRenderDrawColor(ren, 190, 80, 255, 230);
        SDL_RenderLine(ren, cx - CS, cy, cx + CS, cy);
        SDL_RenderLine(ren, cx, cy - CS, cx, cy + CS);
        SDL_SetRenderDrawColor(ren, 220, 140, 255, 255);
        SDL_FRect shDot = {(float)(cx - 2), (float)(cy - 2), 5.f, 5.f};
        SDL_RenderFillRect(ren, &shDot);

        // Tile hover highlight
        if (cy >= TOOLBAR_H && cx < CanvasW()) {
            int hovTi = HitTile(cx, cy);
            if (hovTi >= 0) {
                const auto& ht  = mLevel.tiles[hovTi];
                auto [tsx, tsy] = WorldToScreen(ht.x, ht.y);
                float tsw = ht.w * mCamera.Zoom();
                float tsh = ht.h * mCamera.Zoom();
                bool hasShield = ht.HasShield();
                SDL_SetRenderDrawColor(ren, 190, 80, 255, hasShield ? 60 : 25);
                SDL_FRect shFill = {(float)tsx, (float)tsy, tsw, tsh};
                SDL_RenderFillRect(ren, &shFill);
                SDL_SetRenderDrawColor(ren, 210, 100, 255, 220);
                SDL_FRect shOut = {(float)(tsx-1), (float)(tsy-1), tsw+2.f, tsh+2.f};
                SDL_RenderRect(ren, &shOut);
                if (hasShield) {
                    Text shLbl("shield", {220, 140, 255, 240}, (int)tsx+2, (int)tsy+2, 10);
                    shLbl.Render(ren);
                }
            }
        }

        Text shHint("LS=move  A=toggle shield pickup  SELECT=tools",
                    {200, 100, 255, 215}, 6, H - 18, 11);
        shHint.Render(ren);
    }

    // --- Gamepad Select tool overlay ---
    if (mActiveToolId == ToolId::Select && mTool && mEditorPad && !mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        auto* sel = dynamic_cast<SelectTool*>(mTool.get());

        int cx = (int)mPadCursorX;
        int cy = (int)mPadCursorY;

        // White crosshair (distinctive from erase red / float green / shield purple)
        constexpr int CS = 12;
        SDL_SetRenderDrawColor(ren, 180, 220, 255, 200);
        SDL_RenderLine(ren, cx - CS, cy, cx + CS, cy);
        SDL_RenderLine(ren, cx, cy - CS, cx, cy + CS);
        SDL_SetRenderDrawColor(ren, 220, 240, 255, 255);
        SDL_FRect dot = {(float)(cx - 2), (float)(cy - 2), 5.f, 5.f};
        SDL_RenderFillRect(ren, &dot);

        // When nothing is selected and A is not held: show hover highlight on
        // the tile/enemy under the cursor so the user can see what they'll grab.
        bool hasSelection = sel && (!sel->selIndices.empty() || !sel->selEnemyIndices.empty());
        if (!mPadSelAHeld && !hasSelection && cy >= TOOLBAR_H && cx < CanvasW()) {
            int hovTi = HitTile(cx, cy);
            if (hovTi >= 0) {
                const auto& ht = mLevel.tiles[hovTi];
                auto [tsx, tsy] = WorldToScreen(ht.x, ht.y);
                float tsw = ht.w * mCamera.Zoom();
                float tsh = ht.h * mCamera.Zoom();
                SDL_SetRenderDrawColor(ren, 100, 200, 255, 40);
                SDL_FRect fillR = {(float)tsx, (float)tsy, tsw, tsh};
                SDL_RenderFillRect(ren, &fillR);
                SDL_SetRenderDrawColor(ren, 150, 220, 255, 200);
                SDL_FRect outR = {(float)(tsx - 1), (float)(tsy - 1), tsw + 2.f, tsh + 2.f};
                SDL_RenderRect(ren, &outR);
            }
        }

        // Hint bar — changes based on state
        const char* selHint =
            (mPadSelAHeld && sel && sel->selDragging)
                ? "Release A=drop  B=delete  LS=drag"
            : (mPadSelAHeld && sel && sel->selBoxing)
                ? "Release A=commit selection  LS=extend box"
            : hasSelection
                ? "Hold A=drag  B=delete  LS=pan  SELECT=tools"
                : "Hold A=rubber-band select  LS=pan  SELECT=tools";
        Text selHintTxt(selHint, {180, 220, 255, 210}, 6, H - 18, 11);
        selHintTxt.Render(ren);
    }

    // --- Gamepad Erase tool overlay ---
    if (mActiveToolId == ToolId::Erase && mTool && mEditorPad && !mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        int cx = (int)mPadCursorX;
        int cy = (int)mPadCursorY;

        // Red crosshair
        constexpr int CS = 12;
        SDL_SetRenderDrawColor(ren, 220, 50, 50, 230);
        SDL_RenderLine(ren, cx - CS, cy, cx + CS, cy);
        SDL_RenderLine(ren, cx, cy - CS, cx, cy + CS);
        SDL_SetRenderDrawColor(ren, 255, 80, 80, 255);
        SDL_FRect dot = {(float)(cx - 2), (float)(cy - 2), 5.f, 5.f};
        SDL_RenderFillRect(ren, &dot);

        // Tile hover highlight (red)
        if (cy >= TOOLBAR_H && cx < CanvasW()) {
            int hovTi = HitTile(cx, cy);
            if (hovTi >= 0) {
                const auto& ht = mLevel.tiles[hovTi];
                auto [tsx, tsy] = WorldToScreen(ht.x, ht.y);
                float tsw = ht.w * mCamera.Zoom();
                float tsh = ht.h * mCamera.Zoom();
                SDL_SetRenderDrawColor(ren, 220, 50, 50, 55);
                SDL_FRect fillR = {(float)tsx, (float)tsy, tsw, tsh};
                SDL_RenderFillRect(ren, &fillR);
                SDL_SetRenderDrawColor(ren, 255, 80, 80, 240);
                SDL_FRect outR = {(float)(tsx - 1), (float)(tsy - 1), tsw + 2.f, tsh + 2.f};
                SDL_RenderRect(ren, &outR);
            }
        }

        // Hint bar — changes text while A is held
        const char* eraseHint = mPadEraseAHeld
            ? "Erasing\xe2\x80\xa6 release A to stop  LS=drag erase"
            : "LS=move  A=erase  hold A+LS=drag  SELECT=tools";
        Text erHintLbl(eraseHint, {255, 100, 100, 215}, 6, H - 18, 11);
        erHintLbl.Render(ren);
    }

    // --- Gamepad Pan tool overlay ---
    if (mActiveToolId == ToolId::MoveCam && mEditorPad && !mPadToolbarActive) {
        Text panHint("LS=pan camera  RS=zoom in/out  SELECT=tools",
                     {180, 180, 255, 200}, 6, H - 18, 11);
        panHint.Render(ren);
    }

    // --- Gamepad toolbar overlay (SELECT mode, any tool) ---
    if (mEditorPad && mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
        auto visible       = VisibleToolbarButtons();
        bool onTool        = (mPadToolbarBtnIdx < (int)visible.size());
        bool onGrpPills    = (mPadToolbarBtnIdx == (int)visible.size());
        bool onPalCollapse = (mPadToolbarBtnIdx == (int)visible.size() + 1);

        // ----- Tool button highlight (gold double-outline) -----
        if (onTool && mPadToolbarBtnIdx >= 0 &&
            mPadToolbarBtnIdx < (int)visible.size()) {
            SDL_Rect r = mToolbar.Rect(visible[mPadToolbarBtnIdx]);
            SDL_SetRenderDrawColor(ren, 255, 220, 50, 255);
            SDL_FRect ro1 = {(float)(r.x - 2), (float)(r.y - 2),
                             (float)(r.w + 4),  (float)(r.h + 4)};
            SDL_RenderRect(ren, &ro1);
            SDL_SetRenderDrawColor(ren, 255, 180, 0, 140);
            SDL_FRect ro2 = {(float)(r.x - 4), (float)(r.y - 4),
                             (float)(r.w + 8),  (float)(r.h + 8)};
            SDL_RenderRect(ren, &ro2);
            SDL_SetRenderDrawColor(ren, 255, 220, 50, 30);
            SDL_FRect fill = {(float)r.x, (float)r.y, (float)r.w, (float)r.h};
            SDL_RenderFillRect(ren, &fill);
        }

        // ----- Group-collapse pills highlight -----
        // Highlight the focused group's existing pill with the gold outline.
        if (onGrpPills) {
            auto grp = static_cast<TBGrp>(mPadToolbarGrpIdx);
            SDL_Rect pr = mToolbar.PillRect(grp);
            SDL_SetRenderDrawColor(ren, 255, 220, 50, 255);
            SDL_FRect pr1 = {(float)(pr.x - 2), (float)(pr.y - 2),
                             (float)(pr.w + 4),  (float)(pr.h + 4)};
            SDL_RenderRect(ren, &pr1);
            SDL_SetRenderDrawColor(ren, 255, 180, 0, 140);
            SDL_FRect pr2 = {(float)(pr.x - 4), (float)(pr.y - 4),
                             (float)(pr.w + 8),  (float)(pr.h + 8)};
            SDL_RenderRect(ren, &pr2);
            SDL_SetRenderDrawColor(ren, 255, 220, 50, 40);
            SDL_FRect prFill = {(float)pr.x, (float)pr.y, (float)pr.w, (float)pr.h};
            SDL_RenderFillRect(ren, &prFill);

            // Show the toggle label to the right of the pill.
            bool collapsed = mToolbar.IsCollapsed(grp);
            const char* pillLbl = collapsed ? "Show" : "Hide";
            Text pillText(pillLbl, {255, 240, 100, 230}, pr.x + pr.w + 5, pr.y + 3, 10);
            pillText.Render(ren);
        }

        // ----- Palette collapse tab highlight -----
        if (onPalCollapse) {
            SDL_Rect colR = {cw, TOOLBAR_H, PALETTE_TAB_W, 28};
            SDL_SetRenderDrawColor(ren, 255, 220, 50, 255);
            SDL_FRect co1 = {(float)(colR.x - 2), (float)(colR.y - 2),
                             (float)(colR.w + 4),  (float)(colR.h + 4)};
            SDL_RenderRect(ren, &co1);
            SDL_SetRenderDrawColor(ren, 255, 180, 0, 140);
            SDL_FRect co2 = {(float)(colR.x - 4), (float)(colR.y - 4),
                             (float)(colR.w + 8),  (float)(colR.h + 8)};
            SDL_RenderRect(ren, &co2);
            SDL_SetRenderDrawColor(ren, 255, 220, 50, 50);
            SDL_FRect coFill = {(float)colR.x, (float)colR.y,
                                (float)colR.w,  (float)colR.h};
            SDL_RenderFillRect(ren, &coFill);
            // Label always to the LEFT of the tab (fixes off-screen bug when collapsed).
            const char* colLbl = mPalette.IsCollapsed() ? "Show palette" : "Hide palette";
            constexpr int LBL_W = 88;
            int labelX = std::max(4, colR.x - LBL_W - 6);
            Text colText(colLbl, {255, 240, 100, 230}, labelX, colR.y + 7, 10);
            colText.Render(ren);
        }

        // ----- Hint bar at bottom — changes based on focused row -----
        const char* tbHintStr =
            onPalCollapse ? "A=toggle palette  D-pad \xe2\x86\x91=groups  B=close"
          : onGrpPills    ? "A=toggle group  \xe2\x86\x90\xe2\x86\x92=Place/Mod/Actions  D-pad \xe2\x86\x91=tools  D-pad \xe2\x86\x93=palette  B=close"
                          : "SELECT/B=close  LS/\xe2\x86\x90\xe2\x86\x92=navigate  D-pad \xe2\x86\x93=groups  A=activate";
        Text tbHint(tbHintStr, {255, 220, 80, 220}, 6, H - 18, 11);
        tbHint.Render(ren);
    }

    // --- Gamepad overlay ---
    if (mActiveToolId == ToolId::Tile && mEditorPad && !mPadToolbarActive) {
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        if (mPadPaletteActive) {
            // ---- PALETTE MODE: gold outline around focused cell ----
            constexpr int PAD = 4, LBL_H = 14;
            const int cellW   = (PALETTE_W - PAD * (PAL_COLS + 1)) / PAL_COLS;
            const int cellH   = cellW + LBL_H;
            const int itemH   = cellH + PAD;
            const int palY    = TOOLBAR_H + TAB_H + 44;
            const int startI  = mPalette.TileScroll() * PAL_COLS;
            const int visIdx  = mPadPaletteIdx - startI;

            if (visIdx >= 0) {
                int col = visIdx % PAL_COLS;
                int row = visIdx / PAL_COLS;
                int ix  = cw + PAD + col * (cellW + PAD);
                int iy  = palY + PAD + row * itemH;

                SDL_SetRenderDrawColor(ren, 255, 220, 50, 255);
                SDL_FRect r1 = {(float)(ix-2), (float)(iy-2),
                                (float)(cellW+4), (float)(cellH+4)};
                SDL_RenderRect(ren, &r1);
                SDL_SetRenderDrawColor(ren, 255, 180, 0, 140);
                SDL_FRect r2 = {(float)(ix-4), (float)(iy-4),
                                (float)(cellW+8), (float)(cellH+8)};
                SDL_RenderRect(ren, &r2);
            }

            Text hintTxt("LS=Navigate  A=Select tile  LB/RB=Rotate",
                         {255, 220, 80, 200}, 6, H - 18, 11);
            hintTxt.Render(ren);

        } else {
            // ---- PLACEMENT MODE: hint only (ghost drawn in canvas renderer) ----
            std::string hint = mPadAHeld
                ? "Placing...  release A to stop"
                : "LS=Move tile  A=Place  B or D-pad=Pick new tile  LB/RB=Rotate";
            SDL_Color hc = mPadAHeld ? SDL_Color{80,255,100,220}
                                     : SDL_Color{180,200,255,200};
            Text hintTxt(hint, hc, 6, H - 18, 11);
            hintTxt.Render(ren);
        }
    }

    window.Update();
}

// --- NextScene ---
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
// --- ApplyPowerUpPickerEntry ---
// Select entry idx from the power-up registry (reg.size() = "None").
// Mirrors EditorPopups::HandlePowerUpPickerEvent but driven by gamepad index.
void LevelEditorScene::ApplyPowerUpPickerEntry(int entryIdx) {
    if (!mPopups.powerUpOpen || mPopups.powerUpTileIdx < 0 ||
        mPopups.powerUpTileIdx >= (int)mLevel.tiles.size())
        return;

    const auto& reg = GetPowerUpRegistry();
    auto&       t   = mLevel.tiles[mPopups.powerUpTileIdx];
    int         ti  = mPopups.powerUpTileIdx;

    if (entryIdx < (int)reg.size()) {
        if (reg[entryIdx].id == "shooter") {
            if (t.HasShooter()) {
                t.shooter.reset();
                SetStatus("Tile " + std::to_string(ti) + " -> shooter removed");
            } else {
                t.shooter = ShooterData{};
                SetStatus("Tile " + std::to_string(ti) +
                          " -> shooter  (RClick=cycle side  RS=fire rate)");
            }
        } else {
            t.powerUp = PowerUpData{reg[entryIdx].id, reg[entryIdx].defaultDuration};
            SetStatus("Tile " + std::to_string(ti) +
                      " -> PowerUp: " + reg[entryIdx].label);
        }
    } else {
        // "None" — remove both
        t.powerUp.reset();
        t.shooter.reset();
        SetStatus("Tile " + std::to_string(ti) + " -> PowerUp/Shooter removed");
    }

    // After closing the popup, check whether a teleport entrance was just set
    // and enter linking mode (identical logic to the mouse path in HandleEvent).
    if (t.HasPowerUp() && t.powerUp->type == "teleport" && !t.powerUp->teleportDest) {
        int grp = mTeleportNextGroupId++;
        t.powerUp->teleportGroup = grp;
        mTeleportLinking   = true;
        mTeleportLinkGroup = grp;
        SetStatus("Teleport entrance (group " + std::to_string(grp) +
                  "). A on next tile to set DESTINATION.");
    }

    mPopups.ClosePowerUpPicker();
}

// --- ActivateToolbarButton ---
// Single dispatch point shared by mouse-click handler and gamepad SELECT flow.
void LevelEditorScene::ActivateToolbarButton(TBBtn btn) {
    switch (btn) {
        case TBBtn::Goal:
            SwitchTool(ToolId::Goal);
            lblTool->CreateSurface("Goal");
            break;
        case TBBtn::Enemy:
            SwitchTool(ToolId::Enemy);
            lblTool->CreateSurface("Enemy");
            break;
        case TBBtn::Tile:
            SwitchTool(ToolId::Tile);
            lblTool->CreateSurface("Tile");
            mPalette.SetActiveTab(EditorPalette::Tab::Tiles);
            break;
        case TBBtn::Erase:
            SwitchTool(ToolId::Erase);
            lblTool->CreateSurface("Erase");
            break;
        case TBBtn::PlayerStart:
            SwitchTool(ToolId::PlayerStart);
            lblTool->CreateSurface("Player");
            break;
        case TBBtn::Select:
            SwitchTool(ToolId::Select);
            lblTool->CreateSurface("Select");
            break;
        case TBBtn::MoveCam:
            SwitchTool(ToolId::MoveCam);
            lblTool->CreateSurface("Pan");
            break;
        case TBBtn::Prop:
            SwitchTool(ToolId::Prop);
            lblTool->CreateSurface("Prop");
            break;
        case TBBtn::Ladder:
            SwitchTool(ToolId::Ladder);
            lblTool->CreateSurface("Ladder");
            break;
        case TBBtn::Action:
            SwitchTool(ToolId::Action);
            lblTool->CreateSurface("Action");
            CloseAnimPicker();
            break;
        case TBBtn::Slope:
            SwitchTool(ToolId::Slope);
            lblTool->CreateSurface("Slope");
            break;
        case TBBtn::Resize:
            SwitchTool(ToolId::Resize);
            lblTool->CreateSurface("Resize");
            break;
        case TBBtn::Hitbox:
            SwitchTool(ToolId::Hitbox);
            lblTool->CreateSurface("Hitbox");
            break;
        case TBBtn::Hazard:
            SwitchTool(ToolId::Hazard);
            lblTool->CreateSurface("Hazard");
            break;
        case TBBtn::AntiGrav:
            SwitchTool(ToolId::AntiGrav);
            lblTool->CreateSurface("Float");
            break;
        case TBBtn::PowerUp:
            SwitchTool(ToolId::PowerUp);
            mPopups.powerUpOpen     = false;
            mPopups.powerUpTileIdx  = -1;
            mPopups.powerUpRegistry = &GetPowerUpRegistry();
            lblTool->CreateSurface("PowerUp");
            SetStatus("PowerUp: click tile to assign  |  RClick=cycle turret side  |  Scroll=fire rate");
            break;
        case TBBtn::Shield:
            SwitchTool(ToolId::Shield);
            lblTool->CreateSurface("Shield");
            SetStatus("Shield: LClick tile to toggle shield pickup (slash to collect)");
            break;
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
                const auto& mpi        = *mLevel.tiles[i].moving;
                mPopups.movPlatHoriz   = mpi.horiz;
                mMovPlatRange          = mpi.range;
                mPopups.movPlatSpeed   = mpi.speed;
                mPopups.movPlatLoop    = mpi.loop;
                mPopups.movPlatTrigger = mpi.trigger;
                break;
            }
            mPopups.movPlatOpen       = true;
            mPopups.movPlatSpeedInput = false;
            mPopups.movPlatSpeedStr   = std::to_string((int)mPopups.movPlatSpeed);
            SetStatus("MovingPlat: click tiles to add. RClick=axis/range. New group ID=" +
                      std::to_string(mMovPlatCurGroupId));
            break;
        }
        case TBBtn::Save: {
            fs::create_directories("levels");
            std::string path = "levels/" + mLevelName + ".json";
            mLevel.name      = mLevelName;
            SaveLevel(mLevel, path);
            SetStatus("Saved: " + path);
            break;
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
            } else {
                SetStatus("No file: " + path);
            }
            break;
        }
        case TBBtn::Clear:
            mLevel.enemies.clear();
            mLevel.tiles.clear();
            SetStatus("Cleared");
            break;
        case TBBtn::Play: {
            fs::create_directories("levels");
            std::string path = "levels/" + mLevelName + ".json";
            mLevel.name      = mLevelName;
            SaveLevel(mLevel, path);
            mLaunchGame = true;
            break;
        }
        case TBBtn::Back: {
            fs::create_directories("levels");
            std::string path = "levels/" + mLevelName + ".json";
            mLevel.name      = mLevelName;
            SaveLevel(mLevel, path);
            mGoBack = true;
            break;
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
                (mLevel.gravityMode == GravityMode::WallRun)     ? "Mode: Wall Run"
                : (mLevel.gravityMode == GravityMode::OpenWorld) ? "Mode: Open World (top-down)"
                                                                 : "Mode: Platformer";
            mToolbar.SetGravityLabel(gLbl);
            SetStatus(gStatus);
            break;
        }
        default:
            break;
    }
}

// --- Tile tool accessors ---
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
