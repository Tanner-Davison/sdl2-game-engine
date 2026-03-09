#pragma once
#include "EditorCamera.hpp"
#include "EditorPalette.hpp"
#include "EditorSurfaceCache.hpp"
#include "EditorToolbar.hpp"
#include "Image.hpp"
#include "Level.hpp"
#include "LevelSerializer.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SpriteSheet.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include "tools/EditorTools.hpp"
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
    // Toolbar type aliases for brevity at call sites
    using TBBtn = EditorToolbar::ButtonId;
    using TBGrp = EditorToolbar::Group;

    // -------------------------------------------------------------------------
    // Constants
    // -------------------------------------------------------------------------
    static constexpr int   GRID          = 38;
    static constexpr int   TOOLBAR_H     = EditorToolbar::TOOLBAR_H;
    static constexpr int   PALETTE_W     = EditorPalette::PALETTE_W;
    static constexpr int   PALETTE_TAB_W = EditorPalette::PALETTE_TAB_W;
    static constexpr int   ICON_SIZE     = 40;
    static constexpr int   PAL_ICON      = EditorPalette::PAL_ICON;
    static constexpr int   PAL_COLS      = EditorPalette::PAL_COLS;
    static constexpr int   TAB_H         = EditorPalette::TAB_H;
    static constexpr float ENEMY_SPEED   = 120.0f;

    // Toolbar layout constants -- canonical source is EditorToolbar
    static constexpr int BTN_H      = EditorToolbar::BTN_H;
    static constexpr int BTN_Y      = EditorToolbar::BTN_Y;
    static constexpr int BTN_TOOL_W = EditorToolbar::BTN_TOOL_W;
    static constexpr int BTN_ACT_W  = EditorToolbar::BTN_ACT_W;
    static constexpr int BTN_GAP    = EditorToolbar::BTN_GAP;
    static constexpr int GRP_GAP    = EditorToolbar::GRP_GAP;

    static constexpr const char* TILE_ROOT = EditorPalette::TILE_ROOT;
    static constexpr const char* BG_ROOT   = EditorPalette::BG_ROOT;

    // -------------------------------------------------------------------------
    // Tool system
    // -------------------------------------------------------------------------
    ToolId                      mActiveToolId = ToolId::MoveCam;
    std::unique_ptr<EditorTool> mTool;        // active tool (nullptr for complex inline tools)

    // Build a fresh EditorToolContext pointing at current state.
    // Called before every tool dispatch so the context is always up-to-date.
    EditorToolContext MakeToolCtx() {
        return EditorToolContext{
            .level        = mLevel,
            .camera       = mCamera,
            .surfaceCache = mSurfaceCache,
            .grid         = GRID,
            .toolbarH     = TOOLBAR_H,
            .setStatus    = [this](const std::string& msg) { SetStatus(msg); },
            .canvasW      = [this]() { return CanvasW(); },
            .sdlWindow    = mWindow ? mWindow->GetRaw() : nullptr,
        };
    }

    // Switch to a new tool. Deactivates the old tool, creates the new one,
    // and activates it. Updates the toolbar label.
    void SwitchTool(ToolId id) {
        if (mTool) {
            auto ctx = MakeToolCtx();
            mTool->OnDeactivate(ctx);
        }
        mActiveToolId = id;
        mTool         = MakeEditorTool(id);
        if (mTool) {
            auto ctx = MakeToolCtx();
            mTool->OnActivate(ctx);
            if (lblTool) lblTool->CreateSurface(mTool->Name());
        }
        // For complex inline tools (Action, PowerUp, MovingPlat), mTool is null.
        // The orchestrator sets lblTool manually in those cases.
    }

    // Map ToolId -> TBBtn for toolbar active-state highlighting
    static TBBtn ToolIdToBtn(ToolId id) {
        switch (id) {
            case ToolId::Coin:        return TBBtn::Coin;
            case ToolId::Enemy:       return TBBtn::Enemy;
            case ToolId::Tile:        return TBBtn::Tile;
            case ToolId::Erase:       return TBBtn::Erase;
            case ToolId::PlayerStart: return TBBtn::PlayerStart;
            case ToolId::Select:      return TBBtn::Select;
            case ToolId::MoveCam:     return TBBtn::MoveCam;
            case ToolId::Prop:        return TBBtn::Prop;
            case ToolId::Ladder:      return TBBtn::Ladder;
            case ToolId::Action:      return TBBtn::Action;
            case ToolId::Slope:       return TBBtn::Slope;
            case ToolId::Resize:      return TBBtn::Resize;
            case ToolId::Hitbox:      return TBBtn::Hitbox;
            case ToolId::Hazard:      return TBBtn::Hazard;
            case ToolId::AntiGrav:    return TBBtn::AntiGrav;
            case ToolId::MovingPlat:  return TBBtn::MovingPlat;
            case ToolId::PowerUp:     return TBBtn::PowerUp;
        }
        return TBBtn::COUNT;
    }

    // -------------------------------------------------------------------------
    // Editor state
    // -------------------------------------------------------------------------
    std::string mOpenPath;
    bool        mForceNew = false;
    std::string mPresetName;
    std::string mProfilePath;
    Window*     mWindow     = nullptr;
    bool        mLaunchGame = false;
    bool        mGoBack     = false;

    // ── Cached static UI text ────────────────────────────────────────────────
    std::unique_ptr<Text> lblPalHeader;
    std::unique_ptr<Text> lblPalHint1;
    std::unique_ptr<Text> lblPalHint2;
    std::unique_ptr<Text> lblBgHeader;
    std::unique_ptr<Text> lblStatusBar;
    std::unique_ptr<Text> lblCamPos;
    std::unique_ptr<Text> lblBottomHint;
    std::unique_ptr<Text> lblToolPrefix;
    int         mLastTileCount  = -1;
    int         mLastCoinCount  = -1;
    int         mLastEnemyCount = -1;
    int         mLastCamX       = INT_MIN;
    int         mLastCamY       = INT_MIN;
    int         mLastTileSizeW  = -1;
    std::string mLastPalHeaderPath;

    // ── Generic state still used by inline complex tools ─────────────────────
    std::string          mStatusMsg       = "New level";
    std::string          mLevelName       = "level1";
    float mScrollAccum = 0.0f;

    // Surface cache
    EditorSurfaceCache mSurfaceCache;
    int mActionAnimDropHover = -1;

    // ── Destroy-anim picker popup ──────────────────────────────────────────
    int      mActionAnimPickerTile = -1;
    SDL_Rect mActionAnimPickerRect{};
    struct AnimPickerEntry {
        std::string  path;
        std::string  name;
        SDL_Surface* thumb = nullptr;
    };
    std::vector<AnimPickerEntry> mAnimPickerEntries;

    void OpenAnimPicker(int tileIdx);
    void CloseAnimPicker();

    // ── Toolbar subsystem ─────────────────────────────────────────────────────
    EditorToolbar mToolbar;

    // Status / active tool display
    std::unique_ptr<Text> lblStatus, lblTool;

    // Delete confirmation popup state
    bool        mDelConfirmActive  = false;
    std::string mDelConfirmPath;
    bool        mDelConfirmIsDir   = false;
    std::string mDelConfirmName;
    SDL_Rect    mDelConfirmYes{};
    SDL_Rect    mDelConfirmNo{};

    // Drop / import state
    bool        mDropActive        = false;
    bool        mImportInputActive = false;
    std::string mImportInputText;

    // Editor camera
    EditorCamera mCamera;

    Level mLevel;

    // ── Palette ──────────────────────────────────────────────────────────────
    using PaletteItem = EditorPalette::PaletteItem;
    using BgItem      = EditorPalette::BgItem;
    EditorPalette mPalette;

    // ── Assets ──────────────────────────────────────────────────────────────
    std::unique_ptr<Image>       background;
    std::unique_ptr<SpriteSheet> coinSheet;
    std::unique_ptr<SpriteSheet> enemySheet;
    SDL_Surface*                 mFolderIcon = nullptr;

    // ── Power-up tool popup state ──────────────────────────────────────────
    bool     mPowerUpPopupOpen  = false;
    int      mPowerUpTileIdx    = -1;
    SDL_Rect mPowerUpPopupRect{};
    struct PowerUpEntry {
        std::string id;
        std::string label;
        float       defaultDuration = 15.0f;
    };
    static const std::vector<PowerUpEntry>& GetPowerUpRegistry();
    int  mPowerUpSelectedEntry = 0;

    // Moving-platform tool state
    std::vector<int> mMovPlatIndices;
    int              mMovPlatNextGroupId = 1;
    int              mMovPlatCurGroupId  = 1;
    bool             mMovPlatHoriz       = true;
    float            mMovPlatRange       = 96.0f;
    float            mMovPlatSpeed       = 60.0f;
    bool             mMovPlatLoop        = false;
    bool             mMovPlatTrigger     = false;

    // Moving-platform config popup
    bool        mMovPlatPopupOpen   = false;
    bool        mMovPlatSpeedInput  = false;
    std::string mMovPlatSpeedStr    = "60";
    SDL_Rect    mMovPlatPopupRect{};

    // -------------------------------------------------------------------------
    // Helpers
    // -------------------------------------------------------------------------
    int CanvasW() const {
        if (!mWindow)
            return 800;
        return mPalette.IsCollapsed() ? mWindow->GetWidth() - PALETTE_TAB_W
                                      : mWindow->GetWidth() - PALETTE_W;
    }

    SDL_Point ScreenToWorld(int sx, int sy) const { return mCamera.ScreenToWorld(sx, sy); }
    SDL_Point WorldToScreen(float wx, float wy) const { return mCamera.WorldToScreen(wx, wy); }
    SDL_Point SnapToGrid(int sx, int sy) const { return mCamera.SnapToGrid(sx, sy, GRID); }

    bool HitTest(const SDL_Rect& r, int x, int y) const {
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    }

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

    SDL_Surface* GetBadge(const std::string& text, SDL_Color col) {
        return mSurfaceCache.GetBadge(text, col);
    }
    SDL_Surface* GetRotated(const std::string& path, SDL_Surface* src, int deg) {
        return mSurfaceCache.GetRotated(path, src, deg);
    }
    SDL_Surface* GetDestroyAnimThumb(const std::string& jsonPath) {
        return mSurfaceCache.GetDestroyAnimThumb(jsonPath);
    }

    void LoadTileView(const std::string& dir) { mPalette.LoadTileView(dir, mLevel); }
    void LoadBgPalette() { mPalette.LoadBgPalette(mLevel); }
    void ApplyBackground(int idx);
    bool ImportPath(const std::string& srcPath);

    // ── Tile tool helpers (used by Render for ghost preview) ─────────────────
    // These delegate to the TileTool if it's the active tool.
    int  GetTileW() const;
    int  GetTileH() const;
    int  GetGhostRotation() const;

    // =========================================================================
    // COMPAT SHIM — remaining state for inline tools (Action, PowerUp,
    // MovingPlat) that haven't been extracted yet.
    // =========================================================================

    // Generic entity drag (used by Action/PowerUp/MovingPlat inline tools)
    bool mIsDragging  = false;
    int  mDragIndex   = -1;
    bool mDragIsCoin  = false;
    bool mDragIsTile  = false;

    // Tile tool state (used by Render ghost preview, delegated to TileTool)
    int mTileW         = GRID;
    int mTileH         = GRID;
    int mGhostRotation = 0;
};
