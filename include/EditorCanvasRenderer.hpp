#pragma once
// EditorCanvasRenderer.hpp
//
// Renders the canvas region of the level editor:
//   - Background (delegated to Image)
//   - Grid lines
//   - Placed tiles (with rotation, color-mod, badges, slope lines, overlays)
//   - Moving-platform overlay (paths, ghost tiles, config popup)
//   - Enemies / player marker
//   - Tool overlay dispatch (SelectTool marquee, ResizeTool/HitboxTool handles)
//   - Tile ghost (placement preview under cursor)
//
// Receives non-owning references to every shared object it needs so it
// remains dependency-free from LevelEditorScene directly.

#include "EditorCamera.hpp"
#include "EditorPalette.hpp"
#include "EditorSurfaceCache.hpp"
#include "Image.hpp"
#include "LevelData.hpp"
#include "SpriteSheet.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include "tools/EditorTool.hpp"
#include "tools/EditorToolContext.hpp"
#include "tools/EditorTools.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <string>
#include <vector>

class EditorCanvasRenderer {
  public:
    // All fields are non-owning references / pointers set before each Render call.
    // LevelEditorScene owns the actual objects.

    struct MovPlatState {
        const std::vector<int>* indices = nullptr;
        int   curGroupId  = 1;
        bool  horiz       = true;
        float range       = 96.0f;
        float speed       = 60.0f;
        bool  loop        = false;
        bool  trigger     = false;
        bool  popupOpen   = false;
        bool  speedInput  = false;
        std::string speedStr;
        SDL_Rect    popupRect{};
    };

    // ── Call once per frame from LevelEditorScene::Render ───────────────────
    void Render(Window&              window,
                SDL_Surface*         screen,
                int                  canvasW,
                int                  toolbarH,
                int                  grid,
                const Level&         level,
                const EditorCamera&  camera,
                EditorSurfaceCache&  cache,
                const EditorPalette& palette,
                Image*               background,
                SpriteSheet*         enemySheet,
                ToolId               activeToolId,
                EditorTool*          activeTool,
                EditorToolContext    toolCtx,
                int                  actionAnimDropHover,
                const MovPlatState&  movPlat,
                const std::vector<std::unique_ptr<Image>>* parallaxImages = nullptr,
                const std::vector<float>* parallaxFactors = nullptr);

    // Returns the updated popup rect (computed each frame when popup is open)
    [[nodiscard]] SDL_Rect MovPlatPopupRect() const { return mMovPlatPopupRect; }

  private:
    // ── Internal helpers ─────────────────────────────────────────────────────
    static void DrawRect(SDL_Surface* s, SDL_Rect r, SDL_Color c);
    static void DrawRectAlpha(SDL_Surface* s, SDL_Rect r, SDL_Color c);
    static void DrawOutline(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1);

    void BlitBadge(SDL_Surface* screen, SDL_Surface* badge, int bx, int by);
    SDL_Surface* Badge(EditorSurfaceCache& cache, const std::string& text, SDL_Color col);

    void RenderGrid(SDL_Surface* screen, int canvasW, int toolbarH, int winH,
                    const EditorCamera& cam, int grid);

    void RenderTiles(SDL_Surface* screen, int canvasW, int toolbarH, int winH,
                     const Level& level, const EditorCamera& cam,
                     EditorSurfaceCache& cache, ToolId activeToolId,
                     int actionAnimDropHover);

    void RenderMovingPlatOverlay(SDL_Surface* screen, int canvasW, int toolbarH,
                                 const Level& level, const EditorCamera& cam,
                                 EditorSurfaceCache& cache, int grid,
                                 const MovPlatState& mp);

    void RenderMovPlatPopup(SDL_Surface* screen, int canvasW, int toolbarH,
                            EditorSurfaceCache& cache, const MovPlatState& mp);

    void RenderEntities(SDL_Surface* screen, int canvasW, int toolbarH, int winH,
                        const Level& level, const EditorCamera& cam,
                        EditorSurfaceCache& cache,
                        SpriteSheet* enemies, int grid);

    void RenderPlayerMarker(SDL_Surface* screen, const Level& level,
                            const EditorCamera& cam);

    void RenderGhost(SDL_Surface* screen, int canvasW, int toolbarH,
                     const EditorCamera& cam, const EditorPalette& palette,
                     EditorSurfaceCache& cache, ToolId activeToolId,
                     EditorTool* activeTool, int grid);

    SDL_Rect mMovPlatPopupRect{};
};
