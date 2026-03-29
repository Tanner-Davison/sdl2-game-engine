#pragma once
// TileAnimCreatorScene.hpp — Create/save animated tile definitions.
// Drop a folder or individual PNGs, preview, save to animated_tiles/<name>.json.

#include "AnimatedTile.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

class TileAnimCreatorScene : public Scene {
  public:
    void                   Load(Window& window) override;
    void                   Unload() override;
    bool                   HandleEvent(SDL_Event& e) override;
    void                   Update(float dt) override;
    void                   Render(Window& window, float alpha = 1.0f) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    static constexpr int   PAD         = 14;
    static constexpr int   PREVIEW_SZ  = 280;  // square preview area
    static constexpr int   FRAME_ROW_H = 52;
    static constexpr float DEFAULT_FPS = 8.0f;

    int         mW = 0, mH = 0;
    SDL_Window* mSDLWin = nullptr;
    bool        mGoBack = false;

    AnimatedTileDef              mDef;
    std::vector<SDL_Surface*>    mFrameSurfs; // parallel to mDef.framePaths, owned here

    bool        mNameActive = false;
    std::string mSaveMsg;   // feedback after save

    float mAnimTimer  = 0.0f;
    int   mAnimFrame  = 0;
    bool  mPaused     = false;

    bool        mFpsActive   = false;
    std::string mFpsEditStr; // string buffer while editing

    bool        mDropHover     = false;  // file hovering over drop zone
    bool        mFolderHover   = false;  // file hovering over folder-drop zone
    std::string mDropMsg;

    int  mSelectedFrame  = -1;  // highlighted frame in the list
    int  mFrameScroll    = 0;   // scroll offset for the frame list

    struct RosterEntry {
        std::string name;
        std::string path;
        SDL_Rect    loadRect{};
        SDL_Rect    delRect{};
    };
    std::vector<RosterEntry> mRoster;
    int                      mRosterScroll = 0;
    void refreshRoster();
    void loadRosterEntry(int idx);

    SDL_Rect mFramePanel{};    // left — frame list
    SDL_Rect mCenterPanel{};   // middle — preview + controls
    SDL_Rect mRosterPanel{};   // right — saved animations

    SDL_Rect mNameFieldRect{};
    SDL_Rect mFpsFieldRect{};
    SDL_Rect mPreviewRect{};
    SDL_Rect mDropZoneSingle{};   // "Add one frame" drop zone
    SDL_Rect mDropZoneFolder{};   // "Drop whole folder" drop zone
    SDL_Rect mSaveBtnRect{};
    SDL_Rect mClearBtnRect{};
    SDL_Rect mBackBtnRect{};
    SDL_Rect mPauseBtn{};
    SDL_Rect mMoveUpBtn{};
    SDL_Rect mMoveDownBtn{};
    SDL_Rect mDeleteFrameBtn{};

    void computeLayout();

    void importFolder(const std::string& dir);
    void importSingleFrame(const std::string& path);
    void sortFramesNumerically();

    void clearFrames();

    void startNameInput();
    void stopNameInput();
    void startFpsInput();
    void stopFpsInput();

    static void fillRect(SDL_Surface* s, SDL_Rect r, SDL_Color c);
    static void outlineRect(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1);
    static void drawText(SDL_Surface* s, const std::string& str,
                         int x, int y, int ptSize,
                         SDL_Color col = {220, 220, 220, 255});
    static void drawTextCentered(SDL_Surface* s, const std::string& str,
                                 SDL_Rect r, int ptSize,
                                 SDL_Color col = {220, 220, 220, 255});
    static void blitScaled(SDL_Surface* dst, SDL_Surface* src, SDL_Rect dstRect);

    static constexpr SDL_Color BG         = {18,  20,  30,  255};
    static constexpr SDL_Color PANEL_BG   = {28,  32,  50,  255};
    static constexpr SDL_Color PANEL_OUT  = {60,  70, 110,  255};
    static constexpr SDL_Color SEL_BG     = {50,  80, 160,  255};
    static constexpr SDL_Color DROP_IDLE  = {35,  45,  80,  255};
    static constexpr SDL_Color DROP_HOV   = {50, 100, 200,  255};
    static constexpr SDL_Color BTN_SAVE   = {40, 160,  80,  255};
    static constexpr SDL_Color BTN_CLEAR  = {140, 60,  60,  255};
    static constexpr SDL_Color BTN_BACK   = {80,  80, 160,  255};
    static constexpr SDL_Color BTN_DEL    = {160, 50,  50,  255};
    static constexpr SDL_Color BTN_LOAD   = {40, 100, 180,  255};
    static constexpr SDL_Color BTN_MOVE   = {60,  90, 140,  255};
    static constexpr SDL_Color ACCENT     = {255, 160,  40,  255}; // animated tile orange
};
