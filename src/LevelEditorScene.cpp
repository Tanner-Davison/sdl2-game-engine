#include "LevelEditorScene.hpp"
#include "GameScene.hpp"
#include "TitleScene.hpp"
#include "SurfaceUtils.hpp"
#include <climits>
#include <print>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

// Build a PAL_ICON×PAL_ICON thumbnail from a full-res SDL_Surface.
// Returns nullptr on failure. Caller owns the result.
static SDL_Surface* MakeThumb(SDL_Surface* src, int w, int h) {
    SDL_Surface* t = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
    if (!t) return nullptr;
    SDL_SetSurfaceBlendMode(t, SDL_BLENDMODE_NONE);
    SDL_Rect sr = {0, 0, src->w, src->h};
    SDL_Rect dr = {0, 0, w, h};
    SDL_BlitSurfaceScaled(src, &sr, t, &dr, SDL_SCALEMODE_LINEAR);
    SDL_SetSurfaceBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

// Load a PNG from disk, convert to ARGB8888, return it (caller owns).
static SDL_Surface* LoadPNG(const fs::path& p) {
    SDL_Surface* raw = IMG_Load(p.string().c_str());
    if (!raw) return nullptr;
    SDL_Surface* c = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(raw);
    return c;
}

// ─── LoadTileView ──────────────────────────────────────────────────────────
void LevelEditorScene::LoadTileView(const std::string& dir) {
    // Free existing surface memory — folders use non-owning mFolderIcon pointer,
    // so only free surfaces for file entries.
    for (auto& item : mPaletteItems) {
        if (!item.isFolder) {
            if (item.thumb) SDL_DestroySurface(item.thumb);
            if (item.full)  SDL_DestroySurface(item.full);
        }
    }
    mPaletteItems.clear();
    mPaletteScroll = 0;
    mSelectedTile  = 0;   // reset selection — old index may be out of bounds in new view
    mTileCurrentDir = dir;

    if (!fs::exists(dir)) return;

    // ── "◀ Back" entry when we're inside a subfolder ──────────────────────────
    fs::path dirPath(dir);
    fs::path rootPath(TILE_ROOT);
    // Use lexically_relative to check if we're deeper than the root
    fs::path rel = fs::path(dir).lexically_relative(TILE_ROOT);
    bool atRoot  = (rel.empty() || rel == ".");
    if (!atRoot) {
        PaletteItem back;
        back.path     = dirPath.parent_path().string();
        back.label    = "◀ Back";
        back.isFolder = true;
        back.thumb    = mFolderIcon;  // non-owning
        back.full     = nullptr;
        mPaletteItems.push_back(std::move(back));
    }

    std::vector<fs::path> folders, files;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_directory())
            folders.push_back(entry.path());
        else if (entry.path().extension() == ".png")
            files.push_back(entry.path());
    }
    std::sort(folders.begin(), folders.end());
    std::sort(files.begin(), files.end());

    // Folders first
    for (const auto& p : folders) {
        // Count PNGs inside so we can show a count badge
        int count = 0;
        for (const auto& e : fs::directory_iterator(p))
            if (e.path().extension() == ".png") count++;

        PaletteItem item;
        item.path     = p.string();
        item.label    = p.filename().string() + " (" + std::to_string(count) + ")";
        item.isFolder = true;
        item.thumb    = mFolderIcon;  // non-owning pointer — shared, never freed per-item
        item.full     = nullptr;
        mPaletteItems.push_back(std::move(item));
    }

    // Then individual PNG files
    for (const auto& p : files) {
        SDL_Surface* full = LoadPNG(p);
        if (!full) continue;
        SDL_SetSurfaceBlendMode(full, SDL_BLENDMODE_BLEND);
        SDL_Surface* thumb = MakeThumb(full, PAL_ICON, PAL_ICON);

        PaletteItem item;
        item.path     = p.string();
        item.label    = p.stem().string();
        item.isFolder = false;
        item.thumb    = thumb;
        item.full     = full;
        mPaletteItems.push_back(std::move(item));
    }
}

// ─── DetectResizeEdge ───────────────────────────────────────────────────────────
LevelEditorScene::ResizeEdge LevelEditorScene::DetectResizeEdge(int idx, int mx, int my) const {
    if (idx < 0 || idx >= (int)mLevel.tiles.size()) return ResizeEdge::None;
    const auto& t = mLevel.tiles[idx];
    // mx/my are screen-space; convert to world space to compare with tile world coords
    mx = (int)(mx + mCamX);
    my = (int)(my + mCamY);
    int rx = (int)t.x, ry = (int)t.y, rw = t.w, rh = t.h;

    // Must be inside (or very near) the tile first
    if (mx < rx - RESIZE_HANDLE || mx > rx + rw + RESIZE_HANDLE) return ResizeEdge::None;
    if (my < ry - RESIZE_HANDLE || my > ry + rh + RESIZE_HANDLE) return ResizeEdge::None;

    bool nearRight  = (mx >= rx + rw - RESIZE_HANDLE && mx <= rx + rw + RESIZE_HANDLE);
    bool nearBottom = (my >= ry + rh - RESIZE_HANDLE && my <= ry + rh + RESIZE_HANDLE);
    bool insideH    = (mx >= rx && mx <= rx + rw);
    bool insideV    = (my >= ry && my <= ry + rh);

    if (nearRight && nearBottom)              return ResizeEdge::Corner;
    if (nearRight  && (insideV || nearBottom)) return ResizeEdge::Right;
    if (nearBottom && (insideH || nearRight))  return ResizeEdge::Bottom;
    return ResizeEdge::None;
}

// ─── LoadBgPalette ────────────────────────────────────────────────────────────
void LevelEditorScene::LoadBgPalette() {
    for (auto& item : mBgItems)
        if (item.thumb) SDL_DestroySurface(item.thumb);
    mBgItems.clear();

    if (!fs::exists(BG_ROOT)) return;

    std::vector<fs::path> paths;
    for (const auto& e : fs::directory_iterator(BG_ROOT))
        if (e.path().extension() == ".png")
            paths.push_back(e.path());
    std::sort(paths.begin(), paths.end());

    const int thumbW = PALETTE_W - 8;
    const int thumbH = thumbW / 2;

    for (const auto& p : paths) {
        SDL_Surface* full = LoadPNG(p);
        if (!full) continue;
        SDL_Surface* thumb = MakeThumb(full, thumbW, thumbH);
        SDL_DestroySurface(full);
        mBgItems.push_back({p.string(), p.stem().string(), thumb});

        if (p.string() == mLevel.background)
            mSelectedBg = (int)mBgItems.size() - 1;
    }
}

// ─── ApplyBackground ─────────────────────────────────────────────────────────
void LevelEditorScene::ApplyBackground(int idx) {
    if (idx < 0 || idx >= (int)mBgItems.size()) return;
    mSelectedBg       = idx;
    mLevel.background = mBgItems[idx].path;
    background        = std::make_unique<Image>(mLevel.background, nullptr, FitMode::PRESCALED);
    SetStatus("Background: " + mBgItems[idx].label);
}

// ─── Load ─────────────────────────────────────────────────────────────────────
void LevelEditorScene::Load(Window& window) {
    mWindow     = &window;
    mLaunchGame = false;

    background = std::make_unique<Image>(
        "game_assets/backgrounds/deepspace_scene.png", nullptr, FitMode::PRESCALED);
    coinSheet  = std::make_unique<SpriteSheet>(
        "game_assets/gold_coins/", "Gold_", 30, ICON_SIZE, ICON_SIZE);
    enemySheet = std::make_unique<SpriteSheet>(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");

    // Determine which level file to load on startup.
    // Three cases:
    //   mForceNew == true          → skip all loading, start blank
    //   mOpenPath non-empty        → load that specific file
    //   mOpenPath empty, no force  → auto-resume levels/level1.json if it exists
    if (!mForceNew && mLevel.coins.empty() && mLevel.enemies.empty() && mLevel.tiles.empty()) {
        std::string autoPath;
        if (!mOpenPath.empty()) {
            autoPath   = mOpenPath;
            fs::path p(mOpenPath);
            mLevelName = p.stem().string();
        } else {
            autoPath = "levels/" + mLevelName + ".json";
        }
        if (fs::exists(autoPath)) {
            LoadLevel(autoPath, mLevel);
            SetStatus("Resumed: " + autoPath);
            if (!mLevel.background.empty())
                background = std::make_unique<Image>(mLevel.background, nullptr, FitMode::PRESCALED);
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
        mLevel.player.y = static_cast<float>(window.GetHeight() - PLAYER_STAND_HEIGHT - GRID * 2);
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

    LoadTileView(TILE_ROOT);
    LoadBgPalette();

    // ── Toolbar layout ──────────────────────────────────────────────────────────
    // Three groups separated by GRP_GAP visual dividers.
    // All buttons share BTN_H height and sit at BTN_Y from the top.
    {
        int x = BTN_GAP;
        auto nextTool = [&]() -> SDL_Rect {
            SDL_Rect r = {x, BTN_Y, BTN_TOOL_W, BTN_H};
            x += BTN_TOOL_W + BTN_GAP;
            return r;
        };
        auto nextAct = [&]() -> SDL_Rect {
            SDL_Rect r = {x, BTN_Y, BTN_ACT_W, BTN_H};
            x += BTN_ACT_W + BTN_GAP;
            return r;
        };
        auto gap = [&]() { x += GRP_GAP; };

        // Group 1 — Place
        btnCoin        = nextTool();
        btnEnemy       = nextTool();
        btnTile        = nextTool();
        btnErase       = nextTool();
        btnPlayerStart = nextTool();
        btnSelect      = nextTool();
        gap();
        // Group 2 — Modifiers
        btnProp        = nextTool();
        btnLadder      = nextTool();
        btnAction      = nextTool();
        btnSlope       = nextTool();
        btnResize      = nextTool();
        btnHitbox      = nextTool();
        btnHazard        = nextTool();
        btnAntiGrav      = nextTool();
        gap();
        // Group 3 — Actions (slightly wider)
        btnGravity     = nextAct();
        btnSave        = nextAct();
        btnLoad        = nextAct();
        btnClear       = nextAct();
        btnPlay        = nextAct();
        gap();
        btnBack        = nextAct();
    }

    // White label maker — used for all tool/action buttons
    auto mkLbl = [](const std::string& s, SDL_Rect r, int sz = 12) {
        auto [x, y] = Text::CenterInRect(s, sz, r);
        return std::make_unique<Text>(s, SDL_Color{255,255,255,255}, x, y, sz);
    };
    // Tiny shortcut hint in bottom-right of button
    auto mkHint = [](const std::string& s, SDL_Rect r) {
        return std::make_unique<Text>(s, SDL_Color{180,180,180,160},
                                     r.x + r.w - 14, r.y + r.h - 13, 9);
    };

    // Group 1 labels + hints
    lblCoin        = mkLbl("Coin",    btnCoin);
    lblEnemy       = mkLbl("Enemy",   btnEnemy);
    lblTile        = mkLbl("Tile",    btnTile);
    lblErase       = mkLbl("Erase",   btnErase);
    lblPlayer      = mkLbl("Player",  btnPlayerStart);
    lblSelect      = mkLbl("Select",  btnSelect, 11);
    hintCoin       = mkHint("1", btnCoin);
    hintEnemy      = mkHint("2", btnEnemy);
    hintTile       = mkHint("3", btnTile);
    hintErase      = mkHint("4", btnErase);
    hintPlayer     = mkHint("5", btnPlayerStart);
    hintSelect     = mkHint("Q", btnSelect);

    // Group 2 labels + hints
    lblProp        = mkLbl("Prop",    btnProp);
    lblLadder      = mkLbl("Ladder",  btnLadder);
    lblAction      = mkLbl("Action",  btnAction);
    lblSlope       = mkLbl("Slope",   btnSlope);
    lblResize      = mkLbl("Resize",  btnResize);
    hintProp       = mkHint("8", btnProp);
    hintLadder     = mkHint("9", btnLadder);
    hintAction     = mkHint("0", btnAction);
    hintSlope      = mkHint("-", btnSlope);
    hintResize     = mkHint("R", btnResize);
    lblHitbox      = mkLbl("Hitbox",  btnHitbox);
    lblHazard        = mkLbl("Hazard",  btnHazard);
    lblAntiGrav      = mkLbl("Float",   btnAntiGrav,     11);
    // (no shortcut hints for hitbox/hazard/break/float — buttons are narrow)

    // Group 3 labels (no shortcut hints — these are action buttons)
    lblSave        = mkLbl("Save",    btnSave);
    lblLoad        = mkLbl("Load",    btnLoad);
    lblClear       = mkLbl("Clear",   btnClear);
    lblPlay        = mkLbl("Play",    btnPlay);
    lblBack        = mkLbl("< Menu",  btnBack);
    // Gravity label reflects current mode
    {
        std::string gLbl = (mLevel.gravityMode == GravityMode::WallRun)  ? "Wall Run"
                         : (mLevel.gravityMode == GravityMode::OpenWorld) ? "Open World"
                                                                          : "Platform";
        lblGravity = mkLbl(gLbl, btnGravity, 11);
    }

    lblStatus = std::make_unique<Text>(mStatusMsg, SDL_Color{180,180,200,255},
                                       BTN_GAP, TOOLBAR_H + 4, 12);
    lblTool   = std::make_unique<Text>("Coin", SDL_Color{255,215,0,255},
                                       window.GetWidth() - PALETTE_W - 120, 22, 13);
}

// ─── Unload ───────────────────────────────────────────────────────────────────
void LevelEditorScene::Unload() {
    // PaletteItems for folders point to mFolderIcon (non-owning), so don't free
    // item.thumb for folder entries — only free file thumbnails.
    for (auto& i : mPaletteItems) {
        if (!i.isFolder) {
            if (i.thumb) SDL_DestroySurface(i.thumb);
            if (i.full)  SDL_DestroySurface(i.full);
        }
    }
    mPaletteItems.clear();
    for (auto& i : mBgItems) { if (i.thumb) SDL_DestroySurface(i.thumb); }
    mBgItems.clear();
    if (mFolderIcon) { SDL_DestroySurface(mFolderIcon); mFolderIcon = nullptr; }
    mWindow = nullptr;
}

// ─── HandleEvent ──────────────────────────────────────────────────────────────
bool LevelEditorScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) return false;

    // ── File / folder drop ────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_DROP_BEGIN)    { mDropActive = true;  SetStatus("Drop a .png or folder..."); return true; }
    if (e.type == SDL_EVENT_DROP_COMPLETE) { mDropActive = false; return true; }
    if (e.type == SDL_EVENT_DROP_FILE) {
        mDropActive = false;
        std::string path = e.drop.data ? std::string(e.drop.data) : "";
        if (!path.empty()) ImportPath(path);
        return true;
    }

    // ── Import text input ─────────────────────────────────────────────────────
    if (mImportInputActive) {
        if (e.type == SDL_EVENT_TEXT_INPUT) { mImportInputText += e.text.text; return true; }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_ESCAPE:
                    mImportInputActive = false; mImportInputText.clear();
                    SetStatus("Import cancelled");
                    SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                    return true;
                case SDLK_BACKSPACE:
                    if (!mImportInputText.empty()) mImportInputText.pop_back();
                    return true;
                case SDLK_RETURN: case SDLK_KP_ENTER: {
                    std::string path = mImportInputText;
                    mImportInputActive = false; mImportInputText.clear();
                    SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                    if (!path.empty()) ImportPath(path);
                    return true;
                }
                default: break;
            }
        }
        return true;
    }

    // ── Pan: middle-mouse drag OR Ctrl + left-mouse drag ────────────────────
    auto startPan = [&](int mx, int my) {
        mIsPanning    = true;
        mPanStartX    = mx;
        mPanStartY    = my;
        mPanCamStartX = mCamX;
        mPanCamStartY = mCamY;
    };

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_MIDDLE) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (mx < CanvasW() && my >= TOOLBAR_H) { startPan(mx, my); return true; }
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        if (SDL_GetModState() & SDL_KMOD_CTRL) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (mx < CanvasW() && my >= TOOLBAR_H) { startPan(mx, my); return true; }
        }
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP &&
        (e.button.button == SDL_BUTTON_MIDDLE || e.button.button == SDL_BUTTON_LEFT)) {
        if (mIsPanning) { mIsPanning = false; return true; }
    }
    // Pan motion is handled in Update() by polling SDL_GetMouseState every frame
    // for smoothness — no event handler needed here.

    // ── Mouse wheel ───────────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        float fmx, fmy; SDL_GetMouseState(&fmx, &fmy);
        int mx=(int)fmx;
        if (mx >= CanvasW()) {
            if (mActiveTab == PaletteTab::Tiles) {
                mPaletteScroll = std::max(0, mPaletteScroll - (int)e.wheel.y);
                int rows = ((int)mPaletteItems.size() + PAL_COLS - 1) / PAL_COLS;
                mPaletteScroll = std::min(mPaletteScroll, std::max(0, rows - 1));
            } else {
                mBgPaletteScroll = std::max(0, mBgPaletteScroll - (int)e.wheel.y);
                mBgPaletteScroll = std::min(mBgPaletteScroll, std::max(0,(int)mBgItems.size()-1));
            }
        } else if (mActiveTool == Tool::Tile) {
            mTileW = std::max(GRID, mTileW + (int)e.wheel.y * GRID);
            mTileH = mTileW;
            SetStatus("Tile size: " + std::to_string(mTileW));
        }
    }

    // ── Key down ──────────────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
            case SDLK_Q:
                mActiveTool=Tool::Select; lblTool->CreateSurface("Select");
                mSelBoxing=false; mSelDragging=false;
                break;
            case SDLK_1: mActiveTool=Tool::Coin;        lblTool->CreateSurface("Coin");   break;
            case SDLK_2: mActiveTool=Tool::Enemy;       lblTool->CreateSurface("Enemy");  break;
            case SDLK_3: mActiveTool=Tool::Tile;        lblTool->CreateSurface("Tile");
                         mActiveTab=PaletteTab::Tiles;  break;
            case SDLK_4: mActiveTool=Tool::Erase;       lblTool->CreateSurface("Erase");  break;
            case SDLK_5: mActiveTool=Tool::PlayerStart; lblTool->CreateSurface("Player"); break;
            case SDLK_6: mActiveTab=PaletteTab::Backgrounds; lblTool->CreateSurface("Backgrounds"); break;
            case SDLK_7:
            case SDLK_R:
                mActiveTool=Tool::Resize; lblTool->CreateSurface("Resize");
                mIsResizing=false; mHoverEdge=ResizeEdge::None; mHoverTileIdx=-1;
                break;
            case SDLK_8:
            case SDLK_P:
                mActiveTool=Tool::Prop; lblTool->CreateSurface("Prop");
                break;
            case SDLK_9:
            case SDLK_L:
                mActiveTool=Tool::Ladder; lblTool->CreateSurface("Ladder");
                break;
            case SDLK_0:
            case SDLK_T:
                mActiveTool=Tool::Action; lblTool->CreateSurface("Action");
                break;
            case SDLK_MINUS:
                mActiveTool=Tool::Slope; lblTool->CreateSurface("Slope");
                break;
            case SDLK_G: {
                if      (mLevel.gravityMode == GravityMode::Platformer) mLevel.gravityMode = GravityMode::WallRun;
                else if (mLevel.gravityMode == GravityMode::WallRun)    mLevel.gravityMode = GravityMode::OpenWorld;
                else                                                     mLevel.gravityMode = GravityMode::Platformer;
                std::string gLbl = (mLevel.gravityMode == GravityMode::WallRun)  ? "Wall Run"
                                 : (mLevel.gravityMode == GravityMode::OpenWorld) ? "Open World"
                                                                                  : "Platform";
                std::string gStatus = (mLevel.gravityMode == GravityMode::WallRun)  ? "Mode: Wall Run"
                                    : (mLevel.gravityMode == GravityMode::OpenWorld) ? "Mode: Open World (top-down)"
                                                                                     : "Mode: Platformer";
                auto [gx,gy] = Text::CenterInRect(gLbl, 11, btnGravity);
                lblGravity = std::make_unique<Text>(gLbl, SDL_Color{255,255,255,255}, gx, gy, 11);
                SetStatus(gStatus);
                break;
            }

            case SDLK_I:
                mImportInputActive = true; mImportInputText.clear();
                SDL_StartTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                SetStatus(mActiveTab==PaletteTab::Backgrounds
                    ? "Import bg path or folder (Enter=go, Esc=cancel):"
                    : "Import tile path or folder (Enter=go, Esc=cancel):");
                break;
            case SDLK_S:
                if (e.key.mod & SDL_KMOD_CTRL) {
                    fs::create_directories("levels");
                    std::string path = "levels/" + mLevelName + ".json";
                    mLevel.name = mLevelName;
                    SaveLevel(mLevel, path); SetStatus("Saved: " + path);
                }
                break;
            case SDLK_Z:
                if (e.key.mod & SDL_KMOD_CTRL) {
                    if (!mLevel.tiles.empty())        { mLevel.tiles.pop_back();   SetStatus("Undo tile"); }
                    else if (!mLevel.coins.empty())   { mLevel.coins.pop_back();   SetStatus("Undo coin"); }
                    else if (!mLevel.enemies.empty()) { mLevel.enemies.pop_back(); SetStatus("Undo enemy"); }
                }
                break;
            case SDLK_DELETE: case SDLK_BACKSPACE:
                // Delete all selected tiles
                if (!mSelIndices.empty()) {
                    // Sort descending so erasing by index doesn't shift remaining indices
                    std::sort(mSelIndices.begin(), mSelIndices.end(), std::greater<int>());
                    for (int idx : mSelIndices)
                        if (idx >= 0 && idx < (int)mLevel.tiles.size())
                            mLevel.tiles.erase(mLevel.tiles.begin() + idx);
                    SetStatus("Deleted " + std::to_string(mSelIndices.size()) + " tile(s)");
                    mSelIndices.clear();
                }
                break;
            case SDLK_ESCAPE:
                if (mActiveTool == Tool::Select) {
                    mSelIndices.clear(); mSelBoxing = false; mSelDragging = false;
                    SetStatus("Selection cleared");
                    break; // don't fall through to the tile-browser Esc handler
                }
                // Navigate back up in tile browser
                if (mActiveTab == PaletteTab::Tiles && mTileCurrentDir != TILE_ROOT) {
                    fs::path parent = fs::path(mTileCurrentDir).parent_path();
                    std::string up = parent.string();
                    if (up.rfind(TILE_ROOT, 0) != 0) up = TILE_ROOT;
                    LoadTileView(up);
                }
                break;
            default: break;
        }
    }

    // ── Mouse button down ─────────────────────────────────────────────────────
    // ── Right-click: group cycling for action tiles ──────────────────────────
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
        int mx=(int)e.button.x, my=(int)e.button.y;
        if (mActiveTool == Tool::Action && my >= TOOLBAR_H && mx < CanvasW()) {
            int ti = HitTile(mx, my);
            if (ti >= 0 && mLevel.tiles[ti].action) {
                // Cycle group: 0 → 1 → 2 → … → 9 → 0
                int& grp = mLevel.tiles[ti].actionGroup;
                grp = (grp + 1) % 10;
                SetStatus("Tile " + std::to_string(ti) +
                          " group → " + (grp == 0 ? "standalone" : std::to_string(grp)));
                return true;
            }
        }
    }

    // Right-click on canvas: cycle tile rotation (or action group for Action tool)
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
        int mx=(int)e.button.x, my=(int)e.button.y;
        if (my >= TOOLBAR_H && mx < CanvasW()) {
            int ti = HitTile(mx, my);
            if (ti >= 0) {
                if (mActiveTool == Tool::Action && mLevel.tiles[ti].action) {
                    int& grp = mLevel.tiles[ti].actionGroup;
                    grp = (grp + 1) % 10;
                    SetStatus("Tile " + std::to_string(ti) + " group -> " +
                              (grp == 0 ? "standalone" : std::to_string(grp)));
                } else {
                    int& rot = mLevel.tiles[ti].rotation;
                    rot = (rot + 90) % 360;
                    SetStatus("Tile " + std::to_string(ti) +
                              " rotated to " + std::to_string(rot) + "deg");
                }
                return true;
            }
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx=(int)e.button.x, my=(int)e.button.y;

        // Tab bar
        if (mx >= CanvasW() && my >= TOOLBAR_H && my < TOOLBAR_H + TAB_H) {
            mActiveTab = (mx < CanvasW() + PALETTE_W/2) ? PaletteTab::Tiles : PaletteTab::Backgrounds;
            return true;
        }

        // Toolbar
        auto tb = [&](SDL_Rect r, Tool t, const std::string& l) {
            if (!HitTest(r,mx,my)) return false;
            if (t != Tool::Select) { mSelIndices.clear(); mSelBoxing = false; mSelDragging = false; }
            mActiveTool = t; lblTool->CreateSurface(l); return true;
        };
        if (tb(btnCoin,        Tool::Coin,        "Coin"))   return true;
        if (tb(btnEnemy,       Tool::Enemy,       "Enemy"))  return true;
        if (tb(btnTile,        Tool::Tile,        "Tile"))   return true;
        if (tb(btnErase,       Tool::Erase,       "Erase"))  return true;
        if (tb(btnPlayerStart, Tool::PlayerStart, "Player")) return true;
        if (HitTest(btnSelect,mx,my)) {
            mActiveTool=Tool::Select; lblTool->CreateSurface("Select");
            mSelBoxing=false; mSelDragging=false;
            return true;
        }
        if (tb(btnProp,        Tool::Prop,        "Prop"))   return true;
        if (tb(btnLadder,      Tool::Ladder,      "Ladder")) return true;
        if (tb(btnAction,      Tool::Action,      "Action")) return true;
        if (tb(btnSlope,       Tool::Slope,       "Slope"))  return true;
        if (HitTest(btnResize,mx,my)) { mActiveTool=Tool::Resize; lblTool->CreateSurface("Resize"); mIsResizing=false; mHoverEdge=ResizeEdge::None; mHoverTileIdx=-1; return true; }
        if (HitTest(btnHitbox,mx,my)) { mActiveTool=Tool::Hitbox; lblTool->CreateSurface("Hitbox"); mHitboxDragging=false; mHoverHitboxHdl=HitboxHandle::None; return true; }
        if (HitTest(btnHazard,mx,my))   { mActiveTool=Tool::Hazard;   lblTool->CreateSurface("Hazard"); return true; }
        if (HitTest(btnAntiGrav,mx,my)) { mActiveTool=Tool::AntiGrav; lblTool->CreateSurface("Float");  return true; }

        if (HitTest(btnSave,mx,my)) {
            fs::create_directories("levels");
            std::string path="levels/"+mLevelName+".json";
            mLevel.name=mLevelName; SaveLevel(mLevel,path); SetStatus("Saved: "+path); return true;
        }
        if (HitTest(btnLoad,mx,my)) {
            std::string path="levels/"+mLevelName+".json";
            if (LoadLevel(path,mLevel)) {
                SetStatus("Loaded: "+path);
                if (!mLevel.background.empty())
                    background=std::make_unique<Image>(mLevel.background,nullptr,FitMode::PRESCALED);
                LoadBgPalette();
                mCamX = mCamY = 0.0f; // reset editor camera on load
            } else SetStatus("No file: "+path);
            return true;
        }
        if (HitTest(btnClear,mx,my)) {
            mLevel.coins.clear(); mLevel.enemies.clear(); mLevel.tiles.clear();
            SetStatus("Cleared"); return true;
        }
        if (HitTest(btnPlay,mx,my)) {
            fs::create_directories("levels");
            std::string path="levels/"+mLevelName+".json";
            mLevel.name=mLevelName; SaveLevel(mLevel,path); mLaunchGame=true; return true;
        }
        if (HitTest(btnBack,mx,my)) {
            // Auto-save before leaving so work isn't lost
            fs::create_directories("levels");
            std::string path="levels/"+mLevelName+".json";
            mLevel.name=mLevelName; SaveLevel(mLevel,path);
            mGoBack=true; return true;
        }
        if (HitTest(btnGravity,mx,my)) {
            if      (mLevel.gravityMode == GravityMode::Platformer) mLevel.gravityMode = GravityMode::WallRun;
            else if (mLevel.gravityMode == GravityMode::WallRun)    mLevel.gravityMode = GravityMode::OpenWorld;
            else                                                     mLevel.gravityMode = GravityMode::Platformer;
            std::string gLbl = (mLevel.gravityMode == GravityMode::WallRun)  ? "Wall Run"
                             : (mLevel.gravityMode == GravityMode::OpenWorld) ? "Open World"
                                                                              : "Platform";
            std::string gStatus = (mLevel.gravityMode == GravityMode::WallRun)  ? "Mode: Wall Run"
                                : (mLevel.gravityMode == GravityMode::OpenWorld) ? "Mode: Open World (top-down)"
                                                                                 : "Mode: Platformer";
            auto [gx,gy] = Text::CenterInRect(gLbl, 11, btnGravity);
            lblGravity = std::make_unique<Text>(gLbl, SDL_Color{255,255,255,255}, gx, gy, 11);
            SetStatus(gStatus);
            return true;
        }

        // ── Palette panel ──────────────────────────────────────────────────────
        if (mx >= CanvasW() && my >= TOOLBAR_H + TAB_H) {
            if (mActiveTab == PaletteTab::Tiles) {
                // Resolve which palette entry was clicked (same grid as render)
                constexpr int PAD=4, LBL_H=14;
                const int cellW = (PALETTE_W - PAD*(PAL_COLS+1)) / PAL_COLS;
                const int cellH = cellW + LBL_H;
                const int itemH = cellH + PAD;
                int relX = mx - CanvasW() - PAD;
                int relY = my - TOOLBAR_H - TAB_H - PAD;

                // header strip (44px)
                if (relY < 44) return true;
                relY -= 44;

                int col = relX / (cellW + PAD);
                int row = relY / itemH;
                if (col < 0 || col >= PAL_COLS) return true;

                int idx = (mPaletteScroll + row) * PAL_COLS + col;
                if (idx < 0 || idx >= (int)mPaletteItems.size()) return true;

                const auto& item = mPaletteItems[idx];

                if (item.isFolder) {
                    // Copy the path BEFORE calling LoadTileView — LoadTileView clears
                    // and rebuilds mPaletteItems, which invalidates the 'item' reference.
                    std::string folderPath = item.path;
                    std::string folderName = fs::path(folderPath).filename().string();
                    LoadTileView(folderPath);
                    SetStatus("Opened: " + folderName);
                } else {
                    // ── Double-click detection ────────────────────────────────
                    Uint64 now = SDL_GetTicks();
                    bool isDouble = (idx == mLastClickIndex &&
                                     (now - mLastClickTime) < DOUBLE_CLICK_MS);
                    mLastClickIndex = idx;
                    mLastClickTime  = now;

                    mSelectedTile = idx;
                    mActiveTool   = Tool::Tile;
                    lblTool->CreateSurface("Tool: Tile");
                    SetStatus("Selected: " + item.label + (isDouble ? " (double)" : ""));
                }
            } else {
                // Backgrounds — single column
                constexpr int PAD=4, LBL_H=16;
                const int thumbW = PALETTE_W - PAD*2;
                const int thumbH = thumbW / 2;
                const int itemH  = thumbH + LBL_H + PAD;
                int relY = my - TOOLBAR_H - TAB_H - 24 - PAD; // 24 = bg header strip
                int row  = relY / itemH;
                int idx  = mBgPaletteScroll + row;
                if (idx >= 0 && idx < (int)mBgItems.size())
                    ApplyBackground(idx);
            }
            return true;
        }

        // ── Canvas ─────────────────────────────────────────────────────────────
        if (my < TOOLBAR_H || mx >= CanvasW()) return true;
        auto [sx, sy] = SnapToGrid(mx, my);

        switch (mActiveTool) {
            case Tool::Coin:
                mLevel.coins.push_back({(float)sx,(float)sy});
                SetStatus("Coin at "+std::to_string(sx)+","+std::to_string(sy)); break;
            case Tool::Enemy:
                mLevel.enemies.push_back({(float)sx,(float)sy,ENEMY_SPEED});
                SetStatus("Enemy at "+std::to_string(sx)+","+std::to_string(sy)); break;
            case Tool::Tile:
                if (!mPaletteItems.empty() && !mPaletteItems[mSelectedTile].isFolder) {
                    mLevel.tiles.push_back({(float)sx,(float)sy,mTileW,mTileH,
                                            mPaletteItems[mSelectedTile].path});
                    SetStatus("Tile: "+mPaletteItems[mSelectedTile].label);
                }
                break;
            case Tool::Erase: {
                int ti=HitTile(mx,my); if(ti>=0){mLevel.tiles.erase(mLevel.tiles.begin()+ti);SetStatus("Erased tile");break;}
                int ci=HitCoin(mx,my); if(ci>=0){mLevel.coins.erase(mLevel.coins.begin()+ci);SetStatus("Erased coin");break;}
                int ei=HitEnemy(mx,my);if(ei>=0){mLevel.enemies.erase(mLevel.enemies.begin()+ei);SetStatus("Erased enemy");break;}
                break;
            }
            case Tool::PlayerStart:
                mLevel.player={(float)sx,(float)sy}; SetStatus("Player start set"); break;
            case Tool::Prop: {
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    bool nowProp = !mLevel.tiles[ti].prop;
                    mLevel.tiles[ti].prop = nowProp;
                    // Prop, Ladder, and Action are mutually exclusive
                    if (nowProp) { mLevel.tiles[ti].ladder = false; mLevel.tiles[ti].action = false; mLevel.tiles[ti].slope = SlopeType::None; }
                    SetStatus(std::string("Tile ") + std::to_string(ti) +
                              (nowProp ? " → prop (no collision)" : " → solid (collision on)"));
                }
                return true;
            }
            case Tool::Ladder: {
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    bool nowLadder = !mLevel.tiles[ti].ladder;
                    mLevel.tiles[ti].ladder = nowLadder;
                    // Ladder, Prop, and Action are mutually exclusive
                    if (nowLadder) { mLevel.tiles[ti].prop = false; mLevel.tiles[ti].action = false; mLevel.tiles[ti].slope = SlopeType::None; }
                    SetStatus(std::string("Tile ") + std::to_string(ti) +
                              (nowLadder ? " → ladder (climbable)" : " → solid (ladder removed)"));
                }
                return true;
            }
            case Tool::Action: {
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    bool nowAction = !mLevel.tiles[ti].action;
                    mLevel.tiles[ti].action = nowAction;
                    // Action, Prop, Ladder, and Slope are mutually exclusive
                    if (nowAction) { mLevel.tiles[ti].prop = false; mLevel.tiles[ti].ladder = false; mLevel.tiles[ti].slope = SlopeType::None; }
                    SetStatus(std::string("Tile ") + std::to_string(ti) +
                              (nowAction ? " → action (disappears on contact)" : " → solid (action removed)"));
                }
                return true;
            }
            case Tool::Slope: {
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    // Cycle: None → DiagUpRight → DiagUpLeft → None
                    SlopeType next;
                    std::string label;
                    if (mLevel.tiles[ti].slope == SlopeType::None) {
                        next = SlopeType::DiagUpRight; label = "DiagUpRight (rises left→right)";
                    } else if (mLevel.tiles[ti].slope == SlopeType::DiagUpRight) {
                        next = SlopeType::DiagUpLeft;  label = "DiagUpLeft  (rises right→left)";
                    } else {
                        next = SlopeType::None;         label = "slope removed";
                    }
                    mLevel.tiles[ti].slope = next;
                    // Slope, Prop, Ladder, and Action are mutually exclusive
                    if (next != SlopeType::None) { mLevel.tiles[ti].prop = false; mLevel.tiles[ti].ladder = false; mLevel.tiles[ti].action = false; }
                    SetStatus(std::string("Tile ") + std::to_string(ti) + " → " + label);
                }
                return true;
            }
            case Tool::Hitbox: {
                // Click a tile to select it for hitbox editing.
                // Click empty space to deselect.
                // Drag handles are handled in MOUSE_MOTION.
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    mHitboxTileIdx = ti;
                    auto& t = mLevel.tiles[ti];
                    // Initialise hitbox to full tile on first use
                    if (t.hitboxW == 0 && t.hitboxH == 0) {
                        t.hitboxOffX = 0; t.hitboxOffY = 0;
                        t.hitboxW = t.w; t.hitboxH = t.h;
                    }
                    SetStatus("Hitbox: tile " + std::to_string(ti) +
                              "  drag edges to adjust");
                } else if (mHoverHitboxHdl == HitboxHandle::None) {
                    mHitboxTileIdx = -1; // deselect
                }
                // Start drag if hovering a handle
                if (mHitboxTileIdx >= 0 && mHoverHitboxHdl != HitboxHandle::None) {
                    auto& t = mLevel.tiles[mHitboxTileIdx];
                    mHitboxDragging  = true;
                    mHitboxHandle    = mHoverHitboxHdl;
                    mHitboxDragX     = mx;
                    mHitboxDragY     = my;
                    mHitboxOrigOffX  = t.hitboxOffX;
                    mHitboxOrigOffY  = t.hitboxOffY;
                    mHitboxOrigW     = t.hitboxW;
                    mHitboxOrigH     = t.hitboxH;
                }
                return true;
            }
            case Tool::Hazard: {
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    bool nowHazard = !mLevel.tiles[ti].hazard;
                    mLevel.tiles[ti].hazard = nowHazard;
                    if (nowHazard) {
                        mLevel.tiles[ti].prop   = false;
                        mLevel.tiles[ti].ladder = false;
                        mLevel.tiles[ti].action = false;
                        mLevel.tiles[ti].slope  = SlopeType::None;
                    }
                    SetStatus(std::string("Tile ") + std::to_string(ti) +
                              (nowHazard ? " → hazard (30 HP/sec)" : " → solid (hazard removed)"));
                }
                return true;
            }
            case Tool::AntiGrav: {
                // Toggle antiGravity on tiles. Enemies are toggled via HitEnemy.
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    bool now = !mLevel.tiles[ti].antiGravity;
                    mLevel.tiles[ti].antiGravity = now;
                    SetStatus("Tile " + std::to_string(ti) +
                              (now ? " → floating (anti-gravity)" : " → normal gravity"));
                    return true;
                }
                int ei = HitEnemy(mx, my);
                if (ei >= 0) {
                    bool now = !mLevel.enemies[ei].antiGravity;
                    mLevel.enemies[ei].antiGravity = now;
                    SetStatus("Enemy " + std::to_string(ei) +
                              (now ? " → floating" : " → normal gravity"));
                    return true;
                }
                return true;
            }
            case Tool::Select: {
                // Click on a selected tile: start moving. Click elsewhere: start rubber-band.
                auto [wx, wy] = ScreenToWorld(mx, my);
                int ti = HitTile(mx, my);
                bool hitSelected = (ti >= 0 && std::find(mSelIndices.begin(), mSelIndices.end(), ti) != mSelIndices.end());
                if (hitSelected) {
                    // Begin drag-move of entire selection
                    mSelDragging    = true;
                    mSelDragStartWX = wx;
                    mSelDragStartWY = wy;
                    mSelOrigPositions.clear();
                    for (int idx : mSelIndices)
                        mSelOrigPositions.push_back({mLevel.tiles[idx].x, mLevel.tiles[idx].y});
                } else {
                    // Click on unselected tile: select just it (or begin rubber-band on empty space)
                    if (ti >= 0) {
                        // Shift-click: add/remove from selection
                        if (SDL_GetModState() & SDL_KMOD_SHIFT) {
                            auto it = std::find(mSelIndices.begin(), mSelIndices.end(), ti);
                            if (it != mSelIndices.end()) mSelIndices.erase(it);
                            else                         mSelIndices.push_back(ti);
                        } else {
                            mSelIndices = {ti};
                        }
                        SetStatus("Selected " + std::to_string(mSelIndices.size()) + " tile(s)");
                    } else {
                        // Empty space: begin rubber-band, clear selection unless Shift
                        if (!(SDL_GetModState() & SDL_KMOD_SHIFT)) mSelIndices.clear();
                        mSelBoxing = true;
                        mSelBoxX0 = mSelBoxX1 = wx;
                        mSelBoxY0 = mSelBoxY1 = wy;
                    }
                }
                return true;
            }
            case Tool::Resize:
                // Mouse-down on a resize handle starts the drag
                if (mHoverTileIdx >= 0 && mHoverEdge != ResizeEdge::None) {
                    mIsResizing    = true;
                    mResizeTileIdx = mHoverTileIdx;
                    mResizeEdge    = mHoverEdge;
                    mResizeDragX   = mx;
                    mResizeDragY   = my;
                    mResizeOrigW   = mLevel.tiles[mHoverTileIdx].w;
                    mResizeOrigH   = mLevel.tiles[mHoverTileIdx].h;
                    SetStatus("Resizing tile "+std::to_string(mHoverTileIdx));
                    return true;
                }
                break;
        }

        if (mActiveTool != Tool::Erase) {
            int ti=HitTile(mx,my);  if(ti>=0){mIsDragging=true;mDragIndex=ti;mDragIsTile=true; mDragIsCoin=false;return true;}
            int ci=HitCoin(mx,my);  if(ci>=0){mIsDragging=true;mDragIndex=ci;mDragIsCoin=true; mDragIsTile=false;return true;}
            int ei=HitEnemy(mx,my); if(ei>=0){mIsDragging=true;mDragIndex=ei;mDragIsCoin=false;mDragIsTile=false;return true;}
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        mIsDragging = false;
        if (mIsResizing) {
            mIsResizing    = false;
            mResizeTileIdx = -1;
            SetStatus("Resize committed");
        }
        if (mHitboxDragging) {
            mHitboxDragging = false;
            mHitboxHandle   = HitboxHandle::None;
            if (mHitboxTileIdx >= 0) {
                auto& t = mLevel.tiles[mHitboxTileIdx];
                SetStatus("Hitbox: off(" + std::to_string(t.hitboxOffX) + ","
                          + std::to_string(t.hitboxOffY) + ") size("
                          + std::to_string(t.hitboxW) + "x"
                          + std::to_string(t.hitboxH) + ")");
            }
        }
        // Commit rubber-band selection
        if (mSelBoxing) {
            mSelBoxing = false;
            int rx0 = std::min(mSelBoxX0, mSelBoxX1);
            int ry0 = std::min(mSelBoxY0, mSelBoxY1);
            int rx1 = std::max(mSelBoxX0, mSelBoxX1);
            int ry1 = std::max(mSelBoxY0, mSelBoxY1);
            if (!(SDL_GetModState() & SDL_KMOD_SHIFT)) mSelIndices.clear();
            for (int i = 0; i < (int)mLevel.tiles.size(); i++) {
                const auto& t = mLevel.tiles[i];
                // Include tile if it overlaps the marquee rect
                if ((int)t.x + t.w > rx0 && (int)t.x < rx1 &&
                    (int)t.y + t.h > ry0 && (int)t.y < ry1) {
                    if (std::find(mSelIndices.begin(), mSelIndices.end(), i) == mSelIndices.end())
                        mSelIndices.push_back(i);
                }
            }
            SetStatus("Selected " + std::to_string(mSelIndices.size()) + " tile(s)");
        }
        if (mSelDragging) {
            mSelDragging = false;
            SetStatus("Moved " + std::to_string(mSelIndices.size()) + " tile(s)");
        }
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        int mx=(int)e.motion.x, my=(int)e.motion.y;

        // ── Selection rubber-band / drag-move ────────────────────────────────────
        if (mSelBoxing) {
            auto [wx, wy] = ScreenToWorld(mx, my);
            mSelBoxX1 = wx; mSelBoxY1 = wy;
            return true;
        }
        if (mSelDragging && !mSelOrigPositions.empty()) {
            auto [wx, wy] = ScreenToWorld(mx, my);
            int dx = wx - mSelDragStartWX;
            int dy = wy - mSelDragStartWY;
            for (int i = 0; i < (int)mSelIndices.size(); i++) {
                int idx = mSelIndices[i];
                if (idx < (int)mLevel.tiles.size()) {
                    // Snap each tile's displaced position to grid
                    float rawX = mSelOrigPositions[i].first  + dx;
                    float rawY = mSelOrigPositions[i].second + dy;
                    mLevel.tiles[idx].x = (float)((int(rawX) / GRID) * GRID);
                    mLevel.tiles[idx].y = (float)((int(rawY) / GRID) * GRID);
                }
            }
            return true;
        }

        // ── Resize drag ───────────────────────────────────────────────────────
        if (mIsResizing && mResizeTileIdx >= 0 && mResizeTileIdx < (int)mLevel.tiles.size()) {
            auto& t = mLevel.tiles[mResizeTileIdx];
            int dx = mx - mResizeDragX;
            int dy = my - mResizeDragY;
            if (mResizeEdge == ResizeEdge::Right || mResizeEdge == ResizeEdge::Corner) {
                int newW = std::max(GRID, ((mResizeOrigW + dx + GRID/2) / GRID) * GRID);
                t.w = newW;
            }
            if (mResizeEdge == ResizeEdge::Bottom || mResizeEdge == ResizeEdge::Corner) {
                int newH = std::max(GRID, ((mResizeOrigH + dy + GRID/2) / GRID) * GRID);
                t.h = newH;
            }
            SetStatus("Resize: "+std::to_string(t.w)+"x"+std::to_string(t.h));
            return true;
        }

        // ── Hitbox drag update ────────────────────────────────────────────────
        if (mHitboxDragging && mHitboxTileIdx >= 0 && mHitboxTileIdx < (int)mLevel.tiles.size()) {
            auto& t  = mLevel.tiles[mHitboxTileIdx];
            int   dx = mx - mHitboxDragX;
            int   dy = my - mHitboxDragY;
            // Minimum hitbox side: 4px
            constexpr int MIN_SIDE = 4;
            switch (mHitboxHandle) {
                case HitboxHandle::Left: {
                    int newOff = std::min(mHitboxOrigOffX + dx, mHitboxOrigOffX + mHitboxOrigW - MIN_SIDE);
                    int delta  = newOff - mHitboxOrigOffX;
                    t.hitboxOffX = newOff;
                    t.hitboxW    = std::max(MIN_SIDE, mHitboxOrigW - delta);
                    break;
                }
                case HitboxHandle::Right:
                    t.hitboxW = std::max(MIN_SIDE, mHitboxOrigW + dx);
                    // Clamp to tile width
                    t.hitboxW = std::min(t.hitboxW, t.w - t.hitboxOffX);
                    break;
                case HitboxHandle::Top: {
                    int newOff = std::min(mHitboxOrigOffY + dy, mHitboxOrigOffY + mHitboxOrigH - MIN_SIDE);
                    int delta  = newOff - mHitboxOrigOffY;
                    t.hitboxOffY = newOff;
                    t.hitboxH    = std::max(MIN_SIDE, mHitboxOrigH - delta);
                    break;
                }
                case HitboxHandle::Bottom:
                    t.hitboxH = std::max(MIN_SIDE, mHitboxOrigH + dy);
                    t.hitboxH = std::min(t.hitboxH, t.h - t.hitboxOffY);
                    break;
                case HitboxHandle::TopLeft: {
                    int newOffX = std::min(mHitboxOrigOffX + dx, mHitboxOrigOffX + mHitboxOrigW - MIN_SIDE);
                    int newOffY = std::min(mHitboxOrigOffY + dy, mHitboxOrigOffY + mHitboxOrigH - MIN_SIDE);
                    t.hitboxW   = std::max(MIN_SIDE, mHitboxOrigW - (newOffX - mHitboxOrigOffX));
                    t.hitboxH   = std::max(MIN_SIDE, mHitboxOrigH - (newOffY - mHitboxOrigOffY));
                    t.hitboxOffX = newOffX; t.hitboxOffY = newOffY;
                    break;
                }
                case HitboxHandle::TopRight: {
                    int newOffY = std::min(mHitboxOrigOffY + dy, mHitboxOrigOffY + mHitboxOrigH - MIN_SIDE);
                    t.hitboxW   = std::max(MIN_SIDE, mHitboxOrigW + dx);
                    t.hitboxH   = std::max(MIN_SIDE, mHitboxOrigH - (newOffY - mHitboxOrigOffY));
                    t.hitboxOffY = newOffY;
                    break;
                }
                case HitboxHandle::BotLeft: {
                    int newOffX = std::min(mHitboxOrigOffX + dx, mHitboxOrigOffX + mHitboxOrigW - MIN_SIDE);
                    t.hitboxW   = std::max(MIN_SIDE, mHitboxOrigW - (newOffX - mHitboxOrigOffX));
                    t.hitboxH   = std::max(MIN_SIDE, mHitboxOrigH + dy);
                    t.hitboxOffX = newOffX;
                    break;
                }
                case HitboxHandle::BotRight:
                    t.hitboxW = std::max(MIN_SIDE, mHitboxOrigW + dx);
                    t.hitboxH = std::max(MIN_SIDE, mHitboxOrigH + dy);
                    break;
                default: break;
            }
            // Clamp offsets to stay inside the visual tile
            t.hitboxOffX = std::max(0, std::min(t.hitboxOffX, t.w - MIN_SIDE));
            t.hitboxOffY = std::max(0, std::min(t.hitboxOffY, t.h - MIN_SIDE));
            SetStatus("Hitbox: off(" + std::to_string(t.hitboxOffX) + ","
                      + std::to_string(t.hitboxOffY) + ") size("
                      + std::to_string(t.hitboxW) + "x" + std::to_string(t.hitboxH) + ")");
            return true;
        }

        // ── Hitbox handle hover detection ─────────────────────────────────────
        if (mActiveTool == Tool::Hitbox && mHitboxTileIdx >= 0 && !mHitboxDragging) {
            mHoverHitboxHdl = HitboxHandle::None;
            const auto& t   = mLevel.tiles[mHitboxTileIdx];
            // Hitbox is in world space; convert to screen space for mouse comparison
            int hx = (int)(t.x - mCamX) + t.hitboxOffX;
            int hy = (int)(t.y - mCamY) + t.hitboxOffY;
            int hw = t.hitboxW;
            int hh = t.hitboxH;
            const int H = HBOX_HANDLE;
            bool nearL = (mx >= hx - H      && mx <= hx + H);
            bool nearR = (mx >= hx + hw - H && mx <= hx + hw + H);
            bool nearT = (my >= hy - H      && my <= hy + H);
            bool nearB = (my >= hy + hh - H && my <= hy + hh + H);
            bool inH   = (mx >= hx && mx <= hx + hw);
            bool inV   = (my >= hy && my <= hy + hh);
            if      (nearL && nearT)          mHoverHitboxHdl = HitboxHandle::TopLeft;
            else if (nearR && nearT)          mHoverHitboxHdl = HitboxHandle::TopRight;
            else if (nearL && nearB)          mHoverHitboxHdl = HitboxHandle::BotLeft;
            else if (nearR && nearB)          mHoverHitboxHdl = HitboxHandle::BotRight;
            else if (nearL && inV)            mHoverHitboxHdl = HitboxHandle::Left;
            else if (nearR && inV)            mHoverHitboxHdl = HitboxHandle::Right;
            else if (nearT && inH)            mHoverHitboxHdl = HitboxHandle::Top;
            else if (nearB && inH)            mHoverHitboxHdl = HitboxHandle::Bottom;
        }

        // ── Resize hover detection ────────────────────────────────────────────
        if (mActiveTool == Tool::Resize && my >= TOOLBAR_H && mx < CanvasW()) {
            mHoverEdge    = ResizeEdge::None;
            mHoverTileIdx = -1;
            for (int i = 0; i < (int)mLevel.tiles.size(); i++) {
                ResizeEdge edge = DetectResizeEdge(i, mx, my);
                if (edge != ResizeEdge::None) {
                    mHoverEdge    = edge;
                    mHoverTileIdx = i;
                    break;
                }
            }
        }

        // ── Entity drag ───────────────────────────────────────────────────────
        if (mIsDragging && mDragIndex >= 0 && my >= TOOLBAR_H && mx < CanvasW()) {
            auto [sx,sy] = SnapToGrid(mx,my);
            if (mDragIsTile  && mDragIndex<(int)mLevel.tiles.size())  { mLevel.tiles[mDragIndex].x=(float)sx; mLevel.tiles[mDragIndex].y=(float)sy; }
            else if (mDragIsCoin && mDragIndex<(int)mLevel.coins.size()) { mLevel.coins[mDragIndex].x=(float)sx; mLevel.coins[mDragIndex].y=(float)sy; }
            else if (!mDragIsCoin&&!mDragIsTile&&mDragIndex<(int)mLevel.enemies.size()) { mLevel.enemies[mDragIndex].x=(float)sx; mLevel.enemies[mDragIndex].y=(float)sy; }
        }
    }

    return true;
}

// ─── Update ──────────────────────────────────────────────────────────────────
void LevelEditorScene::Update(float /*dt*/) {
    if (!mIsPanning) return;
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    mCamX = mPanCamStartX - (mx - mPanStartX);
    mCamY = mPanCamStartY - (my - mPanStartY);
    if (mCamX < 0.0f) mCamX = 0.0f;
    if (mCamY < 0.0f) mCamY = 0.0f;
}

// ─── Render ───────────────────────────────────────────────────────────────────
void LevelEditorScene::Render(Window& window) {
    window.Render();
    SDL_Surface* screen = window.GetSurface();
    int cw = CanvasW();

    background->Render(screen);

    // Grid — offset by camera so grid lines stay pinned to world coordinates
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(screen->format);
    Uint32 gridCol = SDL_MapRGBA(fmt,nullptr,255,255,255,20);
    // Grid lines are drawn at world-space multiples of GRID.
    // Convert to screen space: screenX = worldX - camX.
    // First visible world grid line >= camX: ceil(camX/GRID)*GRID
    int camOffX = (int)mCamX;
    int camOffY = (int)mCamY;
    int gridStartX = (camOffX / GRID) * GRID - camOffX;  // screen X of first world grid column
    int gridStartY = (camOffY / GRID) * GRID - camOffY;  // screen Y of first world grid row
    // Clamp so lines never draw over the toolbar
    for (int x = gridStartX; x < cw; x += GRID) { SDL_Rect l={x,TOOLBAR_H,1,window.GetHeight()-TOOLBAR_H}; SDL_FillSurfaceRect(screen,&l,gridCol); }
    for (int y = gridStartY; y < window.GetHeight(); y += GRID) { if (y < TOOLBAR_H) continue; SDL_Rect l={0,y,cw,1}; SDL_FillSurfaceRect(screen,&l,gridCol); }

    // Placed tiles — all positions are in world space; subtract camera offset for screen space
    for (int ti = 0; ti < (int)mLevel.tiles.size(); ti++) {
        const auto& t = mLevel.tiles[ti];
        // Cull tiles fully outside the viewport
        int tsx = (int)(t.x - mCamX);
        int tsy = (int)(t.y - mCamY);
        if (tsx + t.w <= 0 || tsx >= cw || tsy + t.h <= TOOLBAR_H || tsy >= window.GetHeight())
            continue;
        SDL_Surface* ts = nullptr;
        for (const auto& item : mPaletteItems)
            if (item.path == t.imagePath) { ts = item.full ? item.full : item.thumb; break; }
        SDL_Rect dst={tsx, tsy, t.w, t.h};
        if (ts) {
            // Apply rotation: build a rotated copy, blit it, then free it
            SDL_Surface* rotated = (t.rotation != 0) ? RotateSurfaceDeg(ts, t.rotation) : nullptr;
            SDL_Surface* draw    = rotated ? rotated : ts;
            if (t.prop)   SDL_SetSurfaceColorMod(draw, 120, 255, 120);
            if (t.ladder) SDL_SetSurfaceColorMod(draw, 120, 220, 255);
            if (t.action) SDL_SetSurfaceColorMod(draw, 255, 160,  80);
            if (t.hazard)       SDL_SetSurfaceColorMod(draw, 255,  80,  80);
            SDL_BlitSurfaceScaled(draw,nullptr,screen,&dst,SDL_SCALEMODE_LINEAR);
            if (t.prop || t.ladder || t.action || t.hazard) SDL_SetSurfaceColorMod(draw, 255, 255, 255);
            if (rotated) SDL_DestroySurface(rotated);
        } else {
            SDL_Surface* l=IMG_Load(t.imagePath.c_str());
            if(l){
                SDL_Surface* rotated = (t.rotation != 0) ? RotateSurfaceDeg(l, t.rotation) : nullptr;
                SDL_Surface* draw    = rotated ? rotated : l;
                SDL_BlitSurfaceScaled(draw,nullptr,screen,&dst,SDL_SCALEMODE_LINEAR);
                if (rotated) SDL_DestroySurface(rotated);
                SDL_DestroySurface(l);
            } else DrawRect(screen,dst,{80,80,120,200});
        }
        // Outline colour: cyan for ladder, green for prop, orange for action, blue for solid
        SDL_Color outlineCol = t.ladder  ? SDL_Color{0,220,220,255}
                             : t.prop    ? SDL_Color{80,255,80,255}
                             : t.action  ? SDL_Color{255,160,60,255}
                             : t.hazard  ? SDL_Color{255,60,60,255}
                                        : SDL_Color{100,180,255,255};
        DrawOutline(screen,dst,outlineCol);
        // Badges use tsx/tsy (screen space)
        if (t.prop) {
            DrawRect(screen,{tsx+2,tsy+2,14,14},{0,180,0,200});
            Text propBadge("P",SDL_Color{255,255,255,255},tsx+4,tsy+2,10);
            propBadge.Render(screen);
        }
        if (t.ladder) {
            DrawRect(screen,{tsx+2,tsy+2,14,14},{0,160,180,200});
            Text ladderBadge("L",SDL_Color{255,255,255,255},tsx+4,tsy+2,10);
            ladderBadge.Render(screen);
        }
        if (t.action) {
            std::string abadge = (t.actionGroup > 0)
                ? "A" + std::to_string(t.actionGroup) : "A";
            int bw = (t.actionGroup > 0) ? 20 : 14;
            DrawRect(screen,{tsx+2,tsy+2,bw,14},{200,100,0,200});
            Text actionBadge(abadge,SDL_Color{255,255,255,255},tsx+4,tsy+2,10);
            actionBadge.Render(screen);
        }
        if (t.antiGravity) {
            DrawRect(screen,{tsx+2,tsy+2,14,14},{0,180,200,220});
            Text floatBadge("F",SDL_Color{255,255,255,255},tsx+4,tsy+2,10);
            floatBadge.Render(screen);
        }
        if (t.hazard) {
            DrawRect(screen,{tsx+2,tsy+2,14,14},{200,0,0,220});
            Text hazardBadge("H",SDL_Color{255,255,255,255},tsx+4,tsy+2,10);
            hazardBadge.Render(screen);
        }
        if (t.slope != SlopeType::None) {
            int lx0, ly0, lx1, ly1;
            if (t.slope == SlopeType::DiagUpRight) {
                lx0=tsx;       ly0=tsy+t.h;  lx1=tsx+t.w; ly1=tsy;
            } else {
                lx0=tsx;       ly0=tsy;      lx1=tsx+t.w; ly1=tsy+t.h;
            }
            int ddx=lx1-lx0, ddy=ly1-ly0;
            int steps=std::abs(ddx)>std::abs(ddy)?std::abs(ddx):std::abs(ddy);
            if (steps>0) {
                float ssx=(float)ddx/steps, ssy=(float)ddy/steps;
                float ccx=(float)lx0, ccy=(float)ly0;
                for (int s=0;s<=steps;++s) {
                    DrawRect(screen,{(int)ccx,(int)ccy,2,2},{255,220,50,220});
                    ccx+=ssx; ccy+=ssy;
                }
            }
            DrawRect(screen,{tsx+2,tsy+2,14,14},{160,120,0,200});
            std::string badge = (t.slope==SlopeType::DiagUpRight) ? "/" : "\\";
            Text slopeBadge(badge,SDL_Color{255,255,255,255},tsx+5,tsy+2,10);
            slopeBadge.Render(screen);
        }
        if (t.rotation != 0) {
            std::string rbadge = std::to_string(t.rotation);
            int rbw = 22;
            int rbx = tsx + t.w - rbw - 2;
            int rby = tsy + t.h - 14 - 2;
            DrawRect(screen,{rbx,rby,rbw,14},{60,60,180,200});
            Text rotBadge(rbadge,SDL_Color{200,220,255,255},rbx+2,rby+1,9);
            rotBadge.Render(screen);
        }
    }

    // Coins, enemies, player marker — convert world positions to screen space
    auto coinFrames = coinSheet->GetAnimation("Gold_");
    if (!coinFrames.empty())
        for (const auto& c:mLevel.coins){
            int cx=(int)(c.x-mCamX), cy=(int)(c.y-mCamY);
            if (cx+ICON_SIZE<=0||cx>=cw||cy+ICON_SIZE<=TOOLBAR_H||cy>=window.GetHeight()) continue;
            SDL_Rect s=coinFrames[0],d={cx,cy,ICON_SIZE,ICON_SIZE};
            SDL_BlitSurfaceScaled(coinSheet->GetSurface(),&s,screen,&d,SDL_SCALEMODE_LINEAR);
            DrawOutline(screen,d,{255,215,0,255});
        }
    auto slimeFrames=enemySheet->GetAnimation("slimeWalk");
    if (!slimeFrames.empty())
        for (const auto& en:mLevel.enemies){
            int ex=(int)(en.x-mCamX), ey=(int)(en.y-mCamY);
            if (ex+ICON_SIZE<=0||ex>=cw||ey+ICON_SIZE<=TOOLBAR_H||ey>=window.GetHeight()) continue;
            SDL_Rect s=slimeFrames[0],d={ex,ey,ICON_SIZE,ICON_SIZE};
            SDL_BlitSurfaceScaled(enemySheet->GetSurface(),&s,screen,&d,SDL_SCALEMODE_LINEAR);
            SDL_Color eOutline = en.antiGravity ? SDL_Color{0,220,220,255} : SDL_Color{255,80,80,255};
            DrawOutline(screen,d,eOutline);
            if (en.antiGravity) {
                DrawRect(screen,{ex+ICON_SIZE/2-4,ey-10,8,8},{0,200,220,220});
                Text fb("F",SDL_Color{255,255,255,255},ex+ICON_SIZE/2-3,ey-11,8);
                fb.Render(screen);
            }
        }
    // Player marker
    int pmx=(int)(mLevel.player.x-mCamX), pmy=(int)(mLevel.player.y-mCamY);
    DrawRect(screen,{pmx,pmy,PLAYER_STAND_WIDTH,PLAYER_STAND_HEIGHT},{0,200,80,180});
    DrawOutline(screen,{pmx,pmy,PLAYER_STAND_WIDTH,PLAYER_STAND_HEIGHT},{0,255,100,255},2);

    // ── Selection tool feedback ─────────────────────────────────────────────────
    if (!mSelIndices.empty()) {
        // Teal highlight overlay on each selected tile
        for (int idx : mSelIndices) {
            if (idx < 0 || idx >= (int)mLevel.tiles.size()) continue;
            const auto& t = mLevel.tiles[idx];
            int sx = (int)(t.x - mCamX), sy = (int)(t.y - mCamY);
            DrawRect(screen, {sx, sy, t.w, t.h}, {0, 220, 220, 60});
            DrawOutline(screen, {sx, sy, t.w, t.h}, {0, 255, 255, 220}, 2);
        }
        // Bounding box around entire selection
        int bx0 = INT_MAX, by0 = INT_MAX, bx1 = INT_MIN, by1 = INT_MIN;
        for (int idx : mSelIndices) {
            if (idx < 0 || idx >= (int)mLevel.tiles.size()) continue;
            const auto& t = mLevel.tiles[idx];
            bx0 = std::min(bx0, (int)t.x); by0 = std::min(by0, (int)t.y);
            bx1 = std::max(bx1, (int)t.x + t.w); by1 = std::max(by1, (int)t.y + t.h);
        }
        int sbx = bx0 - (int)mCamX, sby = by0 - (int)mCamY;
        DrawOutline(screen, {sbx-2, sby-2, bx1-bx0+4, by1-by0+4}, {0, 255, 255, 180}, 1);
        // Count label
        std::string selLabel = std::to_string(mSelIndices.size()) + " selected";
        DrawRect(screen, {sbx, sby-16, (int)selLabel.size()*7+4, 14}, {0, 60, 70, 200});
        Text selT(selLabel, SDL_Color{0,255,255,255}, sbx+2, sby-15, 10);
        selT.Render(screen);
    }
    // Rubber-band marquee rect
    if (mSelBoxing) {
        int rx0 = (int)(std::min(mSelBoxX0, mSelBoxX1) - mCamX);
        int ry0 = (int)(std::min(mSelBoxY0, mSelBoxY1) - mCamY);
        int rx1 = (int)(std::max(mSelBoxX0, mSelBoxX1) - mCamX);
        int ry1 = (int)(std::max(mSelBoxY0, mSelBoxY1) - mCamY);
        int rw  = rx1 - rx0, rh = ry1 - ry0;
        if (rw > 0 && rh > 0) {
            DrawRect(screen,    {rx0, ry0, rw, rh}, {100, 220, 255, 25});
            DrawOutline(screen, {rx0, ry0, rw, rh}, {100, 220, 255, 200}, 1);
        }
    }

    // ── Resize tool feedback ─────────────────────────────────────────────────
    if (mActiveTool == Tool::Resize) {
        const SDL_Color handleCol  = {255, 180, 0, 220};
        const SDL_Color dragCol    = {255, 220, 80, 180};
        const int HS = RESIZE_HANDLE;

        if (mHoverTileIdx >= 0 && !mIsResizing) {
            const auto& t = mLevel.tiles[mHoverTileIdx];
            int rx=(int)(t.x-mCamX), ry=(int)(t.y-mCamY), rw=t.w, rh=t.h;
            if (mHoverEdge==ResizeEdge::Right || mHoverEdge==ResizeEdge::Corner)
                DrawRect(screen,{rx+rw-HS, ry+HS, HS, rh-HS*2}, handleCol);
            if (mHoverEdge==ResizeEdge::Bottom || mHoverEdge==ResizeEdge::Corner)
                DrawRect(screen,{rx+HS, ry+rh-HS, rw-HS*2, HS}, handleCol);
            if (mHoverEdge==ResizeEdge::Corner)
                DrawRect(screen,{rx+rw-HS, ry+rh-HS, HS, HS}, handleCol);
        }

        if (mIsResizing && mResizeTileIdx >= 0 && mResizeTileIdx < (int)mLevel.tiles.size()) {
            const auto& t = mLevel.tiles[mResizeTileIdx];
            int rx=(int)(t.x-mCamX), ry=(int)(t.y-mCamY);
            DrawOutline(screen,{rx,ry,t.w,t.h},{255,220,80,255},2);
            Text szT(std::to_string(t.w/GRID)+"x"+std::to_string(t.h/GRID)+" tiles",
                     dragCol,rx+4,ry+4,12);
            szT.Render(screen);
        }
    }

    // ── Hitbox tool feedback ───────────────────────────────────────────────────
    if (mActiveTool == Tool::Hitbox && mHitboxTileIdx >= 0
        && mHitboxTileIdx < (int)mLevel.tiles.size()) {
        const auto& t  = mLevel.tiles[mHitboxTileIdx];
        int hx = (int)(t.x - mCamX) + t.hitboxOffX;
        int hy = (int)(t.y - mCamY) + t.hitboxOffY;
        int hw = t.hitboxW;
        int hh = t.hitboxH;

        // Semi-transparent fill inside the hitbox
        DrawRect(screen, {hx, hy, hw, hh}, {80, 160, 255, 40});
        // Bright blue border
        DrawOutline(screen, {hx, hy, hw, hh}, {80, 180, 255, 255}, 2);

        // Draw a faint dashed outline of the visual tile for reference
        DrawOutline(screen, {(int)(t.x-mCamX), (int)(t.y-mCamY), t.w, t.h}, {255, 255, 255, 60}, 1);

        // 8 drag handles: corners + edge midpoints
        const int HS  = HBOX_HANDLE;          // half-side of handle square
        const SDL_Color hcNorm = {80,  180, 255, 220}; // normal
        const SDL_Color hcHov  = {255, 220,  80, 255}; // hovered

        auto hdlColor = [&](HitboxHandle h) -> SDL_Color {
            return (mHoverHitboxHdl == h) ? hcHov : hcNorm;
        };
        auto drawHandle = [&](int cx, int cy, HitboxHandle h) {
            DrawRect(screen, {cx - HS/2, cy - HS/2, HS, HS}, hdlColor(h));
            DrawOutline(screen, {cx - HS/2, cy - HS/2, HS, HS}, {20, 20, 40, 255});
        };

        int cx = hx + hw / 2;
        int cy = hy + hh / 2;
        drawHandle(hx,       hy,       HitboxHandle::TopLeft);
        drawHandle(cx,       hy,       HitboxHandle::Top);
        drawHandle(hx + hw,  hy,       HitboxHandle::TopRight);
        drawHandle(hx,       cy,       HitboxHandle::Left);
        drawHandle(hx + hw,  cy,       HitboxHandle::Right);
        drawHandle(hx,       hy + hh,  HitboxHandle::BotLeft);
        drawHandle(cx,       hy + hh,  HitboxHandle::Bottom);
        drawHandle(hx + hw,  hy + hh,  HitboxHandle::BotRight);

        // Info label in top-left corner of the hitbox
        std::string info = "HB: " + std::to_string(hw) + "x" + std::to_string(hh)
                         + " @(" + std::to_string(t.hitboxOffX) + ","
                         + std::to_string(t.hitboxOffY) + ")";
        DrawRect(screen, {hx, hy - 16, (int)info.size() * 7, 14}, {10, 20, 50, 200});
        Text infoT(info, SDL_Color{180, 220, 255, 255}, hx + 2, hy - 15, 10);
        infoT.Render(screen);
    }

    // Tile ghost — SnapToGrid returns world coords; convert to screen for drawing
    if (mActiveTool==Tool::Tile && !mPaletteItems.empty() && !mPaletteItems[mSelectedTile].isFolder) {
        float fmx,fmy; SDL_GetMouseState(&fmx,&fmy); int mx=(int)fmx,my=(int)fmy;
        if (my>=TOOLBAR_H && mx<cw) {
            auto [wx,wy]=SnapToGrid(mx,my); // world space
            int gsx=(int)(wx-mCamX), gsy=(int)(wy-mCamY); // screen space
            DrawRect(screen,{gsx,gsy,mTileW,mTileH},{100,180,255,60});
            DrawOutline(screen,{gsx,gsy,mTileW,mTileH},{100,180,255,200});
        }
    }

    // ── Toolbar ───────────────────────────────────────────────────────────────
    // Base bar — dark slate
    DrawRect(screen, {0, 0, window.GetWidth(), TOOLBAR_H}, {22, 22, 32, 255});

    // Draw a subtle bottom border on the whole toolbar
    DrawRect(screen, {0, TOOLBAR_H - 1, window.GetWidth(), 1}, {60, 60, 80, 255});

    // Shared button draw helper.
    // active=true   -> bright blue fill + white top accent bar
    // active=false  -> dark slate fill, colored top accent bar per group
    // accentColor is the 3-px top bar that gives each group its identity.
    auto drawBtn = [&](SDL_Rect r, SDL_Color accentColor, Text* lbl,
                       Text* hint, bool active) {
        // Button background
        SDL_Color bg = active ? SDL_Color{50, 100, 210, 255} : SDL_Color{35, 35, 48, 255};
        DrawRect(screen, r, bg);

        // Subtle inner border
        SDL_Color border = active ? SDL_Color{100, 160, 255, 255} : SDL_Color{55, 55, 72, 255};
        DrawOutline(screen, r, border);

        // 3-px colored accent bar at the top of each button
        SDL_Color topBar = active ? SDL_Color{130, 190, 255, 255} : accentColor;
        DrawRect(screen, {r.x + 1, r.y + 1, r.w - 2, 3}, topBar);

        // Label (white) — centered in full button area
        if (lbl) lbl->Render(screen);

        // Keyboard shortcut hint (bottom-right corner, dimmed)
        if (hint) hint->Render(screen);
    };

    // Group accent colours
    constexpr SDL_Color ACCENT_PLACE    = {80,  160, 255, 255}; // blue  — Group 1: place tools
    constexpr SDL_Color ACCENT_MODIFIER = {80,  220, 140, 255}; // green — Group 2: modifiers
    constexpr SDL_Color ACCENT_ACTION   = {200, 160, 60,  255}; // amber — Group 3: actions

    // Group 1 — Place
    drawBtn(btnCoin,        ACCENT_PLACE, lblCoin.get(),   hintCoin.get(),   mActiveTool==Tool::Coin);
    drawBtn(btnEnemy,       ACCENT_PLACE, lblEnemy.get(),  hintEnemy.get(),  mActiveTool==Tool::Enemy);
    drawBtn(btnTile,        ACCENT_PLACE, lblTile.get(),   hintTile.get(),   mActiveTool==Tool::Tile);
    drawBtn(btnErase,       ACCENT_PLACE, lblErase.get(),  hintErase.get(),  mActiveTool==Tool::Erase);
    drawBtn(btnPlayerStart, ACCENT_PLACE, lblPlayer.get(), hintPlayer.get(), mActiveTool==Tool::PlayerStart);
    drawBtn(btnSelect,      ACCENT_PLACE, lblSelect.get(), hintSelect.get(), mActiveTool==Tool::Select);

    // Group 1/2 divider
    int div1x = btnPlayerStart.x + btnPlayerStart.w + BTN_GAP + GRP_GAP/2;
    DrawRect(screen, {div1x, BTN_Y + 4, 1, BTN_H - 8}, {70, 70, 90, 255});

    // Group 2 — Modifiers
    drawBtn(btnProp,    ACCENT_MODIFIER, lblProp.get(),   hintProp.get(),   mActiveTool==Tool::Prop);
    drawBtn(btnLadder,  ACCENT_MODIFIER, lblLadder.get(), hintLadder.get(), mActiveTool==Tool::Ladder);
    drawBtn(btnAction,  ACCENT_MODIFIER, lblAction.get(), hintAction.get(), mActiveTool==Tool::Action);
    drawBtn(btnSlope,   ACCENT_MODIFIER, lblSlope.get(),  hintSlope.get(),  mActiveTool==Tool::Slope);
    drawBtn(btnResize,  ACCENT_MODIFIER, lblResize.get(), hintResize.get(), mActiveTool==Tool::Resize);
    drawBtn(btnHitbox,  ACCENT_MODIFIER, lblHitbox.get(), nullptr,          mActiveTool==Tool::Hitbox);
    drawBtn(btnHazard,   {220, 60,  60,  255}, lblHazard.get(),   nullptr, mActiveTool==Tool::Hazard);
    drawBtn(btnAntiGrav, {0,   180, 200, 255}, lblAntiGrav.get(), nullptr, mActiveTool==Tool::AntiGrav);

    // Group 2/3 divider
    int div2x = btnAntiGrav.x + btnAntiGrav.w + BTN_GAP + GRP_GAP/2;
    DrawRect(screen, {div2x, BTN_Y + 4, 1, BTN_H - 8}, {70, 70, 90, 255});

    // Group 3 — Actions (gravity cycles through 3 modes, special coloring)
    {
        SDL_Color gravAccent = (mLevel.gravityMode == GravityMode::WallRun)  ? SDL_Color{100, 140, 255, 255}
                             : (mLevel.gravityMode == GravityMode::OpenWorld) ? SDL_Color{80,  220, 120, 255}
                                                                              : ACCENT_ACTION;
        drawBtn(btnGravity, gravAccent, lblGravity.get(), nullptr, false);
    }
    drawBtn(btnSave,  ACCENT_ACTION, lblSave.get(),  nullptr, false);
    drawBtn(btnLoad,  ACCENT_ACTION, lblLoad.get(),  nullptr, false);
    drawBtn(btnClear, {220, 80,  80,  255}, lblClear.get(), nullptr, false);
    drawBtn(btnPlay,  {80,  220, 100, 255}, lblPlay.get(),  nullptr, false);
    // Back-to-menu button gets a faint separator then a muted slate color
    {
        int sep = btnPlay.x + btnPlay.w + BTN_GAP + GRP_GAP/2;
        DrawRect(screen, {sep, BTN_Y + 4, 1, BTN_H - 8}, {70, 70, 90, 255});
    }
    drawBtn(btnBack,  {120, 100, 160, 255}, lblBack.get(),  nullptr, false);

    // Status bar below the toolbar
    DrawRect(screen, {0, TOOLBAR_H, cw, 20}, {16, 16, 24, 230});
    // Active tool indicator in status bar (right-aligned, gold)
    if (lblTool) {
        // "Tool:" prefix label
        int tx = window.GetWidth() - PALETTE_W - 8;
        // We reposition lblTool to be right-aligned each frame
        Text toolPfx("Tool:", SDL_Color{120,120,150,255},
                     tx - 80, TOOLBAR_H + 3, 12);
        toolPfx.Render(screen);
        // Reposition lblTool dynamically
        lblTool->SetPosition(tx - 40, TOOLBAR_H + 3);
        lblTool->Render(screen);
    }
    if (lblStatus) lblStatus->Render(screen);

    // ── Palette panel ─────────────────────────────────────────────────────────
    DrawRect(screen,{cw,0,PALETTE_W,window.GetHeight()},{20,20,30,255});
    DrawOutline(screen,{cw,0,PALETTE_W,window.GetHeight()},{60,60,80,255});

    // Tab bar
    {
        int hw=PALETTE_W/2;
        bool ta=(mActiveTab==PaletteTab::Tiles);
        SDL_Rect r0={cw,TOOLBAR_H,hw,TAB_H}, r1={cw+hw,TOOLBAR_H,hw,TAB_H};
        SDL_Color ac={50,100,200,255}, ic={30,30,45,255}, bc={80,120,200,255};
        DrawRect(screen,r0,ta?ac:ic); DrawRect(screen,r1,!ta?ac:ic);
        DrawOutline(screen,r0,bc); DrawOutline(screen,r1,bc);
        auto[tx,ty]=Text::CenterInRect("Tiles",      11,r0); Text t0("Tiles",      SDL_Color{(Uint8)(ta?255:160),255,255,255},tx,ty,11); t0.Render(screen);
        auto[bx,by]=Text::CenterInRect("Backgrounds",11,r1); Text t1("Backgrounds",SDL_Color{(Uint8)(!ta?255:160),255,255,255},bx,by,11); t1.Render(screen);
    }

    int palY = TOOLBAR_H + TAB_H;

    if (mActiveTab == PaletteTab::Tiles) {
        // ── Breadcrumb / header ───────────────────────────────────────────────
        DrawRect(screen,{cw,palY,PALETTE_W,44},{30,30,45,255});

        // Show current folder relative to root
        std::string loc = mTileCurrentDir;
        if (loc.rfind(TILE_ROOT, 0) == 0) loc = loc.substr(std::string(TILE_ROOT).size());
        if (loc.empty() || loc == "/") loc = "/";
        Text hdr("Tiles"+loc, SDL_Color{200,200,220,255}, cw+4, palY+4,  10);
        Text hint("Size: "+std::to_string(mTileW)+"  Esc=up  Click=enter", SDL_Color{100,120,140,255}, cw+4, palY+18, 9);
        Text hint2("Click folder to open", SDL_Color{100,120,140,255}, cw+4, palY+30, 9);
        hdr.Render(screen); hint.Render(screen); hint2.Render(screen);
        palY += 44;

        // Grid of items
        constexpr int PAD=4, LBL_H=14;
        const int cellW=(PALETTE_W-PAD*(PAL_COLS+1))/PAL_COLS;
        const int cellH=cellW+LBL_H;
        const int itemH=cellH+PAD;
        const int visRows=(window.GetHeight()-palY)/itemH;
        const int startI=mPaletteScroll*PAL_COLS;
        const int endI=std::min(startI+(visRows+1)*PAL_COLS,(int)mPaletteItems.size());

        for (int i=startI;i<endI;i++) {
            int col=(i-startI)%PAL_COLS;
            int row=(i-startI)/PAL_COLS;
            int ix=cw+PAD+col*(cellW+PAD);
            int iy=palY+PAD+row*itemH;
            const auto& item=mPaletteItems[i];
            bool sel=(i==mSelectedTile && !item.isFolder && mActiveTool==Tool::Tile);

            SDL_Rect cell={ix,iy,cellW,cellH};

            if (item.isFolder) {
                // ── Folder cell: warm amber tint ──────────────────────────────
                SDL_Color folderBg  = (item.label.rfind("◀",0)==0)
                                    ? SDL_Color{35,50,35,220}   // back arrow: greenish
                                    : SDL_Color{55,45,20,220};  // folder: amber
                SDL_Color folderBdr = (item.label.rfind("◀",0)==0)
                                    ? SDL_Color{80,200,80,255}
                                    : SDL_Color{200,160,60,255};
                DrawRect(screen,cell,folderBg);
                DrawOutline(screen,cell,folderBdr);

                // Optional preview thumbnail (first PNG inside folder)
                if (item.thumb) {
                    SDL_Rect imgDst={ix+1,iy+1,cellW-2,cellW-2};
                    SDL_SetSurfaceColorMod(item.thumb,120,100,60); // darken for folder feel
                    SDL_BlitSurfaceScaled(item.thumb,nullptr,screen,&imgDst,SDL_SCALEMODE_LINEAR);
                    SDL_SetSurfaceColorMod(item.thumb,255,255,255);
                } else {
                    // Folder icon: simple rectangle grid visual
                    DrawRect(screen,{ix+cellW/2-14,iy+8,28,20},{200,160,60,180});
                    DrawRect(screen,{ix+cellW/2-14,iy+4,12,8},{200,160,60,180});
                }

                // Label
                std::string lbl=item.label;
                if((int)lbl.size()>9) lbl=lbl.substr(0,8)+"~";
                Text lblT(lbl,SDL_Color{220,180,80,255},ix+2,iy+cellW+2,9);
                lblT.Render(screen);

            } else {
                // ── File cell ─────────────────────────────────────────────────
                DrawRect(screen,cell,sel?SDL_Color{50,100,200,220}:SDL_Color{35,35,55,220});
                DrawOutline(screen,cell,sel?SDL_Color{100,180,255,255}:SDL_Color{55,55,80,255});

                SDL_Surface* ts=item.thumb?item.thumb:item.full;
                if(ts){SDL_Rect imgDst={ix+1,iy+1,cellW-2,cellW-2};SDL_BlitSurfaceScaled(ts,nullptr,screen,&imgDst,SDL_SCALEMODE_LINEAR);}
                else  DrawRect(screen,{ix+1,iy+1,cellW-2,cellW-2},{60,40,80,255});

                std::string lbl=item.label;
                if((int)lbl.size()>9)lbl=lbl.substr(0,8)+"~";
                Text lblT(lbl,SDL_Color{(Uint8)(sel?255:170),(Uint8)(sel?255:170),(Uint8)(sel?255:190),255},ix+2,iy+cellW+2,9);
                lblT.Render(screen);
            }
        }

        // Scroll indicator
        int totalRows=((int)mPaletteItems.size()+PAL_COLS-1)/PAL_COLS;
        if(totalRows>visRows){
            float pct=(float)mPaletteScroll/std::max(1,totalRows-visRows);
            int sh=std::max(20,(int)((window.GetHeight()-palY)*visRows/(float)totalRows));
            int sy2=palY+(int)((window.GetHeight()-palY-sh)*pct);
            DrawRect(screen,{cw+PALETTE_W-4,sy2,3,sh},{100,150,255,180});
        }

    } else {
        // ── Backgrounds palette ───────────────────────────────────────────────
        DrawRect(screen,{cw,palY,PALETTE_W,24},{30,30,45,255});
        Text bgHdr("Backgrounds  (I=import)",SDL_Color{200,200,220,255},cw+4,palY+6,10);
        bgHdr.Render(screen);
        palY+=24;

        constexpr int PAD=4, LBL_H=16;
        const int thumbW=PALETTE_W-PAD*2;
        const int thumbH=thumbW/2;
        const int itemH=thumbH+LBL_H+PAD;
        int vis=(window.GetHeight()-palY)/itemH;
        int startI=mBgPaletteScroll;
        int endI=std::min(startI+vis+1,(int)mBgItems.size());

        for(int i=startI;i<endI;i++){
            int iy=palY+PAD+(i-startI)*itemH;
            bool sel=(i==mSelectedBg);
            SDL_Rect cell={cw+PAD,iy,thumbW,thumbH+LBL_H};
            DrawRect(screen,cell,sel?SDL_Color{50,100,200,220}:SDL_Color{35,35,55,220});
            DrawOutline(screen,cell,sel?SDL_Color{100,220,255,255}:SDL_Color{55,55,80,255},sel?2:1);
            SDL_Rect imgDst={cw+PAD+1,iy+1,thumbW-2,thumbH-2};
            if(mBgItems[i].thumb)SDL_BlitSurfaceScaled(mBgItems[i].thumb,nullptr,screen,&imgDst,SDL_SCALEMODE_LINEAR);
            else DrawRect(screen,imgDst,{40,40,70,255});
            std::string lbl=mBgItems[i].label; if((int)lbl.size()>14)lbl=lbl.substr(0,13)+"~";
            Text lblT(lbl,SDL_Color{(Uint8)(sel?255:170),(Uint8)(sel?255:170),(Uint8)(sel?255:190),255},cw+PAD+2,iy+thumbH+2,10);
            lblT.Render(screen);
        }
        if((int)mBgItems.size()>vis){
            float pct=(float)mBgPaletteScroll/std::max(1,(int)mBgItems.size()-vis);
            int sh=std::max(20,(int)((window.GetHeight()-palY)*vis/(float)mBgItems.size()));
            int sy2=palY+(int)((window.GetHeight()-palY-sh)*pct);
            DrawRect(screen,{cw+PALETTE_W-4,sy2,3,sh},{100,150,255,180});
        }
    }

    // ── Bottom hint bar ────────────────────────────────────────────────────────
    DrawRect(screen, {0, window.GetHeight()-22, cw, 22}, {16,16,24,220});
    std::string cnt = std::to_string(mLevel.coins.size()) + " coins  "
                    + std::to_string(mLevel.enemies.size()) + " enemies  "
                    + std::to_string(mLevel.tiles.size()) + " tiles";
    Text cntT(cnt, SDL_Color{120,120,150,255}, 8, window.GetHeight()-18, 11);
    cntT.Render(screen);
    std::string camStr = "Cam: " + std::to_string((int)mCamX) + "," + std::to_string((int)mCamY);
    Text camT(camStr, SDL_Color{70,70,90,255}, cw - 100, window.GetHeight()-18, 11);
    camT.Render(screen);
    Text hintT("RClick:rotate  MMB:pan  G:Mode  I:Import  Ctrl+S:Save  Ctrl+Z:Undo",
               SDL_Color{70,70,90,255}, cw/2 - 170, window.GetHeight()-18, 11);
    hintT.Render(screen);

    // ── Import input bar ──────────────────────────────────────────────────────
    if (mImportInputActive) {
        int panelH=44, panelY=window.GetHeight()-24-panelH;
        DrawRect(screen,{0,panelY,cw,panelH},{10,20,50,240});
        DrawOutline(screen,{0,panelY,cw,panelH},{80,180,255,255},2);
        std::string dest=(mActiveTab==PaletteTab::Backgrounds)?"game_assets/backgrounds/":"game_assets/tiles/";
        Text il("Import into "+dest+"  — file or folder path  (Enter=go, Esc=cancel)",SDL_Color{140,200,255,255},8,panelY+4,11);
        il.Render(screen);
        int fx=8,fy=panelY+18,fw=cw-16,fh=20;
        DrawRect(screen,{fx,fy,fw,fh},{20,35,80,255}); DrawOutline(screen,{fx,fy,fw,fh},{80,180,255,200});
        Text it(mImportInputText+"|",SDL_Color{255,255,255,255},fx+4,fy+2,12);
        it.Render(screen);
    }

    // ── Drop overlay ──────────────────────────────────────────────────────────
    if (mDropActive) {
        DrawRect(screen,{0,TOOLBAR_H,cw,window.GetHeight()-TOOLBAR_H},{20,80,160,80});
        constexpr int B=6; SDL_Color bc={80,180,255,220};
        DrawRect(screen,{0,TOOLBAR_H,cw,B},bc); DrawRect(screen,{0,window.GetHeight()-B,cw,B},bc);
        DrawRect(screen,{0,TOOLBAR_H,B,window.GetHeight()-TOOLBAR_H},bc);
        DrawRect(screen,{cw-B,TOOLBAR_H,B,window.GetHeight()-TOOLBAR_H},bc);
        int cx2=cw/2,cy2=window.GetHeight()/2;
        DrawRect(screen,{cx2-220,cy2-44,440,88},{10,30,70,220});
        DrawOutline(screen,{cx2-220,cy2-44,440,88},{80,180,255,255},2);
        std::string hint=(mActiveTab==PaletteTab::Backgrounds)?"Drop .png or folder → backgrounds":"Drop .png or folder → tiles";
        Text d1(hint,SDL_Color{255,255,255,255},cx2-168,cy2-32,24); d1.Render(screen);
        Text d2("Folders become subfolders in the palette",SDL_Color{140,200,255,255},cx2-150,cy2+4,16); d2.Render(screen);
    }

    window.Update();
}

// ─── NextScene ────────────────────────────────────────────────────────────────
std::unique_ptr<Scene> LevelEditorScene::NextScene() {
    if (mLaunchGame) { mLaunchGame=false; return std::make_unique<GameScene>("levels/"+mLevelName+".json", true); }
    if (mGoBack)     { mGoBack=false;     return std::make_unique<TitleScene>(); }
    return nullptr;
}

// ─── ImportPath ───────────────────────────────────────────────────────────────
bool LevelEditorScene::ImportPath(const std::string& srcPath) {
    fs::path src(srcPath);

    // ── Directory import ──────────────────────────────────────────────────────
    if (fs::is_directory(src)) {
        if (mActiveTab == PaletteTab::Backgrounds) {
            // Backgrounds doesn't support folders — import all PNGs flat
            int count = 0;
            for (const auto& entry : fs::directory_iterator(src)) {
                if (entry.path().extension() == ".png")
                    count += ImportPath(entry.path().string()) ? 1 : 0;
            }
            SetStatus("Imported " + std::to_string(count) + " backgrounds from " + src.filename().string());
            return count > 0;
        }

        // Tiles: copy the whole folder into game_assets/tiles/<foldername>/
        fs::path destDir = fs::path(TILE_ROOT) / src.filename();
        std::error_code ec;
        fs::create_directories(destDir, ec);
        if (ec) { SetStatus("Import failed: can't create " + destDir.string()); return false; }

        int count = 0;
        for (const auto& entry : fs::directory_iterator(src)) {
            if (entry.path().extension() != ".png") continue;
            fs::path dest = destDir / entry.path().filename();
            if (!fs::exists(dest)) {
                fs::copy_file(entry.path(), dest, ec);
                if (ec) continue;
            }
            count++;
        }

        if (count == 0) { SetStatus("No PNGs found in " + src.filename().string()); return false; }

        // Reload the tile view — navigate into the new folder
        LoadTileView(destDir.string());
        SetStatus("Imported folder: " + src.filename().string() +
                  " (" + std::to_string(count) + " tiles) — now browsing it");
        return true;
    }

    // ── Single file import ────────────────────────────────────────────────────
    std::string ext = src.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".png") { SetStatus("Import failed: only .png supported (got " + ext + ")"); return false; }

    bool isBg = (mActiveTab == PaletteTab::Backgrounds);
    std::string destDirStr = isBg ? BG_ROOT : TILE_ROOT;

    // If we're currently browsing a subfolder, import into that subfolder
    if (!isBg && mTileCurrentDir != TILE_ROOT)
        destDirStr = mTileCurrentDir;

    fs::path destDir(destDirStr);
    std::error_code ec;
    fs::create_directories(destDir, ec);
    if (ec) { SetStatus("Import failed: can't create " + destDirStr); return false; }

    fs::path dest = destDir / src.filename();
    if (!fs::exists(dest)) {
        fs::copy_file(src, dest, ec);
        if (ec) { SetStatus("Import failed: " + ec.message()); return false; }
    }

    if (isBg) {
        SDL_Surface* full = LoadPNG(dest);
        if (!full) { SetStatus("Import failed: can't load " + dest.string()); return false; }
        const int tw = PALETTE_W-8, th = tw/2;
        SDL_Surface* thumb = MakeThumb(full, tw, th);
        SDL_DestroySurface(full);
        mBgItems.push_back({dest.string(), dest.stem().string(), thumb});
        mBgPaletteScroll = std::max(0,(int)mBgItems.size()-1);
        ApplyBackground((int)mBgItems.size()-1);
        SetStatus("Imported & applied: " + dest.filename().string());
    } else {
        SDL_Surface* full = LoadPNG(dest);
        if (!full) { SetStatus("Import failed: can't load " + dest.string()); return false; }
        SDL_SetSurfaceBlendMode(full, SDL_BLENDMODE_BLEND);
        SDL_Surface* thumb = MakeThumb(full, PAL_ICON, PAL_ICON);

        // Reload the current folder view so the new file appears in the right place
        // with correct sorting (simpler than inserting into the right position)
        LoadTileView(mTileCurrentDir);

        // Find and select the newly added item
        for (int i=0;i<(int)mPaletteItems.size();i++) {
            if (mPaletteItems[i].path == dest.string()) {
                mSelectedTile = i;
                // Scroll to show it
                int row = i / PAL_COLS;
                mPaletteScroll = std::max(0, row);
                break;
            }
        }
        // Free the surfaces we loaded manually — LoadTileView built its own
        if (thumb) SDL_DestroySurface(thumb);
        SDL_DestroySurface(full);

        mActiveTool = Tool::Tile;
        if (lblTool) lblTool->CreateSurface("Tile");
        SetStatus("Imported: " + dest.filename().string() + " -> auto-selected");
    }
    return true;
}
