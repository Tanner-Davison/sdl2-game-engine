#include "LevelEditorScene.hpp"
#include "AnimatedTile.hpp"
#include "GameScene.hpp"
#include "SurfaceUtils.hpp"
#include "TitleScene.hpp"
#include <SDL3_ttf/SDL_ttf.h>
#include <climits>
#include <print>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Internal helpers — delegate to EditorSurfaceCache static methods
// ---------------------------------------------------------------------------
static SDL_Surface* MakeThumb(SDL_Surface* src, int w, int h) {
    return EditorSurfaceCache::MakeThumb(src, w, h);
}
static SDL_Surface* LoadPNG(const fs::path& p) {
    return EditorSurfaceCache::LoadPNG(p);
}

// --- RebuildToolbarLayout --------------------------------------------------
// Recomputes all button rects based on current collapse state.
// Called once in Load() and again whenever a group is toggled.
void LevelEditorScene::RebuildToolbarLayout() {
    constexpr int PILL_W = 28; // width of a collapsed group pill
    int           x      = BTN_GAP;

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

    // Strip constants must match Render's STRIP_Y / STRIP_H
    constexpr int STRIP_Y_L = BTN_Y + BTN_H + 2; // 66
    constexpr int STRIP_H_L = 14;

    // Group 1 — Place
    int grp1X0 = x;
    if (mGrp1Collapsed) {
        btnCoin = btnEnemy = btnTile = btnErase = btnPlayerStart = btnSelect =
            btnMoveCam = {-200, BTN_Y, BTN_TOOL_W, BTN_H};
        x += PILL_W + BTN_GAP;
    } else {
        btnCoin        = nextTool();
        btnEnemy       = nextTool();
        btnTile        = nextTool();
        btnErase       = nextTool();
        btnPlayerStart = nextTool();
        btnSelect      = nextTool();
        btnMoveCam     = nextTool();
    }
    // Pill covers the full group width at the strip position
    mGrp1Pill = {grp1X0, STRIP_Y_L, x - grp1X0, STRIP_H_L};
    gap();

    // Group 2 — Modifiers
    int grp2X0 = x;
    if (mGrp2Collapsed) {
        btnProp = btnLadder = btnAction = btnSlope = btnResize = btnHitbox = btnHazard =
            btnAntiGrav = btnMovingPlat = btnPowerUp = {-200, BTN_Y, BTN_TOOL_W, BTN_H};
        x += PILL_W + BTN_GAP;
    } else {
        btnProp       = nextTool();
        btnLadder     = nextTool();
        btnAction     = nextTool();
        btnSlope      = nextTool();
        btnResize     = nextTool();
        btnHitbox     = nextTool();
        btnHazard     = nextTool();
        btnAntiGrav   = nextTool();
        btnMovingPlat = nextTool();
        btnPowerUp    = nextTool();
    }
    mGrp2Pill = {grp2X0, STRIP_Y_L, x - grp2X0, STRIP_H_L};
    gap();

    // Group 3 — Actions
    int grp3X0 = x;
    if (mGrp3Collapsed) {
        btnGravity = btnSave = btnLoad = btnClear = btnPlay =
            btnBack                               = {-200, BTN_Y, BTN_ACT_W, BTN_H};
        x += PILL_W + BTN_GAP;
    } else {
        btnGravity = nextAct();
        btnSave    = nextAct();
        btnLoad    = nextAct();
        btnClear   = nextAct();
        btnPlay    = nextAct();
        gap();
        btnBack = nextAct();
    }
    mGrp3Pill = {grp3X0, STRIP_Y_L, x - grp3X0, STRIP_H_L};

    // Re-centre all labels in their (possibly moved) buttons
    auto recentre =
        [](std::unique_ptr<Text>& t, const std::string& s, SDL_Rect r, int sz = 12) {
            if (!t)
                return;
            auto [lx, ly] = Text::CenterInRect(s, sz, r);
            t->SetPosition(lx, ly);
        };
    auto recentreHint = [](std::unique_ptr<Text>& t, SDL_Rect r) {
        if (!t)
            return;
        t->SetPosition(r.x + r.w - 14, r.y + r.h - 13);
    };

    recentre(lblCoin, "Coin", btnCoin);
    recentreHint(hintCoin, btnCoin);
    recentre(lblEnemy, "Enemy", btnEnemy);
    recentreHint(hintEnemy, btnEnemy);
    recentre(lblTile, "Tile", btnTile);
    recentreHint(hintTile, btnTile);
    recentre(lblErase, "Erase", btnErase);
    recentreHint(hintErase, btnErase);
    recentre(lblPlayer, "Player", btnPlayerStart, 12);
    recentreHint(hintPlayer, btnPlayerStart);
    recentre(lblSelect, "Select", btnSelect, 11);
    recentreHint(hintSelect, btnSelect);
    recentre(lblMoveCam, "Pan", btnMoveCam, 11);
    recentreHint(hintMoveCam, btnMoveCam);
    recentre(lblProp, "Prop", btnProp);
    recentreHint(hintProp, btnProp);
    recentre(lblLadder, "Ladder", btnLadder);
    recentreHint(hintLadder, btnLadder);
    recentre(lblAction, "Action", btnAction);
    recentreHint(hintAction, btnAction);
    recentre(lblSlope, "Slope", btnSlope);
    recentreHint(hintSlope, btnSlope);
    recentre(lblResize, "Resize", btnResize);
    recentreHint(hintResize, btnResize);
    recentre(lblHitbox, "Hitbox", btnHitbox);
    recentre(lblHazard, "Hazard", btnHazard);
    recentre(lblAntiGrav, "Float", btnAntiGrav, 11);
    recentre(lblMovingPlat, "MovePlat", btnMovingPlat, 10);
    recentre(lblPowerUp, "PowerUp", btnPowerUp, 10);
    if (lblGravity) {
        std::string gLbl = "Platform";
        auto [gx, gy]    = Text::CenterInRect(gLbl, 11, btnGravity);
        lblGravity->SetPosition(gx, gy);
    }
    recentre(lblSave, "Save", btnSave);
    recentre(lblLoad, "Load", btnLoad);
    recentre(lblClear, "Clear", btnClear);
    recentre(lblPlay, "Play", btnPlay);
    recentre(lblBack, "< Menu", btnBack);

    // Rebuild collapse tab labels (used by drawCollapseBtn in Render)
    lblGrp1Tab = std::make_unique<Text>(
        mGrp1Collapsed ? "+" : "-", SDL_Color{200, 220, 255, 200}, 0, 0, 9);
    lblGrp2Tab = std::make_unique<Text>(
        mGrp2Collapsed ? "+" : "-", SDL_Color{200, 220, 255, 200}, 0, 0, 9);
    lblGrp3Tab = std::make_unique<Text>(
        mGrp3Collapsed ? "+" : "-", SDL_Color{200, 220, 255, 200}, 0, 0, 9);
}



// --- OpenAnimPicker / CloseAnimPicker --------------------------------------
void LevelEditorScene::OpenAnimPicker(int tileIdx) {
    mActionAnimPickerTile = tileIdx;
    mAnimPickerEntries.clear();

    // "None" entry first — always available to clear any existing assignment
    mAnimPickerEntries.push_back({"", "None (no death anim)", nullptr});

    // Scan animated_tiles/ for all .json manifests
    auto manifests = ScanAnimatedTiles();
    for (const auto& p : manifests) {
        AnimatedTileDef def;
        if (!LoadAnimatedTileDef(p.string(), def))
            continue;
        SDL_Surface* thumb = mSurfaceCache.GetDestroyAnimThumb(p.string()); // builds & caches 48x48
        mAnimPickerEntries.push_back({p.string(), def.name, thumb});
    }

    // Layout: popup centred horizontally over the tile, above or below depending on space.
    // We don't know window size here, so defer rect computation to Render.
    // Just mark as open — Render computes mActionAnimPickerRect each frame.
}

void LevelEditorScene::CloseAnimPicker() {
    mActionAnimPickerTile = -1;
    mAnimPickerEntries.clear();
}

// --- GetPowerUpRegistry ----------------------------------------------------
// The single authoritative list of power-up types for both the editor UI and
// the game runtime. To add a new power-up:
//   1. Add an entry here (id, label, duration)
//   2. Add a PowerUpType enum value in Components.hpp
//   3. Handle the id string in GameScene::Spawn() -> PowerUpTag
//   4. Handle the PowerUpType in MovementSystem / GameScene::Update
const std::vector<LevelEditorScene::PowerUpEntry>& LevelEditorScene::GetPowerUpRegistry() {
    static const std::vector<PowerUpEntry> kRegistry = {
        {"antigravity", "Anti-Gravity (15s)", 15.0f},
        // Add future power-ups here:
        // {"speedboost",  "Speed Boost (10s)",  10.0f},
        // {"invincible",  "Invincibility (8s)",  8.0f},
    };
    return kRegistry;
}

// --- LoadTileView ----------------------------------------------------------
void LevelEditorScene::LoadTileView(const std::string& dir) {
    // Free existing surface memory — folders use non-owning mFolderIcon pointer,
    // so only free surfaces for file entries.
    for (auto& item : mPaletteItems) {
        if (!item.isFolder) {
            if (item.thumb)
                SDL_DestroySurface(item.thumb);
            if (item.full)
                SDL_DestroySurface(item.full);
        }
    }
    mPaletteItems.clear();
    mPaletteScroll  = 0;
    mSelectedTile   = 0; // reset selection — old index may be out of bounds in new view
    mTileCurrentDir = dir;

    if (!fs::exists(dir))
        return;

    // ── "◀ Back" entry when we're inside a subfolder ──────────────────────────
    fs::path dirPath(dir);
    fs::path rootPath(TILE_ROOT);
    // Use lexically_relative to check if we're deeper than the root
    fs::path rel    = fs::path(dir).lexically_relative(TILE_ROOT);
    bool     atRoot = (rel.empty() || rel == ".");
    if (!atRoot) {
        PaletteItem back;
        back.path     = dirPath.parent_path().string();
        back.label    = "◀ Back";
        back.isFolder = true;
        back.thumb    = mFolderIcon; // non-owning
        back.full     = nullptr;
        mPaletteItems.push_back(std::move(back));
    }

    // ── Virtual "Animated Tiles" folder entry (shown at root level only) ─────
    if (!atRoot) { /* skip */
    } else {
        if (fs::exists(ANIMATED_TILE_DIR)) {
            int count = 0;
            for (const auto& e : fs::directory_iterator(ANIMATED_TILE_DIR))
                if (e.path().extension() == ".json")
                    ++count;
            if (count > 0) {
                PaletteItem anim;
                anim.path     = ANIMATED_TILE_DIR;
                anim.label    = "Animated Tiles (" + std::to_string(count) + ")";
                anim.isFolder = true;
                anim.thumb    = mFolderIcon;
                anim.full     = nullptr;
                mPaletteItems.push_back(std::move(anim));
            }
        }
    }

    std::vector<fs::path> folders, files, manifests;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_directory()) {
            // Skip animated_tiles dir at root — already represented by the virtual entry
            // above
            if (atRoot && entry.path() == fs::path(ANIMATED_TILE_DIR))
                continue;
            folders.push_back(entry.path());
        } else if (entry.path().extension() == ".png") {
            files.push_back(entry.path());
        } else if (entry.path().extension() == ".json") {
            manifests.push_back(entry.path());
        }
    }
    std::sort(folders.begin(), folders.end());
    std::sort(files.begin(), files.end());
    std::sort(manifests.begin(), manifests.end());

    // Folders first
    for (const auto& p : folders) {
        // Count PNGs inside so we can show a count badge
        int count = 0;
        for (const auto& e : fs::directory_iterator(p))
            if (e.path().extension() == ".png")
                count++;

        PaletteItem item;
        item.path     = p.string();
        item.label    = p.filename().string() + " (" + std::to_string(count) + ")";
        item.isFolder = true;
        item.thumb    = mFolderIcon; // non-owning pointer — shared, never freed per-item
        item.full     = nullptr;
        mPaletteItems.push_back(std::move(item));
    }

    // Then individual PNG files
    for (const auto& p : files) {
        SDL_Surface* full = LoadPNG(p);
        if (!full)
            continue;
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

    // ── Animated tile manifests (only shown when we're inside ANIMATED_TILE_DIR) ──
    for (const auto& p : manifests) {
        AnimatedTileDef def;
        if (!LoadAnimatedTileDef(p.string(), def) || def.framePaths.empty())
            continue;

        // Load first frame as thumbnail
        SDL_Surface* firstFrame = nullptr;
        SDL_Surface* thumb      = nullptr;
        for (const auto& fp : def.framePaths) {
            SDL_Surface* raw = IMG_Load(fp.c_str());
            if (!raw)
                continue;
            firstFrame = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
            SDL_DestroySurface(raw);
            if (firstFrame) {
                SDL_SetSurfaceBlendMode(firstFrame, SDL_BLENDMODE_BLEND);
                break;
            }
        }
        if (firstFrame) {
            thumb = MakeThumb(firstFrame, PAL_ICON, PAL_ICON);
            SDL_DestroySurface(firstFrame);
        }

        PaletteItem item;
        item.path  = p.string();       // .json manifest path
        item.label = def.name + " [~"; // ~ prefix = animated
        item.label += std::to_string(def.framePaths.size()) + "f]";
        item.isFolder = false;
        item.thumb    = thumb;
        item.full     = thumb ? SDL_DuplicateSurface(thumb) : nullptr;
        mPaletteItems.push_back(std::move(item));
    }

    // Rebuild the path->surface cache so Render never does a linear search
    mSurfaceCache.ClearTileSurfaceCache();
    for (const auto& item : mPaletteItems)
        if (!item.isFolder && item.full)
            mSurfaceCache.InsertTileSurface(item.path, item.full);

    // Seed the cache for tiles already in the level whose images live in
    // subdirectories not currently visible in the palette view.
    // Collect unique paths first to avoid redundant disk I/O for levels that
    // reuse the same image many times (e.g. 200 grass tiles = 1 load, not 200).
    {
        // Deduplicate: build a set of (path, w, h) tuples we actually need.
        // Use the TILE display size (ts.w x ts.h) for the cached surface so
        // Render blits at 1:1 without extra scaling overhead.
        struct TileLoad {
            std::string path;
            int         w;
            int         h;
            bool        animated;
        };
        std::unordered_map<std::string, TileLoad> needed;
        for (const auto& ts : mLevel.tiles) {
            if (ts.imagePath.empty() || mSurfaceCache.HasTileSurface(ts.imagePath))
                continue;
            // Store only the first size we see per path — all instances of the
            // same image in a level should share the same w/h.
            if (!needed.count(ts.imagePath))
                needed[ts.imagePath] = {
                    ts.imagePath, ts.w, ts.h, IsAnimatedTile(ts.imagePath)};
        }

        for (auto& [path, info] : needed) {
            if (info.animated) {
                AnimatedTileDef def;
                if (!LoadAnimatedTileDef(info.path, def) || def.framePaths.empty())
                    continue;
                SDL_Surface* firstFrame = nullptr;
                for (const auto& fp : def.framePaths) {
                    SDL_Surface* raw = IMG_Load(fp.c_str());
                    if (!raw)
                        continue;
                    firstFrame = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
                    SDL_DestroySurface(raw);
                    if (firstFrame) {
                        SDL_SetSurfaceBlendMode(firstFrame, SDL_BLENDMODE_BLEND);
                        break;
                    }
                }
                if (!firstFrame)
                    continue;
                SDL_Surface* scaled =
                    SDL_CreateSurface(info.w, info.h, SDL_PIXELFORMAT_ARGB8888);
                if (scaled) {
                    SDL_SetSurfaceBlendMode(firstFrame, SDL_BLENDMODE_NONE);
                    SDL_BlitSurfaceScaled(
                        firstFrame, nullptr, scaled, nullptr, SDL_SCALEMODE_LINEAR);
                    SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);
                }
                SDL_DestroySurface(firstFrame);
                if (!scaled)
                    continue;
                mSurfaceCache.InsertTileSurface(info.path, scaled);
                mSurfaceCache.AddExtraTileSurface(scaled);
            } else {
                // Normal PNG — load once and scale down to tile display size
                // immediately so we store a small surface instead of a full-res one.
                SDL_Surface* raw = IMG_Load(info.path.c_str());
                if (!raw)
                    continue;
                SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
                SDL_DestroySurface(raw);
                if (!conv)
                    continue;
                // Scale to tile size if the raw image differs (avoids storing
                // 512x512 PNGs when the tile is placed as 48x48 in the editor).
                SDL_Surface* result = conv;
                if (conv->w != info.w || conv->h != info.h) {
                    SDL_Surface* scaled =
                        SDL_CreateSurface(info.w, info.h, SDL_PIXELFORMAT_ARGB8888);
                    if (scaled) {
                        SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_NONE);
                        SDL_BlitSurfaceScaled(
                            conv, nullptr, scaled, nullptr, SDL_SCALEMODE_LINEAR);
                        SDL_SetSurfaceBlendMode(scaled, SDL_BLENDMODE_BLEND);
                        SDL_DestroySurface(conv);
                        result = scaled;
                    }
                } else {
                    SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_BLEND);
                }
                mSurfaceCache.InsertTileSurface(info.path, result);
                mSurfaceCache.AddExtraTileSurface(result);
            }
        }
    }
}

// --- DetectResizeEdge ------------------------------------------------------
LevelEditorScene::ResizeEdge LevelEditorScene::DetectResizeEdge(int idx,
                                                                int mx,
                                                                int my) const {
    if (idx < 0 || idx >= (int)mLevel.tiles.size())
        return ResizeEdge::None;
    const auto& t = mLevel.tiles[idx];
    // mx/my are screen-space; convert to world space to compare with tile world coords
    auto [wx, wy] = mCamera.ScreenToWorld(mx, my);
    int rx = (int)t.x, ry = (int)t.y, rw = t.w, rh = t.h;

    // Cursor must be strictly inside the tile's visual bounds — this prevents
    // handle zones on one tile from bleeding into an adjacent tile.
    if (wx < rx || wx > rx + rw)
        return ResizeEdge::None;
    if (wy < ry || wy > ry + rh)
        return ResizeEdge::None;

    bool nearRight  = (wx >= rx + rw - RESIZE_HANDLE);
    bool nearBottom = (wy >= ry + rh - RESIZE_HANDLE);

    if (nearRight && nearBottom)
        return ResizeEdge::Corner;
    if (nearRight)
        return ResizeEdge::Right;
    if (nearBottom)
        return ResizeEdge::Bottom;
    return ResizeEdge::None;
}

// --- LoadBgPalette ---------------------------------------------------------
void LevelEditorScene::LoadBgPalette() {
    for (auto& item : mBgItems)
        if (item.thumb)
            SDL_DestroySurface(item.thumb);
    mBgItems.clear();

    if (!fs::exists(BG_ROOT))
        return;

    std::vector<fs::path> paths;
    for (const auto& e : fs::directory_iterator(BG_ROOT))
        if (e.path().extension() == ".png")
            paths.push_back(e.path());
    std::sort(paths.begin(), paths.end());

    const int thumbW = PALETTE_W - 8;
    const int thumbH = thumbW / 2;

    for (const auto& p : paths) {
        SDL_Surface* full = LoadPNG(p);
        if (!full)
            continue;
        SDL_Surface* thumb = MakeThumb(full, thumbW, thumbH);
        SDL_DestroySurface(full);
        mBgItems.push_back({p.string(), p.stem().string(), thumb});

        if (p.string() == mLevel.background)
            mSelectedBg = (int)mBgItems.size() - 1;
    }
}

// --- ApplyBackground -------------------------------------------------------
void LevelEditorScene::ApplyBackground(int idx) {
    if (idx < 0 || idx >= (int)mBgItems.size())
        return;
    mSelectedBg       = idx;
    mLevel.background = mBgItems[idx].path;
    background =
        std::make_unique<Image>(mLevel.background, FitModeFromString(mLevel.bgFitMode));
    SetStatus("Background: " + mBgItems[idx].label + "  [" + mLevel.bgFitMode + "]");
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
    if (!mForceNew && mLevel.coins.empty() && mLevel.enemies.empty() &&
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
            if (!mLevel.background.empty())
                background = std::make_unique<Image>(mLevel.background,
                                                     FitModeFromString(mLevel.bgFitMode));
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

    LoadTileView(TILE_ROOT);
    LoadBgPalette();

    // ── Toolbar layout ────────────────────────────────────────────────────────
    RebuildToolbarLayout();

    // White label maker — used for all tool/action buttons
    auto mkLbl = [](const std::string& s, SDL_Rect r, int sz = 12) {
        auto [x, y] = Text::CenterInRect(s, sz, r);
        return std::make_unique<Text>(s, SDL_Color{255, 255, 255, 255}, x, y, sz);
    };
    // Tiny shortcut hint in bottom-right of button
    auto mkHint = [](const std::string& s, SDL_Rect r) {
        return std::make_unique<Text>(
            s, SDL_Color{180, 180, 180, 160}, r.x + r.w - 14, r.y + r.h - 13, 9);
    };

    // Group 1 labels + hints
    lblCoin     = mkLbl("Coin", btnCoin);
    lblEnemy    = mkLbl("Enemy", btnEnemy);
    lblTile     = mkLbl("Tile", btnTile);
    lblErase    = mkLbl("Erase", btnErase);
    lblPlayer   = mkLbl("Player", btnPlayerStart);
    lblSelect   = mkLbl("Select", btnSelect, 11);
    hintCoin    = mkHint("1", btnCoin);
    hintEnemy   = mkHint("2", btnEnemy);
    hintTile    = mkHint("3", btnTile);
    hintErase   = mkHint("4", btnErase);
    hintPlayer  = mkHint("5", btnPlayerStart);
    hintSelect  = mkHint("Q", btnSelect);
    lblMoveCam  = mkLbl("Pan", btnMoveCam, 11);
    hintMoveCam = mkHint("T", btnMoveCam);

    // Group 2 labels + hints
    lblProp       = mkLbl("Prop", btnProp);
    lblLadder     = mkLbl("Ladder", btnLadder);
    lblAction     = mkLbl("Action", btnAction);
    lblSlope      = mkLbl("Slope", btnSlope);
    lblResize     = mkLbl("Resize", btnResize);
    hintProp      = mkHint("8", btnProp);
    hintLadder    = mkHint("9", btnLadder);
    hintAction    = mkHint("0", btnAction);
    hintSlope     = mkHint("-", btnSlope);
    hintResize    = mkHint("R", btnResize);
    lblHitbox     = mkLbl("Hitbox", btnHitbox);
    lblHazard     = mkLbl("Hazard", btnHazard);
    lblAntiGrav   = mkLbl("Float", btnAntiGrav, 11);
    lblMovingPlat = mkLbl("MovePlat", btnMovingPlat, 10);
    lblPowerUp    = mkLbl("PowerUp", btnPowerUp, 10);
    // (no shortcut hints for hitbox/hazard/break/float — buttons are narrow)

    // Group 3 labels (no shortcut hints — these are action buttons)
    lblSave  = mkLbl("Save", btnSave);
    lblLoad  = mkLbl("Load", btnLoad);
    lblClear = mkLbl("Clear", btnClear);
    lblPlay  = mkLbl("Play", btnPlay);
    lblBack  = mkLbl("< Menu", btnBack);
    // Gravity label reflects current mode
    {
        std::string gLbl = (mLevel.gravityMode == GravityMode::WallRun)     ? "Wall Run"
                           : (mLevel.gravityMode == GravityMode::OpenWorld) ? "Open World"
                                                                            : "Platform";
        lblGravity       = mkLbl(gLbl, btnGravity, 11);
    }

    lblStatus = std::make_unique<Text>(
        mStatusMsg, SDL_Color{180, 180, 200, 255}, BTN_GAP, TOOLBAR_H + 4, 12);
    lblTool = std::make_unique<Text>(
        "Pan", SDL_Color{255, 215, 0, 255}, window.GetWidth() - PALETTE_W - 120, 22, 13);
}

// --- Unload ----------------------------------------------------------------
void LevelEditorScene::Unload() {
    // PaletteItems for folders point to mFolderIcon (non-owning), so don't free
    // item.thumb for folder entries — only free file thumbnails.
    for (auto& i : mPaletteItems) {
        if (!i.isFolder) {
            if (i.thumb)
                SDL_DestroySurface(i.thumb);
            if (i.full)
                SDL_DestroySurface(i.full);
        }
    }
    mPaletteItems.clear();
    for (auto& i : mBgItems) {
        if (i.thumb)
            SDL_DestroySurface(i.thumb);
    }
    mBgItems.clear();
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
            if (mActiveTool == Tool::Action && IsAnimatedTile(path)) {
                float fmx, fmy;
                SDL_GetMouseState(&fmx, &fmy);
                int mx = (int)fmx, my = (int)fmy;
                int ti = (my >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, my) : -1;
                if (ti >= 0 && mLevel.tiles[ti].action) {
                    mLevel.tiles[ti].actionDestroyAnim = path;
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
    if (mMovPlatSpeedInput) {
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            // Only allow digits
            for (char ch : std::string(e.text.text))
                if (ch >= '0' && ch <= '9')
                    mMovPlatSpeedStr += ch;
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_BACKSPACE && !mMovPlatSpeedStr.empty()) {
                mMovPlatSpeedStr.pop_back();
                return true;
            }
            if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER ||
                e.key.key == SDLK_ESCAPE || e.key.key == SDLK_TAB) {
                // Commit value
                if (!mMovPlatSpeedStr.empty()) {
                    int v            = std::clamp(std::stoi(mMovPlatSpeedStr), 10, 2000);
                    mMovPlatSpeed    = (float)v;
                    mMovPlatSpeedStr = std::to_string(v);
                    // Apply to current session tiles
                    for (int idx : mMovPlatIndices)
                        mLevel.tiles[idx].moveSpeed = mMovPlatSpeed;
                    // Also apply to ALL moving tiles in the current group
                    for (auto& t : mLevel.tiles) {
                        if (!t.moving)
                            continue;
                        bool inGroup =
                            (mMovPlatCurGroupId != 0 &&
                             t.moveGroupId == mMovPlatCurGroupId) ||
                            std::any_of(mMovPlatIndices.begin(),
                                        mMovPlatIndices.end(),
                                        [&](int i) { return &t == &mLevel.tiles[i]; });
                        if (inGroup)
                            t.moveSpeed = mMovPlatSpeed;
                    }
                }
                mMovPlatSpeedInput = false;
                SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                return true;
            }
        }
        return true; // swallow all other input while field is focused
    }

    // ── Import text input ─────────────────────────────────────────────────────
    if (mImportInputActive) {
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            mImportInputText += e.text.text;
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_ESCAPE:
                    mImportInputActive = false;
                    mImportInputText.clear();
                    SetStatus("Import cancelled");
                    SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                    return true;
                case SDLK_BACKSPACE:
                    if (!mImportInputText.empty())
                        mImportInputText.pop_back();
                    return true;
                case SDLK_RETURN:
                case SDLK_KP_ENTER: {
                    std::string path   = mImportInputText;
                    mImportInputActive = false;
                    mImportInputText.clear();
                    SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                    if (!path.empty())
                        ImportPath(path);
                    return true;
                }
                default:
                    break;
            }
        }
        return true;
    }

    // ── Delete confirmation popup ──────────────────────────────────────────
    if (mDelConfirmActive) {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
            mDelConfirmActive = false;
            SetStatus("Delete cancelled");
            return true;
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (HitTest(mDelConfirmYes, mx, my)) {
                std::error_code ec;
                if (mDelConfirmIsDir)
                    fs::remove_all(mDelConfirmPath, ec);
                else
                    fs::remove(mDelConfirmPath, ec);
                mDelConfirmActive = false;
                // Refresh the right palette depending on what was deleted
                bool wasBg = (mDelConfirmPath.rfind(BG_ROOT, 0) == 0);
                if (wasBg)
                    LoadBgPalette();
                else
                    LoadTileView(mTileCurrentDir);
                SetStatus((mDelConfirmIsDir ? "Deleted folder: " : "Deleted: ") +
                          mDelConfirmName);
                return true;
            }
            if (HitTest(mDelConfirmNo, mx, my)) {
                mDelConfirmActive = false;
                SetStatus("Delete cancelled");
                return true;
            }
        }
        return true; // swallow all other input while popup is open
    }

    // ── Pan: middle-mouse drag OR Ctrl + left-mouse drag ────────────────────
    auto startPan = [&](int mx, int my) {
        mCamera.StartPan(mx, my);
    };

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
        !(SDL_GetModState() & SDL_KMOD_CTRL) && mActiveTool == Tool::MoveCam) {
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
        if ((SDL_GetModState() & SDL_KMOD_CTRL) && mActiveTool == Tool::MovingPlat &&
            mx < CanvasW()) {
            mMovPlatRange = std::max(GRID * 1.0f, mMovPlatRange + e.wheel.y * GRID);
            // Update current session tiles
            for (int idx : mMovPlatIndices)
                mLevel.tiles[idx].moveRange = mMovPlatRange;
            // Also update the hovered tile's group (handles already-placed platforms)
            int hovTi = (my >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, my) : -1;
            if (hovTi >= 0 && mLevel.tiles[hovTi].moving) {
                int grp = mLevel.tiles[hovTi].moveGroupId;
                for (auto& t : mLevel.tiles) {
                    if (!t.moving)
                        continue;
                    if (grp != 0 ? (t.moveGroupId == grp) : (&t == &mLevel.tiles[hovTi]))
                        t.moveRange = mMovPlatRange;
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
            if (mActiveTab == PaletteTab::Tiles) {
                mPaletteScroll = std::max(0, mPaletteScroll - (int)e.wheel.y);
                int rows       = ((int)mPaletteItems.size() + PAL_COLS - 1) / PAL_COLS;
                mPaletteScroll = std::min(mPaletteScroll, std::max(0, rows - 1));
            } else {
                mBgPaletteScroll = std::max(0, mBgPaletteScroll - (int)e.wheel.y);
                mBgPaletteScroll =
                    std::min(mBgPaletteScroll, std::max(0, (int)mBgItems.size() - 1));
            }
        } else if (mActiveTool == Tool::Tile) {
            // e.wheel.y is a float in SDL3 — accumulate fractional ticks
            // and step by GRID only when the buffer crosses ±0.5.
            mScrollAccum += e.wheel.y;
            int steps = (int)mScrollAccum; // truncate toward zero
            if (steps != 0) {
                mScrollAccum -= steps;
                mTileW = std::max(GRID, mTileW + steps * GRID);
                mTileH = mTileW;
                SetStatus("Tile size: " + std::to_string(mTileW));
            }
        } else if (mActiveTool == Tool::Action) {
            // Accumulate fractional SDL3 scroll ticks, step by 1 hit per full tick
            float fmya;
            SDL_GetMouseState(nullptr, &fmya);
            int mya       = (int)fmya;
            int hovAction = (mya >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, mya) : -1;
            if (hovAction >= 0 && mLevel.tiles[hovAction].action) {
                mScrollAccum += e.wheel.y;
                int steps = (int)mScrollAccum;
                if (steps != 0) {
                    mScrollAccum -= steps;
                    int& hits = mLevel.tiles[hovAction].actionHits;
                    hits      = std::clamp(hits + steps, 1, 99);
                    SetStatus("Action tile hits: " + std::to_string(hits));
                }
            } else {
                mScrollAccum = 0.0f; // not hovering a tile, discard accumulation
            }
        } else if (mActiveTool == Tool::Slope) {
            // Shift+scroll on a slope tile: adjust slopeHeightFrac in 0.05 steps
            float fmxs, fmys;
            SDL_GetMouseState(&fmxs, &fmys);
            int mxs = (int)fmxs, mys = (int)fmys;
            int hovSlope = (mys >= TOOLBAR_H && mxs < CanvasW()) ? HitTile(mxs, mys) : -1;
            if (hovSlope >= 0 && mLevel.tiles[hovSlope].slope != SlopeType::None) {
                float& frac = mLevel.tiles[hovSlope].slopeHeightFrac;
                frac        = std::clamp(frac + e.wheel.y * 0.05f, 0.05f, 1.0f);
                // Round to nearest 0.05 to avoid floating-point drift
                frac = std::round(frac * 20.0f) / 20.0f;
                SetStatus("Slope height: " + std::to_string((int)(frac * 100)) +
                          "%  (scroll to adjust)");
            }
        } else if (mActiveTool == Tool::MovingPlat) {
            float fmx2, fmy2;
            SDL_GetMouseState(&fmx2, &fmy2);
            int  mxw = (int)fmx2, myw = (int)fmy2;
            int  hovTi = (myw >= TOOLBAR_H && mxw < CanvasW()) ? HitTile(mxw, myw) : -1;
            bool ctrl  = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
            bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
            if (ctrl && hovTi >= 0 && mLevel.tiles[hovTi].moving) {
                // Ctrl+scroll: adjust starting phase for the tile AND its whole group
                auto& ht       = mLevel.tiles[hovTi];
                float newPhase = ht.movePhase + e.wheel.y * 0.05f;
                // Wrap instead of clamp so you can scroll continuously
                if (newPhase < 0.0f)
                    newPhase += 1.0f;
                if (newPhase > 1.0f)
                    newPhase -= 1.0f;
                int grp = ht.moveGroupId;
                for (auto& t : mLevel.tiles) {
                    if (!t.moving)
                        continue;
                    if (grp != 0 ? (t.moveGroupId == grp) : (&t == &ht))
                        t.movePhase = newPhase;
                }
                SetStatus((grp != 0 ? "Group " + std::to_string(grp)
                                    : "Tile " + std::to_string(hovTi)) +
                          "  phase=" + std::to_string(newPhase).substr(0, 4) +
                          "  dir=" + (ht.moveLoopDir > 0 ? "+1(right)" : "-1(left)"));
            } else if (shift && hovTi >= 0 && mLevel.tiles[hovTi].moving) {
                // Shift+scroll: flip start direction for the tile AND its whole group
                int newDir = (e.wheel.y > 0) ? 1 : -1;
                int grp    = mLevel.tiles[hovTi].moveGroupId;
                for (auto& t : mLevel.tiles) {
                    if (!t.moving)
                        continue;
                    if (grp != 0 ? (t.moveGroupId == grp) : (&t == &mLevel.tiles[hovTi]))
                        t.moveLoopDir = newDir;
                }
                SetStatus(
                    (grp != 0 ? "Group " + std::to_string(grp)
                              : "Tile " + std::to_string(hovTi)) +
                    "  dir=" + (newDir > 0 ? "+1 (starts right)" : "-1 (starts left)"));
            } else {
                // Plain scroll: adjust range for current group
                mMovPlatRange = std::max(48.0f, mMovPlatRange + (int)e.wheel.y * GRID);
                for (int idx : mMovPlatIndices)
                    mLevel.tiles[idx].moveRange = mMovPlatRange;
                SetStatus("MovePlat range=" + std::to_string((int)mMovPlatRange) +
                          "  spd=" + std::to_string((int)mMovPlatSpeed) +
                          (mMovPlatLoop ? "  LOOP" : "") +
                          (mMovPlatTrigger ? "  TRIGGER" : ""));
            }
        }
    }

    // ── Key down ──────────────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
            case SDLK_Q:
                mActiveTool = Tool::Select;
                lblTool->CreateSurface("Select");
                mSelBoxing   = false;
                mSelDragging = false;
                break;
            case SDLK_1:
                mActiveTool = Tool::Coin;
                lblTool->CreateSurface("Coin");
                break;
            case SDLK_2:
                mActiveTool = Tool::Enemy;
                lblTool->CreateSurface("Enemy");
                break;
            case SDLK_3:
                mActiveTool = Tool::Tile;
                lblTool->CreateSurface("Tile");
                mActiveTab = PaletteTab::Tiles;
                break;
            case SDLK_4:
                mActiveTool = Tool::Erase;
                lblTool->CreateSurface("Erase");
                break;
            case SDLK_5:
                mActiveTool = Tool::PlayerStart;
                lblTool->CreateSurface("Player");
                break;
            case SDLK_6:
                mActiveTab = PaletteTab::Backgrounds;
                lblTool->CreateSurface("Backgrounds");
                break;
            case SDLK_7:
            case SDLK_R:
                mActiveTool = Tool::Resize;
                lblTool->CreateSurface("Resize");
                mIsResizing   = false;
                mHoverEdge    = ResizeEdge::None;
                mHoverTileIdx = -1;
                break;
            case SDLK_8:
            case SDLK_P:
                mActiveTool = Tool::Prop;
                lblTool->CreateSurface("Prop");
                break;
            case SDLK_9:
            case SDLK_L:
                mActiveTool = Tool::Ladder;
                lblTool->CreateSurface("Ladder");
                break;
            case SDLK_0:
                mActiveTool = Tool::Action;
                lblTool->CreateSurface("Action");
                CloseAnimPicker();
                break;
            case SDLK_T:
                mActiveTool = Tool::MoveCam;
                lblTool->CreateSurface("Pan");
                break;
            case SDLK_MINUS:
                mActiveTool = Tool::Slope;
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
                auto [gx, gy]       = Text::CenterInRect(gLbl, 11, btnGravity);
                lblGravity =
                    std::make_unique<Text>(gLbl, SDL_Color{255, 255, 255, 255}, gx, gy, 11);
                SetStatus(gStatus);
                break;
            }

            case SDLK_I:
                mImportInputActive = true;
                mImportInputText.clear();
                SDL_StartTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                SetStatus(mActiveTab == PaletteTab::Backgrounds
                              ? "Import bg path or folder (Enter=go, Esc=cancel):"
                              : "Import tile path or folder (Enter=go, Esc=cancel):");
                break;
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
                    } else if (!mLevel.coins.empty()) {
                        mLevel.coins.pop_back();
                        SetStatus("Undo coin");
                    } else if (!mLevel.enemies.empty()) {
                        mLevel.enemies.pop_back();
                        SetStatus("Undo enemy");
                    }
                }
                break;
            case SDLK_DELETE:
            case SDLK_BACKSPACE:
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
            case SDLK_TAB:
                mPaletteCollapsed = !mPaletteCollapsed;
                SetStatus(mPaletteCollapsed ? "Palette hidden (Tab to show)"
                                            : "Palette visible (Tab to hide)");
                break;
            case SDLK_ESCAPE:
                if (mActionAnimPickerTile >= 0) {
                    CloseAnimPicker();
                    break;
                }
                if (mActiveTool == Tool::Select) {
                    mSelIndices.clear();
                    mSelBoxing   = false;
                    mSelDragging = false;
                    SetStatus("Selection cleared");
                    break; // don't fall through to the tile-browser Esc handler
                }
                // Navigate back up in tile browser
                if (mActiveTab == PaletteTab::Tiles && mTileCurrentDir != TILE_ROOT) {
                    fs::path    parent = fs::path(mTileCurrentDir).parent_path();
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
        if (mActiveTool == Tool::Action && my >= TOOLBAR_H && mx < CanvasW()) {
            int ti = HitTile(mx, my);
            if (ti >= 0 && mLevel.tiles[ti].action) {
                // Right-click cycles through available death animations:
                // None -> anim1 -> anim2 -> ... -> last -> None -> ...
                // The thumbnail badge updates immediately on each click.
                {
                    auto  manifests = ScanAnimatedTiles();
                    auto& cur       = mLevel.tiles[ti].actionDestroyAnim;
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
        if (mActiveTool == Tool::MovingPlat && my >= TOOLBAR_H && mx < CanvasW()) {
            // Preset cycle:
            //  H  96  60  (default horiz, medium)
            //  H  48  40  (horiz, short)
            //  H 192  80  (horiz, long)
            //  V  96  60  (vert, medium)
            //  V 192  50  (vert, long)
            //  V  48  40  (vert, short)
            //  → back to H 96 60
            if (mMovPlatHoriz && mMovPlatRange == 96 && mMovPlatSpeed == 60 &&
                !mMovPlatLoop) {
                mMovPlatHoriz = true;
                mMovPlatRange = 48;
                mMovPlatSpeed = 40;
                mMovPlatLoop  = false;
            } else if (mMovPlatHoriz && mMovPlatRange == 48 && !mMovPlatLoop) {
                mMovPlatHoriz = true;
                mMovPlatRange = 192;
                mMovPlatSpeed = 80;
                mMovPlatLoop  = false;
            } else if (mMovPlatHoriz && mMovPlatRange == 192 && !mMovPlatLoop) {
                mMovPlatHoriz = false;
                mMovPlatRange = 96;
                mMovPlatSpeed = 60;
                mMovPlatLoop  = false;
            } else if (!mMovPlatHoriz && mMovPlatRange == 96 && !mMovPlatLoop) {
                mMovPlatHoriz = false;
                mMovPlatRange = 192;
                mMovPlatSpeed = 50;
                mMovPlatLoop  = false;
            } else if (!mMovPlatHoriz && mMovPlatRange == 192 && !mMovPlatLoop) {
                mMovPlatHoriz = false;
                mMovPlatRange = 48;
                mMovPlatSpeed = 40;
                mMovPlatLoop  = false;
            } else if (!mMovPlatLoop) {
                mMovPlatHoriz   = true;
                mMovPlatRange   = 1800;
                mMovPlatSpeed   = 150;
                mMovPlatLoop    = true;
                mMovPlatTrigger = true;
            } else {
                mMovPlatHoriz   = true;
                mMovPlatRange   = 96;
                mMovPlatSpeed   = 60;
                mMovPlatLoop    = false;
                mMovPlatTrigger = false;
            }
            // Update all tiles already in the group
            for (int idx : mMovPlatIndices) {
                mLevel.tiles[idx].moveHoriz   = mMovPlatHoriz;
                mLevel.tiles[idx].moveRange   = mMovPlatRange;
                mLevel.tiles[idx].moveSpeed   = mMovPlatSpeed;
                mLevel.tiles[idx].moveLoop    = mMovPlatLoop;
                mLevel.tiles[idx].moveTrigger = mMovPlatTrigger;
            }
            SetStatus(std::string(mMovPlatHoriz ? "H" : "V") +
                      "  range=" + std::to_string((int)mMovPlatRange) + "  spd=" +
                      std::to_string((int)mMovPlatSpeed) + "  (RClick cycles presets)");
            return true;
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_RIGHT) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (my >= TOOLBAR_H && mx < CanvasW()) {
            int ti = HitTile(mx, my);
            if (ti >= 0) {
                if (mActiveTool == Tool::Action && mLevel.tiles[ti].action) {
                    int& grp = mLevel.tiles[ti].actionGroup;
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
            } else if (mActiveTool == Tool::Tile) {
                // Hovering empty canvas space — cycle ghost rotation
                mGhostRotation = (mGhostRotation + 90) % 360;
                SetStatus("Ghost rotation: " + std::to_string(mGhostRotation) +
                          "deg  (RClick to cycle)");
                return true;
            }
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = (int)e.button.x, my = (int)e.button.y;

        // ── Destroy-anim picker — handle clicks inside the popup ──────────────────
        if (mActionAnimPickerTile >= 0 && !mAnimPickerEntries.empty()) {
            if (HitTest(mActionAnimPickerRect, mx, my)) {
                // Re-derive cell geometry (must match Render exactly)
                const int THUMB    = 48;
                const int ROW_H    = THUMB + 10;
                const int PAD      = 8;
                const int COL_W    = THUMB + PAD * 2;
                const int COLS     = 4;
                const int TITLE_H  = 28;
                int       px       = mActionAnimPickerRect.x;
                int       py       = mActionAnimPickerRect.y;
                int       ey       = py + TITLE_H;
                int       nEntries = (int)mAnimPickerEntries.size();
                for (int i = 0; i < nEntries; i++) {
                    int      col  = i % COLS;
                    int      row  = i / COLS;
                    int      ex   = px + PAD + col * COL_W;
                    int      ey2  = ey + PAD + row * (ROW_H + PAD);
                    SDL_Rect cell = {ex, ey2, COL_W - PAD, ROW_H};
                    if (HitTest(cell, mx, my)) {
                        const auto& entry = mAnimPickerEntries[i];
                        if (mActionAnimPickerTile < (int)mLevel.tiles.size()) {
                            mLevel.tiles[mActionAnimPickerTile].actionDestroyAnim =
                                entry.path;
                            if (!entry.path.empty())
                                GetDestroyAnimThumb(entry.path);
                            SetStatus("Tile " + std::to_string(mActionAnimPickerTile) +
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
                mPaletteCollapsed = !mPaletteCollapsed;
                return true;
            }
        }

        // Tab bar (only when expanded)
        if (!mPaletteCollapsed && mx >= CanvasW() + PALETTE_TAB_W && my >= TOOLBAR_H &&
            my < TOOLBAR_H + TAB_H) {
            int hw     = (PALETTE_W - PALETTE_TAB_W) / 2;
            mActiveTab = (mx < CanvasW() + PALETTE_TAB_W + hw) ? PaletteTab::Tiles
                                                               : PaletteTab::Backgrounds;
            return true;
        }

        // Toolbar group collapse pills (strip lives below the buttons at STRIP_Y_L)
        if (HitTest(mGrp1Pill, mx, my)) {
            mGrp1Collapsed = !mGrp1Collapsed;
            RebuildToolbarLayout();
            SetStatus(mGrp1Collapsed ? "Place group collapsed" : "Place group expanded");
            return true;
        }
        if (HitTest(mGrp2Pill, mx, my)) {
            mGrp2Collapsed = !mGrp2Collapsed;
            RebuildToolbarLayout();
            SetStatus(mGrp2Collapsed ? "Modifier group collapsed"
                                     : "Modifier group expanded");
            return true;
        }
        if (HitTest(mGrp3Pill, mx, my)) {
            mGrp3Collapsed = !mGrp3Collapsed;
            RebuildToolbarLayout();
            SetStatus(mGrp3Collapsed ? "Actions group collapsed" : "Actions group expanded");
            return true;
        }

        // Toolbar
        // Any toolbar click closes the anim picker
        if (mActionAnimPickerTile >= 0 && my < TOOLBAR_H)
            CloseAnimPicker();

        auto tb = [&](SDL_Rect r, Tool t, const std::string& l) {
            if (!HitTest(r, mx, my))
                return false;
            if (t != Tool::Select) {
                mSelIndices.clear();
                mSelBoxing   = false;
                mSelDragging = false;
            }
            CloseAnimPicker();
            mActiveTool = t;
            lblTool->CreateSurface(l);
            return true;
        };
        if (tb(btnCoin, Tool::Coin, "Coin"))
            return true;
        if (tb(btnEnemy, Tool::Enemy, "Enemy"))
            return true;
        if (tb(btnTile, Tool::Tile, "Tile"))
            return true;
        if (tb(btnErase, Tool::Erase, "Erase"))
            return true;
        if (tb(btnPlayerStart, Tool::PlayerStart, "Player"))
            return true;
        if (HitTest(btnSelect, mx, my)) {
            mActiveTool = Tool::Select;
            lblTool->CreateSurface("Select");
            mSelBoxing   = false;
            mSelDragging = false;
            return true;
        }
        if (HitTest(btnMoveCam, mx, my)) {
            mActiveTool = Tool::MoveCam;
            lblTool->CreateSurface("Pan");
            return true;
        }
        if (tb(btnProp, Tool::Prop, "Prop"))
            return true;
        if (tb(btnLadder, Tool::Ladder, "Ladder"))
            return true;
        if (tb(btnAction, Tool::Action, "Action"))
            return true;
        if (tb(btnSlope, Tool::Slope, "Slope"))
            return true;
        if (HitTest(btnResize, mx, my)) {
            mActiveTool = Tool::Resize;
            lblTool->CreateSurface("Resize");
            mIsResizing   = false;
            mHoverEdge    = ResizeEdge::None;
            mHoverTileIdx = -1;
            return true;
        }
        if (HitTest(btnHitbox, mx, my)) {
            mActiveTool = Tool::Hitbox;
            lblTool->CreateSurface("Hitbox");
            mHitboxDragging = false;
            mHoverHitboxHdl = HitboxHandle::None;
            return true;
        }
        if (HitTest(btnHazard, mx, my)) {
            mActiveTool = Tool::Hazard;
            lblTool->CreateSurface("Hazard");
            return true;
        }
        if (HitTest(btnAntiGrav, mx, my)) {
            mActiveTool = Tool::AntiGrav;
            lblTool->CreateSurface("Float");
            return true;
        }
        if (HitTest(btnPowerUp, mx, my)) {
            mActiveTool       = Tool::PowerUp;
            mPowerUpPopupOpen = true;
            mPowerUpTileIdx   = -1;
            lblTool->CreateSurface("PowerUp");
            SetStatus("PowerUp: click a tile to assign a power-up pickup");
            return true;
        }
        if (HitTest(btnMovingPlat, mx, my)) {
            mActiveTool = Tool::MovingPlat;
            lblTool->CreateSurface("MovingPlat");
            // Pick next group ID by scanning existing tiles so we never reuse one
            int maxUsed = 0;
            for (const auto& ts : mLevel.tiles)
                if (ts.moveGroupId > maxUsed)
                    maxUsed = ts.moveGroupId;
            mMovPlatNextGroupId = maxUsed + 1;
            mMovPlatCurGroupId  = mMovPlatNextGroupId++;
            mMovPlatIndices.clear();
            // Seed editor state from existing moving tiles so re-selecting the tool
            // doesn't reset values that were already saved on the level tiles.
            // Find the first existing moving tile and read its values back.
            for (int i = 0; i < (int)mLevel.tiles.size(); i++) {
                if (!mLevel.tiles[i].moving)
                    continue;
                const auto& first = mLevel.tiles[i];
                mMovPlatHoriz     = first.moveHoriz;
                mMovPlatRange     = first.moveRange;
                mMovPlatSpeed     = first.moveSpeed;
                mMovPlatLoop      = first.moveLoop;
                mMovPlatTrigger   = first.moveTrigger;
                break;
            }
            // Open config popup and seed speed string from current value
            mMovPlatPopupOpen  = true;
            mMovPlatSpeedInput = false;
            mMovPlatSpeedStr   = std::to_string((int)mMovPlatSpeed);
            SetStatus("MovingPlat: click tiles to add. RClick=axis/range. New group ID=" +
                      std::to_string(mMovPlatCurGroupId));
            return true;
        }

        if (HitTest(btnSave, mx, my)) {
            fs::create_directories("levels");
            std::string path = "levels/" + mLevelName + ".json";
            mLevel.name      = mLevelName;
            SaveLevel(mLevel, path);
            SetStatus("Saved: " + path);
            return true;
        }
        if (HitTest(btnLoad, mx, my)) {
            std::string path = "levels/" + mLevelName + ".json";
            if (LoadLevel(path, mLevel)) {
                SetStatus("Loaded: " + path);
                if (!mLevel.background.empty())
                    background = std::make_unique<Image>(
                        mLevel.background, FitModeFromString(mLevel.bgFitMode));
                LoadBgPalette();
                mCamera.SetPosition(0.0f, 0.0f); // reset editor camera on load
            } else
                SetStatus("No file: " + path);
            return true;
        }
        if (HitTest(btnClear, mx, my)) {
            mLevel.coins.clear();
            mLevel.enemies.clear();
            mLevel.tiles.clear();
            SetStatus("Cleared");
            return true;
        }
        if (HitTest(btnPlay, mx, my)) {
            fs::create_directories("levels");
            std::string path = "levels/" + mLevelName + ".json";
            mLevel.name      = mLevelName;
            SaveLevel(mLevel, path);
            mLaunchGame = true;
            return true;
        }
        if (HitTest(btnBack, mx, my)) {
            // Auto-save before leaving so work isn't lost
            fs::create_directories("levels");
            std::string path = "levels/" + mLevelName + ".json";
            mLevel.name      = mLevelName;
            SaveLevel(mLevel, path);
            mGoBack = true;
            return true;
        }
        if (HitTest(btnGravity, mx, my)) {
            if (mLevel.gravityMode == GravityMode::Platformer)
                mLevel.gravityMode = GravityMode::WallRun;
            else if (mLevel.gravityMode == GravityMode::WallRun)
                mLevel.gravityMode = GravityMode::OpenWorld;
            else
                mLevel.gravityMode = GravityMode::Platformer;
            std::string gLbl    = (mLevel.gravityMode == GravityMode::WallRun) ? "Wall Run"
                                  : (mLevel.gravityMode == GravityMode::OpenWorld)
                                      ? "Open World"
                                      : "Platform";
            std::string gStatus = (mLevel.gravityMode == GravityMode::WallRun)
                                      ? "Mode: Wall Run"
                                  : (mLevel.gravityMode == GravityMode::OpenWorld)
                                      ? "Mode: Open World (top-down)"
                                      : "Mode: Platformer";
            auto [gx, gy]       = Text::CenterInRect(gLbl, 11, btnGravity);
            lblGravity =
                std::make_unique<Text>(gLbl, SDL_Color{255, 255, 255, 255}, gx, gy, 11);
            SetStatus(gStatus);
            return true;
        }

        // ── Palette panel ──────────────────────────────────────────────────────
        if (mx >= CanvasW() && my >= TOOLBAR_H + TAB_H) {
            if (mActiveTab == PaletteTab::Tiles) {
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

                int idx = (mPaletteScroll + row) * PAL_COLS + col;
                if (idx < 0 || idx >= (int)mPaletteItems.size())
                    return true;

                const auto& item = mPaletteItems[idx];

                // Delete button hit? open confirm popup instead of deleting immediately
                if (item.delBtn.x >= 0 && HitTest(item.delBtn, mx, my)) {
                    mDelConfirmActive = true;
                    mDelConfirmPath   = item.path;
                    mDelConfirmIsDir  = item.isFolder;
                    mDelConfirmName   = fs::path(item.path).filename().string();
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
                    Uint64 now = SDL_GetTicks();
                    bool   isDouble =
                        (idx == mLastClickIndex && (now - mLastClickTime) < DOUBLE_CLICK_MS);
                    mLastClickIndex = idx;
                    mLastClickTime  = now;

                    mSelectedTile  = idx;
                    mGhostRotation = 0; // reset rotation when a new tile is picked
                    mActiveTool    = Tool::Tile;
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
                        // Cycle: cover -> contain -> stretch -> tile -> scroll -> cover
                        auto& fm = mLevel.bgFitMode;
                        if (fm == "cover")
                            fm = "contain";
                        else if (fm == "contain")
                            fm = "stretch";
                        else if (fm == "stretch")
                            fm = "tile";
                        else if (fm == "tile")
                            fm = "scroll";
                        else
                            fm = "cover";
                        // Rebuild background image with new fit mode
                        if (!mLevel.background.empty())
                            background = std::make_unique<Image>(mLevel.background,
                                                                 FitModeFromString(fm));
                        // Force badge cache rebuild for the new label
                        lblBgHeader.reset();
                        SetStatus("Background fit: " + fm);
                        return true;
                    }
                }

                int relY = my - TOOLBAR_H - TAB_H - 24 - PAD; // 24 = bg header strip
                int row  = relY / itemH;
                int idx  = mBgPaletteScroll + row;
                if (idx >= 0 && idx < (int)mBgItems.size()) {
                    // Delete button?
                    if (mBgItems[idx].delBtn.x >= 0 &&
                        HitTest(mBgItems[idx].delBtn, mx, my)) {
                        mDelConfirmActive = true;
                        mDelConfirmPath   = mBgItems[idx].path;
                        mDelConfirmIsDir  = false;
                        mDelConfirmName   = fs::path(mBgItems[idx].path).filename().string();
                    } else {
                        ApplyBackground(idx);
                    }
                }
            }
            return true;
        }

        // ── Canvas ─────────────────────────────────────────────────────────────
        // Moving-platform config popup click handling
        if (mMovPlatPopupOpen && HitTest(mMovPlatPopupRect, mx, my)) {
            const int PW      = 280;
            const int PAD     = 8;
            const int ROW_H   = 26;
            const int TITLE_H = 30;
            const int px      = mMovPlatPopupRect.x;
            const int py      = mMovPlatPopupRect.y;
            int       ry      = py + TITLE_H;

            // Row 0: speed field + close button
            SDL_Rect speedField = {
                px + PAD + 90, ry + (ROW_H - 20) / 2, PW - PAD * 2 - 90 - 44, 20};
            SDL_Rect closeBtnR = {px + PW - PAD - 36, ry + (ROW_H - 20) / 2, 36, 20};
            if (HitTest(speedField, mx, my)) {
                mMovPlatSpeedInput = true;
                SDL_StartTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                return true;
            }
            if (HitTest(closeBtnR, mx, my)) {
                if (!mMovPlatSpeedStr.empty()) {
                    int v         = std::clamp(std::stoi(mMovPlatSpeedStr), 10, 2000);
                    mMovPlatSpeed = (float)v;
                    // Apply to current session tiles
                    for (int idx : mMovPlatIndices)
                        mLevel.tiles[idx].moveSpeed = mMovPlatSpeed;
                    // Also apply to ALL moving tiles in the current group so editing
                    // an already-placed platform actually takes effect.
                    for (auto& t : mLevel.tiles) {
                        if (!t.moving)
                            continue;
                        bool inGroup =
                            (mMovPlatCurGroupId != 0 &&
                             t.moveGroupId == mMovPlatCurGroupId) ||
                            std::any_of(mMovPlatIndices.begin(),
                                        mMovPlatIndices.end(),
                                        [&](int i) { return &t == &mLevel.tiles[i]; });
                        if (inGroup)
                            t.moveSpeed = mMovPlatSpeed;
                    }
                }
                mMovPlatPopupOpen  = false;
                mMovPlatSpeedInput = false;
                SDL_StopTextInput(mWindow ? mWindow->GetRaw() : nullptr);
                return true;
            }
            ry += ROW_H + PAD;

            // Row 1: H/V direction toggle
            SDL_Rect btnH = {px + PAD + 90, ry, 48, ROW_H - 4};
            SDL_Rect btnV = {px + PAD + 90 + 54, ry, 48, ROW_H - 4};
            if (HitTest(btnH, mx, my)) {
                mMovPlatHoriz = true;
                for (int idx : mMovPlatIndices)
                    mLevel.tiles[idx].moveHoriz = true;
                return true;
            }
            if (HitTest(btnV, mx, my)) {
                mMovPlatHoriz = false;
                for (int idx : mMovPlatIndices)
                    mLevel.tiles[idx].moveHoriz = false;
                return true;
            }
            ry += ROW_H + PAD;

            // Row 2: Loop (ping-pong) checkbox row
            SDL_Rect loopRow = {px + PAD, ry, PW - PAD * 2, ROW_H};
            if (HitTest(loopRow, mx, my)) {
                mMovPlatLoop = !mMovPlatLoop;
                if (!mMovPlatLoop)
                    mMovPlatTrigger = false;
                for (int idx : mMovPlatIndices) {
                    mLevel.tiles[idx].moveLoop    = mMovPlatLoop;
                    mLevel.tiles[idx].moveTrigger = mMovPlatTrigger;
                }
                return true;
            }
            ry += ROW_H + PAD;

            // Row 3: Move on Touch checkbox row
            SDL_Rect trigRow = {px + PAD, ry, PW - PAD * 2, ROW_H};
            if (HitTest(trigRow, mx, my)) {
                mMovPlatTrigger = !mMovPlatTrigger;
                for (int idx : mMovPlatIndices)
                    mLevel.tiles[idx].moveTrigger = mMovPlatTrigger;
                return true;
            }

            return true; // absorb all other clicks inside popup
        }

        if (my < TOOLBAR_H || mx >= CanvasW())
            return true;
        auto [sx, sy] = SnapToGrid(mx, my);

        switch (mActiveTool) {
            case Tool::Coin:
                mLevel.coins.push_back({(float)sx, (float)sy});
                SetStatus("Coin at " + std::to_string(sx) + "," + std::to_string(sy));
                break;
            case Tool::Enemy:
                mLevel.enemies.push_back({(float)sx, (float)sy, ENEMY_SPEED});
                SetStatus("Enemy at " + std::to_string(sx) + "," + std::to_string(sy));
                break;
            case Tool::Tile:
                if (!mPaletteItems.empty() && !mPaletteItems[mSelectedTile].isFolder) {
                    auto newTile     = TileSpawn{(float)sx,
                                             (float)sy,
                                             mTileW,
                                             mTileH,
                                             mPaletteItems[mSelectedTile].path};
                    newTile.rotation = mGhostRotation;
                    mLevel.tiles.push_back(std::move(newTile));
                    SetStatus(
                        "Tile: " + mPaletteItems[mSelectedTile].label +
                        (mGhostRotation ? "  rot=" + std::to_string(mGhostRotation) : ""));
                }
                break;
            case Tool::Erase: {
                // Coins and enemies are checked before tiles so that objects
                // placed on top of tiles are deleted first (layering order).
                int ci = HitCoin(mx, my);
                if (ci >= 0) {
                    mLevel.coins.erase(mLevel.coins.begin() + ci);
                    SetStatus("Erased coin");
                    break;
                }
                int ei = HitEnemy(mx, my);
                if (ei >= 0) {
                    mLevel.enemies.erase(mLevel.enemies.begin() + ei);
                    SetStatus("Erased enemy");
                    break;
                }
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    mLevel.tiles.erase(mLevel.tiles.begin() + ti);
                    SetStatus("Erased tile");
                    break;
                }
                break;
            }
            case Tool::PlayerStart:
                mLevel.player = {(float)sx, (float)sy};
                SetStatus("Player start set");
                break;
            case Tool::Prop: {
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    bool nowProp          = !mLevel.tiles[ti].prop;
                    mLevel.tiles[ti].prop = nowProp;
                    // Prop, Ladder, and Action are mutually exclusive; Hazard is compatible
                    // with Prop
                    if (nowProp) {
                        mLevel.tiles[ti].ladder = false;
                        mLevel.tiles[ti].action = false;
                        mLevel.tiles[ti].slope  = SlopeType::None;
                    }
                    bool isHazard = mLevel.tiles[ti].hazard;
                    SetStatus(std::string("Tile ") + std::to_string(ti) +
                              (nowProp ? (isHazard ? " → prop+hazard (walk-through, damages)"
                                                   : " → prop (no collision)")
                                       : " → solid (collision on)"));
                }
                return true;
            }
            case Tool::Ladder: {
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    bool nowLadder          = !mLevel.tiles[ti].ladder;
                    mLevel.tiles[ti].ladder = nowLadder;
                    // Ladder, Prop, and Action are mutually exclusive
                    if (nowLadder) {
                        mLevel.tiles[ti].prop   = false;
                        mLevel.tiles[ti].action = false;
                        mLevel.tiles[ti].slope  = SlopeType::None;
                    }
                    SetStatus(
                        std::string("Tile ") + std::to_string(ti) +
                        (nowLadder ? " → ladder (climbable)" : " → solid (ladder removed)"));
                }
                return true;
            }
            case Tool::Action: {
                // If the picker is open and the click landed outside it, close it
                // and fall through so the click can act on whatever tile is below.
                if (mActionAnimPickerTile >= 0) {
                    if (HitTest(mActionAnimPickerRect, mx, my))
                        return true; // handled by the picker block above
                    CloseAnimPicker();
                    // fall through — open picker for the newly clicked tile if applicable
                }
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    if (mLevel.tiles[ti].action) {
                        // Tile is already an action tile — open the anim picker
                        OpenAnimPicker(ti);
                        SetStatus("Tile " + std::to_string(ti) + ": choose death animation");
                    } else {
                        // Not an action tile yet — make it one
                        mLevel.tiles[ti].action = true;
                        mLevel.tiles[ti].prop   = false;
                        mLevel.tiles[ti].ladder = false;
                        mLevel.tiles[ti].slope  = SlopeType::None;
                        SetStatus("Tile " + std::to_string(ti) +
                                  " → action  (click again to assign death anim)");
                    }
                } else {
                    CloseAnimPicker();
                }
                return true;
            }
            case Tool::Slope: {
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    // Cycle: None → DiagUpRight → DiagUpLeft → None
                    SlopeType   next;
                    std::string label;
                    if (mLevel.tiles[ti].slope == SlopeType::None) {
                        next  = SlopeType::DiagUpRight;
                        label = "DiagUpRight (rises left→right)";
                    } else if (mLevel.tiles[ti].slope == SlopeType::DiagUpRight) {
                        next  = SlopeType::DiagUpLeft;
                        label = "DiagUpLeft  (rises right→left)";
                    } else {
                        next  = SlopeType::None;
                        label = "slope removed";
                    }
                    mLevel.tiles[ti].slope = next;
                    // Slope, Prop, Ladder, and Action are mutually exclusive
                    if (next != SlopeType::None) {
                        mLevel.tiles[ti].prop   = false;
                        mLevel.tiles[ti].ladder = false;
                        mLevel.tiles[ti].action = false;
                    }
                    SetStatus(std::string("Tile ") + std::to_string(ti) + " → " + label);
                }
                return true;
            }
            case Tool::Hitbox: {
                // Priority 1: if we already have a tile selected and the cursor
                // is on one of its handles, start the drag immediately — don't
                // let HitTile() steal focus to an adjacent tile.
                if (mHitboxTileIdx >= 0 && mHoverHitboxHdl != HitboxHandle::None) {
                    auto& t         = mLevel.tiles[mHitboxTileIdx];
                    mHitboxDragging = true;
                    mHitboxHandle   = mHoverHitboxHdl;
                    mHitboxDragX    = mx;
                    mHitboxDragY    = my;
                    mHitboxOrigOffX = t.hitboxOffX;
                    mHitboxOrigOffY = t.hitboxOffY;
                    mHitboxOrigW    = t.hitboxW;
                    mHitboxOrigH    = t.hitboxH;
                    return true;
                }
                // Priority 2: if a tile is already selected and the click lands
                // inside its visual bounds, keep it selected (don't switch to a
                // neighbour that also happens to overlap the cursor position).
                bool keptCurrent = false;
                if (mHitboxTileIdx >= 0 && mHitboxTileIdx < (int)mLevel.tiles.size()) {
                    const auto& ct  = mLevel.tiles[mHitboxTileIdx];
                    auto [csx, csy]  = mCamera.WorldToScreen(ct.x, ct.y);
                    int         csw = (int)(ct.w * mCamera.Zoom());
                    int         csh = (int)(ct.h * mCamera.Zoom());
                    if (mx >= csx && mx <= csx + csw && my >= csy && my <= csy + csh)
                        keptCurrent = true;
                }
                if (!keptCurrent) {
                    // Click is outside the current tile — select whichever tile
                    // the cursor is over, or deselect if on empty space.
                    int ti = HitTile(mx, my);
                    if (ti >= 0) {
                        mHitboxTileIdx = ti;
                        auto& t        = mLevel.tiles[ti];
                        // Initialise hitbox to full tile on first use
                        if (t.hitboxW == 0 && t.hitboxH == 0) {
                            t.hitboxOffX = 0;
                            t.hitboxOffY = 0;
                            t.hitboxW    = t.w;
                            t.hitboxH    = t.h;
                        }
                        SetStatus("Hitbox: tile " + std::to_string(ti) +
                                  "  drag edges to adjust");
                    } else {
                        mHitboxTileIdx = -1; // deselect on empty space
                    }
                }
                return true;
            }
            case Tool::Hazard: {
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    bool nowHazard          = !mLevel.tiles[ti].hazard;
                    mLevel.tiles[ti].hazard = nowHazard;
                    if (nowHazard) {
                        // Prop is intentionally NOT cleared — hazard+prop = walk-through
                        // hazard
                        mLevel.tiles[ti].ladder = false;
                        mLevel.tiles[ti].action = false;
                        mLevel.tiles[ti].slope  = SlopeType::None;
                    }
                    bool isProp = mLevel.tiles[ti].prop;
                    SetStatus(std::string("Tile ") + std::to_string(ti) +
                              (nowHazard ? (isProp ? " → hazard+prop (walk-through, damages)"
                                                   : " → hazard (solid, 30 HP/sec)")
                                         : " → solid (hazard removed)"));
                }
                return true;
            }
            case Tool::AntiGrav: {
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    bool now                     = !mLevel.tiles[ti].antiGravity;
                    mLevel.tiles[ti].antiGravity = now;
                    SetStatus("Tile " + std::to_string(ti) +
                              (now ? " -> floating (anti-gravity)" : " -> normal gravity"));
                    return true;
                }
                int ei = HitEnemy(mx, my);
                if (ei >= 0) {
                    bool now                       = !mLevel.enemies[ei].antiGravity;
                    mLevel.enemies[ei].antiGravity = now;
                    SetStatus("Enemy " + std::to_string(ei) +
                              (now ? " -> floating" : " -> normal gravity"));
                    return true;
                }
                return true;
            }
            case Tool::PowerUp: {
                // Handle PowerUp popup click: close popup if open and click is in it
                if (mPowerUpPopupOpen && mPowerUpTileIdx >= 0 &&
                    HitTest(mPowerUpPopupRect, mx, my)) {
                    // Re-derive cell geometry matching Render
                    const auto& reg = GetPowerUpRegistry();
                    const int   PAD = 8, ROW_H = 28, TITLE_H = 32;
                    int         py = mPowerUpPopupRect.y + TITLE_H;
                    for (int i = 0; i < (int)reg.size(); i++) {
                        SDL_Rect row = {mPowerUpPopupRect.x + PAD,
                                        py + i * (ROW_H + 2),
                                        mPowerUpPopupRect.w - PAD * 2,
                                        ROW_H};
                        if (HitTest(row, mx, my)) {
                            // Assign this power-up to the tile
                            auto& t           = mLevel.tiles[mPowerUpTileIdx];
                            t.powerUp         = true;
                            t.powerUpType     = reg[i].id;
                            t.powerUpDuration = reg[i].defaultDuration;
                            SetStatus("Tile " + std::to_string(mPowerUpTileIdx) +
                                      " -> PowerUp: " + reg[i].label);
                            mPowerUpPopupOpen = false;
                            mPowerUpTileIdx   = -1;
                            return true;
                        }
                    }
                    // 'None' row (clear)
                    SDL_Rect noneRow = {mPowerUpPopupRect.x + PAD,
                                        py + (int)reg.size() * (ROW_H + 2),
                                        mPowerUpPopupRect.w - PAD * 2,
                                        ROW_H};
                    if (HitTest(noneRow, mx, my)) {
                        mLevel.tiles[mPowerUpTileIdx].powerUp     = false;
                        mLevel.tiles[mPowerUpTileIdx].powerUpType = "";
                        SetStatus("Tile " + std::to_string(mPowerUpTileIdx) +
                                  " -> PowerUp removed");
                        mPowerUpPopupOpen = false;
                        mPowerUpTileIdx   = -1;
                        return true;
                    }
                    return true; // absorb
                }
                // Click outside popup or no popup open: open/reopen on tile
                mPowerUpPopupOpen = false;
                mPowerUpTileIdx   = -1;
                int ti            = HitTile(mx, my);
                if (ti >= 0) {
                    mPowerUpTileIdx   = ti;
                    mPowerUpPopupOpen = true;
                    // Position popup near the tile
                    auto [wsx, wsy] = WorldToScreen(mLevel.tiles[ti].x, mLevel.tiles[ti].y);
                    const auto& reg = GetPowerUpRegistry();
                    int         ph  = 32 + (int)(reg.size() + 1) * 30 + 8;
                    int         pw  = 200;
                    int px2 = std::clamp(wsx, 0, (mWindow ? mWindow->GetWidth() : 800) - pw);
                    int py2 = std::clamp(wsy + mLevel.tiles[ti].h,
                                         TOOLBAR_H,
                                         (mWindow ? mWindow->GetHeight() : 600) - ph);
                    mPowerUpPopupRect = {px2, py2, pw, ph};
                    SetStatus("Tile " + std::to_string(ti) + ": choose power-up type");
                } else {
                    SetStatus("PowerUp: click a tile to assign a power-up pickup");
                }
                return true;
            }
            case Tool::MovingPlat: {
                // Left-click: toggle tile into/out of the current moving group
                int ti = HitTile(mx, my);
                if (ti >= 0) {
                    auto& t = mLevel.tiles[ti];
                    // If the tile is already moving but from a *different* session group,
                    // adopt its group so the speed popup edits the right group.
                    if (t.moving &&
                        std::find(mMovPlatIndices.begin(), mMovPlatIndices.end(), ti) ==
                            mMovPlatIndices.end()) {
                        // Clicking an existing platform from a previous session: adopt its
                        // group
                        mMovPlatCurGroupId =
                            (t.moveGroupId != 0) ? t.moveGroupId : mMovPlatNextGroupId++;
                        mMovPlatHoriz    = t.moveHoriz;
                        mMovPlatRange    = t.moveRange;
                        mMovPlatSpeed    = t.moveSpeed;
                        mMovPlatLoop     = t.moveLoop;
                        mMovPlatTrigger  = t.moveTrigger;
                        mMovPlatSpeedStr = std::to_string((int)mMovPlatSpeed);
                        // Collect all tiles in that group into mMovPlatIndices
                        mMovPlatIndices.clear();
                        for (int i = 0; i < (int)mLevel.tiles.size(); i++) {
                            if (!mLevel.tiles[i].moving)
                                continue;
                            bool inGrp = (t.moveGroupId != 0)
                                             ? (mLevel.tiles[i].moveGroupId == t.moveGroupId)
                                             : (i == ti);
                            if (inGrp)
                                mMovPlatIndices.push_back(i);
                        }
                        SetStatus("Adopted platform group " +
                                  std::to_string(mMovPlatCurGroupId) +
                                  "  spd=" + std::to_string((int)mMovPlatSpeed) +
                                  "  tiles=" + std::to_string(mMovPlatIndices.size()));
                        return true;
                    }
                    // If already in this group, remove it
                    auto it = std::find(mMovPlatIndices.begin(), mMovPlatIndices.end(), ti);
                    if (it != mMovPlatIndices.end()) {
                        mMovPlatIndices.erase(it);
                        t.moving      = false;
                        t.moveGroupId = 0;
                        SetStatus("Tile " + std::to_string(ti) +
                                  " removed from platform group " +
                                  std::to_string(mMovPlatCurGroupId));
                    } else {
                        // Add to current group
                        mMovPlatIndices.push_back(ti);
                        t.moving      = true;
                        t.moveHoriz   = mMovPlatHoriz;
                        t.moveRange   = mMovPlatRange;
                        t.moveSpeed   = mMovPlatSpeed;
                        t.moveLoop    = mMovPlatLoop;
                        t.moveTrigger = mMovPlatTrigger;
                        t.movePhase   = 0.0f; // set per-tile via Ctrl+scroll in editor
                        t.moveLoopDir = 1;    // set per-tile via Shift+scroll in editor
                        t.moveGroupId =
                            (mMovPlatIndices.size() > 1) ? mMovPlatCurGroupId : 0;
                        // Re-apply group id to all tiles in group
                        if (mMovPlatIndices.size() > 1) {
                            for (int idx : mMovPlatIndices)
                                mLevel.tiles[idx].moveGroupId = mMovPlatCurGroupId;
                        }
                        SetStatus("Tile " + std::to_string(ti) +
                                  " added to platform group " +
                                  std::to_string(mMovPlatCurGroupId) + "  " +
                                  (mMovPlatHoriz ? "H" : "V") +
                                  "  range=" + std::to_string((int)mMovPlatRange) +
                                  "  spd=" + std::to_string((int)mMovPlatSpeed));
                    }
                }
                return true;
            }
            case Tool::Select: {
                // Click on a selected tile: start moving. Click elsewhere: start
                // rubber-band.
                auto [wx, wy] = ScreenToWorld(mx, my);
                int  ti       = HitTile(mx, my);
                bool hitSelected =
                    (ti >= 0 && std::find(mSelIndices.begin(), mSelIndices.end(), ti) !=
                                    mSelIndices.end());
                if (hitSelected) {
                    // Begin drag-move of entire selection
                    mSelDragging    = true;
                    mSelDragStartWX = wx;
                    mSelDragStartWY = wy;
                    mSelOrigPositions.clear();
                    for (int idx : mSelIndices)
                        mSelOrigPositions.push_back(
                            {mLevel.tiles[idx].x, mLevel.tiles[idx].y});
                } else {
                    // Click on unselected tile: select just it (or begin rubber-band on
                    // empty space)
                    if (ti >= 0) {
                        // Shift-click: add/remove from selection
                        if (SDL_GetModState() & SDL_KMOD_SHIFT) {
                            auto it = std::find(mSelIndices.begin(), mSelIndices.end(), ti);
                            if (it != mSelIndices.end())
                                mSelIndices.erase(it);
                            else
                                mSelIndices.push_back(ti);
                        } else {
                            mSelIndices = {ti};
                        }
                        SetStatus("Selected " + std::to_string(mSelIndices.size()) +
                                  " tile(s)");
                    } else {
                        // Empty space: begin rubber-band, clear selection unless Shift
                        if (!(SDL_GetModState() & SDL_KMOD_SHIFT))
                            mSelIndices.clear();
                        mSelBoxing = true;
                        mSelBoxX0 = mSelBoxX1 = wx;
                        mSelBoxY0 = mSelBoxY1 = wy;
                    }
                }
                return true;
            }
            case Tool::MoveCam:
                // Pan is handled by the dedicated pan block above; nothing to do on click.
                break;
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
                    SetStatus("Resizing tile " + std::to_string(mHoverTileIdx));
                    return true;
                }
                break;
        }

        // Only start drag for placement tools that don't have their own case above.
        // Coin/Enemy/Tile/PlayerStart already returned — this catches Select and modifier
        // tools that might be clicked on top of an existing entity.
        if (mActiveTool != Tool::Erase && mActiveTool != Tool::Coin &&
            mActiveTool != Tool::Enemy && mActiveTool != Tool::Tile &&
            mActiveTool != Tool::MoveCam && mActiveTool != Tool::PlayerStart) {
            int ti = HitTile(mx, my);
            if (ti >= 0) {
                mIsDragging = true;
                mDragIndex  = ti;
                mDragIsTile = true;
                mDragIsCoin = false;
                return true;
            }
            int ci = HitCoin(mx, my);
            if (ci >= 0) {
                mIsDragging = true;
                mDragIndex  = ci;
                mDragIsCoin = true;
                mDragIsTile = false;
                return true;
            }
            int ei = HitEnemy(mx, my);
            if (ei >= 0) {
                mIsDragging = true;
                mDragIndex  = ei;
                mDragIsCoin = false;
                mDragIsTile = false;
                return true;
            }
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
                SetStatus("Hitbox: off(" + std::to_string(t.hitboxOffX) + "," +
                          std::to_string(t.hitboxOffY) + ") size(" +
                          std::to_string(t.hitboxW) + "x" + std::to_string(t.hitboxH) + ")");
            }
        }
        // Commit rubber-band selection
        if (mSelBoxing) {
            mSelBoxing = false;
            int rx0    = std::min(mSelBoxX0, mSelBoxX1);
            int ry0    = std::min(mSelBoxY0, mSelBoxY1);
            int rx1    = std::max(mSelBoxX0, mSelBoxX1);
            int ry1    = std::max(mSelBoxY0, mSelBoxY1);
            if (!(SDL_GetModState() & SDL_KMOD_SHIFT))
                mSelIndices.clear();
            for (int i = 0; i < (int)mLevel.tiles.size(); i++) {
                const auto& t = mLevel.tiles[i];
                // Include tile if it overlaps the marquee rect
                if ((int)t.x + t.w > rx0 && (int)t.x < rx1 && (int)t.y + t.h > ry0 &&
                    (int)t.y < ry1) {
                    if (std::find(mSelIndices.begin(), mSelIndices.end(), i) ==
                        mSelIndices.end())
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
        if (mDropActive && mActiveTool == Tool::Action) {
            int ti = (my >= TOOLBAR_H && mx < CanvasW()) ? HitTile(mx, my) : -1;
            mActionAnimDropHover = (ti >= 0 && mLevel.tiles[ti].action) ? ti : -1;
        } else {
            mActionAnimDropHover = -1;
        }

        // ── Selection rubber-band / drag-move ────────────────────────────────────
        if (mSelBoxing) {
            auto [wx, wy] = ScreenToWorld(mx, my);
            mSelBoxX1     = wx;
            mSelBoxY1     = wy;
            return true;
        }
        if (mSelDragging && !mSelOrigPositions.empty()) {
            auto [wx, wy] = ScreenToWorld(mx, my);
            int dx        = wx - mSelDragStartWX;
            int dy        = wy - mSelDragStartWY;
            for (int i = 0; i < (int)mSelIndices.size(); i++) {
                int idx = mSelIndices[i];
                if (idx < (int)mLevel.tiles.size()) {
                    // Snap each tile's displaced position to grid
                    float rawX          = mSelOrigPositions[i].first + dx;
                    float rawY          = mSelOrigPositions[i].second + dy;
                    mLevel.tiles[idx].x = (float)((int(rawX) / GRID) * GRID);
                    mLevel.tiles[idx].y = (float)((int(rawY) / GRID) * GRID);
                }
            }
            return true;
        }

        // ── Resize drag ───────────────────────────────────────────────────────
        if (mIsResizing && mResizeTileIdx >= 0 &&
            mResizeTileIdx < (int)mLevel.tiles.size()) {
            auto& t  = mLevel.tiles[mResizeTileIdx];
            int   dx = mx - mResizeDragX;
            int   dy = my - mResizeDragY;
            if (mResizeEdge == ResizeEdge::Right || mResizeEdge == ResizeEdge::Corner) {
                int newW = std::max(GRID, ((mResizeOrigW + dx + GRID / 2) / GRID) * GRID);
                t.w      = newW;
            }
            if (mResizeEdge == ResizeEdge::Bottom || mResizeEdge == ResizeEdge::Corner) {
                int newH = std::max(GRID, ((mResizeOrigH + dy + GRID / 2) / GRID) * GRID);
                t.h      = newH;
            }
            SetStatus("Resize: " + std::to_string(t.w) + "x" + std::to_string(t.h));
            return true;
        }

        // ── Hitbox drag update ────────────────────────────────────────────────
        if (mHitboxDragging && mHitboxTileIdx >= 0 &&
            mHitboxTileIdx < (int)mLevel.tiles.size()) {
            auto& t  = mLevel.tiles[mHitboxTileIdx];
            int   dx = mx - mHitboxDragX;
            int   dy = my - mHitboxDragY;
            // Minimum hitbox side: 4px
            constexpr int MIN_SIDE = 4;
            switch (mHitboxHandle) {
                case HitboxHandle::Left: {
                    int newOff   = std::min(mHitboxOrigOffX + dx,
                                          mHitboxOrigOffX + mHitboxOrigW - MIN_SIDE);
                    int delta    = newOff - mHitboxOrigOffX;
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
                    int newOff   = std::min(mHitboxOrigOffY + dy,
                                          mHitboxOrigOffY + mHitboxOrigH - MIN_SIDE);
                    int delta    = newOff - mHitboxOrigOffY;
                    t.hitboxOffY = newOff;
                    t.hitboxH    = std::max(MIN_SIDE, mHitboxOrigH - delta);
                    break;
                }
                case HitboxHandle::Bottom:
                    t.hitboxH = std::max(MIN_SIDE, mHitboxOrigH + dy);
                    t.hitboxH = std::min(t.hitboxH, t.h - t.hitboxOffY);
                    break;
                case HitboxHandle::TopLeft: {
                    int newOffX = std::min(mHitboxOrigOffX + dx,
                                           mHitboxOrigOffX + mHitboxOrigW - MIN_SIDE);
                    int newOffY = std::min(mHitboxOrigOffY + dy,
                                           mHitboxOrigOffY + mHitboxOrigH - MIN_SIDE);
                    t.hitboxW =
                        std::max(MIN_SIDE, mHitboxOrigW - (newOffX - mHitboxOrigOffX));
                    t.hitboxH =
                        std::max(MIN_SIDE, mHitboxOrigH - (newOffY - mHitboxOrigOffY));
                    t.hitboxOffX = newOffX;
                    t.hitboxOffY = newOffY;
                    break;
                }
                case HitboxHandle::TopRight: {
                    int newOffY = std::min(mHitboxOrigOffY + dy,
                                           mHitboxOrigOffY + mHitboxOrigH - MIN_SIDE);
                    t.hitboxW   = std::max(MIN_SIDE, mHitboxOrigW + dx);
                    t.hitboxH =
                        std::max(MIN_SIDE, mHitboxOrigH - (newOffY - mHitboxOrigOffY));
                    t.hitboxOffY = newOffY;
                    break;
                }
                case HitboxHandle::BotLeft: {
                    int newOffX = std::min(mHitboxOrigOffX + dx,
                                           mHitboxOrigOffX + mHitboxOrigW - MIN_SIDE);
                    t.hitboxW =
                        std::max(MIN_SIDE, mHitboxOrigW - (newOffX - mHitboxOrigOffX));
                    t.hitboxH    = std::max(MIN_SIDE, mHitboxOrigH + dy);
                    t.hitboxOffX = newOffX;
                    break;
                }
                case HitboxHandle::BotRight:
                    t.hitboxW = std::max(MIN_SIDE, mHitboxOrigW + dx);
                    t.hitboxH = std::max(MIN_SIDE, mHitboxOrigH + dy);
                    break;
                default:
                    break;
            }
            // Clamp offsets to stay inside the visual tile
            t.hitboxOffX = std::max(0, std::min(t.hitboxOffX, t.w - MIN_SIDE));
            t.hitboxOffY = std::max(0, std::min(t.hitboxOffY, t.h - MIN_SIDE));
            SetStatus("Hitbox: off(" + std::to_string(t.hitboxOffX) + "," +
                      std::to_string(t.hitboxOffY) + ") size(" + std::to_string(t.hitboxW) +
                      "x" + std::to_string(t.hitboxH) + ")");
            return true;
        }

        // ── Hitbox handle hover detection ─────────────────────────────────────
        if (mActiveTool == Tool::Hitbox && mHitboxTileIdx >= 0 && !mHitboxDragging) {
            mHoverHitboxHdl = HitboxHandle::None;
            const auto& t   = mLevel.tiles[mHitboxTileIdx];
            // Hitbox is in world space; convert to screen space for mouse comparison
            auto [htsx, htsy] = mCamera.WorldToScreen(t.x + t.hitboxOffX, t.y + t.hitboxOffY);
            int       hx    = htsx;
            int       hy    = htsy;
            int       hw    = (int)(t.hitboxW * mCamera.Zoom());
            int       hh    = (int)(t.hitboxH * mCamera.Zoom());
            const int H     = HBOX_HANDLE;
            bool      nearL = (mx >= hx - H && mx <= hx + H);
            bool      nearR = (mx >= hx + hw - H && mx <= hx + hw + H);
            bool      nearT = (my >= hy - H && my <= hy + H);
            bool      nearB = (my >= hy + hh - H && my <= hy + hh + H);
            bool      inH   = (mx >= hx && mx <= hx + hw);
            bool      inV   = (my >= hy && my <= hy + hh);
            if (nearL && nearT)
                mHoverHitboxHdl = HitboxHandle::TopLeft;
            else if (nearR && nearT)
                mHoverHitboxHdl = HitboxHandle::TopRight;
            else if (nearL && nearB)
                mHoverHitboxHdl = HitboxHandle::BotLeft;
            else if (nearR && nearB)
                mHoverHitboxHdl = HitboxHandle::BotRight;
            else if (nearL && inV)
                mHoverHitboxHdl = HitboxHandle::Left;
            else if (nearR && inV)
                mHoverHitboxHdl = HitboxHandle::Right;
            else if (nearT && inH)
                mHoverHitboxHdl = HitboxHandle::Top;
            else if (nearB && inH)
                mHoverHitboxHdl = HitboxHandle::Bottom;
        }

        // ── Resize hover detection ────────────────────────────────────────────
        if (mActiveTool == Tool::Resize && my >= TOOLBAR_H && mx < CanvasW()) {
            mHoverEdge    = ResizeEdge::None;
            mHoverTileIdx = -1;
            // Iterate reverse (topmost drawn tile first) so a small tile in
            // front of a large one gets handle priority, matching HitTile().
            for (int i = (int)mLevel.tiles.size() - 1; i >= 0; i--) {
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
            auto [sx, sy] = SnapToGrid(mx, my);
            if (mDragIsTile && mDragIndex < (int)mLevel.tiles.size()) {
                mLevel.tiles[mDragIndex].x = (float)sx;
                mLevel.tiles[mDragIndex].y = (float)sy;
            } else if (mDragIsCoin && mDragIndex < (int)mLevel.coins.size()) {
                mLevel.coins[mDragIndex].x = (float)sx;
                mLevel.coins[mDragIndex].y = (float)sy;
            } else if (!mDragIsCoin && !mDragIsTile &&
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
void LevelEditorScene::Render(Window& window) {
    window.Render();
    SDL_Renderer* ren = window.GetRenderer();
    // The editor is an SDL_Surface-based pipeline (pixel manipulation, cached
    // surface transforms, badge blitting). Render to an intermediate surface
    // then upload to the GPU renderer once per frame.
    int          W = mWindow->GetWidth(), H = mWindow->GetHeight();
    SDL_Surface* screen = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_ARGB8888);
    if (!screen) {
        window.Update();
        return;
    }
    // Transparent initial state so background shows through undrawn areas
    SDL_SetSurfaceBlendMode(screen, SDL_BLENDMODE_BLEND);
    SDL_FillSurfaceRect(
        screen,
        nullptr,
        SDL_MapRGBA(SDL_GetPixelFormatDetails(screen->format), nullptr, 0, 0, 0, 0));
    int cw = CanvasW();

    // ── Camera aliases for brevity — mCamera owns these values now ──────────
    const float mCamX = mCamera.X();
    const float mCamY = mCamera.Y();
    const float mZoom = mCamera.Zoom();

    // Background renders directly to the GPU renderer (it's a full-screen image)
    if (background->GetFitMode() == FitMode::SCROLL)
        background->RenderScrolling(ren, mCamX, 0.0f); // 0 = use image width as scroll range
    else
        background->Render(ren);

    // Shared badge blit helper — hoisted here so it's available throughout Render.
    auto blitBadge = [&](SDL_Surface* s, int bx, int by) {
        if (!s)
            return;
        SDL_Rect d = {bx, by, s->w, s->h};
        SDL_BlitSurface(s, nullptr, screen, &d);
    };

    // Grid — each line's screen position is computed fresh from its world coordinate
    // so lines always land exactly where tile edges land. Stepping by an integer
    // pixel count (gridPx) accumulates truncation error; recomputing per-line avoids it.
    //
    // Alpha scales with zoom: at 100% zoom lines are fully visible (alpha=20);
    // as you zoom out they fade so the canvas doesn't become a dense mesh.
    // At ZOOM_MIN (0.25) alpha bottoms out at ~4 so lines are still faintly there.
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(screen->format);
    {
        // Map zoom range [ZOOM_MIN..1.0] → alpha [4..20] linearly, clamped.
        // Above 1.0 (zoomed in) alpha stays at 20 — lines are already very visible.
        float  zoomT     = std::clamp((mZoom - EditorCamera::ZOOM_MIN) / (1.0f - EditorCamera::ZOOM_MIN), 0.0f, 1.0f);
        Uint8  gridAlpha = (Uint8)(4.0f + zoomT * 16.0f); // 4 at min zoom, 20 at 100%+
        Uint32 gridCol   = SDL_MapRGBA(fmt, nullptr, 255, 255, 255, gridAlpha);

        int firstCol = (int)std::floor(mCamX / GRID);
        int firstRow = (int)std::floor(mCamY / GRID);
        int numCols  = (int)std::ceil(cw / (GRID * mZoom)) + 2;
        int numRows  = (int)std::ceil(window.GetHeight() / (GRID * mZoom)) + 2;
        for (int i = 0; i < numCols; i++) {
            float worldX = (firstCol + i) * (float)GRID;
            int   sx     = (int)std::round((worldX - mCamX) * mZoom);
            if (sx < 0 || sx >= cw)
                continue;
            SDL_Rect l = {sx, TOOLBAR_H, 1, window.GetHeight() - TOOLBAR_H};
            SDL_FillSurfaceRect(screen, &l, gridCol);
        }
        for (int i = 0; i < numRows; i++) {
            float worldY = (firstRow + i) * (float)GRID;
            int   sy     = (int)std::round((worldY - mCamY) * mZoom);
            if (sy < TOOLBAR_H || sy >= window.GetHeight())
                continue;
            SDL_Rect l = {0, sy, cw, 1};
            SDL_FillSurfaceRect(screen, &l, gridCol);
        }
    }

    // Placed tiles — all positions are in world space; subtract camera offset for screen
    // space
    for (int ti = 0; ti < (int)mLevel.tiles.size(); ti++) {
        const auto& t = mLevel.tiles[ti];
        // Cull tiles fully outside the viewport
        int tsx = (int)((t.x - mCamX) * mZoom);
        int tsy = (int)((t.y - mCamY) * mZoom);
        int tsw = (int)(t.w * mZoom);
        int tsh = (int)(t.h * mZoom);
        if (tsx + tsw <= 0 || tsx >= cw || tsy + tsh <= TOOLBAR_H ||
            tsy >= window.GetHeight())
            continue;
        // O(1) cache lookup — no linear search, no IMG_Load per frame
        SDL_Surface* ts = mSurfaceCache.FindTileSurface(t.imagePath);
        SDL_Rect     dst = {tsx, tsy, tsw, tsh};
        if (ts) {
            // Rotation: use the per-path rotation cache so we build each
            // rotated surface at most once and reuse it every frame.
            SDL_Surface* draw =
                (t.rotation != 0) ? GetRotated(t.imagePath, ts, t.rotation) : ts;
            if (t.prop)
                SDL_SetSurfaceColorMod(draw, 120, 255, 120);
            if (t.ladder)
                SDL_SetSurfaceColorMod(draw, 120, 220, 255);
            if (t.action)
                SDL_SetSurfaceColorMod(draw, 255, 160, 80);
            if (t.hazard)
                SDL_SetSurfaceColorMod(draw, 255, 80, 80);
            SDL_BlitSurfaceScaled(draw, nullptr, screen, &dst, SDL_SCALEMODE_LINEAR);
            if (t.prop || t.ladder || t.action || t.hazard)
                SDL_SetSurfaceColorMod(draw, 255, 255, 255);
        } else {
            DrawRect(screen, dst, {80, 80, 120, 200}); // missing asset placeholder
        }
        // Outline colour: cyan for ladder, green for prop, orange for action, blue for solid
        SDL_Color outlineCol = t.ladder   ? SDL_Color{0, 220, 220, 255}
                               : t.prop   ? SDL_Color{80, 255, 80, 255}
                               : t.action ? SDL_Color{255, 160, 60, 255}
                               : t.hazard ? SDL_Color{255, 60, 60, 255}
                                          : SDL_Color{100, 180, 255, 255};
        DrawOutline(screen, dst, outlineCol);
        // ── Stacked top-left badges — each one shifts right so all are visible ──
        // Order: P → L → A → H → F → M (moving platform handled separately below)
        {
            int           bx = tsx + 2; // cursor: advances right as badges are added
            constexpr int BH = 14, BW = 14, GAP = 2;
            auto          drawBadge = [&](const char* label, SDL_Color bg, SDL_Color fg) {
                DrawRect(screen, {bx, tsy + 2, BW, BH}, bg);
                blitBadge(GetBadge(label, fg), bx + 2, tsy + 2);
                bx += BW + GAP;
            };
            if (t.prop)
                drawBadge("P", {0, 180, 0, 210}, {255, 255, 255, 255});
            if (t.ladder)
                drawBadge("L", {0, 160, 180, 210}, {255, 255, 255, 255});
            if (t.action) {
                std::string ab = "A";
                if (t.actionGroup > 0)
                    ab += std::to_string(t.actionGroup);
                if (t.actionHits > 1)
                    ab += "x" + std::to_string(t.actionHits);
                int abw = (int)ab.size() * 6 + 4;
                DrawRect(screen, {bx, tsy + 2, abw, BH}, {200, 100, 0, 200});
                blitBadge(GetBadge(ab, {255, 255, 255, 255}), bx + 2, tsy + 2);
                bx += abw + GAP;
            }
            if (t.hazard)
                drawBadge("H", {200, 0, 0, 220}, {255, 255, 255, 255});
            if (t.antiGravity)
                drawBadge("F", {0, 180, 200, 220}, {255, 255, 255, 255});
            if (t.powerUp) {
                std::string pb  = t.powerUpType.empty() ? "PU" : t.powerUpType.substr(0, 2);
                int         pw2 = (int)pb.size() * 6 + 4;
                DrawRect(screen, {bx, tsy + 2, pw2, BH}, {180, 0, 220, 220});
                DrawOutline(screen, {bx, tsy + 2, pw2, BH}, {255, 80, 255, 255});
                blitBadge(GetBadge(pb, {255, 255, 255, 255}), bx + 2, tsy + 2);
                bx += pw2 + GAP;
            }
            // slope badge — slightly different shape so keep its own block
            if (t.slope != SlopeType::None) {
                std::string badge = (t.slope == SlopeType::DiagUpRight) ? "/" : "\\";
                if (t.slopeHeightFrac < 0.99f)
                    badge += std::to_string((int)std::round(t.slopeHeightFrac * 100)) + "%";
                int bw2 = (int)badge.size() * 6 + 4;
                DrawRect(screen, {bx, tsy + 2, bw2, BH}, {160, 120, 0, 200});
                blitBadge(GetBadge(badge, {255, 255, 255, 255}), bx + 2, tsy + 2);
                bx += bw2 + GAP;
            }
            (void)bx; // suppress unused-variable warning if all are off
        }
        if (t.action) {
            // Bottom-right corner: death-anim indicator + group number label.
            // Shows a thumbnail of the assigned death anim, or a "+" hint when empty.
            // The group number is shown below the thumbnail so right-click cycling
            // is immediately visible without needing to read the top-left badge.
            constexpr int ANIM_BADGE_SZ = 16;
            int           abx           = tsx + tsw - ANIM_BADGE_SZ - 2;
            int           aby           = tsy + tsh - ANIM_BADGE_SZ - 2;
            if (abx > tsx + 14 + 2 && aby > tsy + 14) { // only draw if tile is large enough
                if (!t.actionDestroyAnim.empty()) {
                    // Purple tinted box + thumbnail of the first anim frame
                    DrawRect(screen,
                             {abx - 1, aby - 1, ANIM_BADGE_SZ + 2, ANIM_BADGE_SZ + 2},
                             {120, 0, 200, 200});
                    SDL_Surface* animThumb = GetDestroyAnimThumb(t.actionDestroyAnim);
                    if (animThumb) {
                        SDL_Rect dst2 = {abx, aby, ANIM_BADGE_SZ, ANIM_BADGE_SZ};
                        SDL_BlitSurfaceScaled(
                            animThumb, nullptr, screen, &dst2, SDL_SCALEMODE_LINEAR);
                    }
                    DrawOutline(screen,
                                {abx - 1, aby - 1, ANIM_BADGE_SZ + 2, ANIM_BADGE_SZ + 2},
                                {200, 80, 255, 255});
                } else if (mActiveTool == Tool::Action && tsw >= 24 && tsh >= 24) {
                    // Faint "+anim" hint to signal the tile can accept a drop
                    DrawRect(
                        screen, {abx, aby, ANIM_BADGE_SZ, ANIM_BADGE_SZ}, {60, 20, 80, 140});
                    DrawOutline(screen,
                                {abx, aby, ANIM_BADGE_SZ, ANIM_BADGE_SZ},
                                {140, 60, 180, 160});
                    blitBadge(GetBadge("+", {180, 100, 220, 200}), abx + 4, aby + 3);
                }
                // Group number label just to the left of the bottom-right badge.
                // Always shown on action tiles so right-click cycling is visible.
                // "G0" = standalone (no group), "G1".."G9" = grouped.
                if (mActiveTool == Tool::Action && tsw >= 40) {
                    std::string grpStr =
                        (t.actionGroup == 0) ? "G-" : ("G" + std::to_string(t.actionGroup));
                    SDL_Color grpCol =
                        (t.actionGroup == 0)
                            ? SDL_Color{120, 120, 140, 200} // dim for standalone
                            : SDL_Color{255, 220, 60, 255}; // bright gold when grouped
                    SDL_Surface* grpBadge = GetBadge(grpStr, grpCol);
                    if (grpBadge) {
                        int gx = abx - grpBadge->w - 3;
                        int gy = aby + ANIM_BADGE_SZ / 2 - grpBadge->h / 2;
                        blitBadge(grpBadge, gx, gy);
                    }
                }
            }

            // Drop-hover highlight: bright purple overlay + "Drop anim here" label
            if (ti == mActionAnimDropHover) {
                DrawRect(screen, {tsx, tsy, tsw, tsh}, {140, 0, 220, 70});
                DrawOutline(screen, {tsx, tsy, tsw, tsh}, {200, 80, 255, 255}, 2);
                blitBadge(GetBadge("Drop anim here", {255, 200, 255, 255}),
                          tsx + tsw / 2 - 42,
                          tsy + tsh / 2 - 5);
            }
        }
        if (t.slope != SlopeType::None) {
            // Draw the actual slope surface line respecting slopeHeightFrac.
            // High corner is always at tile top (tsy); low corner at tsy+riseH.
            int riseH = (int)(t.h * t.slopeHeightFrac);
            int highY = tsy;         // anchored at tile top
            int lowY  = tsy + riseH; // low corner descends by riseH
            int lx0, ly0, lx1, ly1;
            if (t.slope == SlopeType::DiagUpLeft) {
                lx0 = tsx;
                ly0 = lowY;
                lx1 = tsx + t.w;
                ly1 = highY;
            } else {
                lx0 = tsx;
                ly0 = highY;
                lx1 = tsx + t.w;
                ly1 = lowY;
            }
            int ddx = lx1 - lx0, ddy = ly1 - ly0;
            int steps = std::abs(ddx) > std::abs(ddy) ? std::abs(ddx) : std::abs(ddy);
            if (steps > 0) {
                float ssx = (float)ddx / steps, ssy = (float)ddy / steps;
                float ccx = (float)lx0, ccy = (float)ly0;
                for (int s = 0; s <= steps; ++s) {
                    DrawRect(screen, {(int)ccx, (int)ccy, 2, 2}, {255, 220, 50, 220});
                    ccx += ssx;
                    ccy += ssy;
                }
            }
            // Slope badge is now drawn in the stacked top-left badge block above
        }
        if (t.rotation != 0) {
            std::string rbadge = std::to_string(t.rotation);
            int         rbw    = 22;
            int         rbx    = tsx + t.w - rbw - 2;
            int         rby    = tsy + t.h - 14 - 2;
            DrawRect(screen, {rbx, rby, rbw, 14}, {60, 60, 180, 200});
            blitBadge(GetBadge(rbadge, {200, 220, 255, 255}), rbx + 2, rby + 1);
        }
    }

    // ── Moving platform tool overlay ───────────────────────────────────────────────────
    if (mActiveTool == Tool::MovingPlat) {
        // Highlight every moving tile in the level; brighten the current group
        for (int ti = 0; ti < (int)mLevel.tiles.size(); ti++) {
            const auto& t = mLevel.tiles[ti];
            if (!t.moving)
                continue;
            int  tsx = (int)((t.x - mCamX) * mZoom), tsy = (int)((t.y - mCamY) * mZoom);
            int  tsw = (int)(t.w * mZoom), tsh = (int)(t.h * mZoom);
            bool inCurGroup =
                std::find(mMovPlatIndices.begin(), mMovPlatIndices.end(), ti) !=
                mMovPlatIndices.end();
            // Teal fill for current group, purple for other groups
            SDL_Color fill =
                inCurGroup ? SDL_Color{0, 200, 200, 60} : SDL_Color{160, 80, 220, 40};
            DrawRect(screen, {tsx, tsy, tsw, tsh}, fill);
            SDL_Color border =
                inCurGroup ? SDL_Color{0, 255, 255, 220} : SDL_Color{180, 100, 255, 160};
            DrawOutline(screen, {tsx, tsy, tsw, tsh}, border, 2);

            // Travel path line and ghost tiles.
            // For grouped platforms, ALL tiles share the same end position (groupOriginX +
            // range) so the whole group's end ghost moves together as one.
            int cx = tsx + tsw / 2, cy = tsy + tsh / 2;
            int sw = (int)(t.w * mZoom), sh = (int)(t.h * mZoom);

            int lineStartX, lineEndX, lineStartY, lineEndY;
            if (t.moveHoriz) {
                lineStartX         = tsx;
                float endWX        = t.x + t.moveRange;
                int   endWXSnapped = ((int)endWX / GRID) * GRID;
                lineEndX           = (int)std::round((endWXSnapped - mCamX) * mZoom);
                DrawRect(screen,
                         {lineStartX, cy - 1, lineEndX - lineStartX, 2},
                         {0, 255, 255, 100});
            } else {
                lineStartY         = tsy;
                float endWY        = t.y + t.moveRange;
                int   endWYSnapped = ((int)endWY / GRID) * GRID;
                lineEndY           = (int)std::round((endWYSnapped - mCamY) * mZoom);
                DrawRect(screen,
                         {cx - 1, lineStartY, 2, lineEndY - lineStartY},
                         {0, 255, 255, 100});
            }

            // ── Direction arrow ──────────────────────────────────────────────
            // Draw a filled triangle on the tile showing start direction.
            // For loop (ping-pong) platforms also draw a start-position dot.
            if (t.moveHoriz) {
                // Arrow drawn at vertical centre of tile, offset from tile centre
                int acy = tsy + t.h / 2;                  // arrow vertical centre
                int dir = t.moveLoop ? t.moveLoopDir : 1; // sine always shows right
                // Arrow tip and base
                int tipX  = cx + dir * (t.w / 2 + 10); // tip points in travel direction
                int baseX = tipX - dir * 12;           // base of triangle
                // Triangle: tip + two base corners
                // Draw as 3 filled rects approximating a triangle
                for (int row = 0; row < 7; row++) {
                    int half = row; // widens from tip to base
                    int rx   = (dir > 0) ? tipX + row - 7 : tipX - row + 1;
                    DrawRect(screen, {rx, acy - half, 1, half * 2 + 1}, {255, 220, 0, 220});
                }
                // Ghost tiles at start (green) and end (red) of travel line.
                // Start ghost uses tsx/tsy exactly so it overlaps the real tile perfectly.
                // End ghost snaps to the nearest grid cell at t.x + moveRange.
                if (t.moveRange > 0.0f) {
                    // Start ghost: sits exactly on the real tile
                    DrawRectAlpha(screen, {tsx, tsy, sw, sh}, {0, 200, 80, 60});
                    DrawOutline(screen, {tsx, tsy, sw, sh}, {0, 255, 100, 200}, 2);
                    blitBadge(GetBadge("S", {0, 255, 120, 255}), tsx + 2, tsy + 2);
                    // End ghost: each tile shows its own end at t.x + moveRange.
                    // All tiles in a group have the same moveRange so they all shift
                    // by the same distance -- the whole group's ghosts move together.
                    float endWX        = t.x + t.moveRange;
                    int   endWXSnapped = ((int)endWX / GRID) * GRID;
                    int   endSX        = (int)std::round((endWXSnapped - mCamX) * mZoom);
                    DrawRectAlpha(screen, {endSX, tsy, sw, sh}, {220, 60, 60, 60});
                    DrawOutline(screen, {endSX, tsy, sw, sh}, {255, 80, 80, 200}, 2);
                    blitBadge(GetBadge("E", {255, 100, 100, 255}), endSX + 2, tsy + 2);
                    lineEndX = endSX;
                    // Phase tick on line
                    int phasePx = tsx + (int)((lineEndX - tsx) * t.movePhase);
                    DrawRect(screen, {phasePx - 1, cy - 12, 2, 24}, {255, 255, 0, 200});
                    std::string pctStr = std::to_string((int)(t.movePhase * 100)) + "%";
                    blitBadge(GetBadge(pctStr, {255, 255, 180, 255}), phasePx - 8, cy - 24);
                }
            } else {
                // Vertical arrow at horizontal centre
                int acx  = tsx + t.w / 2;
                int dir  = t.moveLoop ? t.moveLoopDir : 1;
                int tipY = cy + dir * (t.h / 2 + 10);
                for (int row = 0; row < 7; row++) {
                    int half = row;
                    int ry   = (dir > 0) ? tipY + row - 7 : tipY - row + 1;
                    DrawRect(screen, {acx - half, ry, half * 2 + 1, 1}, {255, 220, 0, 220});
                }
                // Ghost tiles at start (green) and end (red) of vertical travel line.
                // Start ghost uses tsx/tsy exactly. End ghost snaps to grid.
                if (t.moveRange > 0.0f) {
                    // Start ghost: sits exactly on the real tile
                    DrawRectAlpha(screen, {tsx, tsy, sw, sh}, {0, 200, 80, 60});
                    DrawOutline(screen, {tsx, tsy, sw, sh}, {0, 255, 100, 200}, 2);
                    blitBadge(GetBadge("S", {0, 255, 120, 255}), tsx + 2, tsy + 2);
                    // End ghost: each tile at its own t.y + moveRange
                    float endWY        = t.y + t.moveRange;
                    int   endWYSnapped = ((int)endWY / GRID) * GRID;
                    int   endSY        = (int)std::round((endWYSnapped - mCamY) * mZoom);
                    DrawRectAlpha(screen, {tsx, endSY, sw, sh}, {220, 60, 60, 60});
                    DrawOutline(screen, {tsx, endSY, sw, sh}, {255, 80, 80, 200}, 2);
                    blitBadge(GetBadge("E", {255, 100, 100, 255}), tsx + 2, endSY + 2);
                    lineEndY = endSY;
                    // Phase tick
                    int phasePy = tsy + (int)((lineEndY - tsy) * t.movePhase);
                    DrawRect(screen, {acx - 12, phasePy - 1, 24, 2}, {255, 255, 0, 200});
                    std::string pctStr = std::to_string((int)(t.movePhase * 100)) + "%";
                    blitBadge(GetBadge(pctStr, {255, 255, 180, 255}), acx + 8, phasePy - 6);
                }
            }

            // "M" badge
            blitBadge(GetBadge("M", {0, 255, 255, 255}), tsx + 2, tsy + 2);
            if (t.moveGroupId != 0)
                blitBadge(GetBadge(std::to_string(t.moveGroupId), {200, 255, 255, 255}),
                          tsx + 12,
                          tsy + 2);
        }

        // Mouse-cursor preview: show what the travel path will look like
        // for the CURRENT preset before any tiles are placed.
        {
            float fmx, fmy;
            SDL_GetMouseState(&fmx, &fmy);
            int mx = (int)fmx, my = (int)fmy;
            if (my >= TOOLBAR_H && mx < cw) {
                int       range      = (int)mMovPlatRange;
                SDL_Color previewCol = {0, 255, 200, 160};
                if (mMovPlatHoriz) {
                    DrawRect(screen, {mx - range, my - 1, range * 2, 2}, previewCol);
                    DrawRect(screen, {mx - range - 4, my - 4, 4, 8}, previewCol);
                    DrawRect(screen, {mx + range, my - 4, 4, 8}, previewCol);
                } else {
                    DrawRect(screen, {mx - 1, my - range, 2, range * 2}, previewCol);
                    DrawRect(screen, {mx - 4, my - range - 4, 8, 4}, previewCol);
                    DrawRect(screen, {mx - 4, my + range, 8, 4}, previewCol);
                }
            }
        }

        // Param overlay at bottom of canvas
        std::string paramStr = "MovePlat  " + std::string(mMovPlatHoriz ? "Horiz" : "Vert") +
                               "  range=" + std::to_string((int)mMovPlatRange) +
                               "  spd=" + std::to_string((int)mMovPlatSpeed) +
                               (mMovPlatLoop ? "  LOOP" : "") +
                               (mMovPlatTrigger ? "  TOUCH" : "") +
                               "  grp=" + std::to_string(mMovPlatCurGroupId) +
                               "  tiles=" + std::to_string(mMovPlatIndices.size()) +
                               "  LClick=add  RClick=cycle preset";
        DrawRect(screen, {0, TOOLBAR_H + 20, cw, 18}, {10, 30, 50, 210});
        blitBadge(GetBadge(paramStr, {0, 230, 230, 255}), 6, TOOLBAR_H + 23);

        // Config popup panel
        if (mMovPlatPopupOpen) {
            const int PW      = 280;
            const int PAD     = 8;
            const int ROW_H   = 26;
            const int TITLE_H = 30;
            const int ROWS    = 4; // speed, direction, loop, touch
            const int PH      = TITLE_H + ROWS * (ROW_H + PAD) + PAD;
            // Pin to top-right of canvas area
            int px            = cw - PW - 8;
            int py            = TOOLBAR_H + 42; // just below the param bar
            mMovPlatPopupRect = {px, py, PW, PH};

            // Panel background + border
            DrawRect(screen, {px, py, PW, PH}, {14, 22, 40, 245});
            DrawOutline(screen, {px, py, PW, PH}, {0, 200, 160, 255}, 2);
            DrawRect(screen, {px + 1, py + 1, PW - 2, 3}, {0, 200, 160, 255});

            // Title
            {
                std::string title =
                    "Platform Config  (grp " + std::to_string(mMovPlatCurGroupId) + ")";
                auto [tx, ty] = Text::CenterInRect(title, 12, {px, py + 4, PW, TITLE_H - 4});
                Text tt(title, SDL_Color{0, 230, 200, 255}, tx, ty, 12);
                tt.RenderToSurface(screen);
            }

            int ry = py + TITLE_H;

            // Row 0: Speed
            {
                DrawRect(screen, {px + PAD, ry, PW - PAD * 2, ROW_H}, {20, 32, 55, 200});
                blitBadge(GetBadge("Speed (px/s)", {160, 200, 220, 255}),
                          px + PAD + 4,
                          ry + (ROW_H - 10) / 2);
                // Text input field
                int       fw      = PW - PAD * 2 - 90 - 44;
                SDL_Rect  field   = {px + PAD + 90, ry + (ROW_H - 20) / 2, fw, 20};
                SDL_Color fieldBg = mMovPlatSpeedInput ? SDL_Color{40, 80, 160, 255}
                                                       : SDL_Color{25, 40, 70, 255};
                SDL_Color fieldBd = mMovPlatSpeedInput ? SDL_Color{100, 180, 255, 255}
                                                       : SDL_Color{60, 100, 140, 255};
                DrawRect(screen, field, fieldBg);
                DrawOutline(screen, field, fieldBd, 1);
                std::string disp = mMovPlatSpeedInput ? mMovPlatSpeedStr + "|"
                                                      : std::to_string((int)mMovPlatSpeed);
                blitBadge(GetBadge(disp, {220, 240, 255, 255}),
                          field.x + 4,
                          field.y + (20 - 10) / 2);
                // Close button
                SDL_Rect closeBtn = {px + PW - PAD - 36, ry + (ROW_H - 20) / 2, 36, 20};
                DrawRect(screen, closeBtn, {60, 30, 30, 220});
                DrawOutline(screen, closeBtn, {180, 80, 80, 255});
                blitBadge(
                    GetBadge("close", {220, 140, 140, 255}), closeBtn.x + 4, closeBtn.y + 4);
            }
            ry += ROW_H + PAD;

            // Row 1: Direction H/V toggle
            {
                DrawRect(screen, {px + PAD, ry, PW - PAD * 2, ROW_H}, {20, 32, 55, 200});
                blitBadge(GetBadge("Direction", {160, 200, 220, 255}),
                          px + PAD + 4,
                          ry + (ROW_H - 10) / 2);
                SDL_Rect btnH = {px + PAD + 90, ry + 2, 48, ROW_H - 4};
                SDL_Rect btnV = {px + PAD + 90 + 54, ry + 2, 48, ROW_H - 4};
                DrawRect(screen,
                         btnH,
                         mMovPlatHoriz ? SDL_Color{0, 160, 255, 255}
                                       : SDL_Color{30, 40, 60, 220});
                DrawRect(screen,
                         btnV,
                         !mMovPlatHoriz ? SDL_Color{0, 160, 255, 255}
                                        : SDL_Color{30, 40, 60, 220});
                DrawOutline(screen, btnH, {0, 120, 200, 255});
                DrawOutline(screen, btnV, {0, 120, 200, 255});
                {
                    auto [hx, hy] = Text::CenterInRect("Horiz", 10, btnH);
                    blitBadge(GetBadge("Horiz",
                                       mMovPlatHoriz ? SDL_Color{255, 255, 255, 255}
                                                     : SDL_Color{120, 160, 200, 255}),
                              hx,
                              hy);
                    auto [vx, vy] = Text::CenterInRect("Vert", 10, btnV);
                    blitBadge(GetBadge("Vert",
                                       !mMovPlatHoriz ? SDL_Color{255, 255, 255, 255}
                                                      : SDL_Color{120, 160, 200, 255}),
                              vx,
                              vy);
                }
            }
            ry += ROW_H + PAD;

            // Row 2: Loop (ping-pong) checkbox
            {
                DrawRect(screen, {px + PAD, ry, PW - PAD * 2, ROW_H}, {20, 32, 55, 200});
                blitBadge(GetBadge("Ping-pong loop", {160, 200, 220, 255}),
                          px + PAD + 4,
                          ry + (ROW_H - 10) / 2);
                SDL_Rect cb = {px + PAD + 90, ry + (ROW_H - 16) / 2, 16, 16};
                DrawRect(
                    screen,
                    cb,
                    mMovPlatLoop ? SDL_Color{0, 180, 120, 255} : SDL_Color{30, 40, 60, 255});
                DrawOutline(screen, cb, {0, 160, 100, 255});
                if (mMovPlatLoop)
                    blitBadge(GetBadge("x", {255, 255, 255, 255}), cb.x + 3, cb.y + 2);
            }
            ry += ROW_H + PAD;

            // Row 3: Move on Touch checkbox
            {
                DrawRect(screen, {px + PAD, ry, PW - PAD * 2, ROW_H}, {20, 32, 55, 200});
                blitBadge(GetBadge("Move on touch", {160, 200, 220, 255}),
                          px + PAD + 4,
                          ry + (ROW_H - 10) / 2);
                SDL_Rect cb = {px + PAD + 90, ry + (ROW_H - 16) / 2, 16, 16};
                DrawRect(screen,
                         cb,
                         mMovPlatTrigger ? SDL_Color{255, 160, 0, 255}
                                         : SDL_Color{30, 40, 60, 255});
                DrawOutline(screen, cb, {200, 140, 0, 255});
                if (mMovPlatTrigger)
                    blitBadge(GetBadge("x", {255, 255, 255, 255}), cb.x + 3, cb.y + 2);
                // Hint text
                blitBadge(GetBadge("(waits for player to land)",
                                   mMovPlatTrigger ? SDL_Color{255, 200, 80, 255}
                                                   : SDL_Color{80, 100, 120, 255}),
                          px + PAD + 112,
                          ry + (ROW_H - 10) / 2);
            }
        }
    } else {
        // Outside MovingPlat tool: just draw a subtle M badge on moving tiles
        for (int ti = 0; ti < (int)mLevel.tiles.size(); ti++) {
            const auto& t = mLevel.tiles[ti];
            if (!t.moving)
                continue;
            int tsx = (int)((t.x - mCamX) * mZoom), tsy = (int)((t.y - mCamY) * mZoom);
            int tsw = (int)(t.w * mZoom), tsh = (int)(t.h * mZoom);
            if (tsx + tsw <= 0 || tsx >= cw || tsy + tsh <= TOOLBAR_H ||
                tsy >= window.GetHeight())
                continue;
            DrawRect(screen, {tsx + 2, tsy + 2, 14, 14}, {0, 160, 180, 200});
            blitBadge(GetBadge("M", {255, 255, 255, 255}), tsx + 4, tsy + 2);
        }
    }

    // Coins, enemies, player marker — convert world positions to screen space
    int  iconS      = std::max(4, (int)(ICON_SIZE * mZoom));
    auto coinFrames = coinSheet->GetAnimation("Gold_");
    if (!coinFrames.empty())
        for (const auto& c : mLevel.coins) {
            int cx = (int)((c.x - mCamX) * mZoom), cy = (int)((c.y - mCamY) * mZoom);
            if (cx + iconS <= 0 || cx >= cw || cy + iconS <= TOOLBAR_H ||
                cy >= window.GetHeight())
                continue;
            SDL_Rect s = coinFrames[0], d = {cx, cy, iconS, iconS};
            SDL_BlitSurfaceScaled(
                coinSheet->GetSurface(), &s, screen, &d, SDL_SCALEMODE_LINEAR);
            DrawOutline(screen, d, {255, 215, 0, 255});
        }
    auto slimeFrames = enemySheet->GetAnimation("slimeWalk");
    if (!slimeFrames.empty())
        for (const auto& en : mLevel.enemies) {
            int ex = (int)((en.x - mCamX) * mZoom), ey = (int)((en.y - mCamY) * mZoom);
            if (ex + iconS <= 0 || ex >= cw || ey + iconS <= TOOLBAR_H ||
                ey >= window.GetHeight())
                continue;
            SDL_Rect s = slimeFrames[0], d = {ex, ey, iconS, iconS};
            SDL_BlitSurfaceScaled(
                enemySheet->GetSurface(), &s, screen, &d, SDL_SCALEMODE_LINEAR);
            SDL_Color eOutline =
                en.antiGravity ? SDL_Color{0, 220, 220, 255} : SDL_Color{255, 80, 80, 255};
            DrawOutline(screen, d, eOutline);
            if (en.antiGravity) {
                DrawRect(screen, {ex + iconS / 2 - 4, ey - 10, 8, 8}, {0, 200, 220, 220});
                Text fb("F", SDL_Color{255, 255, 255, 255}, ex + iconS / 2 - 3, ey - 11, 8);
                fb.RenderToSurface(screen);
            }
        }
    // Player marker
    int pmx = (int)((mLevel.player.x - mCamX) * mZoom),
        pmy = (int)((mLevel.player.y - mCamY) * mZoom);
    int pmw = (int)(PLAYER_STAND_WIDTH * mZoom), pmh = (int)(PLAYER_STAND_HEIGHT * mZoom);
    DrawRect(screen, {pmx, pmy, pmw, pmh}, {0, 200, 80, 180});
    DrawOutline(screen, {pmx, pmy, pmw, pmh}, {0, 255, 100, 255}, 2);

    // ── Selection tool feedback ─────────────────────────────────────────────────
    if (!mSelIndices.empty()) {
        // Teal highlight overlay on each selected tile — low alpha fill so tile is visible
        for (int idx : mSelIndices) {
            if (idx < 0 || idx >= (int)mLevel.tiles.size())
                continue;
            const auto& t  = mLevel.tiles[idx];
            int         sx = (int)((t.x - mCamX) * mZoom), sy = (int)((t.y - mCamY) * mZoom);
            int         sw = (int)(t.w * mZoom), sh = (int)(t.h * mZoom);
            DrawRectAlpha(screen, {sx, sy, sw, sh}, {0, 220, 220, 40});
            DrawOutline(screen, {sx, sy, sw, sh}, {0, 255, 255, 220}, 2);
        }
        // Bounding box around entire selection
        int bx0 = INT_MAX, by0 = INT_MAX, bx1 = INT_MIN, by1 = INT_MIN;
        for (int idx : mSelIndices) {
            if (idx < 0 || idx >= (int)mLevel.tiles.size())
                continue;
            const auto& t = mLevel.tiles[idx];
            bx0           = std::min(bx0, (int)t.x);
            by0           = std::min(by0, (int)t.y);
            bx1           = std::max(bx1, (int)t.x + t.w);
            by1           = std::max(by1, (int)t.y + t.h);
        }
        int sbx = (int)((bx0 - mCamX) * mZoom), sby = (int)((by0 - mCamY) * mZoom);
        int sbbw = (int)((bx1 - bx0) * mZoom), sbbh = (int)((by1 - by0) * mZoom);
        DrawOutline(screen, {sbx - 2, sby - 2, sbbw + 4, sbbh + 4}, {0, 255, 255, 180}, 1);
        // Count label
        std::string selLabel = std::to_string(mSelIndices.size()) + " selected";
        DrawRect(
            screen, {sbx, sby - 16, (int)selLabel.size() * 7 + 4, 14}, {0, 60, 70, 200});
        Text selT(selLabel, SDL_Color{0, 255, 255, 255}, sbx + 2, sby - 15, 10);
        selT.RenderToSurface(screen);
    }
    // Rubber-band marquee rect
    if (mSelBoxing) {
        int rx0 = (int)((std::min(mSelBoxX0, mSelBoxX1) - mCamX) * mZoom);
        int ry0 = (int)((std::min(mSelBoxY0, mSelBoxY1) - mCamY) * mZoom);
        int rx1 = (int)((std::max(mSelBoxX0, mSelBoxX1) - mCamX) * mZoom);
        int ry1 = (int)((std::max(mSelBoxY0, mSelBoxY1) - mCamY) * mZoom);
        int rw = rx1 - rx0, rh = ry1 - ry0;
        if (rw > 0 && rh > 0) {
            DrawRectAlpha(screen, {rx0, ry0, rw, rh}, {100, 220, 255, 20});
            DrawOutline(screen, {rx0, ry0, rw, rh}, {100, 220, 255, 180}, 1);
        }
    }

    // ── Resize tool feedback ─────────────────────────────────────────────────
    if (mActiveTool == Tool::Resize) {
        const SDL_Color handleCol = {255, 180, 0, 220};
        const SDL_Color dragCol   = {255, 220, 80, 180};
        const int       HS        = RESIZE_HANDLE;

        if (mHoverTileIdx >= 0 && !mIsResizing) {
            const auto& t  = mLevel.tiles[mHoverTileIdx];
            int         rx = (int)((t.x - mCamX) * mZoom), ry = (int)((t.y - mCamY) * mZoom);
            int         rw = (int)(t.w * mZoom), rh = (int)(t.h * mZoom);
            if (mHoverEdge == ResizeEdge::Right || mHoverEdge == ResizeEdge::Corner)
                DrawRect(screen, {rx + rw - HS, ry + HS, HS, rh - HS * 2}, handleCol);
            if (mHoverEdge == ResizeEdge::Bottom || mHoverEdge == ResizeEdge::Corner)
                DrawRect(screen, {rx + HS, ry + rh - HS, rw - HS * 2, HS}, handleCol);
            if (mHoverEdge == ResizeEdge::Corner)
                DrawRect(screen, {rx + rw - HS, ry + rh - HS, HS, HS}, handleCol);
        }

        if (mIsResizing && mResizeTileIdx >= 0 &&
            mResizeTileIdx < (int)mLevel.tiles.size()) {
            const auto& t  = mLevel.tiles[mResizeTileIdx];
            int         rx = (int)((t.x - mCamX) * mZoom), ry = (int)((t.y - mCamY) * mZoom);
            int         rw = (int)(t.w * mZoom), rh = (int)(t.h * mZoom);
            DrawOutline(screen, {rx, ry, rw, rh}, {255, 220, 80, 255}, 2);
            Text szT(
                std::to_string(t.w / GRID) + "x" + std::to_string(t.h / GRID) + " tiles",
                dragCol,
                rx + 4,
                ry + 4,
                12);
            szT.RenderToSurface(screen);
        }
    }

    // ── Hitbox tool feedback ───────────────────────────────────────────────────
    if (mActiveTool == Tool::Hitbox && mHitboxTileIdx >= 0 &&
        mHitboxTileIdx < (int)mLevel.tiles.size()) {
        const auto& t  = mLevel.tiles[mHitboxTileIdx];
        int         hx = (int)((t.x - mCamX + t.hitboxOffX) * mZoom);
        int         hy = (int)((t.y - mCamY + t.hitboxOffY) * mZoom);
        int         hw = (int)(t.hitboxW * mZoom);
        int         hh = (int)(t.hitboxH * mZoom);

        // Semi-transparent fill inside the hitbox
        DrawRect(screen, {hx, hy, hw, hh}, {80, 160, 255, 40});
        // Bright blue border
        DrawOutline(screen, {hx, hy, hw, hh}, {80, 180, 255, 255}, 2);

        // Draw a faint dashed outline of the visual tile for reference
        DrawOutline(screen,
                    {(int)((t.x - mCamX) * mZoom),
                     (int)((t.y - mCamY) * mZoom),
                     (int)(t.w * mZoom),
                     (int)(t.h * mZoom)},
                    {255, 255, 255, 60},
                    1);

        // 8 drag handles: corners + edge midpoints
        const int       HS     = HBOX_HANDLE;         // half-side of handle square
        const SDL_Color hcNorm = {80, 180, 255, 220}; // normal
        const SDL_Color hcHov  = {255, 220, 80, 255}; // hovered

        auto hdlColor = [&](HitboxHandle h) -> SDL_Color {
            return (mHoverHitboxHdl == h) ? hcHov : hcNorm;
        };
        auto drawHandle = [&](int cx, int cy, HitboxHandle h) {
            DrawRect(screen, {cx - HS / 2, cy - HS / 2, HS, HS}, hdlColor(h));
            DrawOutline(screen, {cx - HS / 2, cy - HS / 2, HS, HS}, {20, 20, 40, 255});
        };

        int cx = hx + hw / 2;
        int cy = hy + hh / 2;
        drawHandle(hx, hy, HitboxHandle::TopLeft);
        drawHandle(cx, hy, HitboxHandle::Top);
        drawHandle(hx + hw, hy, HitboxHandle::TopRight);
        drawHandle(hx, cy, HitboxHandle::Left);
        drawHandle(hx + hw, cy, HitboxHandle::Right);
        drawHandle(hx, hy + hh, HitboxHandle::BotLeft);
        drawHandle(cx, hy + hh, HitboxHandle::Bottom);
        drawHandle(hx + hw, hy + hh, HitboxHandle::BotRight);

        // Info label in top-left corner of the hitbox
        std::string info = "HB: " + std::to_string(hw) + "x" + std::to_string(hh) + " @(" +
                           std::to_string(t.hitboxOffX) + "," +
                           std::to_string(t.hitboxOffY) + ")";
        DrawRect(screen, {hx, hy - 16, (int)info.size() * 7, 14}, {10, 20, 50, 200});
        Text infoT(info, SDL_Color{180, 220, 255, 255}, hx + 2, hy - 15, 10);
        infoT.RenderToSurface(screen);
    }

    // Tile ghost — SnapToGrid returns world coords; convert to screen for drawing
    if (mActiveTool == Tool::Tile && !mPaletteItems.empty() &&
        !mPaletteItems[mSelectedTile].isFolder) {
        float fmx, fmy;
        SDL_GetMouseState(&fmx, &fmy);
        int mx = (int)fmx, my = (int)fmy;
        if (my >= TOOLBAR_H && mx < cw) {
            auto [wx, wy] = SnapToGrid(mx, my); // world space
            // Apply zoom to get screen-space position (was missing mZoom)
            int      gsx      = (int)((wx - mCamX) * mZoom);
            int      gsy      = (int)((wy - mCamY) * mZoom);
            int      gsw      = (int)(mTileW * mZoom);
            int      gsh      = (int)(mTileH * mZoom);
            SDL_Rect ghostDst = {gsx, gsy, gsw, gsh};
            // Draw the actual selected tile image as a semi-transparent ghost
            const auto&  selItem   = mPaletteItems[mSelectedTile];
            SDL_Surface* ghostSurf = mSurfaceCache.FindTileSurface(selItem.path);
            if (!ghostSurf)
                ghostSurf = selItem.full;
            if (ghostSurf) {
                // Apply ghost rotation using the same rotation cache as placed tiles
                SDL_Surface* drawSurf =
                    (mGhostRotation != 0)
                        ? GetRotated(selItem.path, ghostSurf, mGhostRotation)
                        : ghostSurf;
                if (!drawSurf)
                    drawSurf = ghostSurf; // fallback if rotation failed
                SDL_SetSurfaceAlphaMod(drawSurf, 140);
                SDL_BlitSurfaceScaled(
                    drawSurf, nullptr, screen, &ghostDst, SDL_SCALEMODE_LINEAR);
                SDL_SetSurfaceAlphaMod(drawSurf, 255);
            } else {
                DrawRectAlpha(screen, ghostDst, {100, 180, 255, 60});
            }
            // Show rotation badge on ghost so user knows current angle
            if (mGhostRotation != 0) {
                std::string  rBadge = std::to_string(mGhostRotation) + "\xc2\xb0";
                SDL_Surface* rb     = GetBadge(rBadge, {255, 220, 80, 255});
                if (rb) {
                    int      bx = gsx + gsw - rb->w - 3;
                    int      by = gsy + 3;
                    SDL_Rect bd = {bx, by, rb->w, rb->h};
                    SDL_BlitSurface(rb, nullptr, screen, &bd);
                }
            }
            DrawOutline(screen, ghostDst, {100, 180, 255, 200});
        }
    }

    // ── Toolbar — spans the full window width including the palette area ─────
    DrawRect(screen, {0, 0, window.GetWidth(), TOOLBAR_H}, {22, 22, 32, 255});
    DrawRect(screen, {0, TOOLBAR_H - 1, window.GetWidth(), 1}, {60, 60, 80, 255});

    auto drawBtn =
        [&](SDL_Rect r, SDL_Color accentColor, Text* lbl, Text* hint, bool active) {
            SDL_Color bg =
                active ? SDL_Color{50, 100, 210, 255} : SDL_Color{35, 35, 48, 255};
            SDL_Color border =
                active ? SDL_Color{100, 160, 255, 255} : SDL_Color{55, 55, 72, 255};
            SDL_Color topBar = active ? SDL_Color{130, 190, 255, 255} : accentColor;
            DrawRect(screen, r, bg);
            DrawOutline(screen, r, border);
            DrawRect(screen, {r.x + 1, r.y + 1, r.w - 2, 3}, topBar);
            if (lbl)
                lbl->RenderToSurface(screen);
            if (hint)
                hint->RenderToSurface(screen);
        };

    constexpr SDL_Color ACCENT_PLACE    = {80, 160, 255, 255};
    constexpr SDL_Color ACCENT_MODIFIER = {80, 220, 140, 255};
    constexpr SDL_Color ACCENT_ACTION   = {200, 160, 60, 255};

    // Collapse strip: a proper 14px tall clickable bar below each group's buttons.
    // BTN_Y=8, BTN_H=56 -> buttons end at y=64. Strip at y=65, h=14, within TOOLBAR_H=86.
    constexpr int STRIP_Y = BTN_Y + BTN_H + 2; // y=66, 2px breathing room
    constexpr int STRIP_H = 14;                // tall enough to see and click
    auto          drawCollapseBtn =
        [&](SDL_Rect& pill, SDL_Color accent, bool collapsed, int gx0, int gx1) {
            pill = {gx0, STRIP_Y, gx1 - gx0, STRIP_H};
            SDL_Color bg =
                collapsed ? SDL_Color{70, 70, 120, 255} : SDL_Color{38, 38, 58, 255};
            DrawRect(screen, pill, bg);
            DrawOutline(screen,
                        pill,
                        {collapsed ? (Uint8)120 : (Uint8)55,
                         55,
                         collapsed ? (Uint8)180 : (Uint8)80,
                         255});
            // Accent stripe on top edge
            DrawRect(screen, {pill.x, pill.y, pill.w, 2}, accent);
            // Centred +/- label
            const char* sym = collapsed ? "+" : "-";
            SDL_Color   symCol =
                collapsed ? SDL_Color{200, 220, 255, 255} : SDL_Color{100, 120, 160, 255};
            blitBadge(GetBadge(sym, symCol), pill.x + pill.w / 2 - 3, pill.y + 2);
        };

    // Group 1 — Place
    {
        int gx0 = BTN_GAP;
        int gx1;
        if (!mGrp1Collapsed) {
            drawBtn(btnCoin,
                    ACCENT_PLACE,
                    lblCoin.get(),
                    hintCoin.get(),
                    mActiveTool == Tool::Coin);
            drawBtn(btnEnemy,
                    ACCENT_PLACE,
                    lblEnemy.get(),
                    hintEnemy.get(),
                    mActiveTool == Tool::Enemy);
            drawBtn(btnTile,
                    ACCENT_PLACE,
                    lblTile.get(),
                    hintTile.get(),
                    mActiveTool == Tool::Tile);
            drawBtn(btnErase,
                    ACCENT_PLACE,
                    lblErase.get(),
                    hintErase.get(),
                    mActiveTool == Tool::Erase);
            drawBtn(btnPlayerStart,
                    ACCENT_PLACE,
                    lblPlayer.get(),
                    hintPlayer.get(),
                    mActiveTool == Tool::PlayerStart);
            drawBtn(btnSelect,
                    ACCENT_PLACE,
                    lblSelect.get(),
                    hintSelect.get(),
                    mActiveTool == Tool::Select);
            drawBtn(btnMoveCam,
                    ACCENT_PLACE,
                    lblMoveCam.get(),
                    hintMoveCam.get(),
                    mActiveTool == Tool::MoveCam);
            gx1 = btnMoveCam.x + btnMoveCam.w;
        } else {
            SDL_Rect bar = {gx0, BTN_Y, 32, BTN_H};
            DrawRect(screen, bar, {30, 30, 48, 255});
            DrawOutline(screen, bar, {55, 55, 75, 255});
            DrawRect(screen, {bar.x + 1, bar.y + 1, 3, bar.h - 2}, ACCENT_PLACE);
            blitBadge(GetBadge("P", {80, 160, 255, 200}), bar.x + 10, bar.y + BTN_H / 2 - 5);
            gx1 = gx0 + 32;
        }
        drawCollapseBtn(mGrp1Pill, ACCENT_PLACE, mGrp1Collapsed, gx0, gx1);
        DrawRect(screen,
                 {gx1 + BTN_GAP + GRP_GAP / 2, BTN_Y + 4, 1, BTN_H - 8},
                 {70, 70, 90, 255});
    }

    // Group 2 — Modifiers
    {
        int gx0 = mGrp2Pill.x; // always correct from RebuildToolbarLayout
        int gx1;
        if (!mGrp2Collapsed) {
            drawBtn(btnProp,
                    ACCENT_MODIFIER,
                    lblProp.get(),
                    hintProp.get(),
                    mActiveTool == Tool::Prop);
            drawBtn(btnLadder,
                    ACCENT_MODIFIER,
                    lblLadder.get(),
                    hintLadder.get(),
                    mActiveTool == Tool::Ladder);
            drawBtn(btnAction,
                    ACCENT_MODIFIER,
                    lblAction.get(),
                    hintAction.get(),
                    mActiveTool == Tool::Action);
            drawBtn(btnSlope,
                    ACCENT_MODIFIER,
                    lblSlope.get(),
                    hintSlope.get(),
                    mActiveTool == Tool::Slope);
            drawBtn(btnResize,
                    ACCENT_MODIFIER,
                    lblResize.get(),
                    hintResize.get(),
                    mActiveTool == Tool::Resize);
            drawBtn(btnHitbox,
                    ACCENT_MODIFIER,
                    lblHitbox.get(),
                    nullptr,
                    mActiveTool == Tool::Hitbox);
            drawBtn(btnHazard,
                    {220, 60, 60, 255},
                    lblHazard.get(),
                    nullptr,
                    mActiveTool == Tool::Hazard);
            drawBtn(btnAntiGrav,
                    {0, 180, 200, 255},
                    lblAntiGrav.get(),
                    nullptr,
                    mActiveTool == Tool::AntiGrav);
            drawBtn(btnMovingPlat,
                    {0, 200, 160, 255},
                    lblMovingPlat.get(),
                    nullptr,
                    mActiveTool == Tool::MovingPlat);
            drawBtn(btnPowerUp,
                    {200, 80, 255, 255},
                    lblPowerUp.get(),
                    nullptr,
                    mActiveTool == Tool::PowerUp);
            gx1 = btnPowerUp.x + btnPowerUp.w;
        } else {
            SDL_Rect bar = {gx0, BTN_Y, 32, BTN_H};
            DrawRect(screen, bar, {30, 30, 48, 255});
            DrawOutline(screen, bar, {55, 55, 75, 255});
            DrawRect(screen, {bar.x + 1, bar.y + 1, 3, bar.h - 2}, ACCENT_MODIFIER);
            blitBadge(GetBadge("M", {80, 220, 140, 200}), bar.x + 10, bar.y + BTN_H / 2 - 5);
            gx1 = gx0 + 32;
        }
        drawCollapseBtn(mGrp2Pill, ACCENT_MODIFIER, mGrp2Collapsed, gx0, gx1);
        DrawRect(screen,
                 {gx1 + BTN_GAP + GRP_GAP / 2, BTN_Y + 4, 1, BTN_H - 8},
                 {70, 70, 90, 255});
    }

    // Group 3 — Actions
    {
        int gx0 = mGrp3Pill.x; // always correct from RebuildToolbarLayout
        int gx1;
        if (!mGrp3Collapsed) {
            SDL_Color gravAccent = (mLevel.gravityMode == GravityMode::WallRun)
                                       ? SDL_Color{100, 140, 255, 255}
                                   : (mLevel.gravityMode == GravityMode::OpenWorld)
                                       ? SDL_Color{80, 220, 120, 255}
                                       : ACCENT_ACTION;
            drawBtn(btnGravity, gravAccent, lblGravity.get(), nullptr, false);
            drawBtn(btnSave, ACCENT_ACTION, lblSave.get(), nullptr, false);
            drawBtn(btnLoad, ACCENT_ACTION, lblLoad.get(), nullptr, false);
            drawBtn(btnClear, {220, 80, 80, 255}, lblClear.get(), nullptr, false);
            drawBtn(btnPlay, {80, 220, 100, 255}, lblPlay.get(), nullptr, false);
            DrawRect(
                screen,
                {btnPlay.x + btnPlay.w + BTN_GAP + GRP_GAP / 2, BTN_Y + 4, 1, BTN_H - 8},
                {70, 70, 90, 255});
            drawBtn(btnBack, {120, 100, 160, 255}, lblBack.get(), nullptr, false);
            gx1 = btnBack.x + btnBack.w;
        } else {
            SDL_Rect bar = {gx0, BTN_Y, 32, BTN_H};
            DrawRect(screen, bar, {30, 30, 48, 255});
            DrawOutline(screen, bar, {55, 55, 75, 255});
            DrawRect(screen, {bar.x + 1, bar.y + 1, 3, bar.h - 2}, ACCENT_ACTION);
            blitBadge(GetBadge("A", {200, 160, 60, 200}), bar.x + 10, bar.y + BTN_H / 2 - 5);
            gx1 = gx0 + 32;
        }
        drawCollapseBtn(mGrp3Pill, ACCENT_ACTION, mGrp3Collapsed, gx0, gx1);
    }

    // Status bar
    DrawRect(screen, {0, TOOLBAR_H, cw, 20}, {16, 16, 24, 230});
    if (lblTool) {
        int tx = window.GetWidth() - PALETTE_W - 8;
        if (!lblToolPrefix)
            const_cast<std::unique_ptr<Text>&>(lblToolPrefix) = std::make_unique<Text>(
                "Tool:", SDL_Color{120, 120, 150, 255}, tx - 80, TOOLBAR_H + 3, 12);
        lblToolPrefix->RenderToSurface(screen);
        lblTool->SetPosition(tx - 40, TOOLBAR_H + 3);
        lblTool->RenderToSurface(screen);
    }
    if (lblStatus)
        lblStatus->RenderToSurface(screen);

    // ── Palette panel ─────────────────────────────────────────────────────────
    if (mPaletteCollapsed) {
        // Draw just the narrow toggle tab so user can re-open (starts below toolbar)
        DrawRect(screen,
                 {cw, TOOLBAR_H, PALETTE_TAB_W, window.GetHeight() - TOOLBAR_H},
                 {20, 20, 30, 255});
        DrawOutline(screen,
                    {cw, TOOLBAR_H, PALETTE_TAB_W, window.GetHeight() - TOOLBAR_H},
                    {60, 60, 80, 255});
        // Toggle button
        SDL_Rect toggleBtn = {cw, TOOLBAR_H, PALETTE_TAB_W, 28};
        DrawRect(screen, toggleBtn, {40, 80, 180, 255});
        DrawOutline(screen, toggleBtn, {80, 140, 255, 255});
        blitBadge(GetBadge(">", {200, 220, 255, 255}), cw + 4, TOOLBAR_H + 7);
        const char* lbl = (mActiveTab == PaletteTab::Tiles) ? "TILES" : "BG";
        for (int i = 0; lbl[i]; i++) {
            char buf[2] = {lbl[i], '\0'};
            blitBadge(GetBadge(buf, {120, 140, 200, 255}), cw + 4, TOOLBAR_H + 40 + i * 13);
        }
    } else {
        // Palette panel starts below the toolbar so the toolbar renders over it
        DrawRect(screen,
                 {cw, TOOLBAR_H, PALETTE_W, window.GetHeight() - TOOLBAR_H},
                 {20, 20, 30, 255});
        DrawOutline(screen,
                    {cw, TOOLBAR_H, PALETTE_W, window.GetHeight() - TOOLBAR_H},
                    {60, 60, 80, 255});

        // Collapse toggle button
        {
            SDL_Rect toggleBtn = {cw, TOOLBAR_H, PALETTE_TAB_W, TAB_H};
            DrawRect(screen, toggleBtn, {30, 50, 140, 255});
            DrawOutline(screen, toggleBtn, {70, 120, 220, 255});
            blitBadge(GetBadge("<", {180, 210, 255, 255}), cw + 4, TOOLBAR_H + 7);
        }
        // Tab bar
        {
            int      panelX = cw + PALETTE_TAB_W, tabW = (PALETTE_W - PALETTE_TAB_W) / 2;
            bool     ta  = (mActiveTab == PaletteTab::Tiles);
            SDL_Rect r0  = {panelX, TOOLBAR_H, tabW, TAB_H},
                     r1  = {panelX + tabW, TOOLBAR_H, tabW, TAB_H};
            SDL_Color ac = {50, 100, 200, 255}, ic = {30, 30, 45, 255},
                      bc = {80, 120, 200, 255};
            DrawRect(screen, r0, ta ? ac : ic);
            DrawRect(screen, r1, !ta ? ac : ic);
            DrawOutline(screen, r0, bc);
            DrawOutline(screen, r1, bc);
            auto [tx, ty] = Text::CenterInRect("Tiles", 11, r0);
            blitBadge(GetBadge("Tiles", {(Uint8)(ta ? 255 : 160), 255, 255, 255}), tx, ty);
            auto [bx, by] = Text::CenterInRect("Backgrounds", 11, r1);
            blitBadge(
                GetBadge("Backgrounds", {(Uint8)(!ta ? 255 : 160), 255, 255, 255}), bx, by);
        }

        int palY = TOOLBAR_H + TAB_H;

        if (mActiveTab == PaletteTab::Tiles) {
            // ── Breadcrumb / header ───────────────────────────────────────────────
            DrawRect(screen, {cw, palY, PALETTE_W, 44}, {30, 30, 45, 255});

            // Palette header — rebuild only when dir or tile size changes
            {
                std::string loc = mTileCurrentDir;
                if (loc.rfind(TILE_ROOT, 0) == 0)
                    loc = loc.substr(std::string(TILE_ROOT).size());
                if (loc.empty() || loc == "/")
                    loc = "/";
                std::string hdrStr = "Tiles" + loc;
                if (hdrStr != mLastPalHeaderPath || mTileW != mLastTileSizeW) {
                    mLastPalHeaderPath = hdrStr;
                    mLastTileSizeW     = mTileW;
                    const_cast<std::unique_ptr<Text>&>(lblPalHeader) =
                        std::make_unique<Text>(
                            hdrStr, SDL_Color{200, 200, 220, 255}, cw + 4, palY + 4, 10);
                    const_cast<std::unique_ptr<Text>&>(lblPalHint1) = std::make_unique<Text>(
                        "Size: " + std::to_string(mTileW) + "  Esc=up  Click=enter",
                        SDL_Color{100, 120, 140, 255},
                        cw + 4,
                        palY + 18,
                        9);
                    if (!lblPalHint2)
                        const_cast<std::unique_ptr<Text>&>(lblPalHint2) =
                            std::make_unique<Text>("Click folder to open",
                                                   SDL_Color{100, 120, 140, 255},
                                                   cw + 4,
                                                   palY + 30,
                                                   9);
                }
                if (lblPalHeader)
                    lblPalHeader->RenderToSurface(screen);
                if (lblPalHint1)
                    lblPalHint1->RenderToSurface(screen);
                if (lblPalHint2)
                    lblPalHint2->RenderToSurface(screen);
            }
            palY += 44;

            // Grid of items
            constexpr int PAD = 4, LBL_H = 14;
            const int     cellW   = (PALETTE_W - PAD * (PAL_COLS + 1)) / PAL_COLS;
            const int     cellH   = cellW + LBL_H;
            const int     itemH   = cellH + PAD;
            const int     visRows = (window.GetHeight() - palY) / itemH;
            const int     startI  = mPaletteScroll * PAL_COLS;
            const int     endI =
                std::min(startI + (visRows + 1) * PAL_COLS, (int)mPaletteItems.size());

            for (int i = startI; i < endI; i++) {
                int         col  = (i - startI) % PAL_COLS;
                int         row  = (i - startI) / PAL_COLS;
                int         ix   = cw + PAD + col * (cellW + PAD);
                int         iy   = palY + PAD + row * itemH;
                const auto& item = mPaletteItems[i];
                bool        sel =
                    (i == mSelectedTile && !item.isFolder && mActiveTool == Tool::Tile);

                SDL_Rect cell = {ix, iy, cellW, cellH};

                if (item.isFolder) {
                    // ── Folder cell: warm amber tint ──────────────────────────────
                    SDL_Color folderBg  = (item.label.rfind("◀", 0) == 0)
                                              ? SDL_Color{35, 50, 35, 220}
                                             // back arrow: greenish
                                              : SDL_Color{55, 45, 20, 220}; // folder: amber
                    SDL_Color folderBdr = (item.label.rfind("◀", 0) == 0)
                                              ? SDL_Color{80, 200, 80, 255}
                                              : SDL_Color{200, 160, 60, 255};
                    DrawRect(screen, cell, folderBg);
                    DrawOutline(screen, cell, folderBdr);

                    // Optional preview thumbnail (first PNG inside folder)
                    if (item.thumb) {
                        SDL_Rect imgDst = {ix + 1, iy + 1, cellW - 2, cellW - 2};
                        SDL_SetSurfaceColorMod(
                            item.thumb, 120, 100, 60); // darken for folder feel
                        SDL_BlitSurfaceScaled(
                            item.thumb, nullptr, screen, &imgDst, SDL_SCALEMODE_LINEAR);
                        SDL_SetSurfaceColorMod(item.thumb, 255, 255, 255);
                    } else {
                        // Folder icon: simple rectangle grid visual
                        DrawRect(screen,
                                 {ix + cellW / 2 - 14, iy + 8, 28, 20},
                                 {200, 160, 60, 180});
                        DrawRect(screen,
                                 {ix + cellW / 2 - 14, iy + 4, 12, 8},
                                 {200, 160, 60, 180});
                    }

                    // Label
                    std::string lbl = item.label;
                    if ((int)lbl.size() > 9)
                        lbl = lbl.substr(0, 8) + "~";
                    blitBadge(GetBadge(lbl, {220, 180, 80, 255}), ix + 2, iy + cellW + 2);

                    // ── Delete button (skip "◀ Back" entry) ─────────────────────
                    if (item.label.rfind("◄", 0) != 0) {
                        constexpr int DEL_SZ = 14;
                        SDL_Rect      db = {ix + cellW - DEL_SZ - 1, iy + 1, DEL_SZ, DEL_SZ};
                        const_cast<PaletteItem&>(item).delBtn = db;
                        DrawRect(screen, db, {140, 30, 30, 220});
                        DrawOutline(screen, db, {200, 60, 60, 255});
                        {
                            SDL_Surface* xs = GetBadge("x", {255, 180, 180, 255});
                            if (xs)
                                blitBadge(xs,
                                          db.x + (db.w - xs->w) / 2,
                                          db.y + (db.h - xs->h) / 2);
                        }
                    }

                } else {
                    // ── File cell ─────────────────────────────────────────────────
                    DrawRect(
                        screen,
                        cell,
                        sel ? SDL_Color{50, 100, 200, 220} : SDL_Color{35, 35, 55, 220});
                    DrawOutline(
                        screen,
                        cell,
                        sel ? SDL_Color{100, 180, 255, 255} : SDL_Color{55, 55, 80, 255});

                    SDL_Surface* ts = item.thumb ? item.thumb : item.full;
                    if (ts) {
                        SDL_Rect imgDst = {ix + 1, iy + 1, cellW - 2, cellW - 2};
                        SDL_BlitSurfaceScaled(
                            ts, nullptr, screen, &imgDst, SDL_SCALEMODE_LINEAR);
                    } else
                        DrawRect(screen,
                                 {ix + 1, iy + 1, cellW - 2, cellW - 2},
                                 {60, 40, 80, 255});

                    std::string lbl = item.label;
                    if ((int)lbl.size() > 9)
                        lbl = lbl.substr(0, 8) + "~";
                    SDL_Color lc = {(Uint8)(sel ? 255 : 170),
                                    (Uint8)(sel ? 255 : 170),
                                    (Uint8)(sel ? 255 : 190),
                                    255};
                    blitBadge(GetBadge(lbl, lc), ix + 2, iy + cellW + 2);

                    // ── Delete button (top-right corner) ──────────────────────────
                    constexpr int DEL_SZ = 14;
                    SDL_Rect      db     = {ix + cellW - DEL_SZ - 1, iy + 1, DEL_SZ, DEL_SZ};
                    const_cast<PaletteItem&>(item).delBtn = db;
                    DrawRect(screen, db, {140, 30, 30, 220});
                    DrawOutline(screen, db, {200, 60, 60, 255});
                    {
                        SDL_Surface* xs = GetBadge("x", {255, 180, 180, 255});
                        if (xs)
                            blitBadge(
                                xs, db.x + (db.w - xs->w) / 2, db.y + (db.h - xs->h) / 2);
                    }
                }
            }

            // Scroll indicator
            int totalRows = ((int)mPaletteItems.size() + PAL_COLS - 1) / PAL_COLS;
            if (totalRows > visRows) {
                float pct = (float)mPaletteScroll / std::max(1, totalRows - visRows);
                int   sh  = std::max(
                    20, (int)((window.GetHeight() - palY) * visRows / (float)totalRows));
                int sy2 = palY + (int)((window.GetHeight() - palY - sh) * pct);
                DrawRect(screen, {cw + PALETTE_W - 4, sy2, 3, sh}, {100, 150, 255, 180});
            }

        } else {
            // ── Backgrounds palette ───────────────────────────────────────────────
            DrawRect(screen, {cw, palY, PALETTE_W, 24}, {30, 30, 45, 255});
            if (!lblBgHeader)
                const_cast<std::unique_ptr<Text>&>(lblBgHeader) =
                    std::make_unique<Text>("Backgrounds  (I=import)",
                                           SDL_Color{200, 200, 220, 255},
                                           cw + 4,
                                           palY + 6,
                                           10);
            if (lblBgHeader)
                lblBgHeader->RenderToSurface(screen);
            // Fit-mode cycle button — right-aligned in the header strip
            {
                const char* fitLabel = mLevel.bgFitMode.c_str();
                int         bw = 54, bh = 16;
                int         bx  = cw + PALETTE_W - bw - 4;
                int         by2 = palY + (24 - bh) / 2;
                DrawRect(screen, {bx, by2, bw, bh}, {50, 60, 100, 230});
                DrawOutline(screen, {bx, by2, bw, bh}, {100, 140, 220, 255});
                blitBadge(GetBadge(fitLabel, {180, 210, 255, 255}), bx + 3, by2 + 3);
            }
            palY += 24;

            constexpr int PAD = 4, LBL_H = 16;
            const int     thumbW = PALETTE_W - PAD * 2;
            const int     thumbH = thumbW / 2;
            const int     itemH  = thumbH + LBL_H + PAD;
            int           vis    = (window.GetHeight() - palY) / itemH;
            int           startI = mBgPaletteScroll;
            int           endI   = std::min(startI + vis + 1, (int)mBgItems.size());

            for (int i = startI; i < endI; i++) {
                int      iy   = palY + PAD + (i - startI) * itemH;
                bool     sel  = (i == mSelectedBg);
                SDL_Rect cell = {cw + PAD, iy, thumbW, thumbH + LBL_H};
                DrawRect(screen,
                         cell,
                         sel ? SDL_Color{50, 100, 200, 220} : SDL_Color{35, 35, 55, 220});
                DrawOutline(screen,
                            cell,
                            sel ? SDL_Color{100, 220, 255, 255} : SDL_Color{55, 55, 80, 255},
                            sel ? 2 : 1);
                SDL_Rect imgDst = {cw + PAD + 1, iy + 1, thumbW - 2, thumbH - 2};
                if (mBgItems[i].thumb)
                    SDL_BlitSurfaceScaled(
                        mBgItems[i].thumb, nullptr, screen, &imgDst, SDL_SCALEMODE_LINEAR);
                else
                    DrawRect(screen, imgDst, {40, 40, 70, 255});
                std::string lbl = mBgItems[i].label;
                if ((int)lbl.size() > 14)
                    lbl = lbl.substr(0, 13) + "~";
                SDL_Color lc = {(Uint8)(sel ? 255 : 170),
                                (Uint8)(sel ? 255 : 170),
                                (Uint8)(sel ? 255 : 190),
                                255};
                blitBadge(GetBadge(lbl, lc), cw + PAD + 2, iy + thumbH + 2);
                // Delete button
                constexpr int DEL_SZ = 14;
                SDL_Rect      db = {cw + PAD + thumbW - DEL_SZ - 1, iy + 1, DEL_SZ, DEL_SZ};
                mBgItems[i].delBtn = db;
                DrawRect(screen, db, {140, 30, 30, 220});
                DrawOutline(screen, db, {200, 60, 60, 255});
                {
                    SDL_Surface* xs = GetBadge("x", {255, 180, 180, 255});
                    if (xs)
                        blitBadge(xs, db.x + (db.w - xs->w) / 2, db.y + (db.h - xs->h) / 2);
                }
            }
            if ((int)mBgItems.size() > vis) {
                float pct =
                    (float)mBgPaletteScroll / std::max(1, (int)mBgItems.size() - vis);
                int sh = std::max(
                    20, (int)((window.GetHeight() - palY) * vis / (float)mBgItems.size()));
                int sy2 = palY + (int)((window.GetHeight() - palY - sh) * pct);
                DrawRect(screen, {cw + PALETTE_W - 4, sy2, 3, sh}, {100, 150, 255, 180});
            }
        }

    } // end palette expanded

    // ── Bottom hint bar ────────────────────────────────────────────────────────
    DrawRect(screen, {0, window.GetHeight() - 22, cw, 22}, {16, 16, 24, 220});
    {
        int tc = (int)mLevel.tiles.size(), cc = (int)mLevel.coins.size(),
            ec = (int)mLevel.enemies.size();
        if (tc != mLastTileCount || cc != mLastCoinCount || ec != mLastEnemyCount) {
            mLastTileCount  = tc;
            mLastCoinCount  = cc;
            mLastEnemyCount = ec;
            const_cast<std::unique_ptr<Text>&>(lblStatusBar) =
                std::make_unique<Text>(std::to_string(cc) + " coins  " + std::to_string(ec) +
                                           " enemies  " + std::to_string(tc) + " tiles",
                                       SDL_Color{120, 120, 150, 255},
                                       8,
                                       window.GetHeight() - 18,
                                       11);
        }
        if (lblStatusBar)
            lblStatusBar->RenderToSurface(screen);
    }
    {
        int cx = (int)mCamX, cy = (int)mCamY;
        if (cx != mLastCamX || cy != mLastCamY) {
            mLastCamX                                     = cx;
            mLastCamY                                     = cy;
            const_cast<std::unique_ptr<Text>&>(lblCamPos) = std::make_unique<Text>(
                "Cam: " + std::to_string(cx) + "," + std::to_string(cy),
                SDL_Color{70, 70, 90, 255},
                cw - 100,
                window.GetHeight() - 18,
                11);
        }
        if (lblCamPos)
            lblCamPos->RenderToSurface(screen);
    }
    // Rebuild hint — tool-specific for Action, generic otherwise
    {
        std::string hint;
        if (mActiveTool == Tool::Action) {
            hint =
                "LClick:toggle action  Scroll:adjust hits  RClick:cycle group or clear anim "
                " Drop .json:assign death anim";
        } else {
            hint = "RClick:rotate  MMB:pan  Ctrl+Scroll:zoom(" +
                   std::to_string((int)(mZoom * 100)) +
                   "%)  G:Mode  Ctrl+S:Save  Ctrl+Z:Undo";
        }
        const_cast<std::unique_ptr<Text>&>(lblBottomHint) = std::make_unique<Text>(
            hint, SDL_Color{70, 70, 90, 255}, cw / 2 - 200, window.GetHeight() - 18, 11);
    }
    if (lblBottomHint)
        lblBottomHint->RenderToSurface(screen);

    // ── Destroy-anim picker popup ─────────────────────────────────────────────────
    if (mActionAnimPickerTile >= 0 && mActionAnimPickerTile < (int)mLevel.tiles.size() &&
        !mAnimPickerEntries.empty()) {
        const auto& tgt      = mLevel.tiles[mActionAnimPickerTile];
        const int   THUMB    = 48;
        const int   ROW_H    = THUMB + 10; // thumb + label below
        const int   PAD      = 8;
        const int   COL_W    = THUMB + PAD * 2;
        const int   COLS     = 4;
        const int   nEntries = (int)mAnimPickerEntries.size();
        const int   ROWS     = (nEntries + COLS - 1) / COLS;
        const int   TITLE_H  = 28;
        const int   HINT_H   = 16;
        const int   PW       = COL_W * COLS + PAD;
        const int   PH       = TITLE_H + ROWS * (ROW_H + PAD) + PAD + HINT_H;

        // Centre horizontally over the tile, position vertically so it stays on screen
        int tileSx = (int)((tgt.x - mCamX) * mZoom);
        int tileSy = (int)((tgt.y - mCamY) * mZoom);
        int tileSw = (int)(tgt.w * mZoom);
        int tileSh = (int)(tgt.h * mZoom);
        int px     = tileSx + tileSw / 2 - PW / 2;
        int py     = tileSy + tileSh + 6; // prefer below the tile
        // Clamp horizontally
        px = std::max(4, std::min(px, cw - PW - 4));
        // If popup would go off the bottom, put it above the tile instead
        if (py + PH > window.GetHeight() - 30)
            py = tileSy - PH - 6;
        py = std::max(TOOLBAR_H + 4, py);

        mActionAnimPickerRect = {px, py, PW, PH};

        // Panel background
        DrawRect(screen, {px, py, PW, PH}, {18, 14, 28, 245});
        DrawOutline(screen, {px, py, PW, PH}, {160, 80, 255, 255}, 2);
        // Accent bar on top
        DrawRect(screen, {px + 1, py + 1, PW - 2, 3}, {160, 80, 255, 255});

        // Title
        {
            std::string title =
                "Death Animation  — Tile " + std::to_string(mActionAnimPickerTile);
            auto [tx, ty] = Text::CenterInRect(title, 12, {px, py + 4, PW, TITLE_H - 4});
            Text t(title, SDL_Color{200, 160, 255, 255}, tx, ty, 12);
            t.RenderToSurface(screen);
        }

        // Entries grid
        int ey = py + TITLE_H;
        for (int i = 0; i < nEntries; i++) {
            const auto& entry = mAnimPickerEntries[i];
            int         col   = i % COLS;
            int         row   = i / COLS;
            int         ex    = px + PAD + col * COL_W;
            int         ey2   = ey + PAD + row * (ROW_H + PAD);

            SDL_Rect cell = {ex, ey2, COL_W - PAD, ROW_H};

            // Highlight currently-assigned anim
            bool      isCurrent = (entry.path == tgt.actionDestroyAnim);
            SDL_Color cellBg =
                isCurrent ? SDL_Color{80, 20, 130, 220} : SDL_Color{35, 28, 50, 200};
            SDL_Color cellBdr =
                isCurrent ? SDL_Color{200, 100, 255, 255} : SDL_Color{80, 60, 110, 200};
            DrawRect(screen, cell, cellBg);
            DrawOutline(screen, cell, cellBdr);

            // Thumbnail
            SDL_Rect thumbDst = {ex + (COL_W - PAD - THUMB) / 2, ey2 + 2, THUMB, THUMB};
            if (entry.thumb) {
                SDL_BlitSurfaceScaled(
                    entry.thumb, nullptr, screen, &thumbDst, SDL_SCALEMODE_LINEAR);
            } else {
                // "None" — draw a crossed-out box
                DrawRect(screen, thumbDst, {50, 40, 60, 200});
                DrawOutline(screen, thumbDst, {100, 80, 120, 255});
                // X mark
                for (int d = 0; d < THUMB; d++) {
                    DrawRect(
                        screen, {thumbDst.x + d, thumbDst.y + d, 2, 2}, {120, 80, 140, 200});
                    DrawRect(screen,
                             {thumbDst.x + THUMB - 1 - d, thumbDst.y + d, 2, 2},
                             {120, 80, 140, 200});
                }
            }

            // Label (truncated)
            std::string lbl = entry.name;
            if ((int)lbl.size() > 8)
                lbl = lbl.substr(0, 7) + "~";
            SDL_Color lblCol =
                isCurrent ? SDL_Color{255, 200, 255, 255} : SDL_Color{160, 140, 180, 255};
            SDL_Surface* lblSurf = GetBadge(lbl, lblCol);
            if (lblSurf) {
                int lx = ex + (COL_W - PAD - lblSurf->w) / 2;
                blitBadge(lblSurf, lx, ey2 + THUMB + 4);
            }

            // Current-assignment checkmark badge
            if (isCurrent) {
                DrawRect(
                    screen, {cell.x + cell.w - 14, cell.y + 1, 13, 13}, {160, 60, 255, 240});
                blitBadge(GetBadge("\xe2\x9c\x93", {255, 255, 255, 255}),
                          cell.x + cell.w - 12,
                          cell.y + 2);
            }
        }

        // Bottom hint
        int hy = py + PH - HINT_H + 2;
        blitBadge(
            GetBadge("Click to assign  •  RClick tile = remove action  •  Esc to close",
                     {100, 80, 130, 255}),
            px + PAD,
            hy);

        // Handle picker clicks — we need to do this in Render because we compute
        // rects here. Check current mouse button state.
        float  fmx, fmy;
        Uint32 mbState = SDL_GetMouseState(&fmx, &fmy);
        // We only act on a fresh click; detect via a static latch that resets when button is
        // up. Simpler: handle in HandleEvent instead — but we need the rects. So we store
        // the rects and handle in HandleEvent with a "picker click" path that re-derives
        // cells. (Handled in HandleEvent block below — picker rect is now set every frame.)
        (void)mbState;
    }

    // ── PowerUp picker popup ────────────────────────────────────────────────────
    if (mPowerUpPopupOpen && mPowerUpTileIdx >= 0 &&
        mPowerUpTileIdx < (int)mLevel.tiles.size()) {
        const auto& reg     = GetPowerUpRegistry();
        const int   PAD     = 8;
        const int   ROW_H   = 28;
        const int   TITLE_H = 32;
        const int   PW      = mPowerUpPopupRect.w;
        const int   PH      = mPowerUpPopupRect.h;
        const int   px      = mPowerUpPopupRect.x;
        const int   py      = mPowerUpPopupRect.y;

        DrawRectAlpha(screen, {px, py, PW, PH}, {15, 20, 40, 240});
        DrawOutline(screen, {px, py, PW, PH}, {80, 220, 255, 255}, 2);
        DrawRect(screen, {px + 1, py + 1, PW - 2, 3}, {80, 220, 255, 255});

        // Title
        {
            std::string t = "Power-Up  — Tile " + std::to_string(mPowerUpTileIdx);
            auto [tx, ty] = Text::CenterInRect(t, 11, {px, py + 4, PW, TITLE_H - 4});
            Text lbl(t, SDL_Color{80, 220, 255, 255}, tx, ty, 11);
            lbl.RenderToSurface(screen);
        }

        int               ry      = py + TITLE_H;
        const std::string curType = mLevel.tiles[mPowerUpTileIdx].powerUpType;

        // Power-up entries
        for (int i = 0; i < (int)reg.size(); i++) {
            bool     isCur = (mLevel.tiles[mPowerUpTileIdx].powerUp && curType == reg[i].id);
            SDL_Rect row   = {px + PAD, ry + i * (ROW_H + 2), PW - PAD * 2, ROW_H};
            DrawRect(screen,
                     row,
                     isCur ? SDL_Color{20, 80, 160, 220} : SDL_Color{30, 35, 55, 200});
            DrawOutline(screen,
                        row,
                        isCur ? SDL_Color{80, 180, 255, 255} : SDL_Color{50, 60, 90, 200});
            Text lbl(reg[i].label, SDL_Color{200, 240, 255, 255}, row.x + 6, row.y + 7, 11);
            lbl.RenderToSurface(screen);
            if (isCur) {
                blitBadge(
                    GetBadge("ON", {80, 255, 150, 255}), row.x + row.w - 28, row.y + 8);
            }
        }
        // None row
        {
            int      noneY = ry + (int)reg.size() * (ROW_H + 2);
            bool     isCur = !mLevel.tiles[mPowerUpTileIdx].powerUp;
            SDL_Rect row   = {px + PAD, noneY, PW - PAD * 2, ROW_H};
            DrawRect(screen,
                     row,
                     isCur ? SDL_Color{60, 20, 20, 220} : SDL_Color{30, 35, 55, 200});
            DrawOutline(screen,
                        row,
                        isCur ? SDL_Color{200, 80, 80, 255} : SDL_Color{50, 60, 90, 200});
            Text lbl("None (remove power-up)",
                     SDL_Color{200, 180, 180, 255},
                     row.x + 6,
                     row.y + 7,
                     11);
            lbl.RenderToSurface(screen);
        }
    }

    // ── Import input bar ──────────────────────────────────────────────────────
    if (mImportInputActive) {
        int panelH = 44, panelY = window.GetHeight() - 24 - panelH;
        DrawRect(screen, {0, panelY, cw, panelH}, {10, 20, 50, 240});
        DrawOutline(screen, {0, panelY, cw, panelH}, {80, 180, 255, 255}, 2);
        std::string dest = (mActiveTab == PaletteTab::Backgrounds)
                               ? "game_assets/backgrounds/"
                               : "game_assets/tiles/";
        Text il("Import into " + dest + "  — file or folder path  (Enter=go, Esc=cancel)",
                SDL_Color{140, 200, 255, 255},
                8,
                panelY + 4,
                11);
        il.RenderToSurface(screen);
        int fx = 8, fy = panelY + 18, fw = cw - 16, fh = 20;
        DrawRect(screen, {fx, fy, fw, fh}, {20, 35, 80, 255});
        DrawOutline(screen, {fx, fy, fw, fh}, {80, 180, 255, 200});
        Text it(mImportInputText + "|", SDL_Color{255, 255, 255, 255}, fx + 4, fy + 2, 12);
        it.RenderToSurface(screen);
    }

    // ── Drop overlay ──────────────────────────────────────────────────────────
    if (mDropActive) {
        DrawRect(
            screen, {0, TOOLBAR_H, cw, window.GetHeight() - TOOLBAR_H}, {20, 80, 160, 80});
        constexpr int B  = 6;
        SDL_Color     bc = {80, 180, 255, 220};
        DrawRect(screen, {0, TOOLBAR_H, cw, B}, bc);
        DrawRect(screen, {0, window.GetHeight() - B, cw, B}, bc);
        DrawRect(screen, {0, TOOLBAR_H, B, window.GetHeight() - TOOLBAR_H}, bc);
        DrawRect(screen, {cw - B, TOOLBAR_H, B, window.GetHeight() - TOOLBAR_H}, bc);
        int cx2 = cw / 2, cy2 = window.GetHeight() / 2;
        DrawRect(screen, {cx2 - 220, cy2 - 44, 440, 88}, {10, 30, 70, 220});
        DrawOutline(screen, {cx2 - 220, cy2 - 44, 440, 88}, {80, 180, 255, 255}, 2);
        if (mActiveTool == Tool::Action) {
            Text d1("Drop animated tile .json onto an Action tile",
                    SDL_Color{255, 200, 255, 255},
                    cx2 - 200,
                    cy2 - 32,
                    20);
            d1.RenderToSurface(screen);
            blitBadge(GetBadge("The tile will play that animation when destroyed",
                               {200, 160, 255, 255}),
                      cx2 - 164,
                      cy2 + 4);
        } else {
            std::string hint = (mActiveTab == PaletteTab::Backgrounds)
                                   ? "Drop .png or folder → backgrounds"
                                   : "Drop .png or folder → tiles";
            Text        d1(hint, SDL_Color{255, 255, 255, 255}, cx2 - 168, cy2 - 32, 24);
            d1.RenderToSurface(screen);
            blitBadge(
                GetBadge("Folders become subfolders in the palette", {140, 200, 255, 255}),
                cx2 - 150,
                cy2 + 4);
        }
    }

    // ── Delete confirmation popup ──────────────────────────────────────────
    if (mDelConfirmActive) {
        // Dim the whole screen
        SDL_Surface* ov = SDL_CreateSurface(screen->w, screen->h, SDL_PIXELFORMAT_ARGB8888);
        if (ov) {
            SDL_SetSurfaceBlendMode(ov, SDL_BLENDMODE_BLEND);
            SDL_FillSurfaceRect(ov, nullptr, SDL_MapRGBA(fmt, nullptr, 0, 0, 0, 160));
            SDL_BlitSurface(ov, nullptr, screen, nullptr);
            SDL_DestroySurface(ov);
        }
        // Panel
        int pw = 360, ph = 140;
        int px = W / 2 - pw / 2, py = H / 2 - ph / 2;
        DrawRect(screen, {px, py, pw, ph}, {20, 18, 28, 250});
        DrawOutline(screen, {px, py, pw, ph}, {200, 60, 60, 255}, 2);
        // Title
        std::string title = mDelConfirmIsDir ? "Delete Folder?" : "Delete File?";
        auto [tx, ty]     = Text::CenterInRect(title, 18, {px, py + 8, pw, 28});
        Text t1(title, SDL_Color{255, 100, 100, 255}, tx, ty, 18);
        t1.RenderToSurface(screen);
        // Name
        std::string nameStr = mDelConfirmName;
        if ((int)nameStr.size() > 32)
            nameStr = nameStr.substr(0, 30) + "...";
        auto [nx, ny] = Text::CenterInRect(nameStr, 12, {px, py + 38, pw, 20});
        Text t2(nameStr, SDL_Color{220, 200, 200, 255}, nx, ny, 12);
        t2.RenderToSurface(screen);
        // Warning
        std::string warn = mDelConfirmIsDir ? "This will delete all files inside!"
                                            : "This cannot be undone.";
        auto [wx2, wy2]  = Text::CenterInRect(warn, 11, {px, py + 58, pw, 18});
        Text t3(warn, SDL_Color{255, 160, 80, 255}, wx2, wy2, 11);
        t3.RenderToSurface(screen);
        // Buttons
        const_cast<SDL_Rect&>(mDelConfirmYes) = {px + 30, py + ph - 44, 130, 34};
        const_cast<SDL_Rect&>(mDelConfirmNo)  = {px + pw - 30 - 130, py + ph - 44, 130, 34};
        DrawRect(screen, mDelConfirmYes, {160, 30, 30, 255});
        DrawOutline(screen, mDelConfirmYes, {220, 80, 80, 255}, 2);
        auto [yx, yy] = Text::CenterInRect("Delete", 14, mDelConfirmYes);
        Text tb1("Delete", SDL_Color{255, 200, 200, 255}, yx, yy, 14);
        tb1.RenderToSurface(screen);
        DrawRect(screen, mDelConfirmNo, {40, 40, 60, 255});
        DrawOutline(screen, mDelConfirmNo, {80, 80, 120, 255}, 2);
        auto [cx3, cy3] = Text::CenterInRect("Cancel", 14, mDelConfirmNo);
        Text tb2("Cancel", SDL_Color{180, 180, 220, 255}, cx3, cy3, 14);
        tb2.RenderToSurface(screen);
        // Esc hint
        blitBadge(GetBadge("Esc to cancel", {80, 80, 100, 255}), W / 2 - 38, py + ph - 12);
    }

    // Upload completed surface to GPU and present
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, screen);
    SDL_DestroySurface(screen);
    if (tex) {
        SDL_RenderTexture(ren, tex, nullptr, nullptr);
        SDL_DestroyTexture(tex);
    }
    window.Update();
}

// --- NextScene -------------------------------------------------------------
std::unique_ptr<Scene> LevelEditorScene::NextScene() {
    if (mLaunchGame) {
        mLaunchGame = false;
        // Pass mProfilePath through directly — empty string means "Frost Knight (default)",
        // which is correct when the user hasn't selected a custom profile on the title
        // screen.
        return std::make_unique<GameScene>(
            "levels/" + mLevelName + ".json", true, mProfilePath);
    }
    if (mGoBack) {
        mGoBack = false;
        return std::make_unique<TitleScene>();
    }
    return nullptr;
}

// --- ImportPath ------------------------------------------------------------
bool LevelEditorScene::ImportPath(const std::string& srcPath) {
    fs::path src(srcPath);

    // ── Directory import ──────────────────────────────────────────────────────
    if (fs::is_directory(src)) {
        if (mActiveTab == PaletteTab::Backgrounds) {
            // Backgrounds doesn't support folders — import all PNGs flat
            int count = 0;
            for (const auto& entry : fs::recursive_directory_iterator(src)) {
                if (entry.path().extension() == ".png")
                    count += ImportPath(entry.path().string()) ? 1 : 0;
            }
            SetStatus("Imported " + std::to_string(count) + " backgrounds from " +
                      src.filename().string());
            return count > 0;
        }

        // Tiles: copy the folder tree into the CURRENT browse directory,
        // not always the root. This lets you browse into medieval-platformer
        // and drop a subfolder directly inside it.
        fs::path        baseDestDir = fs::path(mTileCurrentDir) / src.filename();
        std::error_code ec;

        // Recursively mirror the source tree: create matching subdirs and
        // copy every PNG, preserving the full relative hierarchy.
        int count = 0;
        for (const auto& entry : fs::recursive_directory_iterator(src)) {
            if (entry.is_directory()) {
                // Mirror subdirectory structure under baseDestDir
                fs::path rel  = fs::relative(entry.path(), src);
                fs::path dest = baseDestDir / rel;
                fs::create_directories(dest, ec);
                continue;
            }
            if (entry.path().extension() != ".png")
                continue;
            fs::path rel  = fs::relative(entry.path(), src);
            fs::path dest = baseDestDir / rel;
            fs::create_directories(dest.parent_path(), ec);
            if (!fs::exists(dest)) {
                fs::copy_file(entry.path(), dest, ec);
                if (ec)
                    continue;
            }
            count++;
        }

        if (count == 0) {
            SetStatus("No PNGs found in " + src.filename().string());
            return false;
        }

        // Navigate into the newly created folder so the user sees its contents
        LoadTileView(baseDestDir.string());
        SetStatus("Imported \"" + src.filename().string() + "\" into " +
                  fs::path(mTileCurrentDir).filename().string() + " (" +
                  std::to_string(count) + " files)");
        return true;
    }

    // ── Single file import ────────────────────────────────────────────────────
    std::string ext = src.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".png") {
        SetStatus("Import failed: only .png supported (got " + ext + ")");
        return false;
    }

    bool        isBg       = (mActiveTab == PaletteTab::Backgrounds);
    std::string destDirStr = isBg ? BG_ROOT : TILE_ROOT;

    // If we're currently browsing a subfolder, import into that subfolder
    if (!isBg && mTileCurrentDir != TILE_ROOT)
        destDirStr = mTileCurrentDir;

    fs::path        destDir(destDirStr);
    std::error_code ec;
    fs::create_directories(destDir, ec);
    if (ec) {
        SetStatus("Import failed: can't create " + destDirStr);
        return false;
    }

    fs::path dest = destDir / src.filename();
    if (!fs::exists(dest)) {
        fs::copy_file(src, dest, ec);
        if (ec) {
            SetStatus("Import failed: " + ec.message());
            return false;
        }
    }

    if (isBg) {
        SDL_Surface* full = LoadPNG(dest);
        if (!full) {
            SetStatus("Import failed: can't load " + dest.string());
            return false;
        }
        const int    tw = PALETTE_W - 8, th = tw / 2;
        SDL_Surface* thumb = MakeThumb(full, tw, th);
        SDL_DestroySurface(full);
        mBgItems.push_back({dest.string(), dest.stem().string(), thumb});
        mBgPaletteScroll = std::max(0, (int)mBgItems.size() - 1);
        ApplyBackground((int)mBgItems.size() - 1);
        SetStatus("Imported & applied: " + dest.filename().string());
    } else {
        SDL_Surface* full = LoadPNG(dest);
        if (!full) {
            SetStatus("Import failed: can't load " + dest.string());
            return false;
        }
        SDL_SetSurfaceBlendMode(full, SDL_BLENDMODE_BLEND);
        SDL_Surface* thumb = MakeThumb(full, PAL_ICON, PAL_ICON);

        // Reload the current folder view so the new file appears in the right place
        // with correct sorting (simpler than inserting into the right position)
        LoadTileView(mTileCurrentDir);

        // Find and select the newly added item
        for (int i = 0; i < (int)mPaletteItems.size(); i++) {
            if (mPaletteItems[i].path == dest.string()) {
                mSelectedTile = i;
                // Scroll to show it
                int row        = i / PAL_COLS;
                mPaletteScroll = std::max(0, row);
                break;
            }
        }
        // Free the surfaces we loaded manually — LoadTileView built its own
        if (thumb)
            SDL_DestroySurface(thumb);
        SDL_DestroySurface(full);

        mActiveTool = Tool::Tile;
        if (lblTool)
            lblTool->CreateSurface("Tile");
        SetStatus("Imported: " + dest.filename().string() + " -> auto-selected");
    }
    return true;
}
