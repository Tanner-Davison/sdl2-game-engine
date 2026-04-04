#pragma once
#include "EditorCamera.hpp"
#include "EditorFileOps.hpp"
#include "EditorPalette.hpp"
#include "EditorPopups.hpp"
#include "EditorSurfaceCache.hpp"
#include "EditorCanvasRenderer.hpp"
#include "EditorToolbar.hpp"
#include "EditorUIRenderer.hpp"
#include "EnemyProfile.hpp"
#include "Image.hpp"
#include "LevelData.hpp"
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
    void                   Render(Window& window, float alpha = 1.0f) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    using TBBtn = EditorToolbar::ButtonId;
    using TBGrp = EditorToolbar::Group;

    // --- Constants ---
    static constexpr int   GRID          = 38;
    static constexpr int   TOOLBAR_H     = EditorToolbar::TOOLBAR_H;
    static constexpr int   PALETTE_W     = EditorPalette::PALETTE_W;
    static constexpr int   PALETTE_TAB_W = EditorPalette::PALETTE_TAB_W;
    static constexpr int   ICON_SIZE     = 40;
    static constexpr int   PAL_ICON      = EditorPalette::PAL_ICON;
    static constexpr int   PAL_COLS      = EditorPalette::PAL_COLS;
    static constexpr int   TAB_H         = EditorPalette::TAB_H;
    static constexpr float ENEMY_SPEED   = 120.0f;

    // Canonical source is EditorToolbar
    static constexpr int BTN_H      = EditorToolbar::BTN_H;
    static constexpr int BTN_Y      = EditorToolbar::BTN_Y;
    static constexpr int BTN_TOOL_W = EditorToolbar::BTN_TOOL_W;
    static constexpr int BTN_ACT_W  = EditorToolbar::BTN_ACT_W;
    static constexpr int BTN_GAP    = EditorToolbar::BTN_GAP;
    static constexpr int GRP_GAP    = EditorToolbar::GRP_GAP;

    static constexpr const char* TILE_ROOT = EditorPalette::TILE_ROOT;
    static constexpr const char* BG_ROOT   = EditorPalette::BG_ROOT;

    // --- Tool system ---
    ToolId                      mActiveToolId = ToolId::MoveCam;
    std::unique_ptr<EditorTool> mTool;

    // Build a fresh EditorToolContext from current state.
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

        // Reset gamepad repeat state when switching tools.
        mPadNavTimer     = 0.f;
        mPadNavRepeating = false;
        // Reset hitbox mode so the new tool starts in cursor mode.
        mPadHbMode       = 0;
        mPadHbNavTimer   = 0.f;
        mPadHbNavRepeat  = false;
        // Reset select A-held state.
        mPadSelAHeld     = false;
    }

    static TBBtn ToolIdToBtn(ToolId id) {
        switch (id) {
            case ToolId::Goal:        return TBBtn::Goal;
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
            case ToolId::Shield:      return TBBtn::Shield;
            case ToolId::Shooter:     return TBBtn::PowerUp; // merged into PowerUp
        }
        return TBBtn::COUNT;
    }

    // --- Editor state ---
    std::string mOpenPath;
    bool        mForceNew = false;
    std::string mPresetName;
    std::string mProfilePath;
    Window*     mWindow     = nullptr;
    bool        mLaunchGame = false;
    bool        mGoBack     = false;

    std::unique_ptr<Text> lblPalHeader;
    std::unique_ptr<Text> lblPalHint1;
    std::unique_ptr<Text> lblPalHint2;
    std::unique_ptr<Text> lblBgHeader;
    std::unique_ptr<Text> lblStatusBar;
    std::unique_ptr<Text> lblCamPos;
    std::unique_ptr<Text> lblBottomHint;
    std::unique_ptr<Text> lblToolPrefix;
    int         mLastTileCount  = -1;

    int         mLastEnemyCount = -1;
    int         mLastCamX       = INT_MIN;
    int         mLastCamY       = INT_MIN;
    int         mLastTileSizeW  = -1;
    std::string mLastPalHeaderPath;

    std::string          mStatusMsg       = "New level";
    std::string          mLevelName       = "level1";
    float mScrollAccum = 0.0f;

    EditorSurfaceCache mSurfaceCache;
    int mActionAnimDropHover = -1;

    EditorPopups mPopups;

    void OpenAnimPicker(int tileIdx);
    void CloseAnimPicker();

    EditorPopups::Ctx MakePopupCtx();
    bool ImportPath(const std::string& srcPath);

    EditorToolbar         mToolbar;
    EditorCanvasRenderer  mCanvasRenderer;
    EditorUIRenderer      mUIRenderer;

    std::unique_ptr<Text> lblStatus, lblTool;

    // Tightly coupled to Action tool hover rendering
    bool mDropActive = false;

    EditorCamera mCamera;

    Level mLevel;

    using PaletteItem = EditorPalette::PaletteItem;
    using BgItem      = EditorPalette::BgItem;
    EditorPalette mPalette;

    // --- Assets ---
    std::unique_ptr<Image>                  background;
    std::vector<std::unique_ptr<Image>>     mParallaxImages;
    void RebuildParallaxImages();
    std::unique_ptr<SpriteSheet> enemySheet;
    static SDL_Surface*          mFolderIcon;

    // Level stash — survives scene transitions so we skip re-parsing JSON.
    static Level       sLevelStash;
    static std::string sLevelStashName;
    static bool        sHasLevelStash;

    // Moving-platform placement state (popup state lives in mPopups)
    std::vector<int> mMovPlatIndices;
    int              mMovPlatNextGroupId = 1;
    int              mMovPlatCurGroupId  = 1;
    float            mMovPlatRange       = 96.0f;

    // Power-up registry — single source of truth shared with GameScene
    static const std::vector<EditorPopups::PowerUpEntry>& GetPowerUpRegistry();

    // --- Helpers ---
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

    int HitEnemy(int sx, int sy) const {
        auto [wx, wy] = ScreenToWorld(sx, sy);
        for (int i = 0; i < (int)mLevel.enemies.size(); i++) {
            const auto& en = mLevel.enemies[i];
            int ew = GRID, eh = GRID;
            if (!en.enemyType.empty()) {
                EnemyProfile prof;
                if (LoadEnemyProfile("enemies/" + en.enemyType + ".json", prof)) {
                    ew = (prof.spriteW > 0) ? prof.spriteW : GRID;
                    eh = (prof.spriteH > 0) ? prof.spriteH : GRID;
                }
            }
            SDL_Rect r = {(int)en.x, (int)en.y, ew, eh};
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

    // Tile tool helpers — delegate to TileTool if active.
    int  GetTileW() const;
    int  GetTileH() const;
    int  GetGhostRotation() const;

    // --- Compat shim: remaining state for inline tools not yet extracted ---

    // Generic entity drag (used by Action/PowerUp/MovingPlat inline tools)
    bool mIsDragging  = false;
    int  mDragIndex   = -1;
    bool mDragIsTile  = false;

    // Tile tool state (used by Render ghost preview, delegated to TileTool)
    int mTileW         = GRID;
    int mTileH         = GRID;
    int mGhostRotation = 0;

    SDL_Rect mMusicVolSlider{};
    bool     mMusicVolDragging = false;

    bool        mMusicConfirmActive = false;
    std::string mMusicConfirmNewPath;
    SDL_Rect    mMusicConfirmYes{};
    SDL_Rect    mMusicConfirmNo{};

    // Teleport linking: after placing a teleport entrance, the next click
    // on a tile marks it as the destination with the same group.
    bool mTeleportLinking      = false;
    int  mTeleportLinkGroup    = 0;
    int  mTeleportNextGroupId  = 1;

    // Activate a toolbar button by id — shared by mouse click and gamepad A press.
    void ActivateToolbarButton(TBBtn btn);

    // Apply power-up picker selection by entry index (for gamepad A press).
    // entryIdx maps to GetPowerUpRegistry(); reg.size() = "None" option.
    void ApplyPowerUpPickerEntry(int entryIdx);

    // Returns the ordered list of toolbar buttons that are currently on-screen
    // (i.e. their group is not collapsed).
    [[nodiscard]] std::vector<TBBtn> VisibleToolbarButtons() const {
        std::vector<TBBtn> out;
        for (const auto& meta : EditorToolbar::AllButtons()) {
            if (mToolbar.Rect(meta.id).x >= 0)
                out.push_back(meta.id);
        }
        return out;
    }

    // --- Gamepad two-mode state machine (tile tool) ---
    //
    //  PALETTE MODE  (mPadPaletteActive == true)
    //    Left stick  → navigate palette tiles (cardinal, key-repeat)
    //    A           → confirm tile, enter PLACEMENT MODE
    //
    //  PLACEMENT MODE (mPadPaletteActive == false)
    //    Left stick  → move virtual cursor / tile ghost around canvas
    //    Right stick → pan camera
    //    A           → place tile (hold = drag-fill)
    //    B or D-pad  → return to PALETTE MODE
    //
    SDL_Gamepad* mEditorPad        = nullptr;
    bool         mPadPaletteActive = true;  // start in palette mode
    int          mPadPaletteIdx    = 0;

    // Key-repeat state for palette navigation.
    float mPadNavTimer      = 0.f;
    bool  mPadNavRepeating  = false;

    // Virtual cursor for placement mode (pinned to canvas centre).
    float mPadCursorX    = 400.f;
    float mPadCursorY    = 300.f;
    bool  mPadAHeld      = false;
    float mPadSizeAccum  = 0.f;   // right-stick size accumulator

    // --- Gamepad toolbar focus (SELECT button) ---
    // Sentinel layout for mPadToolbarBtnIdx:
    //   0 … visible.size()-1  → tool buttons
    //   visible.size()        → group-collapse pills row  (LEFT/RIGHT picks Place/Mod/Actions)
    //   visible.size() + 1   → palette collapse tab
    bool  mPadToolbarActive      = false;
    int   mPadToolbarBtnIdx      = 0;
    int   mPadToolbarGrpIdx      = 0;   // 0=Place 1=Modifier 2=Actions — focused pill
    float mPadToolbarNavTimer    = 0.f;
    bool  mPadToolbarNavRepeat   = false;

    // --- Gamepad PowerUp tool state ---
    // Cursor position is shared via mPadCursorX / mPadCursorY (only one
    // tool active at a time, so these are safe to reuse).
    float mPadPuRateAccum       = 0.f;   // right-stick fire-rate accumulator
    int   mPadPuPickerIdx       = 0;     // highlighted row in the picker popup
    float mPadPuPickerNavTimer  = 0.f;
    bool  mPadPuPickerNavRepeat = false;

    // --- Gamepad Prop tool state ---
    bool  mPadPropFocusFront = true;  // which popup button is highlighted (Front vs Back)
    float mPadPropNavTimer   = 0.f;   // debounce for stick-driven Front/Back toggle

    // --- Gamepad Select tool state ---
    bool mPadSelAHeld = false;        // true while A is held (rubber-band OR drag-move)

    // --- Gamepad Erase tool state ---
    bool mPadEraseAHeld = false;      // true while A is held for continuous drag-erase

    // --- Gamepad Action tool state ---
    int   mPadActionPickerIdx    = 0;    // highlighted entry in the anim picker
    float mPadActionPickerTimer  = 0.f;  // key-repeat timer for picker D-pad nav
    bool  mPadActionPickerRepeat = false;
    float mPadActionHitsAccum    = 0.f;  // accumulator for right-stick hit count adjust

    // --- Gamepad Slope tool state ---
    float mPadSlopeHeightAccum   = 0.f;  // RS Y accumulator for heightFrac steps

    // --- Gamepad Hitbox tool state ---
    // Three modes: 0=cursor  1=node-select  2=node-edit
    int   mPadHbMode       = 0;
    int   mPadHbHandleIdx  = 0;    // 0-7 linear index (TopLeft…BotRight)
    float mPadHbNavTimer   = 0.f;
    bool  mPadHbNavRepeat  = false;
    float mPadHbEditAccumX = 0.f;
    float mPadHbEditAccumY = 0.f;
};
