#pragma once
#include "Components.hpp"
#include "Image.hpp"
#include "Level.hpp"
#include "LevelSerializer.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SpriteSheet.hpp"
#include "GameConfig.hpp"
#include "Systems.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
// GameScene — Level 1
//
// Owns the EnTT registry and orchestrates all systems for a single game level.
// Implementation lives in GameScene.cpp to keep compile times manageable.
// ─────────────────────────────────────────────────────────────────────────────
class GameScene : public Scene {
  public:
    // Default constructor — uses hardcoded / random level data
    GameScene() = default;

    // Load from a saved level file (produced by LevelEditorScene).
    // fromEditor=true means ESC pause menu offers "Back to Editor" instead of "Back to Title".
    explicit GameScene(const std::string& levelPath, bool fromEditor = false);

    void Load(Window& window) override;
    void Unload() override;
    bool HandleEvent(SDL_Event& e) override;
    void Update(float dt) override;
    void Render(Window& window) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    entt::registry reg;
    Camera         mCamera;
    float          mLevelW            = 0.0f; // computed from tile extents on Spawn
    float          mLevelH            = 0.0f;
    bool           gameOver           = false;
    bool           levelComplete      = false;
    float          levelCompleteTimer = 2.0f;
    int            totalCoins         = 0;
    int            coinCount          = 0;
    int            stompCount         = 0;
    Window*        mWindow            = nullptr;
    std::string    mLevelPath;
    bool           mFromEditor        = false;  // true = launched via editor Play button
    bool           mPauseRequested    = false;  // set when ESC pressed during play
    Level          mLevel;
    SDL_Rect       retryBtnRect{};

    // Knight animation sheets (one per animation since they are separate PNG sequences)
    std::unique_ptr<SpriteSheet> knightIdleSheet;
    std::unique_ptr<SpriteSheet> knightWalkSheet;
    std::unique_ptr<SpriteSheet> knightHurtSheet;
    std::unique_ptr<SpriteSheet> knightJumpSheet;
    std::unique_ptr<SpriteSheet> knightFallSheet;
    std::unique_ptr<SpriteSheet> knightSlideSheet;
    std::unique_ptr<SpriteSheet> knightSlashSheet;
    std::unique_ptr<SpriteSheet> enemySheet;
    std::unique_ptr<SpriteSheet> coinSheet;
    std::vector<SDL_Surface*>    tileScaledSurfaces; // owned; freed on Unload/Respawn
    std::vector<SDL_Rect>        walkFrames;
    std::vector<SDL_Rect>        jumpFrames;
    std::vector<SDL_Rect>        idleFrames;
    std::vector<SDL_Rect>        hurtFrames;
    std::vector<SDL_Rect>        duckFrames;
    std::vector<SDL_Rect>        frontFrames;
    std::vector<SDL_Rect>        slashFrames;
    std::vector<SDL_Rect>        enemyWalkFrames;

    std::unique_ptr<Image>     background;
    std::unique_ptr<Text>      locationText;
    std::unique_ptr<Text>      actionText;
    std::unique_ptr<Text>      gameOverText;
    std::unique_ptr<Text>      retryBtnText;
    std::unique_ptr<Text>      retryKeyText;
    std::unique_ptr<Rectangle> retryButton;
    std::unique_ptr<Text>      healthText;
    std::unique_ptr<Text>      gravityText;
    std::unique_ptr<Text>      coinText;
    std::unique_ptr<Text>      stompText;
    std::unique_ptr<Text>      levelCompleteText;

    void Spawn();
    void Respawn();
};
