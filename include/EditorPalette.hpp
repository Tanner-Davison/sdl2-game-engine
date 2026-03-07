#pragma once
// EditorPalette.hpp
// ---------------------------------------------------------------------------
// Owns the tile palette and background palette subsystems for the level
// editor: directory traversal, PNG/JSON loading, thumbnail creation, scroll
// state, selection indices, and the surface-cache seeding needed so tiles
// already placed in a level are always renderable.
//
// Extracted from LevelEditorScene as part of the modular refactor (Prompt #3).
// The orchestrator (LevelEditorScene) owns an EditorPalette instance and
// delegates all palette queries and mutations to it.
// ---------------------------------------------------------------------------

#include <SDL3/SDL.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// Forward declarations — avoid pulling heavy headers into every TU
class EditorSurfaceCache;
class Image;
struct Level;

// Text must be fully defined here because unique_ptr<Text> members are
// destroyed inline (via vector::clear / destructor), which requires sizeof(Text).
#include "Text.hpp"

class EditorPalette {
  public:
    // ── Layout constants (mirrored from the editor for shared use) ──────────
    static constexpr int PALETTE_W     = 180;
    static constexpr int PALETTE_TAB_W = 18;
    static constexpr int PAL_ICON      = 76;
    static constexpr int PAL_COLS      = 2;
    static constexpr int TAB_H         = 28;

    // Root directories for tile and background assets
    static constexpr const char* TILE_ROOT = "game_assets/tiles";
    static constexpr const char* BG_ROOT   = "game_assets/backgrounds";

    // ── Palette item structs ────────────────────────────────────────────────
    struct PaletteItem {
        std::string  path;
        std::string  label;
        SDL_Surface* thumb    = nullptr;
        SDL_Surface* full     = nullptr;
        bool         isFolder = false;
        SDL_Rect     delBtn   = {-1, -1, 0, 0}; // computed each frame in Render
    };

    struct BgItem {
        std::string  path;
        std::string  label;
        SDL_Surface* thumb  = nullptr;
        SDL_Rect     delBtn = {-1, -1, 0, 0};
    };

    // Which tab is active in the palette panel
    enum class Tab { Tiles, Backgrounds };

    // ── Construction / destruction ──────────────────────────────────────────
    EditorPalette() = default;
    ~EditorPalette();

    // Non-copyable, movable
    EditorPalette(const EditorPalette&)            = delete;
    EditorPalette& operator=(const EditorPalette&) = delete;
    EditorPalette(EditorPalette&&)                 = default;
    EditorPalette& operator=(EditorPalette&&)      = default;

    // ── Initialization ──────────────────────────────────────────────────────
    // Must be called once during Load() after the folder icon has been created.
    // cache:      reference to the shared surface cache (for seeding tile lookups)
    // folderIcon: a shared, non-owning folder icon surface (lifetime managed elsewhere)
    void Init(EditorSurfaceCache& cache, SDL_Surface* folderIcon);

    // ── Tile palette ────────────────────────────────────────────────────────

    // Reload the tile palette from the given directory.
    // Seeds the surface cache for tiles already in the level that reference
    // images from directories not currently visible in the palette.
    void LoadTileView(const std::string& dir, const Level& level);

    // Current tile-palette directory being displayed.
    [[nodiscard]] const std::string& CurrentDir() const { return mTileCurrentDir; }

    // Full read/write access to the items vector (rendering, event handling).
    [[nodiscard]] std::vector<PaletteItem>&       Items() { return mPaletteItems; }
    [[nodiscard]] const std::vector<PaletteItem>& Items() const { return mPaletteItems; }

    // Selected tile index within the current palette view.
    [[nodiscard]] int  SelectedTile() const { return mSelectedTile; }
    void               SetSelectedTile(int idx) { mSelectedTile = idx; }

    // Scroll offset (in pixels) for the tile palette.
    [[nodiscard]] int  TileScroll() const { return mPaletteScroll; }
    void               SetTileScroll(int v) { mPaletteScroll = v; }
    void               AdjustTileScroll(int delta) { mPaletteScroll += delta; }

    // Returns the currently selected PaletteItem, or nullptr if out of bounds.
    [[nodiscard]] const PaletteItem* SelectedItem() const;

    // ── Background palette ──────────────────────────────────────────────────

    // Scan BG_ROOT and build the background palette list. If the level already
    // has a background set, automatically selects the matching entry.
    void LoadBgPalette(const Level& level);

    // Apply the background at the given index: sets the level's background
    // path and rebuilds the Image object via the provided callback.
    // The callback signature is: void(const std::string& bgPath)
    // This avoids EditorPalette needing to know about Image or Window.
    using ApplyBgCallback = std::function<void(const std::string& bgPath)>;
    void ApplyBackground(int idx, Level& level, const ApplyBgCallback& onApply);

    [[nodiscard]] std::vector<BgItem>&       BgItems() { return mBgItems; }
    [[nodiscard]] const std::vector<BgItem>& BgItems() const { return mBgItems; }

    [[nodiscard]] int  SelectedBg() const { return mSelectedBg; }
    void               SetSelectedBg(int idx) { mSelectedBg = idx; }

    [[nodiscard]] int  BgScroll() const { return mBgPaletteScroll; }
    void               SetBgScroll(int v) { mBgPaletteScroll = v; }
    void               AdjustBgScroll(int delta) { mBgPaletteScroll += delta; }

    // ── Tab state ───────────────────────────────────────────────────────────
    [[nodiscard]] Tab  ActiveTab() const { return mActiveTab; }
    void               SetActiveTab(Tab t) { mActiveTab = t; }

    // ── Collapse state ──────────────────────────────────────────────────────
    [[nodiscard]] bool IsCollapsed() const { return mCollapsed; }
    void               SetCollapsed(bool v) { mCollapsed = v; }
    void               ToggleCollapsed() { mCollapsed = !mCollapsed; }

    // ── Double-click detection ──────────────────────────────────────────────
    // Returns true if this click constitutes a double-click on the same index.
    bool CheckDoubleClick(int index);

    // ── Cell labels ─────────────────────────────────────────────────────────
    // Pre-rendered text labels for palette cells. Rebuilt when the palette
    // changes to avoid per-frame text construction.
    [[nodiscard]] std::vector<std::unique_ptr<Text>>& CellLabels() { return mCellLabels; }
    void ClearCellLabels() { mCellLabels.clear(); }

    // ── Bulk cleanup ────────────────────────────────────────────────────────
    // Frees all owned surfaces. Called by LevelEditorScene::Unload().
    void Clear();

  private:
    // ── References (non-owning, set via Init) ───────────────────────────────
    EditorSurfaceCache* mCache      = nullptr;
    SDL_Surface*        mFolderIcon = nullptr;

    // ── Tile palette state ──────────────────────────────────────────────────
    std::vector<PaletteItem> mPaletteItems;
    std::string              mTileCurrentDir;
    int                      mSelectedTile  = 0;
    int                      mPaletteScroll = 0;

    // ── Background palette state ────────────────────────────────────────────
    std::vector<BgItem> mBgItems;
    int                 mSelectedBg      = 0;
    int                 mBgPaletteScroll = 0;

    // ── UI state ────────────────────────────────────────────────────────────
    Tab  mActiveTab = Tab::Tiles;
    bool mCollapsed = false;

    // ── Double-click detection ──────────────────────────────────────────────
    Uint64                  mLastClickTime  = 0;
    int                     mLastClickIndex = -1;
    static constexpr Uint64 DOUBLE_CLICK_MS = 400;

    // ── Cell labels ─────────────────────────────────────────────────────────
    std::vector<std::unique_ptr<Text>> mCellLabels;

    // ── Internal helpers ────────────────────────────────────────────────────
    void FreeTileItems();
    void FreeBgItems();
    void SeedCacheForLevel(const Level& level);
    void SeedAnimatedTile(const std::string& path, int w, int h);
    void SeedStaticTile(const std::string& path, int w, int h);
};
