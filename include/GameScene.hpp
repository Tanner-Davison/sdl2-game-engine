#pragma once
#include "AnimatedTile.hpp"
#include "Components.hpp"
#include "Image.hpp"
#include "LevelData.hpp"
#include "LevelSerializer.hpp"
#include "LevelBinary.hpp"
#include "PlayerProfile.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SpatialGrid.hpp"
#include "SpriteSheet.hpp"
#include "GameConfig.hpp"
#include "Systems.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include "systems/BloodParticleSystem.hpp"
#include <SDL3/SDL.h>
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <array>

class GameScene : public Scene {
  public:
    GameScene() = default;

    // fromEditor=true → pause menu offers "Back to Editor" instead of "Back to Title"
    explicit GameScene(const std::string& levelPath, bool fromEditor = false,
                       const std::string& profilePath = "",
                       const std::string& p2ProfilePath = "");

    void Load(Window& window) override;
    void Unload() override;
    static void ClearTextureCache();
    bool HandleEvent(SDL_Event& e) override;
    void Update(float dt) override;
    void Render(Window& window, float alpha = 1.0f) override;
    std::unique_ptr<Scene> NextScene() override;
    entt::registry* GetRegistry() override { return &reg; }

  private:
    entt::registry reg;
    Camera         mCamera;
    float          mLevelW            = 0.0f;
    float          mLevelH            = 0.0f;
    bool           gameOver           = false;
    bool           mPlayerDying       = false;
    float          mDeathAnimTimer    = 0.0f;
    bool           levelComplete      = false;
    float          levelCompleteTimer = 2.0f;
    int            totalGoals         = 0;
    int            goalsCollected     = 0;
    int            stompCount         = 0;
    Window*        mWindow            = nullptr;
    std::string    mLevelPath;
    std::string    mProfilePath;
    std::string    mP2ProfilePath;     // P2's chosen profile (empty = reuse P1's sheets)
    int            mPlayerSpriteW = 0;
    int            mPlayerSpriteH = 0;
    std::array<float, PLAYER_ANIM_SLOT_COUNT> mSlotFps{};
    struct SfxFileInfo { float volume = 1.0f; bool timeStretch = false; float trimStart = 0.0f; float trimEnd = 1.0f; };
    std::array<std::vector<SfxFileInfo>, PLAYER_ANIM_SLOT_COUNT> mSlotSfx{};
    std::array<int, PLAYER_ANIM_SLOT_COUNT> mSlotSfxNext{};
    bool           mHasProfile        = false;
    bool           mFromEditor        = false;
    bool           mPaused            = false;
    bool           mGoBackFromPause   = false;
    bool           mDebugHitboxes     = false;

    // Pause overlay UI (built lazily on first pause, reused)
    SDL_Rect                   mPauseResumeRect{};
    SDL_Rect                   mPauseBackRect{};
    std::unique_ptr<Rectangle> mPauseResumeBtn;
    std::unique_ptr<Rectangle> mPauseBackBtn;
    std::unique_ptr<Text>      mPauseTitleLbl;
    std::unique_ptr<Text>      mPauseResumeLbl;
    std::unique_ptr<Text>      mPauseBackLbl;
    std::unique_ptr<Text>      mPauseHintLbl;
    std::unique_ptr<Text>      mPauseHintLbl2;
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
    std::unique_ptr<SpriteSheet> knightDeathSheet;
    std::unique_ptr<SpriteSheet> enemySheet;
    // Custom enemy type sprite sheets — kept alive so GPU textures remain valid
    std::vector<std::unique_ptr<SpriteSheet>> mEnemySpriteSheets;
    SDL_Texture*                 mBulletTex = nullptr;
    // Static tile texture cache — survives across scene instances so replaying a level
    // or loading a level sharing assets skips all disk I/O and GPU uploads.
    static std::vector<SDL_Texture*>                     sTileScaledTextures;
    static std::unordered_map<std::string, SDL_Texture*> sTileTextureCache;
    SDL_Texture* GetCachedTexture(SDL_Renderer* ren, const std::string& path,
                                  int w, int h, int rot = 0);
    std::unordered_map<entt::entity, std::vector<SDL_Texture*>> tileAnimFrameMap;
    // Pre-sorted render list for tile Pass 1 (avoids per-frame allocation + sort in RenderSystem)
    std::vector<entt::entity> mSortedTileRenderList;
    std::vector<entt::entity> mSortedFrontPropList;
    std::vector<SDL_Rect>        walkFrames;
    std::vector<SDL_Rect>        jumpFrames;
    std::vector<SDL_Rect>        idleFrames;
    std::vector<SDL_Rect>        hurtFrames;
    std::vector<SDL_Rect>        duckFrames;
    std::vector<SDL_Rect>        frontFrames;
    std::vector<SDL_Rect>        slashFrames;
    std::vector<SDL_Rect>        deathFrames;
    std::vector<SDL_Rect>        enemyWalkFrames;

    std::unique_ptr<Image>     background;
    std::vector<std::unique_ptr<Image>> mParallaxImages;
    std::vector<float>                  mParallaxFactors;
    std::unique_ptr<Text>      gameOverText;
    std::unique_ptr<Text>      retryBtnText;
    std::unique_ptr<Text>      retryKeyText;
    std::unique_ptr<Rectangle> retryButton;
    std::unique_ptr<Text>      healthText;
    std::unique_ptr<Text>      gravityText;
    std::unique_ptr<Text>      goalText;
    std::unique_ptr<Text>      stompText;
    std::unique_ptr<Text>      levelCompleteText;

    void Spawn();
    void Respawn();
    void RenderTurrets(SDL_Renderer* ren, int W, int H);
    void RenderShield(SDL_Renderer* ren, int W, int H);
    void RenderTurretPowerUp(SDL_Renderer* ren, int W, int H);

    static void PreloadRawSurfaces(const Level& level);

    SpatialGrid          mTileGrid{64.0f};
    SDL_Gamepad*         mCachedPad      = nullptr;
    SDL_Gamepad*         mCachedPad2     = nullptr;   // P2 gamepad (nullptr when <2 controllers)
    bool                 mMultiplayerActive = false;  // true once P2 has been spawned
    BloodParticleSystem  mBloodParticles;

    // Spawns the second player entity using the current sprite sheets.
    // Called automatically when two Xbox controllers are first detected.
    void SpawnPlayer2();

  public:
    // Raw surface cache: avoids redundant IMG_Load for the same path.
    static std::unordered_map<std::string, SDL_Surface*> sRawSurfaceCache;
    static SDL_Surface* GetRawSurface(const std::string& path);
};
