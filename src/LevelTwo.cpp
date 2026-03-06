#include "LevelTwo.hpp"
#include "LevelThree.hpp"
#include <cstdlib>

void LevelTwo::Load(Window& window) {
    mWindow  = &window;
    gameOver = false;
    SDL_Renderer* ren = window.GetRenderer();

    playerSheet = std::make_unique<SpriteSheet>(
        "game_assets/base_pack/Player/p1_spritesheet.png",
        "game_assets/base_pack/Player/p1_spritesheet.txt");
    playerSheet->CreateTexture(ren);

    walkFrames  = playerSheet->GetAnimation("p1_walk");
    jumpFrames  = walkFrames;
    idleFrames  = {playerSheet->GetFrame("p1_stand")};
    hurtFrames  = {playerSheet->GetFrame("p1_hurt")};
    duckFrames  = {playerSheet->GetFrame("p1_duck")};
    frontFrames = {playerSheet->GetFrame("p1_front")};

    enemySheet = std::make_unique<SpriteSheet>(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");
    enemySheet->CreateTexture(ren);
    enemyWalkFrames = enemySheet->GetAnimation("slimeWalk");

    background   = std::make_unique<Image>("game_assets/backgrounds/deepspace_scene.png", FitMode::PRESCALED);
    locationText = std::make_unique<Text>("You are in space!!", 20, 20);
    actionText   = std::make_unique<Text>("Level 2: Collect ALL the coins!", SDL_Color{255, 255, 255, 0}, 20, 80, 20);

    gameOverText = std::make_unique<Text>("Game Over!",  SDL_Color{255, 0, 0, 255},
                                          window.GetWidth()/2-100, window.GetHeight()/2-60, 64);
    retryBtnText = std::make_unique<Text>("Retry", SDL_Color{0,0,0,255},
                                          window.GetWidth()/2-28, window.GetHeight()/2+22, 32);
    retryKeyText = std::make_unique<Text>("Press R to Retry", SDL_Color{200,200,200,255},
                                          window.GetWidth()/2-100, window.GetHeight()/2+110, 24);

    retryBtnRect = {window.GetWidth()/2-75, window.GetHeight()/2+10, 150, 55};
    retryButton  = std::make_unique<Rectangle>(retryBtnRect);
    retryButton->SetColor({255, 255, 255, 255});
    retryButton->SetHoverColor({180, 180, 180, 255});

    levelCompleteText = std::make_unique<Text>("Level Complete!", SDL_Color{255,215,0,255},
                                               window.GetWidth()/2-160, window.GetHeight()/2-40, 64);
    Spawn();
}

void LevelTwo::Unload() { reg.clear(); mWindow = nullptr; }

std::unique_ptr<Scene> LevelTwo::NextScene() {
    if (levelComplete && levelCompleteTimer <= 0.0f)
        return std::make_unique<LevelThree>();
    return nullptr;
}

bool LevelTwo::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) return false;
    if (!gameOver) {
        InputSystem(reg, e);
    } else {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_R) Respawn();
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (mx >= retryBtnRect.x && mx <= retryBtnRect.x+retryBtnRect.w &&
                my >= retryBtnRect.y && my <= retryBtnRect.y+retryBtnRect.h) Respawn();
        }
        retryButton->HandleEvent(e);
    }
    return true;
}

void LevelTwo::Update(float dt) {
    if (levelComplete) { levelCompleteTimer -= dt; return; }
    if (gameOver) return;
    MovementSystem(reg, dt, mWindow->GetWidth());
    PlayerStateSystem(reg);
    BoundsSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight());
    AnimationSystem(reg, dt);
    auto collision = CollisionSystem(reg, dt, mWindow->GetWidth(), mWindow->GetHeight());
    coinCount  += collision.coinsCollected;
    stompCount += collision.enemiesStomped;
    if (collision.playerDied) gameOver = true;
    if (totalCoins > 0 && coinCount >= totalCoins) levelComplete = true;
}

void LevelTwo::Render(Window& window) {
    SDL_Renderer* ren = window.GetRenderer();
    window.Render();
    background->Render(ren);

    if (levelComplete) {
        RenderSystem(reg, ren);
        HUDSystem(reg, ren, window.GetWidth(), healthText.get(), gravityText.get(),
                  coinText.get(), coinCount, stompText.get(), stompCount);
        if (levelCompleteText) levelCompleteText->Render(ren);
    } else if (gameOver) {
        gameOverText->Render(ren);
        retryButton->Render(ren);
        retryBtnText->Render(ren);
        retryKeyText->Render(ren);
    } else {
        locationText->Render(ren);
        actionText->Render(ren);
        RenderSystem(reg, ren);
        HUDSystem(reg, ren, window.GetWidth(), healthText.get(), gravityText.get(),
                  coinText.get(), coinCount, stompText.get(), stompCount);
    }
    window.Update();
}

void LevelTwo::Spawn() {
    SDL_Renderer* ren = mWindow->GetRenderer();
    healthText  = std::make_unique<Text>("100", SDL_Color{255,255,255,255}, 0, 0, 16);
    gravityText = std::make_unique<Text>("",    SDL_Color{100,200,255,255}, 0, 0, 20);
    coinText    = std::make_unique<Text>("Gold Collected: 0",  SDL_Color{255,215,0,255},   0, 0, 16);
    stompText   = std::make_unique<Text>("Enemies Stomped: 0", SDL_Color{255,100,100,255}, 0, 0, 16);

    coinSheet = std::make_unique<SpriteSheet>("game_assets/gold_coins/", "Gold_", 30, 40, 40);
    coinSheet->CreateTexture(ren);
    std::vector<SDL_Rect> coinFrames = coinSheet->GetAnimation("Gold_");

    for (int i = 0; i < COIN_COUNT; i++) {
        int wall = rand()%3; float cx=0,cy=0; int pad=COIN_SIZE+10;
        switch(wall){
            case 0: cx=5;cy=(float)(pad+rand()%(mWindow->GetHeight()-pad*2)); break;
            case 1: cx=(float)(pad+rand()%(mWindow->GetWidth()-pad*2));cy=5; break;
            case 2: cx=(float)(mWindow->GetWidth()-COIN_SIZE-5);cy=(float)(pad+rand()%(mWindow->GetHeight()-pad*2)); break;
        }
        auto coin = reg.create();
        reg.emplace<Transform>(coin, cx, cy);
        reg.emplace<Renderable>(coin, coinSheet->GetTexture(), coinFrames, false);
        reg.emplace<AnimationState>(coin, 0, (int)coinFrames.size(), 0.0f, 15.0f, true);
        reg.emplace<Collider>(coin, COIN_SIZE, COIN_SIZE);
        reg.emplace<CoinTag>(coin);
    }
    totalCoins = (int)reg.view<CoinTag>().size();

    auto player = reg.create();
    reg.emplace<Transform>(player, (float)(mWindow->GetWidth()/2-33),
                           (float)(mWindow->GetHeight()-PLAYER_SPRITE_HEIGHT));
    reg.emplace<Velocity>(player);
    reg.emplace<AnimationState>(player, 0, (int)walkFrames.size(), 0.0f, 12.0f, true);
    reg.emplace<Renderable>(player, playerSheet->GetTexture(), walkFrames, false);
    reg.emplace<PlayerTag>(player);
    reg.emplace<Health>(player);
    reg.emplace<Collider>(player, PLAYER_SPRITE_WIDTH, PLAYER_SPRITE_HEIGHT);
    reg.emplace<InvincibilityTimer>(player);
    reg.emplace<GravityState>(player);
    reg.emplace<AnimationSet>(player, AnimationSet{
        .idle  = idleFrames,  .idleSheet  = playerSheet->GetTexture(),
        .walk  = walkFrames,  .walkSheet  = playerSheet->GetTexture(),
        .jump  = jumpFrames,  .jumpSheet  = playerSheet->GetTexture(),
        .hurt  = hurtFrames,  .hurtSheet  = playerSheet->GetTexture(),
        .duck  = duckFrames,  .duckSheet  = playerSheet->GetTexture(),
        .front = frontFrames, .frontSheet = playerSheet->GetTexture(),
    });

    for (int i = 0; i < GRAVITYSLUGSCOUNT; ++i) {
        float x = (float)(rand()%(mWindow->GetWidth()-100));
        float y = (float)(rand()%(mWindow->GetHeight()-SLIME_SPRITE_HEIGHT));
        float speed = 60.0f+(float)(rand()%120);
        float dx = (rand()%2==0)?speed:-speed;
        auto enemy = reg.create();
        reg.emplace<Transform>(enemy, x, y);
        reg.emplace<Velocity>(enemy, dx, 0.0f, speed);
        reg.emplace<AnimationState>(enemy, 0, (int)enemyWalkFrames.size(), 0.0f, 7.0f, true);
        reg.emplace<Renderable>(enemy, enemySheet->GetTexture(), enemyWalkFrames, false);
        reg.emplace<Collider>(enemy, SLIME_SPRITE_WIDTH, SLIME_SPRITE_HEIGHT);
        reg.emplace<EnemyTag>(enemy);
    }
}

void LevelTwo::Respawn() {
    reg.clear();
    gameOver=false; levelComplete=false; levelCompleteTimer=2.0f;
    coinCount=0; stompCount=0;
    Spawn();
}
