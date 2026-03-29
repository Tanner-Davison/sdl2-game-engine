#pragma once
// EditorUIRenderer.hpp
//
// Renders all UI chrome that sits on top of the canvas:
//   - Toolbar (buttons, group collapse pills, dividers)
//   - Status bar + active-tool label
//   - Palette panel (tile grid, bg list, scroll indicators, headers)
//   - Bottom hint bar
//   - Destroy-anim picker popup
//   - PowerUp picker popup
//   - Import input bar
//   - Drop overlay
//   - Delete confirmation popup
//
// Like EditorCanvasRenderer, this class holds no state of its own beyond
// cached surface pointers that are rebuilt when inputs change.

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
    // ── Anim-picker entry (mirrors LevelEditorScene::AnimPickerEntry) ────────
    struct AnimPickerEntry {
        std::string  path;
        std::string  name;
        SDL_Surface* thumb = nullptr;
    };

    // ── PowerUp registry entry ───────────────────────────────────────────────
    struct PowerUpEntry {
        std::string id;
        std::string label;
        float       defaultDuration = 15.0f;
    };

    // ── PowerUp popup state ──────────────────────────────────────────────────
    struct PowerUpPopupState {
        bool     open    = false;
        int      tileIdx = -1;
        SDL_Rect rect{};
        const std::vector<PowerUpEntry>* registry = nullptr;
    };

    // ── Delete confirm popup state ───────────────────────────────────────────
    struct DelConfirmState {
        bool        active   = false;
        bool        isDir    = false;
        std::string name;
        SDL_Rect    yesRect{};
        SDL_Rect    noRect{};
    };

    // ── Music change confirm popup state ──────────────────────────────────────
    struct MusicConfirmState {
        bool        active  = false;
        std::string oldName;   // current music filename (or "(none)")
        std::string newName;   // proposed music filename
    };

    // ── Import input state ───────────────────────────────────────────────────
    struct ImportInputState {
        bool        active = false;
        std::string text;
    };

    // ── Moving-plat popup state (mirrors what CanvasRenderer computes) ───────
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

    // ── Main entry point ─────────────────────────────────────────────────────
    // LevelEditorScene calls this after EditorCanvasRenderer::Render.
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
                // anim picker
                int                           animPickerTile,
                const std::vector<AnimPickerEntry>& animPickerEntries,
                SDL_Rect                      animPickerRect,
                // popups
                const PowerUpPopupState&      powerUp,
                const DelConfirmState&        delConfirm,
                const MusicConfirmState&     musicConfirm,
                const ImportInputState&       importInput,
                const MovPlatPopupState&      movPlat,
                bool                          dropActive,
                // cached label pointers (rebuilt by orchestrator when stale)
                std::unique_ptr<Text>&        lblPalHeader,
                std::unique_ptr<Text>&        lblPalHint1,
                std::unique_ptr<Text>&        lblPalHint2,
                std::unique_ptr<Text>&        lblBgHeader,
                std::unique_ptr<Text>&        lblStatusBar,
                std::unique_ptr<Text>&        lblCamPos,
                std::unique_ptr<Text>&        lblBottomHint,
                // dirty flags (updated by Render when stale)
                int&  lastTileCount,
                int&  lastEnemyCount,
                int&  lastCamX,
                int&  lastCamY,
                int&  lastTileSizeW,
                std::string& lastPalHeaderPath,
                // tile-size query (supplied by orchestrator)
                int                           curTileW);

    // Computed Yes/No button rects (written each frame so HandleEvent can use them)
    [[nodiscard]] SDL_Rect DelConfirmYesRect() const { return mDelYes; }
    [[nodiscard]] SDL_Rect DelConfirmNoRect()  const { return mDelNo;  }
    [[nodiscard]] SDL_Rect MusicConfirmYesRect() const { return mMusicYes; }
    [[nodiscard]] SDL_Rect MusicConfirmNoRect()  const { return mMusicNo;  }
    // Updated anim picker rect (computed each frame)
    [[nodiscard]] SDL_Rect AnimPickerRect() const { return mAnimPickerRect; }
    [[nodiscard]] SDL_Rect CamShakeToggleRect() const { return mCamShakeToggleRect; }

  private:
    // ── Internal helpers ─────────────────────────────────────────────────────
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

    // Mutable state written each frame so the orchestrator can read it back
    SDL_Rect mDelYes{};
    SDL_Rect mDelNo{};
    SDL_Rect mMusicYes{};
    SDL_Rect mMusicNo{};
    SDL_Rect mAnimPickerRect{};
    SDL_Rect mCamShakeToggleRect{};
};
