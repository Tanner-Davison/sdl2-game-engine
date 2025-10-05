/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
#include "Image.hpp"
#include "Rectangle.hpp"
#include "Sprite.hpp"
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

    Window GameWindow;
    Image  BackgroundImg{"game_assets/base_pack/bg_castle.png",
                        GameWindow.GetSurface()->format,
                        FitMode::COVER};

    Image ClearPng{"game_assets/clearpng.png",
                   GameWindow.GetSurface()->format,
                   FitMode::CONTAIN};

    std::vector<std::string> playerFrames = {
        "game_assets/base_pack/Player/p1_walk/PNG/p1_walk01.png",
        "game_assets/base_pack/Player/p1_walk/PNG/p1_walk02.png",
        "game_assets/base_pack/Player/p1_walk/PNG/p1_walk03.png",
        "game_assets/base_pack/Player/p1_walk/PNG/p1_walk04.png",
        "game_assets/base_pack/Player/p1_walk/PNG/p1_walk05.png"};

    Sprite PlayerSprite(
        playerFrames, GameWindow.GetSurface()->format, 66, 92, 100, 200);
    PlayerSprite.SetAnimationSpeed(10.0f); // 10 frames per second
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
        lastTime           = currentTime;

        // Event handling
        while (SDL_PollEvent(&E)) {
            UIManager.HandleEvent(E);
            PlayerSprite.HandleEvent(E);
            if (E.type == SDL_QUIT) {
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
        PlayerSprite.Render(GameWindow.GetSurface());

        ClearPng.Render(GameWindow.GetSurface());
        GameWindow.Update();
    }

    IMG_Quit();
    SDL_Quit();
    return 0;
}
