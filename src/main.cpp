/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
#include "SceneManager.hpp"
#include "TitleScene.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cstdlib>
#include <ctime>
#include <print>

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    if (!TTF_Init()) {
        std::print("Error initializing SDL_ttf: {}\n", SDL_GetError());
        return 1;
    }
    srand(static_cast<unsigned int>(time(nullptr)));

    Window       GameWindow;
    SceneManager manager;

    manager.SetScene(std::make_unique<TitleScene>(), GameWindow);

    SDL_Event E;
    Uint64    frequency = SDL_GetPerformanceFrequency();
    Uint64    lastTime  = SDL_GetPerformanceCounter();

    while (true) {
        Uint64 currentTime = SDL_GetPerformanceCounter();
        float  deltaTime =
            static_cast<float>(currentTime - lastTime) / static_cast<float>(frequency);
        lastTime = currentTime;

        while (SDL_PollEvent(&E)) {
            if (!manager.HandleEvent(E)) {
                manager.Shutdown();
                TTF_Quit();
                SDL_Quit();
                return 0;
            }
        }

        manager.Update(deltaTime, GameWindow);
        manager.Render(GameWindow);
    }

    TTF_Quit();
    SDL_Quit();
    return 0;
}
