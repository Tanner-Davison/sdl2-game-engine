#pragma once
// EditorUIRenderer.hpp — renders all UI chrome on top of the canvas: toolbar,
// status bar, palette panel, bottom hint bar, and modal popups.
// Holds no state beyond cached rects written each frame.

#include "EditorCamera.hpp"
#include "EditorPalette.hpp"
#include "EditorSurfaceCache.hpp"
#include "EditorToolbar.hpp"
#include "LevelData.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include "tools/EditorTools.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include <vector>

class EditorUIRenderer {
  public:
    struct AnimPickerEntry {
        std::string  path;
        std::string  name;
        SDL_Surface* thumb = nullptr;
    };

    struct PowerUpEntry {
        std::string id;
        std::string label;
        float       defaultDuration = 15.0f;
    };

    struct PowerUpPopupState {
        bool     open    = false;
        int      tileIdx = -1;
        SDL_Rect rect{};
        const std::vector<PowerUpEntry>* registry = nullptr;
    };

    struct DelConfirmState {
        bool        active   = false;
        bool        isDir    = false;
        std::string name;
        SDL_Rect    yesRect{};
        SDL_Rect    noRect{};
    };

    struct MusicConfirmState {
        bool        active  = false;
        std::string oldName;
        std::string newName;
    };

    struct ImportInputState {
        bool        active = false;
        std::string text;
    };

    struct MovPlatPopupState {
        bool        open       = false;
        bool        speedInput = false;
        std::string speedStr;
        float       speed      = 60.0f;
        bool        horiz      = true;
        bool        loop       = false;
        bool        trigger    = false;
        int         curGroupId = 1;
        SDL_Rect    rect{};
    };

    void Render(Window&                       window,
                SDL_Surface*                  screen,
                int                           canvasW,
                int                           toolbarH,
                int                           grid,
                ToolId                        activeToolId,
                const EditorCamera&           camera,
                const Level&                  level,
                EditorSurfaceCache&           cache,
                EditorToolbar&                toolbar,
                const EditorPalette&          palette,
                Text*                         lblStatus,
                Text*                         lblTool,
                Text*                         lblToolPrefix,
                int                           animPickerTile,
                const std::vector<AnimPickerEntry>& animPickerEntries,
                SDL_Rect                      animPickerRect,
                const PowerUpPopupState&      powerUp,
                const DelConfirmState&        delConfirm,
                const MusicConfirmState&     musicConfirm,
                const ImportInputState&       importInput,
                const MovPlatPopupState&      movPlat,
                bool                          dropActive,
                std::unique_ptr<Text>&        lblPalHeader,
                std::unique_ptr<Text>&        lblPalHint1,
                std::unique_ptr<Text>&        lblPalHint2,
                std::unique_ptr<Text>&        lblBgHeader,
                std::unique_ptr<Text>&        lblStatusBar,
                std::unique_ptr<Text>&        lblCamPos,
                std::unique_ptr<Text>&        lblBottomHint,
                int&  lastTileCount,
                int&  lastEnemyCount,
                int&  lastCamX,
                int&  lastCamY,
                int&  lastTileSizeW,
                std::string& lastPalHeaderPath,
                int                           curTileW);

    [[nodiscard]] SDL_Rect DelConfirmYesRect() const { return mDelYes; }
    [[nodiscard]] SDL_Rect DelConfirmNoRect()  const { return mDelNo;  }
    [[nodiscard]] SDL_Rect MusicConfirmYesRect() const { return mMusicYes; }
    [[nodiscard]] SDL_Rect MusicConfirmNoRect()  const { return mMusicNo;  }
    [[nodiscard]] SDL_Rect AnimPickerRect() const { return mAnimPickerRect; }
    [[nodiscard]] SDL_Rect CamShakeToggleRect() const { return mCamShakeToggleRect; }

  private:
    static void DrawRect(SDL_Surface* s, SDL_Rect r, SDL_Color c);
    static void DrawRectAlpha(SDL_Surface* s, SDL_Rect r, SDL_Color c);
    static void DrawOutline(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1);
    void        BlitBadge(SDL_Surface* screen, SDL_Surface* badge, int bx, int by);
    SDL_Surface* Badge(EditorSurfaceCache& cache, const std::string& text, SDL_Color col);

    void RenderToolbar(SDL_Surface* screen, int winW, int toolbarH,
                       ToolId activeToolId, EditorToolbar& toolbar,
                       const Level& level, EditorSurfaceCache& cache,
                       Text* lblStatus, Text* lblTool, Text* lblToolPrefix,
                       int canvasW);

    void RenderPalettePanel(SDL_Surface* screen, Window& window, int canvasW,
                            int toolbarH, int grid,
                            const EditorPalette& palette, const Level& level,
                            EditorSurfaceCache& cache, ToolId activeToolId,
                            std::unique_ptr<Text>& lblPalHeader,
                            std::unique_ptr<Text>& lblPalHint1,
                            std::unique_ptr<Text>& lblPalHint2,
                            std::unique_ptr<Text>& lblBgHeader,
                            int& lastTileSizeW,
                            std::string& lastPalHeaderPath,
                            int curTileW);

    void RenderBottomBar(SDL_Surface* screen, Window& window, int canvasW,
                         const Level& level, const EditorCamera& camera,
                         ToolId activeToolId,
                         EditorSurfaceCache& cache,
                         std::unique_ptr<Text>& lblStatusBar,
                         std::unique_ptr<Text>& lblCamPos,
                         std::unique_ptr<Text>& lblBottomHint,
                         int& lastTileCount, int& lastEnemyCount,
                         int& lastCamX, int& lastCamY);

    void RenderAnimPicker(SDL_Surface* screen, int canvasW, int toolbarH, int winH,
                          const Level& level, const EditorCamera& cam,
                          int animPickerTile,
                          const std::vector<AnimPickerEntry>& entries,
                          EditorSurfaceCache& cache);

    void RenderPowerUpPopup(SDL_Surface* screen, const Level& level,
                            const PowerUpPopupState& pu,
                            EditorSurfaceCache& cache);

    void RenderImportInput(SDL_Surface* screen, int canvasW, int winH,
                           const EditorPalette& palette,
                           const ImportInputState& imp);

    void RenderDropOverlay(SDL_Surface* screen, int canvasW, int toolbarH,
                           int winH, ToolId activeToolId,
                           const EditorPalette& palette,
                           EditorSurfaceCache& cache);

    void RenderDelConfirm(SDL_Surface* screen, int W, int H,
                          const DelConfirmState& dc,
                          EditorSurfaceCache& cache,
                          const SDL_PixelFormatDetails* fmt);

    void RenderMusicConfirm(SDL_Surface* screen, int W, int H,
                            const MusicConfirmState& mc,
                            EditorSurfaceCache& cache);

    SDL_Rect mDelYes{};
    SDL_Rect mDelNo{};
    SDL_Rect mMusicYes{};
    SDL_Rect mMusicNo{};
    SDL_Rect mAnimPickerRect{};
    SDL_Rect mCamShakeToggleRect{};
};
