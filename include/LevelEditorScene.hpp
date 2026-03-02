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
#include <cmath>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class LevelEditorScene : public Scene {
  public:
    // Construct with an optional level path to load on startup.
    //   LevelEditorScene("")          -> auto-resume levels/level1.json if it exists
    //   LevelEditorScene("", true)    -> always start blank ("+ New Level" button)
    //   LevelEditorScene("path.json") -> open that specific file
    // levelName: when non-empty and forceNew=true, sets mLevelName directly
    // instead of defaulting to "level1"
    explicit LevelEditorScene(std::string levelPath = "",
                              bool        forceNew  = false,
                              std::string levelName = "")
        : mOpenPath(std::move(levelPath))
        , mForceNew(forceNew)
        , mPresetName(std::move(levelName)) {}
    void                   Load(Window& window) override;
    void                   Unload() override;
    bool                   HandleEvent(SDL_Event& e) override;
    void                   Update(float dt) override {}
    void                   Render(Window& window) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    enum class Tool { Coin, Enemy, Erase, PlayerStart, Tile, Resize, Prop, Ladder, Action, Slope, Hitbox };
    enum class PaletteTab { Tiles, Backgrounds };

    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------
    static constexpr int   GRID        = 48;   // 48px gives ~33% more grid cells
    static constexpr int   TOOLBAR_H   = 72;   // taller for clean grouped layout
    static constexpr int   PALETTE_W   = 180;
    static constexpr int   ICON_SIZE   = 40;
    static constexpr int   PAL_ICON    = 76;
    static constexpr int   PAL_COLS    = 2;
    static constexpr int   TAB_H       = 28;
    static constexpr float ENEMY_SPEED = 120.0f;

    // Toolbar layout — all buttons share the same height; widths per group
    static constexpr int BTN_H      = 56;   // button height (fits inside TOOLBAR_H with margins)
    static constexpr int BTN_Y      = 8;    // vertical offset from toolbar top
    static constexpr int BTN_TOOL_W = 68;   // tool buttons (groups 1 & 2)
    static constexpr int BTN_ACT_W  = 72;   // action buttons (group 3)
    static constexpr int BTN_GAP    = 4;    // gap between buttons within a group
    static constexpr int GRP_GAP    = 16;   // gap between groups (shows divider line)

    // Root directories
    static constexpr const char* TILE_ROOT = "game_assets/tiles";
    static constexpr const char* BG_ROOT   = "game_assets/backgrounds";

    // -------------------------------------------------------------------------
    // Editor state
    // -------------------------------------------------------------------------
    std::string mOpenPath;
    bool        mForceNew        = false;
    std::string mPresetName;     // name chosen in title-screen modal (overrides "level1")
    Window*     mWindow          = nullptr;
    Tool        mActiveTool      = Tool::Coin;
    PaletteTab  mActiveTab       = PaletteTab::Tiles;
    bool        mLaunchGame      = false;
    bool        mGoBack          = false;   // true = return to TitleScene

    // ── Hitbox tool state ──────────────────────────────────────────────────
    int  mHitboxTileIdx  = -1;  // tile currently selected for hitbox editing (-1 = none)
    // Which edge/corner of the hitbox is being dragged
    enum class HitboxHandle { None, Left, Right, Top, Bottom, TopLeft, TopRight, BotLeft, BotRight };
    HitboxHandle mHitboxHandle    = HitboxHandle::None;
    HitboxHandle mHoverHitboxHdl  = HitboxHandle::None;
    bool         mHitboxDragging  = false;
    int          mHitboxDragX     = 0;
    int          mHitboxDragY     = 0;
    // Snapshot of hitbox at drag start
    int          mHitboxOrigOffX  = 0;
    int          mHitboxOrigOffY  = 0;
    int          mHitboxOrigW     = 0;
    int          mHitboxOrigH     = 0;
    static constexpr int HBOX_HANDLE = 10; // px from edge that activates handle
    bool        mIsDragging      = false;
    int         mDragIndex       = -1;
    bool        mDragIsCoin      = false;
    bool        mDragIsTile      = false;
    std::string mStatusMsg       = "New level";
    std::string mLevelName       = "level1";
    int         mPaletteScroll   = 0;
    int         mBgPaletteScroll = 0;
    int         mSelectedTile    = 0;
    int         mSelectedBg      = 0;
    int         mTileW = GRID, mTileH = GRID;

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

    // Drop / import state
    bool        mDropActive        = false;
    bool        mImportInputActive = false;
    std::string mImportInputText;

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
    SDL_Rect btnCoin{}, btnEnemy{}, btnTile{}, btnErase{}, btnPlayerStart{};
    // Group 2 — Tile modifier tools
    SDL_Rect btnProp{}, btnLadder{}, btnAction{}, btnSlope{}, btnResize{}, btnHitbox{};
    // Group 3 — Level actions
    SDL_Rect btnGravity{}, btnSave{}, btnLoad{}, btnClear{}, btnPlay{}, btnBack{};

    // -------------------------------------------------------------------------
    // Labels — all white, 12 px, centered in their button
    // -------------------------------------------------------------------------
    // Group 1
    std::unique_ptr<Text> lblCoin, lblEnemy, lblTile, lblErase, lblPlayer;
    // Group 2
    std::unique_ptr<Text> lblProp, lblLadder, lblAction, lblSlope, lblResize, lblHitbox;
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
        return mWindow ? mWindow->GetWidth() - PALETTE_W : 800;
    }

    SDL_Point SnapToGrid(int x, int y) const {
        int cx = (x / GRID) * GRID;
        int cy = ((y - TOOLBAR_H) / GRID) * GRID + TOOLBAR_H;
        return {cx, std::max(TOOLBAR_H, cy)};
    }

    bool HitTest(const SDL_Rect& r, int x, int y) const {
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    }

    int HitCoin(int x, int y) const {
        for (int i = 0; i < (int)mLevel.coins.size(); i++) {
            SDL_Rect r = {(int)mLevel.coins[i].x, (int)mLevel.coins[i].y, GRID, GRID};
            if (HitTest(r, x, y)) return i;
        }
        return -1;
    }
    int HitEnemy(int x, int y) const {
        for (int i = 0; i < (int)mLevel.enemies.size(); i++) {
            SDL_Rect r = {(int)mLevel.enemies[i].x, (int)mLevel.enemies[i].y, GRID, GRID};
            if (HitTest(r, x, y)) return i;
        }
        return -1;
    }
    int HitTile(int x, int y) const {
        for (int i = (int)mLevel.tiles.size() - 1; i >= 0; i--) {
            const auto& t = mLevel.tiles[i];
            SDL_Rect    r = {(int)t.x, (int)t.y, t.w, t.h};
            if (HitTest(r, x, y)) return i;
        }
        return -1;
    }

    void SetStatus(const std::string& msg) {
        mStatusMsg = msg;
        if (lblStatus) lblStatus->CreateSurface(mStatusMsg);
    }

    void DrawRect(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
        SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
    }

    void DrawOutline(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1) {
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
        Uint32 col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
        SDL_Rect rects[4] = {{r.x, r.y, r.w, t},
                             {r.x, r.y + r.h, r.w, t},
                             {r.x, r.y, t, r.h},
                             {r.x + r.w, r.y, t, r.h}};
        for (auto& rr : rects) SDL_FillSurfaceRect(s, &rr, col);
    }

    // Palette loading
    void LoadTileView(const std::string& dir);
    void LoadBgPalette();
    void ApplyBackground(int idx);
    ResizeEdge DetectResizeEdge(int tileIdx, int mx, int my) const;
    bool ImportPath(const std::string& srcPath);
};
