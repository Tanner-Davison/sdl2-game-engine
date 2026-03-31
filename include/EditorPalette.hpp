#pragma once
// EditorPalette.hpp — tile palette and background palette subsystems:
// directory traversal, PNG/JSON loading, thumbnails, scroll state, selection,
// and surface-cache seeding for tiles already placed in a level.

#include <SDL3/SDL.h>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class EditorSurfaceCache;
class Image;
struct Level;

// unique_ptr<Text> members are destroyed inline, which requires sizeof(Text).
#include "Text.hpp"

class EditorPalette {
  public:
    static constexpr int PALETTE_W     = 180;
    static constexpr int PALETTE_TAB_W = 18;
    static constexpr int PAL_ICON      = 76;
    static constexpr int PAL_COLS      = 2;
    static constexpr int TAB_H         = 28;

    static constexpr const char* TILE_ROOT = "game_assets/tiles";
    static constexpr const char* BG_ROOT   = "game_assets/backgrounds";

    struct PaletteItem {
        std::string  path;
        std::string  label;
        SDL_Surface* thumb    = nullptr;
        SDL_Surface* full     = nullptr;
        bool         isFolder = false;
        SDL_Rect     delBtn   = {-1, -1, 0, 0};
    };

    struct BgItem {
        std::string  path;
        std::string  label;
        SDL_Surface* thumb  = nullptr;
        SDL_Rect     delBtn = {-1, -1, 0, 0};
    };

    enum class Tab { Tiles, Backgrounds };

    EditorPalette() = default;
    ~EditorPalette();

    EditorPalette(const EditorPalette&)            = delete;
    EditorPalette& operator=(const EditorPalette&) = delete;
    EditorPalette(EditorPalette&&)                 = default;
    EditorPalette& operator=(EditorPalette&&)      = default;

    // Must be called once during Load() after the folder icon has been created.
    void Init(EditorSurfaceCache& cache, SDL_Surface* folderIcon);

    // Seeds the surface cache for tiles in the level from directories not visible.
    void LoadTileView(const std::string& dir, const Level& level);

    [[nodiscard]] const std::string& CurrentDir() const { return mTileCurrentDir; }

    [[nodiscard]] std::vector<PaletteItem>&       Items() { return mPaletteItems; }
    [[nodiscard]] const std::vector<PaletteItem>& Items() const { return mPaletteItems; }

    [[nodiscard]] int  SelectedTile() const { return mSelectedTile; }
    void               SetSelectedTile(int idx) { mSelectedTile = idx; }

    [[nodiscard]] int  TileScroll() const { return mPaletteScroll; }
    void               SetTileScroll(int v) { mPaletteScroll = v; }
    void               AdjustTileScroll(int delta) { mPaletteScroll += delta; }

    [[nodiscard]] const PaletteItem* SelectedItem() const;

    // Auto-selects the matching entry if the level already has a background.
    void LoadBgPalette(const Level& level);

    using ApplyBgCallback = std::function<void(const std::string& bgPath)>;
    void ApplyBackground(int idx, Level& level, const ApplyBgCallback& onApply);

    [[nodiscard]] std::vector<BgItem>&       BgItems() { return mBgItems; }
    [[nodiscard]] const std::vector<BgItem>& BgItems() const { return mBgItems; }

    [[nodiscard]] int  SelectedBg() const { return mSelectedBg; }
    void               SetSelectedBg(int idx) { mSelectedBg = idx; }

    [[nodiscard]] int  BgScroll() const { return mBgPaletteScroll; }
    void               SetBgScroll(int v) { mBgPaletteScroll = v; }
    void               AdjustBgScroll(int delta) { mBgPaletteScroll += delta; }

    [[nodiscard]] Tab  ActiveTab() const { return mActiveTab; }
    void               SetActiveTab(Tab t) { mActiveTab = t; }

    [[nodiscard]] bool IsCollapsed() const { return mCollapsed; }
    void               SetCollapsed(bool v) { mCollapsed = v; }
    void               ToggleCollapsed() { mCollapsed = !mCollapsed; }

    bool CheckDoubleClick(int index);

    [[nodiscard]] std::vector<std::unique_ptr<Text>>& CellLabels() { return mCellLabels; }
    void ClearCellLabels() { mCellLabels.clear(); }

    void Clear();

    void StashTileItems();
    bool RestoreTileItems(const std::string& dir, const Level& level);

  private:
    EditorSurfaceCache* mCache      = nullptr;
    SDL_Surface*        mFolderIcon = nullptr;

    std::vector<PaletteItem> mPaletteItems;
    std::string              mTileCurrentDir;
    int                      mSelectedTile  = 0;
    int                      mPaletteScroll = 0;

    std::vector<BgItem> mBgItems;
    int                 mSelectedBg      = 0;
    int                 mBgPaletteScroll = 0;

    Tab  mActiveTab = Tab::Tiles;
    bool mCollapsed = false;

    Uint64                  mLastClickTime  = 0;
    int                     mLastClickIndex = -1;
    static constexpr Uint64 DOUBLE_CLICK_MS = 400;

    std::vector<std::unique_ptr<Text>> mCellLabels;

    void FreeTileItems();
    void FreeBgItems();
    void SeedCacheForLevel(const Level& level);
    void SeedAnimatedTile(const std::string& path, int w, int h);
    void SeedStaticTile(const std::string& path, int w, int h);

    static std::vector<PaletteItem> sStashedItems;
    static std::string              sStashedDir;
    static int                      sStashedSelectedTile;
    static int                      sStashedScroll;
};
