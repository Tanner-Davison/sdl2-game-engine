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
    void                   Load(Window& window) override;
    void                   Unload() override;
    bool                   HandleEvent(SDL_Event& e) override;
    void                   Update(float dt) override {}
    void                   Render(Window& window) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    enum class Tool { Coin, Enemy, Erase, PlayerStart, Tile, Resize, Prop, Ladder, Action, Slope };
    enum class PaletteTab { Tiles, Backgrounds };

    // ── Constants ─────────────────────────────────────────────────────────────
    static constexpr int   GRID        = 64;
    static constexpr int   TOOLBAR_H   = 60;
    static constexpr int   PALETTE_W   = 180;
    static constexpr int   ICON_SIZE   = 40;
    static constexpr int   PAL_ICON    = 76;
    static constexpr int   PAL_COLS    = 2;
    static constexpr int   TAB_H       = 28;
    static constexpr float ENEMY_SPEED = 120.0f;

    // Root directories — the palette never navigates above these
    static constexpr const char* TILE_ROOT = "game_assets/tiles";
    static constexpr const char* BG_ROOT   = "game_assets/backgrounds";

    // ── Editor state ──────────────────────────────────────────────────────────
    Window*     mWindow          = nullptr;
    Tool        mActiveTool      = Tool::Coin;
    PaletteTab  mActiveTab       = PaletteTab::Tiles;
    bool        mLaunchGame      = false;
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

    // ── Tile palette navigation ───────────────────────────────────────────────
    // mTileCurrentDir is the directory we are currently browsing.
    // Empty string means we're at TILE_ROOT.
    // Navigating into a subfolder sets this to the full relative path.
    std::string mTileCurrentDir;

    // ── Resize tool state ──────────────────────────────────────────────────────
    enum class ResizeEdge { None, Right, Bottom, Corner };
    ResizeEdge           mHoverEdge     = ResizeEdge::None; // which edge the mouse is near
    int                  mHoverTileIdx  = -1;               // tile index under hover
    bool                 mIsResizing    = false;
    int                  mResizeTileIdx = -1;
    ResizeEdge           mResizeEdge    = ResizeEdge::None;
    int                  mResizeDragX   = 0;  // mouse x when drag started
    int                  mResizeDragY   = 0;  // mouse y when drag started
    int                  mResizeOrigW   = 0;  // tile.w when drag started
    int                  mResizeOrigH   = 0;  // tile.h when drag started
    static constexpr int RESIZE_HANDLE  = 10; // px from edge that counts as a handle

    // ── Drop / import state ───────────────────────────────────────────────────
    bool        mDropActive        = false;
    bool        mImportInputActive = false;
    std::string mImportInputText;

    // Double-click detection (palette items)
    Uint64                  mLastClickTime  = 0;
    int                     mLastClickIndex = -1;
    static constexpr Uint64 DOUBLE_CLICK_MS = 400;

    Level mLevel;

    // ── Palette entry ─────────────────────────────────────────────────────────
    // Used for both tile-files and tile-folders in the current view.
    struct PaletteItem {
        std::string  path;  // full relative path to PNG or directory
        std::string  label; // display name (stem for files, dirname for folders)
        SDL_Surface* thumb    = nullptr; // PAL_ICON×PAL_ICON for files; nullptr for folders
        SDL_Surface* full     = nullptr; // full-res for files; nullptr for folders
        bool         isFolder = false;
    };
    std::vector<PaletteItem> mPaletteItems; // current view (folders first, then files)

    // ── Background item ───────────────────────────────────────────────────────
    struct BgItem {
        std::string  path;
        std::string  label;
        SDL_Surface* thumb = nullptr;
    };
    std::vector<BgItem> mBgItems;

    // ── Assets ────────────────────────────────────────────────────────────────
    std::unique_ptr<Image>       background;
    std::unique_ptr<SpriteSheet> coinSheet;
    std::unique_ptr<SpriteSheet> enemySheet;
    // Folder icon loaded once, used as thumb for every folder cell.
    // PaletteItems point to this surface — they must NOT free it.
    SDL_Surface* mFolderIcon = nullptr;

    // ── Toolbar buttons ───────────────────────────────────────────────────────
    SDL_Rect btnCoin{}, btnEnemy{}, btnErase{}, btnPlayerStart{};
    SDL_Rect btnTile{}, btnResize{}, btnProp{}, btnLadder{}, btnAction{}, btnSlope{}, btnSave{}, btnLoad{}, btnPlay{}, btnClear{}, btnGravity{};

    // ── Labels ────────────────────────────────────────────────────────────────
    std::unique_ptr<Text> lblCoin, lblEnemy, lblErase, lblPlayer;
    std::unique_ptr<Text> lblTile, lblResize, lblProp, lblLadder, lblAction, lblSlope, lblSave, lblLoad, lblPlay, lblClear, lblGravity;
    std::unique_ptr<Text> lblStatus, lblTool;

    // ── Helpers ───────────────────────────────────────────────────────────────
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
            if (HitTest(r, x, y))
                return i;
        }
        return -1;
    }
    int HitEnemy(int x, int y) const {
        for (int i = 0; i < (int)mLevel.enemies.size(); i++) {
            SDL_Rect r = {(int)mLevel.enemies[i].x, (int)mLevel.enemies[i].y, GRID, GRID};
            if (HitTest(r, x, y))
                return i;
        }
        return -1;
    }
    int HitTile(int x, int y) const {
        // Scan backwards so we hit the topmost (last-placed) tile first.
        // This ensures erase, prop/ladder toggle, and drag all affect
        // the tile visually on top rather than the one underneath it.
        for (int i = (int)mLevel.tiles.size() - 1; i >= 0; i--) {
            const auto& t = mLevel.tiles[i];
            SDL_Rect    r = {(int)t.x, (int)t.y, t.w, t.h};
            if (HitTest(r, x, y))
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

    // ── Palette loading ───────────────────────────────────────────────────────

    // Rebuilds mPaletteItems from the given directory (relative path).
    // Shows subfolders first (with a folder icon colour), then PNG files.
    // If dir == TILE_ROOT the "◀ Back" row is not shown.
    void LoadTileView(const std::string& dir);

    void LoadBgPalette();

    // Applies background[idx] as the level background and refreshes the canvas.
    void ApplyBackground(int idx);

    // Returns which resize edge/corner the point (mx,my) is near for tile[idx],
    // or ResizeEdge::None if the point is not near any edge.
    ResizeEdge DetectResizeEdge(int tileIdx, int mx, int my) const;

    // ── Import ────────────────────────────────────────────────────────────────
    // Accepts a file OR a directory path.
    // File  → copies PNG into tiles/ or backgrounds/ (depending on active tab).
    // Dir   → copies entire folder into tiles/<dirname>/, then navigates into it.
    // Both routes add items live without a full palette reload.
    bool ImportPath(const std::string& srcPath);
};
