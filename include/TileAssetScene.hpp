#pragma once
#include "Scene.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

class TitleScene;

class TileAssetScene : public Scene {
  public:
    void                   Load(Window& window) override;
    void                   Unload() override;
    bool                   HandleEvent(SDL_Event& e) override;
    void                   Update(float dt) override;
    void                   Render(Window& window, float alpha = 1.0f) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    static constexpr const char* TILE_ROOT = "game_assets/tiles";
    // Normalized at Load() time for consistent path comparisons
    std::string mTileRoot;
    static constexpr int THUMB_SZ  = 64;
    static constexpr int CELL_PAD  = 6;
    static constexpr int PANEL_PAD = 12;

    static constexpr SDL_Color BG        = {18, 20, 30, 255};
    static constexpr SDL_Color PANEL_BG  = {28, 32, 48, 255};
    static constexpr SDL_Color PANEL_OUT = {55, 60, 85, 255};
    static constexpr SDL_Color BTN_BACK  = {50, 50, 120, 255};
    static constexpr SDL_Color BTN_DEL   = {140, 40, 40, 255};
    static constexpr SDL_Color BTN_ADD   = {40, 120, 60, 255};
    static constexpr SDL_Color FOLDER_BG = {40, 55, 80, 255};
    static constexpr SDL_Color DROP_IDLE = {24, 36, 52, 255};
    static constexpr SDL_Color DROP_HOV  = {40, 70, 120, 255};
    static constexpr SDL_Color SEL_OUT   = {100, 200, 255, 255};

    struct TileEntry {
        std::string  path;
        std::string  name;
        SDL_Surface* thumb   = nullptr;
        bool         isDir   = false;
        SDL_Rect     rect    = {};
        SDL_Rect     delRect = {};
    };

    // Delete confirmation
    bool        mDelConfirm   = false;
    std::string mDelTarget;          // path to delete (supports multi-select)
    bool        mDelIsDir     = false;
    SDL_Rect    mDelYesRect   = {};
    SDL_Rect    mDelNoRect    = {};
    bool        mDelMulti     = false; // deleting entire selection

    // New folder naming
    bool        mNamingFolder = false;
    std::string mNewFolderName;

    // Selection + drag
    std::set<int>  mSelected;       // indices into mEntries
    bool           mDragging   = false;
    int            mDragStartX = 0;
    int            mDragStartY = 0;
    int            mDragCurX   = 0;
    int            mDragCurY   = 0;
    bool           mMarquee    = false; // rubber-band selection active
    SDL_Rect       mMarqueeRect = {};

    int  mW = 0, mH = 0;
    bool mGoBack    = false;
    bool mDropHover = false;
    std::string mStatusMsg;

    std::string              mCurrentDir;
    std::vector<TileEntry>   mEntries;
    int                      mScroll     = 0;
    int                      mMaxScroll  = 0;
    SDL_Window*              mSDLWin     = nullptr;

    SDL_Rect mContentPanel     = {};
    SDL_Rect mBackBtnRect      = {};
    SDL_Rect mUpBtnRect        = {};
    SDL_Rect mNewFolderBtnRect = {};
    SDL_Rect mDropZone         = {};
    SDL_Rect mNameFieldRect    = {};

    void scanDir(std::string dir);  // by value: caller's string may be destroyed inside
    void freeThumbs();
    SDL_Surface* makeThumbnail(const std::string& path, int sz);
    int  entryAt(int mx, int my) const;
    int  folderAt(int mx, int my) const; // returns index of folder entry at pos, or -1
    void moveSelectionToFolder(int folderIdx);

    static void fillRect(SDL_Surface* s, SDL_Rect r, SDL_Color c);
    static void outlineRect(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1);
    static void drawText(SDL_Surface* s, const std::string& str,
                         int x, int y, int ptSize, SDL_Color col = {255,255,255,255});
    static void drawTextCentered(SDL_Surface* s, const std::string& str,
                                 SDL_Rect r, int ptSize, SDL_Color col = {255,255,255,255});
    static bool hit(const SDL_Rect& r, int x, int y) {
        return r.w > 0 && r.h > 0 && x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    }
};
