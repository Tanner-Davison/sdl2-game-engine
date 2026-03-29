#pragma once
// EditorFileOps.hpp — file I/O for the level editor (ImportPath).
// Pure-static helper; operates on data via the Ctx struct.
//
// SDL drop-event handling stays in LevelEditorScene::HandleEvent because it's
// coupled to the active tool state and mDropActive/mActionAnimDropHover.

#include "EditorPalette.hpp"
#include "EditorSurfaceCache.hpp"
#include "LevelData.hpp"
#include <functional>
#include <string>

class EditorFileOps {
  public:
    struct Ctx {
        EditorPalette&      palette;
        EditorSurfaceCache& cache;
        Level&              level;

        std::function<void(const std::string&)> setStatus;
        std::function<void()>                   refreshTileView;
        std::function<void()>                   refreshBgPalette;
        std::function<void(int)>                applyBackground;
        std::function<void()>                   switchToTileTool;

        int         palIcon  = 40;
        int         palCols  = 5;
        int         palW     = 220;
        const char* tileRoot = "game_assets/tiles";
        const char* bgRoot   = "game_assets/backgrounds";
    };

    static bool ImportPath(const std::string& srcPath, Ctx& ctx);
};
