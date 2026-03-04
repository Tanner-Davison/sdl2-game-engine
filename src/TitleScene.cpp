#include "TitleScene.hpp"
#include "GameScene.hpp"
#include "LevelEditorScene.hpp"
#include "PlayerCreatorScene.hpp"
#include "TileAnimCreatorScene.hpp"

std::unique_ptr<Scene> TitleScene::NextScene() {
    if (startGame) {
        startGame = false;
        return std::make_unique<GameScene>(mChosenLevel, false, mChosenProfile);
    }
    if (openEditor) {
        openEditor   = false;
        bool force   = mEditorForce;
        mEditorForce = false;
        std::string name = mEditorName;
        mEditorName.clear();
        return std::make_unique<LevelEditorScene>(mEditorPath, force, name, mChosenProfile);
    }
    if (openPlayerCreator) {
        openPlayerCreator = false;
        return std::make_unique<PlayerCreatorScene>();
    }
    if (openTileAnimCreator) {
        openTileAnimCreator = false;
        return std::make_unique<TileAnimCreatorScene>();
    }
    return nullptr;
}
