/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
#include "Image.hpp"
#include "Rectangle.hpp"
#include "Sprite.hpp"
#include "SpriteSheet.hpp"
#include "Text.h"
#include "UI.hpp"
#include "Window.hpp"
#include <SDL_image.h>
#include <SDL_pixels.h>
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
    if (TTF_Init() < 0) {
        std::cout << "Error initializing SDL_ttf: " << SDL_GetError();
    }
    SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
    Window GameWindow;

    Image       BackgroundImg{"game_assets/base_pack/bg_castle.png",
                        GameWindow.GetSurface()->format,
                        FitMode::COVER};
    Text        Example{"Collect the Coins!", 800, 800};
    Text        Example2 {}
    SpriteSheet playerSheet("game_assets/base_pack/Player/p1_spritesheet.png",
                            "game_assets/base_pack/Player/p1_spritesheet.txt");

    std::vector<SDL_Rect> walkFrames = playerSheet.GetAnimation("p1_walk");

    Sprite PlayerSprite(playerSheet.GetSurface(),
                        walkFrames,
                        GameWindow.GetSurface()->format,
                        GameWindow.GetWidth() / 2 - 33,
                        GameWindow.GetHeight() / 2 - 46);

    PlayerSprite.SetAnimationSpeed(10.0f);
    PlayerSprite.SetLooping(true);

    UI        UIManager;
    SDL_Event E;

    // Timer setup
    Uint64 frequency = SDL_GetPerformanceFrequency();
    Uint64 lastTime  = SDL_GetPerformanceCounter();

    while (true) {
        // Calculate deltaTime
        Uint64 currentTime = SDL_GetPerformanceCounter();
        float  deltaTime   = (float)(currentTime - lastTime) / frequency;

        lastTime = currentTime;

        // Event handling
        while (SDL_PollEvent(&E)) {
            // UIManager.HandleEvent(E);
            PlayerSprite.HandleEvent(E);
            if (E.type == SDL_QUIT) {
                GameWindow.Render();
                IMG_Quit();
                SDL_Quit();
                return 0;
            }
        }

        // Update with ACTUAL deltaTime
        PlayerSprite.Update(deltaTime);

        // Render
        GameWindow.Render();
        BackgroundImg.Render(GameWindow.GetSurface());

        Example.Render(GameWindow.GetSurface());
        PlayerSprite.Render(GameWindow.GetSurface());

        GameWindow.Update();
    }

    IMG_Quit();
    SDL_Quit();
    return 0;
}
