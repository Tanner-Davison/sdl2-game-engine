#pragma once
#include "AnimatedTile.hpp"
#include "Components.hpp"
#include "Image.hpp"
#include "Level.hpp"
#include "LevelSerializer.hpp"
#include "PlayerProfile.hpp"
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
#include <unordered_map>
#include <vector>
#include <array>

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
    // profilePath: path to a saved PlayerProfile JSON (empty = use default frost knight)
    explicit GameScene(const std::string& levelPath, bool fromEditor = false,
                       const std::string& profilePath = "");

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
    std::string    mProfilePath;               // path to PlayerProfile JSON (empty = frost knight)
    int            mPlayerSpriteW = 0;         // resolved sprite width  (set in Load, used in Spawn)
    int            mPlayerSpriteH = 0;         // resolved sprite height (set in Load, used in Spawn)
    std::array<float, PLAYER_ANIM_SLOT_COUNT> mSlotFps{};  // per-slot fps from profile (0 = engine default)
    bool           mHasProfile        = false;  // true = a valid PlayerProfile was loaded
    bool           mFromEditor        = false;  // true = launched via editor Play button
    bool           mPaused            = false;  // true = pause overlay active, simulation frozen
    bool           mGoBackFromPause   = false;  // set by pause overlay "Back" button
    bool           mDebugHitboxes     = false;  // F1 toggles hitbox overlay

    // Pause overlay UI (built lazily on first pause, reused)
    SDL_Rect                   mPauseResumeRect{};
    SDL_Rect                   mPauseBackRect{};
    std::unique_ptr<Rectangle> mPauseResumeBtn;
    std::unique_ptr<Rectangle> mPauseBackBtn;
    std::unique_ptr<Text>      mPauseTitleLbl;
    std::unique_ptr<Text>      mPauseResumeLbl;
    std::unique_ptr<Text>      mPauseBackLbl;
    std::unique_ptr<Text>      mPauseHintLbl;
    void BuildPauseUI(int W, int H);
    void RenderPauseOverlay(Window& window);
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
    std::vector<SDL_Texture*>    tileScaledTextures; // owned; freed on Unload only
    // Tile texture cache: key = "path|WxH|rROT" → non-owning ptr into tileScaledTextures.
    // Populated in Spawn(), never cleared between Respawn() calls — only in Unload().
    std::unordered_map<std::string, SDL_Texture*> tileTextureCache;
    // Animated tile frame textures, keyed by entity. Each vector is parallel to
    // the entity's AnimationState frame count.
    std::unordered_map<entt::entity, std::vector<SDL_Texture*>> tileAnimFrameMap;
    // Pre-sorted render list for tile Pass 1 (built in Spawn, updated when action
    // tiles are destroyed).  Avoids per-frame allocation + sort in RenderSystem.
    std::vector<entt::entity> mSortedTileRenderList;
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
