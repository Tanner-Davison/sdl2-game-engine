#pragma once
#include "EditorCamera.hpp"
#include "EditorSurfaceCache.hpp"
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
        Select,
        MoveCam,  // left-drag pans the camera; no tile interaction
        PowerUp   // mark a tile as a power-up pickup
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
    Tool        mActiveTool = Tool::MoveCam;
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
    int                  mGhostRotation = 0; // pending rotation for next tile placement (0/90/180/270)
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

    // Surface cache — owns rotation, badge, destroy-anim thumb, and tile caches
    EditorSurfaceCache mSurfaceCache;
    // Index of the action tile currently being hovered during a drag-drop of a .json file.
    // -1 when no drop is in flight or cursor is not over an action tile.
    int mActionAnimDropHover = -1;

    // ── Destroy-anim picker popup ──────────────────────────────────────────────
    // Opened when the user clicks an already-action tile while the Action tool
    // is active. Lists all .json files in animated_tiles/ plus a "None" entry.
    int      mActionAnimPickerTile = -1; // tile index the picker is targeting (-1 = closed)
    SDL_Rect mActionAnimPickerRect{};    // screen-space bounding rect of the popup
    struct AnimPickerEntry {
        std::string path;   // empty = "None"
        std::string name;
        SDL_Surface* thumb = nullptr; // non-owning — points into mDestroyAnimThumbCache
    };
    std::vector<AnimPickerEntry> mAnimPickerEntries; // rebuilt when picker opens

    void OpenAnimPicker(int tileIdx);   // build mAnimPickerEntries and set mActionAnimPickerTile
    void CloseAnimPicker();             // reset picker state

    // Palette collapse
    bool mPaletteCollapsed = false; // true = panel hidden, tab visible

    // Toolbar group collapse state
    bool mGrp1Collapsed = false; // Place tools
    bool mGrp2Collapsed = false; // Modifier tools
    bool mGrp3Collapsed = false; // Action buttons

    // Collapsed group pill rects (for click detection)
    SDL_Rect mGrp1Pill{}, mGrp2Pill{}, mGrp3Pill{};

    // Delete confirmation popup state
    bool        mDelConfirmActive  = false;
    std::string mDelConfirmPath;   // path pending deletion
    bool        mDelConfirmIsDir   = false;
    std::string mDelConfirmName;   // display name
    SDL_Rect    mDelConfirmYes{};
    SDL_Rect    mDelConfirmNo{};

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

    // Editor camera — owns position, zoom, pan state, and coordinate helpers
    EditorCamera mCamera;

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
        SDL_Rect     delBtn   = {-1,-1,0,0}; // computed each frame in Render
    };
    std::vector<PaletteItem> mPaletteItems;

    struct BgItem {
        std::string  path;
        std::string  label;
        SDL_Surface* thumb  = nullptr;
        SDL_Rect     delBtn = {-1,-1,0,0};
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
    SDL_Rect btnCoin{}, btnEnemy{}, btnTile{}, btnErase{}, btnPlayerStart{}, btnSelect{}, btnMoveCam{};
    // Group 2 — Tile modifier tools (no Destructible button — merged into Action)
    SDL_Rect btnProp{}, btnLadder{}, btnAction{}, btnSlope{}, btnResize{}, btnHitbox{},
        btnHazard{}, btnAntiGrav{}, btnMovingPlat{}, btnPowerUp{};
    // Group 3 — Level actions
    SDL_Rect btnGravity{}, btnSave{}, btnLoad{}, btnClear{}, btnPlay{}, btnBack{};

    // Labels — all white, 12 px, centered in their button
    // -------------------------------------------------------------------------
    // Group 1
    std::unique_ptr<Text> lblCoin, lblEnemy, lblTile, lblErase, lblPlayer, lblSelect, lblMoveCam;
    std::unique_ptr<Text> hintSelect, hintMoveCam;
    // Group 2
    std::unique_ptr<Text> lblProp, lblLadder, lblAction, lblSlope, lblResize, lblHitbox,
        lblHazard, lblAntiGrav, lblMovingPlat, lblPowerUp;

    // ── Power-up tool popup state ──────────────────────────────────────────
    // Shown when the user clicks a tile with the PowerUp tool active.
    // Lets them choose which power-up type to assign (extensible list).
    bool        mPowerUpPopupOpen  = false;
    int         mPowerUpTileIdx    = -1;  // tile being configured
    SDL_Rect    mPowerUpPopupRect{};
    struct PowerUpEntry {
        std::string id;    // e.g. "antigravity"
        std::string label; // e.g. "Anti-Gravity (15s)"
        float       defaultDuration = 15.0f;
    };
    // The canonical list of power-up types — add new entries here to expose
    // them in the editor without touching any other UI code.
    static const std::vector<PowerUpEntry>& GetPowerUpRegistry();
    int  mPowerUpSelectedEntry = 0; // which entry is highlighted in the popup

    // Moving-platform tool state
    std::vector<int> mMovPlatIndices; // tile indices currently in this platform group
    int              mMovPlatNextGroupId = 1; // auto-incremented group id for new platforms
    int              mMovPlatCurGroupId  = 1; // group id being painted right now
    bool             mMovPlatHoriz       = true; // H or V
    float            mMovPlatRange       = 96.0f;
    float            mMovPlatSpeed       = 60.0f;
    bool             mMovPlatLoop        = false; // true = one-way left→right loop
    bool             mMovPlatTrigger     = false; // true = only starts when player lands on it

    // Moving-platform config popup
    bool        mMovPlatPopupOpen   = false; // true = popup is visible
    bool        mMovPlatSpeedInput  = false; // true = speed text field has focus
    std::string mMovPlatSpeedStr    = "60";  // live text buffer for speed input
    SDL_Rect    mMovPlatPopupRect{};          // screen rect of the popup panel
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

    // ── Camera convenience wrappers ─────────────────────────────────────────
    // Delegate to mCamera so existing call sites compile without modification.
    SDL_Point ScreenToWorld(int sx, int sy) const { return mCamera.ScreenToWorld(sx, sy); }
    SDL_Point WorldToScreen(float wx, float wy) const { return mCamera.WorldToScreen(wx, wy); }
    SDL_Point SnapToGrid(int sx, int sy) const { return mCamera.SnapToGrid(sx, sy, GRID); }

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

    // Alpha-blended fill — creates a temporary surface with BLENDMODE_BLEND so
    // the alpha channel is actually respected. Use this instead of DrawRect
    // whenever you need a semi-transparent overlay over existing content.
    void DrawRectAlpha(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
        if (r.w <= 0 || r.h <= 0) return;
        SDL_Surface* ov = SDL_CreateSurface(r.w, r.h, SDL_PIXELFORMAT_ARGB8888);
        if (!ov) return;
        SDL_SetSurfaceBlendMode(ov, SDL_BLENDMODE_BLEND);
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(ov->format);
        SDL_FillSurfaceRect(ov, nullptr, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
        SDL_BlitSurface(ov, nullptr, s, &r);
        SDL_DestroySurface(ov);
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

    // ── Surface cache convenience wrappers ─────────────────────────────────
    // These delegate to mSurfaceCache so existing call sites (GetBadge, GetRotated,
    // GetDestroyAnimThumb, mTileSurfaceCache) compile without modification.
    SDL_Surface* GetBadge(const std::string& text, SDL_Color col) {
        return mSurfaceCache.GetBadge(text, col);
    }
    SDL_Surface* GetRotated(const std::string& path, SDL_Surface* src, int deg) {
        return mSurfaceCache.GetRotated(path, src, deg);
    }
    SDL_Surface* GetDestroyAnimThumb(const std::string& jsonPath) {
        return mSurfaceCache.GetDestroyAnimThumb(jsonPath);
    }

    // Palette loading
    void       LoadTileView(const std::string& dir);
    void       LoadBgPalette();
    void       ApplyBackground(int idx);
    ResizeEdge DetectResizeEdge(int tileIdx, int mx, int my) const;
    bool       ImportPath(const std::string& srcPath);
    void       RebuildToolbarLayout(); // recompute button rects after collapse state changes
};
