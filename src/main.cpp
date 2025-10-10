/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
#include "Button.hpp"
#include "Image.hpp"
#include "Sprite.hpp"
#include "SpriteSheet.hpp"
#include "Text.h"
#include "UI.hpp"
#include "Window.hpp"
#include <SDL_assert.h>
#include <SDL_image.h>
#include <SDL_pixels.h>
#include <cstdlib>
#include <ctime>
#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif
#include <iostream>

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();
    srand(static_cast<unsigned int>(time(nullptr)));
    if (TTF_Init() < 0) {
        std::cout << "Error initializing SDL_ttf: " << SDL_GetError();
    }
    SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
    Window GameWindow;

    Image BackgroundImg{"game_assets/base_pack/bg_castle.png",
                        GameWindow.GetSurface()->format,
                        FitMode::COVER};

    Text LocationText{"You are in space!!", 20, 20};
    Text ActionText{"Float Around", {100, 100, 100, 0}, 20, 80, 20};

    // Player sprite
    SpriteSheet playerSheet("game_assets/base_pack/Player/p1_spritesheet.png",
                            "game_assets/base_pack/Player/p1_spritesheet.txt");
    std::vector<SDL_Rect> walkFrames = playerSheet.GetAnimation("p1_walk");

    Sprite PlayerSprite(playerSheet.GetSurface(),
                        walkFrames,
                        GameWindow.GetSurface()->format,
                        GameWindow.GetWidth() / 2 - 33,
                        GameWindow.GetHeight() / 2 - 46);
    PlayerSprite.SetAnimationSpeed(12.0f);
    PlayerSprite.SetLooping(true);
    // Enemy Sprites

    // SpriteSheet alienSheet(
    //     "game_assets/extra_animations_and_enemies/Spritesheets/alienBeige.png",
    //     "game_assets/extra_animations_and_enemies/Spritesheets/alienBeige.xml");

    // std::vector<SDL_Rect> alienWalkFrames =
    //     alienSheet.GetAnimation("alienBeige_walk");
    // std::vector<SDL_Rect> alienJumpFrames =
    //     alienSheet.GetAnimation("alienBeige_jump");
    // std::vector<SDL_Rect> alienClimbFrames =
    //     alienSheet.GetAnimation("alienBeige_climb");

    SpriteSheet enemySheet(
        "game_assets/base_pack/Enemies/enemies_spritesheet.png",
        "game_assets/base_pack/Enemies/enemies_spritesheet.txt");

    std::vector<SDL_Rect> enemyWalkFrames =
        enemySheet.GetAnimation("slimeWalk");

    std::vector<std::unique_ptr<Sprite>> Enemies;

    for (int i = 0; i < 15; ++i) {
        // Random position within window bounds (leaving margin for sprite size)
        float xPos = rand() % (GameWindow.GetWidth() -
                               100); // -100 for sprite width margin
        float yPos = rand() % (GameWindow.GetHeight() -
                               100); // -100 for sprite height margin

        Enemies.push_back(
            std::make_unique<Sprite>(enemySheet.GetSurface(),
                                     enemyWalkFrames,
                                     GameWindow.GetSurface()->format,
                                     xPos,
                                     yPos));

        Enemies.back()->SetAnimationSpeed(7.0f);
        Enemies.back()->SetMoveSpeed(20.0f);
        Enemies.back()->SetLooping(true);
    }

    UI        UIManager;
    SDL_Event E;

    // Timer setup
    Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 lastTime  = SDL_GetPerformanceCounter();

    while (true) {
        // Calculate deltaTime
        Uint64 currentTime = SDL_GetPerformanceCounter();
        float  deltaTime   = (float)(currentTime - lastTime) / frequency;
        lastTime           = currentTime;

        // Event handling
        while (SDL_PollEvent(&E)) {
            UIManager.HandleEvent(E);
            PlayerSprite.HandleEvent(E);
            for (auto& sprite : Enemies) {
                sprite->HandleEvent(E);
            }
            if (E.type == SDL_QUIT) {
                GameWindow.Render();
                IMG_Quit();
                SDL_Quit();
                return 0;
            }
        }

        // Update
        PlayerSprite.Update(deltaTime);

        // Update all enemies
        for (auto& enemy : Enemies) {
            enemy->Update(deltaTime);
        }

        // Render
        GameWindow.Render();
        BackgroundImg.Render(GameWindow.GetSurface());
        LocationText.Render(GameWindow.GetSurface());
        ActionText.Render(GameWindow.GetSurface());

        // Render all enemies
        for (auto& enemy : Enemies) {
            enemy->Render(GameWindow.GetSurface());
        }

        PlayerSprite.Render(GameWindow.GetSurface());
        // UIManager.Render(GameWindow.GetSurface());

        GameWindow.Update();
    }

    IMG_Quit();
    SDL_Quit();
    return 0;
}
