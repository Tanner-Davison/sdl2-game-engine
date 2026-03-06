/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
#include "SceneManager.hpp"
#include "Text.hpp"
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
    Uint64       frequency    = SDL_GetPerformanceFrequency();
    Uint64       lastTime     = SDL_GetPerformanceCounter();
    // Cap at 120 FPS — the display refreshes at 60 Hz so anything beyond that
    // burns CPU for zero visible benefit, especially on battery.
    constexpr float TARGET_DT   = 1.0f / 120.0f;
    constexpr float MIN_DT      = 1.0f / 1000.0f; // guard against zero dt

    while (true) {
        Uint64 currentTime = SDL_GetPerformanceCounter();
        float  deltaTime =
            static_cast<float>(currentTime - lastTime) / static_cast<float>(frequency);
        lastTime = currentTime;
        if (deltaTime < MIN_DT) deltaTime = MIN_DT;

        while (SDL_PollEvent(&E)) {
            if (!manager.HandleEvent(E)) {
                manager.Shutdown();
                FontCache::Clear();
                TTF_Quit();
                SDL_Quit();
                return 0;
            }
        }

        manager.Update(deltaTime, GameWindow);
        manager.Render(GameWindow);

        // Sleep until the next frame deadline so the CPU is free for the OS.
        Uint64 afterRender  = SDL_GetPerformanceCounter();
        float  frameCost    = static_cast<float>(afterRender - lastTime) /
                              static_cast<float>(frequency);
        float  sleepSeconds = TARGET_DT - frameCost;
        if (sleepSeconds > 0.001f)
            SDL_Delay(static_cast<Uint32>(sleepSeconds * 1000.0f));
    }

    TTF_Quit();
    SDL_Quit();
    return 0;
}
