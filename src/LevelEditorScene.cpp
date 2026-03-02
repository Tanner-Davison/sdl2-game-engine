#include "LevelEditorScene.hpp"
#include "GameScene.hpp"
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

    if (mLevel.coins.empty() && mLevel.enemies.empty() && mLevel.tiles.empty()) {
        std::string autoPath = "levels/" + mLevelName + ".json";
        if (fs::exists(autoPath)) {
            LoadLevel(autoPath, mLevel);
            SetStatus("Resumed: " + autoPath);
            if (!mLevel.background.empty())
                background = std::make_unique<Image>(mLevel.background, nullptr, FitMode::PRESCALED);
        }
    }

    if (mLevel.player.x == 0 && mLevel.player.y == 0) {
        mLevel.player.x = static_cast<float>(CanvasW() / 2 - 16);
        mLevel.player.y = static_cast<float>(window.GetHeight() - 60);
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

    // ── Toolbar ───────────────────────────────────────────────────────────────
    int bw=80, bh=44, pad=6, sx=pad, y0=8;
    auto nb = [&]() -> SDL_Rect { SDL_Rect r={sx,y0,bw,bh}; sx+=bw+pad; return r; };
    btnCoin=nb(); btnEnemy=nb(); btnTile=nb(); btnResize=nb(); btnProp=nb(); btnLadder=nb(); btnAction=nb(); btnSlope=nb(); btnErase=nb(); btnPlayerStart=nb();
    sx+=pad; btnSave=nb(); btnLoad=nb(); sx+=pad; btnClear=nb(); sx+=pad; btnPlay=nb(); sx+=pad; btnGravity=nb();

    auto mkLbl = [](const std::string& s, SDL_Rect r) {
        auto [x,y] = Text::CenterInRect(s, 13, r);
        return std::make_unique<Text>(s, SDL_Color{0,0,0,255}, x, y, 13);
    };
    lblCoin=mkLbl("Coin",btnCoin); lblEnemy=mkLbl("Enemy",btnEnemy);
    lblTile=mkLbl("Tile",btnTile); lblResize=mkLbl("Resize",btnResize);
    lblProp=mkLbl("Prop",btnProp); lblLadder=mkLbl("Ladder",btnLadder); lblAction=mkLbl("Action",btnAction); lblSlope=mkLbl("Slope",btnSlope); lblErase=mkLbl("Erase",btnErase);
    lblPlayer=mkLbl("Player",btnPlayerStart); lblSave=mkLbl("Save",btnSave);
    lblLoad=mkLbl("Load",btnLoad); lblClear=mkLbl("Clear",btnClear);
    lblPlay=mkLbl("Play",btnPlay);
    // Gravity button label reflects current mode — updated whenever mode changes
    {
        std::string gLbl = (mLevel.gravityMode == GravityMode::WallRun) ? "Gravity:\nWallRun" : "Gravity:\nPlatform";
        auto [gx,gy] = Text::CenterInRect(gLbl, 11, btnGravity);
        lblGravity = std::make_unique<Text>(gLbl, SDL_Color{0,0,0,255}, gx, gy, 11);
    }

    lblStatus = std::make_unique<Text>(mStatusMsg, SDL_Color{220,220,220,255}, pad, TOOLBAR_H+4, 12);
    lblTool   = std::make_unique<Text>("Tool: Coin", SDL_Color{255,215,0,255},
                                       window.GetWidth()-PALETTE_W-140, 18, 13);
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
            case SDLK_1: mActiveTool=Tool::Coin;        lblTool->CreateSurface("Tool: Coin");   break;
            case SDLK_2: mActiveTool=Tool::Enemy;       lblTool->CreateSurface("Tool: Enemy");  break;
            case SDLK_3: mActiveTool=Tool::Tile;        lblTool->CreateSurface("Tool: Tile");
                         mActiveTab=PaletteTab::Tiles;  break;
            case SDLK_4: mActiveTool=Tool::Erase;       lblTool->CreateSurface("Tool: Erase");  break;
            case SDLK_5: mActiveTool=Tool::PlayerStart; lblTool->CreateSurface("Tool: Player"); break;
            case SDLK_6: mActiveTab=PaletteTab::Backgrounds; lblTool->CreateSurface("BG picker"); break;
            case SDLK_7:
            case SDLK_R:
                mActiveTool=Tool::Resize; lblTool->CreateSurface("Tool: Resize");
                mIsResizing=false; mHoverEdge=ResizeEdge::None; mHoverTileIdx=-1;
                break;
            case SDLK_8:
            case SDLK_P:
                mActiveTool=Tool::Prop; lblTool->CreateSurface("Tool: Prop");
                break;
            case SDLK_9:
            case SDLK_L:
                mActiveTool=Tool::Ladder; lblTool->CreateSurface("Tool: Ladder");
                break;
            case SDLK_0:
            case SDLK_T:
                mActiveTool=Tool::Action; lblTool->CreateSurface("Tool: Action");
                break;
            case SDLK_MINUS:
                mActiveTool=Tool::Slope; lblTool->CreateSurface("Tool: Slope");
                break;
            case SDLK_G: {
                // Toggle gravity mode
                mLevel.gravityMode = (mLevel.gravityMode == GravityMode::Platformer)
                    ? GravityMode::WallRun : GravityMode::Platformer;
                bool isWall = (mLevel.gravityMode == GravityMode::WallRun);
                std::string gLbl = isWall ? "Gravity:\nWallRun" : "Gravity:\nPlatform";
                auto [gx,gy] = Text::CenterInRect(gLbl, 11, btnGravity);
                lblGravity = std::make_unique<Text>(gLbl, SDL_Color{0,0,0,255}, gx, gy, 11);
                SetStatus(isWall ? "Gravity: Wall-Run mode" : "Gravity: Platformer mode");
                break;
            }
            case SDLK_ESCAPE:
                // Navigate back up when pressing Esc in tile browser
                if (mActiveTab == PaletteTab::Tiles && mTileCurrentDir != TILE_ROOT) {
                    fs::path parent = fs::path(mTileCurrentDir).parent_path();
                    std::string up = parent.string();
                    // Don't go above root
                    if (up.rfind(TILE_ROOT, 0) != 0) up = TILE_ROOT;
                    LoadTileView(up);
                }
                break;
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
            mActiveTool = t; lblTool->CreateSurface("Tool: "+l); return true;
        };
        if (tb(btnCoin,Tool::Coin,"Coin"))          return true;
        if (tb(btnEnemy,Tool::Enemy,"Enemy"))        return true;
        if (tb(btnTile,Tool::Tile,"Tile"))           return true;
        if (tb(btnResize,Tool::Resize,"Resize"))     { mIsResizing=false; mHoverEdge=ResizeEdge::None; mHoverTileIdx=-1; return true; }
        if (tb(btnProp,Tool::Prop,"Prop"))           return true;
        if (tb(btnLadder,Tool::Ladder,"Ladder"))      return true;
        if (tb(btnAction,Tool::Action,"Action"))      return true;
        if (tb(btnSlope,Tool::Slope,"Slope"))          return true;
        if (tb(btnErase,Tool::Erase,"Erase"))          return true;
        if (tb(btnPlayerStart,Tool::PlayerStart,"Player")) return true;

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
        if (HitTest(btnGravity,mx,my)) {
            mLevel.gravityMode = (mLevel.gravityMode == GravityMode::Platformer)
                ? GravityMode::WallRun : GravityMode::Platformer;
            bool isWall = (mLevel.gravityMode == GravityMode::WallRun);
            std::string gLbl = isWall ? "Gravity:\nWallRun" : "Gravity:\nPlatform";
            auto [gx,gy] = Text::CenterInRect(gLbl, 11, btnGravity);
            lblGravity = std::make_unique<Text>(gLbl, SDL_Color{0,0,0,255}, gx, gy, 11);
            SetStatus(isWall ? "Gravity: Wall-Run mode" : "Gravity: Platformer mode");
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

        // Start drag for canvas entities
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
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        int mx=(int)e.motion.x, my=(int)e.motion.y;

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

// ─── Render ───────────────────────────────────────────────────────────────────
void LevelEditorScene::Render(Window& window) {
    window.Render();
    SDL_Surface* screen = window.GetSurface();
    int cw = CanvasW();

    background->Render(screen);

    // Grid
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(screen->format);
    Uint32 gridCol = SDL_MapRGBA(fmt,nullptr,255,255,255,20);
    for (int x=0;x<cw;x+=GRID) { SDL_Rect l={x,TOOLBAR_H,1,window.GetHeight()-TOOLBAR_H}; SDL_FillSurfaceRect(screen,&l,gridCol); }
    for (int y=TOOLBAR_H;y<window.GetHeight();y+=GRID) { SDL_Rect l={0,y,cw,1}; SDL_FillSurfaceRect(screen,&l,gridCol); }

    // Placed tiles
    for (int ti = 0; ti < (int)mLevel.tiles.size(); ti++) {
        const auto& t = mLevel.tiles[ti];
        SDL_Surface* ts = nullptr;
        for (const auto& item : mPaletteItems)
            if (item.path == t.imagePath) { ts = item.full ? item.full : item.thumb; break; }
        SDL_Rect dst={(int)t.x,(int)t.y,t.w,t.h};
        if (ts) {
            if (t.prop)   SDL_SetSurfaceColorMod(ts, 120, 255, 120); // green tint
            if (t.ladder) SDL_SetSurfaceColorMod(ts, 120, 220, 255); // cyan tint
            if (t.action) SDL_SetSurfaceColorMod(ts, 255, 160,  80); // orange tint
            SDL_BlitSurfaceScaled(ts,nullptr,screen,&dst,SDL_SCALEMODE_LINEAR);
            if (t.prop || t.ladder || t.action) SDL_SetSurfaceColorMod(ts, 255, 255, 255); // reset
        } else {
            SDL_Surface* l=IMG_Load(t.imagePath.c_str());
            if(l){SDL_BlitSurfaceScaled(l,nullptr,screen,&dst,SDL_SCALEMODE_LINEAR);SDL_DestroySurface(l);}
            else DrawRect(screen,dst,{80,80,120,200});
        }
        // Outline colour: cyan for ladder, green for prop, orange for action, blue for solid
        SDL_Color outlineCol = t.ladder ? SDL_Color{0,220,220,255}
                             : t.prop   ? SDL_Color{80,255,80,255}
                             : t.action ? SDL_Color{255,160,60,255}
                                        : SDL_Color{100,180,255,255};
        DrawOutline(screen,dst,outlineCol);
        // Prop badge
        if (t.prop) {
            DrawRect(screen,{(int)t.x+2,(int)t.y+2,14,14},{0,180,0,200});
            Text propBadge("P",SDL_Color{255,255,255,255},(int)t.x+4,(int)t.y+2,10);
            propBadge.Render(screen);
        }
        // Ladder badge
        if (t.ladder) {
            DrawRect(screen,{(int)t.x+2,(int)t.y+2,14,14},{0,160,180,200});
            Text ladderBadge("L",SDL_Color{255,255,255,255},(int)t.x+4,(int)t.y+2,10);
            ladderBadge.Render(screen);
        }
        // Action badge — shows group number when non-zero
        if (t.action) {
            std::string abadge = (t.actionGroup > 0)
                ? "A" + std::to_string(t.actionGroup) : "A";
            int bw = (t.actionGroup > 0) ? 20 : 14;
            DrawRect(screen,{(int)t.x+2,(int)t.y+2,bw,14},{200,100,0,200});
            Text actionBadge(abadge,SDL_Color{255,255,255,255},(int)t.x+4,(int)t.y+2,10);
            actionBadge.Render(screen);
        }
        // Slope: draw diagonal line and badge
        if (t.slope != SlopeType::None) {
            // Diagonal line across the tile
            int tx=(int)t.x, ty2=(int)t.y, tw=t.w, th=t.h;
            int lx0, ly0, lx1, ly1;
            if (t.slope == SlopeType::DiagUpRight) {
                lx0=tx;     ly0=ty2+th;  lx1=tx+tw; ly1=ty2;
            } else {
                lx0=tx;     ly0=ty2;     lx1=tx+tw; ly1=ty2+th;
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
            // Badge
            DrawRect(screen,{tx+2,ty2+2,14,14},{160,120,0,200});
            std::string badge = (t.slope==SlopeType::DiagUpRight) ? "/" : "\\";
            Text slopeBadge(badge,SDL_Color{255,255,255,255},tx+5,ty2+2,10);
            slopeBadge.Render(screen);
        }
    }

    // Coins, enemies, player marker
    auto coinFrames = coinSheet->GetAnimation("Gold_");
    if (!coinFrames.empty())
        for (const auto& c:mLevel.coins){SDL_Rect s=coinFrames[0],d={(int)c.x,(int)c.y,ICON_SIZE,ICON_SIZE};SDL_BlitSurfaceScaled(coinSheet->GetSurface(),&s,screen,&d,SDL_SCALEMODE_LINEAR);DrawOutline(screen,d,{255,215,0,255});}
    auto slimeFrames=enemySheet->GetAnimation("slimeWalk");
    if (!slimeFrames.empty())
        for (const auto& en:mLevel.enemies){SDL_Rect s=slimeFrames[0],d={(int)en.x,(int)en.y,ICON_SIZE,ICON_SIZE};SDL_BlitSurfaceScaled(enemySheet->GetSurface(),&s,screen,&d,SDL_SCALEMODE_LINEAR);DrawOutline(screen,d,{255,80,80,255});}
    // Draw player marker at the actual collider size so placement in the editor
    // matches the in-game hitbox exactly (PLAYER_STAND_WIDTH x PLAYER_STAND_HEIGHT).
    DrawRect(screen,{(int)mLevel.player.x,(int)mLevel.player.y,PLAYER_STAND_WIDTH,PLAYER_STAND_HEIGHT},{0,200,80,180});
    DrawOutline(screen,{(int)mLevel.player.x,(int)mLevel.player.y,PLAYER_STAND_WIDTH,PLAYER_STAND_HEIGHT},{0,255,100,255},2);

    // ── Resize tool feedback ─────────────────────────────────────────────────
    if (mActiveTool == Tool::Resize) {
        const SDL_Color handleCol  = {255, 180, 0, 220};
        const SDL_Color dragCol    = {255, 220, 80, 180};
        const int HS = RESIZE_HANDLE;

        // Highlight hovered edge/corner
        if (mHoverTileIdx >= 0 && !mIsResizing) {
            const auto& t = mLevel.tiles[mHoverTileIdx];
            int rx=(int)t.x, ry=(int)t.y, rw=t.w, rh=t.h;
            if (mHoverEdge==ResizeEdge::Right || mHoverEdge==ResizeEdge::Corner)
                DrawRect(screen,{rx+rw-HS, ry+HS, HS, rh-HS*2}, handleCol);
            if (mHoverEdge==ResizeEdge::Bottom || mHoverEdge==ResizeEdge::Corner)
                DrawRect(screen,{rx+HS, ry+rh-HS, rw-HS*2, HS}, handleCol);
            if (mHoverEdge==ResizeEdge::Corner)
                DrawRect(screen,{rx+rw-HS, ry+rh-HS, HS, HS}, handleCol);
        }

        // Ghost outline while dragging
        if (mIsResizing && mResizeTileIdx >= 0 && mResizeTileIdx < (int)mLevel.tiles.size()) {
            const auto& t = mLevel.tiles[mResizeTileIdx];
            DrawOutline(screen,{(int)t.x,(int)t.y,t.w,t.h},{255,220,80,255},2);
            // Size label
            Text szT(std::to_string(t.w/GRID)+"x"+std::to_string(t.h/GRID)+" tiles",
                     dragCol,(int)t.x+4,(int)t.y+4,12);
            szT.Render(screen);
        }
    }

    // Tile ghost
    if (mActiveTool==Tool::Tile && !mPaletteItems.empty() && !mPaletteItems[mSelectedTile].isFolder) {
        float fmx,fmy; SDL_GetMouseState(&fmx,&fmy); int mx=(int)fmx,my=(int)fmy;
        if (my>=TOOLBAR_H && mx<cw) { auto [sx,sy]=SnapToGrid(mx,my); DrawRect(screen,{sx,sy,mTileW,mTileH},{100,180,255,60}); DrawOutline(screen,{sx,sy,mTileW,mTileH},{100,180,255,200}); }
    }

    // ── Toolbar ───────────────────────────────────────────────────────────────
    DrawRect(screen,{0,0,window.GetWidth(),TOOLBAR_H},{25,25,35,245});
    auto drawBtn=[&](SDL_Rect r,SDL_Color bg,SDL_Color border,Text* lbl,bool active){
        if(active) bg={70,140,255,255};
        DrawRect(screen,r,bg); DrawOutline(screen,r,border); if(lbl)lbl->Render(screen);
    };
    drawBtn(btnCoin,       {55,55,65,255},{180,180,180,255},lblCoin.get(),   mActiveTool==Tool::Coin);
    drawBtn(btnEnemy,      {55,55,65,255},{180,180,180,255},lblEnemy.get(),  mActiveTool==Tool::Enemy);
    drawBtn(btnTile,       {55,55,65,255},{180,180,180,255},lblTile.get(),   mActiveTool==Tool::Tile);
    drawBtn(btnResize,     {55,55,65,255},{180,180,180,255},lblResize.get(), mActiveTool==Tool::Resize);
    drawBtn(btnProp,       {30,80,30,255},    {80,200,80,255},   lblProp.get(),   mActiveTool==Tool::Prop);
    drawBtn(btnLadder,     {20,100,120,255},  {60,200,220,255},  lblLadder.get(), mActiveTool==Tool::Ladder);
    drawBtn(btnAction,     {130,70,10,255},   {255,160,60,255},  lblAction.get(), mActiveTool==Tool::Action);
    drawBtn(btnSlope,      {80,60,20,255},    {255,220,50,255},  lblSlope.get(),  mActiveTool==Tool::Slope);
    drawBtn(btnErase,      {55,55,65,255},    {180,180,180,255}, lblErase.get(),  mActiveTool==Tool::Erase);
    drawBtn(btnPlayerStart,{55,55,65,255},{180,180,180,255},lblPlayer.get(), mActiveTool==Tool::PlayerStart);
    drawBtn(btnSave,  {40,110,40,255},{120,230,120,255},lblSave.get(),  false);
    drawBtn(btnLoad,  {40,70,120,255},{120,160,230,255},lblLoad.get(),  false);
    drawBtn(btnClear, {110,40,40,255},{230,100,100,255},lblClear.get(), false);
    drawBtn(btnPlay,  {40,140,40,255},{80,230,80,255}, lblPlay.get(),   false);
    // Gravity toggle — blue tint for wallrun, dark for platformer
    {
        bool isWall = (mLevel.gravityMode == GravityMode::WallRun);
        SDL_Color bg  = isWall ? SDL_Color{30,60,160,255} : SDL_Color{55,55,65,255};
        SDL_Color bdr = isWall ? SDL_Color{80,140,255,255} : SDL_Color{140,140,160,255};
        drawBtn(btnGravity, bg, bdr, lblGravity.get(), false);
    }
    DrawRect(screen,{0,TOOLBAR_H,cw,22},{18,18,26,220});
    if(lblStatus)lblStatus->Render(screen);
    if(lblTool)lblTool->Render(screen);

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
    Text cntT(std::to_string(mLevel.coins.size())+"c  "+std::to_string(mLevel.enemies.size())+"e  "+std::to_string(mLevel.tiles.size())+"t",SDL_Color{160,160,160,255},6,window.GetHeight()-22,12);
    cntT.Render(screen);
    Text hintT("1-5:Tools 6:BG 7:Resize 8:Prop 9:Ladder 0:Action(R-click=group) -:Slope G:Gravity  I:Import  Ctrl+S:Save  Ctrl+Z:Undo",SDL_Color{100,100,100,255},150,window.GetHeight()-22,11);
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
        if (lblTool) lblTool->CreateSurface("Tool: Tile");
        SetStatus("Imported: " + dest.filename().string() + " → auto-selected");
    }
    return true;
}
