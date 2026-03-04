#pragma once
#include "Image.hpp"
#include "Level.hpp"
#include "LevelSerializer.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SpriteSheet.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <array>
#include <climits>
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class LevelEditorScene : public Scene {
  public:
    // Construct with an optional level path to load on startup.
    //   LevelEditorScene("")          -> auto-resume levels/level1.json if it exists
    //   LevelEditorScene("", true)    -> always start blank ("+ New Level" button)
    //   LevelEditorScene("path.json") -> open that specific file
    // levelName: when non-empty and forceNew=true, sets mLevelName directly
    // instead of defaulting to "level1"
    explicit LevelEditorScene(std::string levelPath   = "",
                              bool        forceNew    = false,
                              std::string levelName   = "",
                              std::string profilePath = "")
        : mOpenPath(std::move(levelPath))
        , mForceNew(forceNew)
        , mPresetName(std::move(levelName))
        , mProfilePath(std::move(profilePath)) {}
    void                   Load(Window& window) override;
    void                   Unload() override;
    bool                   HandleEvent(SDL_Event& e) override;
    void                   Update(float dt) override;
    void                   Render(Window& window) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    // Destructible has been removed — Action tool is the one unified slash-trigger.
    enum class Tool {
        Coin,
        Enemy,
        Erase,
        PlayerStart,
        Tile,
        Resize,
        Prop,
        Ladder,
        Action,
        Slope,
        Hitbox,
        Hazard,
        AntiGrav,
        MovingPlat,
        Select
    };
    enum class PaletteTab { Tiles, Backgrounds };

    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------
    static constexpr int   GRID          = 38;
    static constexpr int   TOOLBAR_H     = 86; // extra room for collapse strip below buttons
    static constexpr int   PALETTE_W     = 180;
    static constexpr int   PALETTE_TAB_W = 18; // width of the collapsed toggle tab
    static constexpr int   ICON_SIZE     = 40;
    static constexpr int   PAL_ICON      = 76;
    static constexpr int   PAL_COLS      = 2;
    static constexpr int   TAB_H         = 28;
    static constexpr float ENEMY_SPEED   = 120.0f;

    // Toolbar layout — all buttons share the same height; widths per group
    static constexpr int BTN_H = 56; // button height (fits inside TOOLBAR_H with margins)
    static constexpr int BTN_Y = 8;  // vertical offset from toolbar top
    static constexpr int BTN_TOOL_W = 68; // tool buttons (groups 1 & 2)
    static constexpr int BTN_ACT_W  = 72; // action buttons (group 3)
    static constexpr int BTN_GAP    = 4;  // gap between buttons within a group
    static constexpr int GRP_GAP    = 16; // gap between groups (shows divider line)

    // Root directories
    static constexpr const char* TILE_ROOT = "game_assets/tiles";
    static constexpr const char* BG_ROOT   = "game_assets/backgrounds";

    // -------------------------------------------------------------------------
    // Editor state
    // -------------------------------------------------------------------------
    std::string mOpenPath;
    bool        mForceNew = false;
    std::string mPresetName;   // name chosen in title-screen modal (overrides "level1")
    std::string mProfilePath;  // PlayerProfile JSON path to pass through to GameScene (empty = frost knight)
    Window*     mWindow     = nullptr;
    Tool        mActiveTool = Tool::Coin;
    PaletteTab  mActiveTab  = PaletteTab::Tiles;
    bool        mLaunchGame = false;
    bool        mGoBack     = false; // true = return to TitleScene

    // ── Cached static UI text (rebuilt only when content changes) ─────────────
    // These avoid constructing Text objects every frame in Render.
    std::unique_ptr<Text> lblPalHeader;  // "Tiles/..." breadcrumb
    std::unique_ptr<Text> lblPalHint1;   // "Size: N  Esc=up"
    std::unique_ptr<Text> lblPalHint2;   // "Click folder to open"
    std::unique_ptr<Text> lblBgHeader;   // "Backgrounds (I=import)"
    std::unique_ptr<Text> lblStatusBar;  // tile/coin/enemy counts
    std::unique_ptr<Text> lblCamPos;     // "Cam: x,y"
    std::unique_ptr<Text> lblBottomHint; // keyboard shortcut hint
    std::unique_ptr<Text> lblToolPrefix; // "Tool:"
    // Per-group collapse tab labels (rebuilt in RebuildToolbarLayout)
    std::unique_ptr<Text> lblGrp1Tab, lblGrp2Tab, lblGrp3Tab;
    // Palette cell labels — rebuilt when palette changes
    std::vector<std::unique_ptr<Text>> mPalCellLabels;
    // Track last known values so we only rebuild when they change
    int         mLastTileCount  = -1;
    int         mLastCoinCount  = -1;
    int         mLastEnemyCount = -1;
    int         mLastCamX       = INT_MIN;
    int         mLastCamY       = INT_MIN;
    int         mLastTileSizeW  = -1;
    std::string mLastPalHeaderPath;

    // ── Hitbox tool state ──────────────────────────────────────────────────
    int mHitboxTileIdx = -1; // tile currently selected for hitbox editing (-1 = none)
    // Which edge/corner of the hitbox is being dragged
    enum class HitboxHandle {
        None,
        Left,
        Right,
        Top,
        Bottom,
        TopLeft,
        TopRight,
        BotLeft,
        BotRight
    };
    HitboxHandle mHitboxHandle   = HitboxHandle::None;
    HitboxHandle mHoverHitboxHdl = HitboxHandle::None;
    bool         mHitboxDragging = false;
    int          mHitboxDragX    = 0;
    int          mHitboxDragY    = 0;
    // Snapshot of hitbox at drag start
    int                  mHitboxOrigOffX  = 0;
    int                  mHitboxOrigOffY  = 0;
    int                  mHitboxOrigW     = 0;
    int                  mHitboxOrigH     = 0;
    static constexpr int HBOX_HANDLE      = 10; // px from edge that activates handle
    bool                 mIsDragging      = false;
    int                  mDragIndex       = -1;
    bool                 mDragIsCoin      = false;
    bool                 mDragIsTile      = false;
    std::string          mStatusMsg       = "New level";
    std::string          mLevelName       = "level1";
    int                  mPaletteScroll   = 0;
    int                  mBgPaletteScroll = 0;
    int                  mSelectedTile    = 0;
    int                  mSelectedBg      = 0;
    int                  mTileW = GRID, mTileH = GRID;
    float mScrollAccum = 0.0f; // fractional scroll accumulator for tile sizing

    // Tile palette navigation
    std::string mTileCurrentDir;

    // Resize tool state
    enum class ResizeEdge { None, Right, Bottom, Corner };
    ResizeEdge           mHoverEdge     = ResizeEdge::None;
    int                  mHoverTileIdx  = -1;
    bool                 mIsResizing    = false;
    int                  mResizeTileIdx = -1;
    ResizeEdge           mResizeEdge    = ResizeEdge::None;
    int                  mResizeDragX   = 0;
    int                  mResizeDragY   = 0;
    int                  mResizeOrigW   = 0;
    int                  mResizeOrigH   = 0;
    static constexpr int RESIZE_HANDLE  = 10;

    // Fast path→surface lookup for rendering tiles (rebuilt in LoadTileView)
    std::unordered_map<std::string, SDL_Surface*> mTileSurfaceCache;
    // Extra surfaces loaded for level tiles from subdirs not in current palette view.
    // These are owned here and freed in Unload() separately from palette items.
    std::vector<SDL_Surface*> mExtraTileSurfaces;

    // Rotation cache: for each image path, pre-built surfaces for 90/180/270 deg.
    // Index 0=90, 1=180, 2=270. Built lazily on first use, freed in Unload.
    std::unordered_map<std::string, std::array<SDL_Surface*, 3>> mRotCache;

    // Badge text surface cache: maps badge string -> pre-rendered SDL_Surface*.
    // Avoids constructing a Text object every frame for P/L/A/F/H/slope badges.
    std::unordered_map<std::string, SDL_Surface*> mBadgeCache;

    SDL_Surface* GetBadge(const std::string& text, SDL_Color col);
    SDL_Surface* GetRotated(const std::string& path, SDL_Surface* src, int deg);

    // Palette collapse
    bool mPaletteCollapsed = false; // true = panel hidden, tab visible

    // Toolbar group collapse state
    bool mGrp1Collapsed = false; // Place tools
    bool mGrp2Collapsed = false; // Modifier tools
    bool mGrp3Collapsed = false; // Action buttons

    // Collapsed group pill rects (for click detection)
    SDL_Rect mGrp1Pill{}, mGrp2Pill{}, mGrp3Pill{};

    // Drop / import state
    bool        mDropActive        = false;
    bool        mImportInputActive = false;
    std::string mImportInputText;

    // ── Selection tool state ───────────────────────────────────────────────
    std::vector<int> mSelIndices; // indices into mLevel.tiles that are selected
    // Rubber-band marquee
    bool mSelBoxing = false; // dragging a selection rect
    int  mSelBoxX0  = 0;     // world-space anchor corner
    int  mSelBoxY0  = 0;
    int  mSelBoxX1  = 0; // world-space live corner
    int  mSelBoxY1  = 0;
    // Moving the selection
    bool mSelDragging    = false;
    int  mSelDragStartWX = 0; // world-space mouse position at drag start
    int  mSelDragStartWY = 0;
    // Per-tile original positions captured at drag-start
    std::vector<std::pair<float, float>> mSelOrigPositions;

    // Editor camera pan
    float mCamX         = 0.0f; // world-space offset applied to all canvas rendering
    float mCamY         = 0.0f;
    bool  mIsPanning    = false; // true while middle-mouse is held
    int   mPanStartX    = 0;
    int   mPanStartY    = 0;
    float mPanCamStartX = 0.0f;
    float mPanCamStartY = 0.0f;

    // Double-click detection (palette items)
    Uint64                  mLastClickTime  = 0;
    int                     mLastClickIndex = -1;
    static constexpr Uint64 DOUBLE_CLICK_MS = 400;

    Level mLevel;

    // -------------------------------------------------------------------------
    // Palette entries
    // -------------------------------------------------------------------------
    struct PaletteItem {
        std::string  path;
        std::string  label;
        SDL_Surface* thumb    = nullptr;
        SDL_Surface* full     = nullptr;
        bool         isFolder = false;
    };
    std::vector<PaletteItem> mPaletteItems;

    struct BgItem {
        std::string  path;
        std::string  label;
        SDL_Surface* thumb = nullptr;
    };
    std::vector<BgItem> mBgItems;

    // -------------------------------------------------------------------------
    // Assets
    // -------------------------------------------------------------------------
    std::unique_ptr<Image>       background;
    std::unique_ptr<SpriteSheet> coinSheet;
    std::unique_ptr<SpriteSheet> enemySheet;
    SDL_Surface*                 mFolderIcon = nullptr;

    // -------------------------------------------------------------------------
    // Toolbar buttons  (three groups separated by GRP_GAP dividers)
    // -------------------------------------------------------------------------
    // Group 1 — Place tools
    SDL_Rect btnCoin{}, btnEnemy{}, btnTile{}, btnErase{}, btnPlayerStart{}, btnSelect{};
    // Group 2 — Tile modifier tools (no Destructible button — merged into Action)
    SDL_Rect btnProp{}, btnLadder{}, btnAction{}, btnSlope{}, btnResize{}, btnHitbox{},
        btnHazard{}, btnAntiGrav{}, btnMovingPlat{};
    // Group 3 — Level actions
    SDL_Rect btnGravity{}, btnSave{}, btnLoad{}, btnClear{}, btnPlay{}, btnBack{};

    // Labels — all white, 12 px, centered in their button
    // -------------------------------------------------------------------------
    // Group 1
    std::unique_ptr<Text> lblCoin, lblEnemy, lblTile, lblErase, lblPlayer, lblSelect;
    std::unique_ptr<Text> hintSelect;
    // Group 2
    std::unique_ptr<Text> lblProp, lblLadder, lblAction, lblSlope, lblResize, lblHitbox,
        lblHazard, lblAntiGrav, lblMovingPlat;

    // Moving-platform tool state
    std::vector<int> mMovPlatIndices; // tile indices currently in this platform group
    int              mMovPlatNextGroupId = 1; // auto-incremented group id for new platforms
    int              mMovPlatCurGroupId  = 1; // group id being painted right now
    bool             mMovPlatHoriz       = true; // H or V
    float            mMovPlatRange       = 96.0f;
    float            mMovPlatSpeed       = 60.0f;
    bool             mMovPlatLoop        = false; // true = one-way left→right loop
    bool             mMovPlatTrigger = false; // true = only starts when player lands on it
    // Group 3
    std::unique_ptr<Text> lblGravity, lblSave, lblLoad, lblClear, lblPlay, lblBack;
    // Status / active tool display
    std::unique_ptr<Text> lblStatus, lblTool;
    // Tiny shortcut hint (bottom-right corner of each tool button, 9 px, dimmed)
    std::unique_ptr<Text> hintCoin, hintEnemy, hintTile, hintErase, hintPlayer;
    std::unique_ptr<Text> hintProp, hintLadder, hintAction, hintSlope, hintResize;

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------
    int CanvasW() const {
        if (!mWindow)
            return 800;
        return mPaletteCollapsed ? mWindow->GetWidth() - PALETTE_TAB_W
                                 : mWindow->GetWidth() - PALETTE_W;
    }

    // Convert a screen-space canvas point to world space
    SDL_Point ScreenToWorld(int sx, int sy) const {
        return {(int)(sx + mCamX), (int)(sy + mCamY)};
    }
    // Convert a world-space point to screen space
    SDL_Point WorldToScreen(float wx, float wy) const {
        return {(int)(wx - mCamX), (int)(wy - mCamY)};
    }

    SDL_Point SnapToGrid(int sx, int sy) const {
        // Convert screen coords to world coords, then snap to grid.
        // The visual grid lines are drawn at world multiples of GRID,
        // so snapping in world space keeps placements aligned with them.
        int wx = (int)(sx + mCamX);
        int wy = (int)(sy + mCamY);
        int cx = (wx / GRID) * GRID;
        int cy = (wy / GRID) * GRID;
        // Never place above world-y=0 (the top of the canvas)
        if (cy < 0)
            cy = 0;
        return {cx, cy};
    }

    bool HitTest(const SDL_Rect& r, int x, int y) const {
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    }

    // Hit-tests take screen-space mouse coords and convert to world space internally
    int HitCoin(int sx, int sy) const {
        auto [wx, wy] = ScreenToWorld(sx, sy);
        for (int i = 0; i < (int)mLevel.coins.size(); i++) {
            SDL_Rect r = {(int)mLevel.coins[i].x, (int)mLevel.coins[i].y, GRID, GRID};
            if (HitTest(r, wx, wy))
                return i;
        }
        return -1;
    }
    int HitEnemy(int sx, int sy) const {
        auto [wx, wy] = ScreenToWorld(sx, sy);
        for (int i = 0; i < (int)mLevel.enemies.size(); i++) {
            SDL_Rect r = {(int)mLevel.enemies[i].x, (int)mLevel.enemies[i].y, GRID, GRID};
            if (HitTest(r, wx, wy))
                return i;
        }
        return -1;
    }
    int HitTile(int sx, int sy) const {
        auto [wx, wy] = ScreenToWorld(sx, sy);
        for (int i = (int)mLevel.tiles.size() - 1; i >= 0; i--) {
            const auto& t = mLevel.tiles[i];
            SDL_Rect    r = {(int)t.x, (int)t.y, t.w, t.h};
            if (HitTest(r, wx, wy))
                return i;
        }
        return -1;
    }

    void SetStatus(const std::string& msg) {
        mStatusMsg = msg;
        if (lblStatus)
            lblStatus->CreateSurface(mStatusMsg);
    }

    void DrawRect(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
        SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
    }

    void DrawOutline(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1) {
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
        Uint32                        col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
        SDL_Rect                      rects[4] = {{r.x, r.y, r.w, t},
                                                  {r.x, r.y + r.h, r.w, t},
                                                  {r.x, r.y, t, r.h},
                                                  {r.x + r.w, r.y, t, r.h}};
        for (auto& rr : rects)
            SDL_FillSurfaceRect(s, &rr, col);
    }

    // Palette loading
    void       LoadTileView(const std::string& dir);
    void       LoadBgPalette();
    void       ApplyBackground(int idx);
    ResizeEdge DetectResizeEdge(int tileIdx, int mx, int my) const;
    bool       ImportPath(const std::string& srcPath);
    void       RebuildToolbarLayout(); // recompute button rects after collapse state changes
};
