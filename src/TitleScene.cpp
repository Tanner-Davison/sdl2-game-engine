#include "TitleScene.hpp"
#include "GameScene.hpp"
#include "LevelEditorScene.hpp"

std::unique_ptr<Scene> TitleScene::NextScene() {
    if (startGame) {
        startGame = false;
        return std::make_unique<GameScene>(mChosenLevel);
    }
    if (openEditor) {
        openEditor   = false;
        bool force   = mEditorForce;
        mEditorForce = false;
        std::string name = mEditorName;
        mEditorName.clear();
        return std::make_unique<LevelEditorScene>(mEditorPath, force, name);
    }
    return nullptr;
}
