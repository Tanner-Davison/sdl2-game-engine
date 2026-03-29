/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
#include "SceneManager.hpp"
#include "Text.hpp"
#include "TitleScene.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <print>

int main(int argc, char** argv) {
    // Must be set before any SDL_Create* calls.
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "1");

    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO);
    if (!TTF_Init()) {
        std::print("Error initializing SDL_ttf: {}\n", SDL_GetError());
        return 1;
    }
    srand(static_cast<unsigned int>(time(nullptr)));

    Window       GameWindow;
    SceneManager manager;

    if (!manager.InitAudio()) {
        std::print("Warning: audio device failed to open -- running without sound\n");
    }

    // Auto-open gamepads connected at startup; hot-plug handled via SDL_EVENT_GAMEPAD_ADDED.
    {
        int             count = 0;
        SDL_JoystickID* ids   = SDL_GetGamepads(&count);
        if (ids) {
            for (int i = 0; i < count; ++i)
                SDL_OpenGamepad(ids[i]);
            SDL_free(ids);
        }
    }

    manager.SetScene(std::make_unique<TitleScene>(), GameWindow);

    SDL_Event E;
    Uint64    frequency = SDL_GetPerformanceFrequency();
    Uint64    lastTime  = SDL_GetPerformanceCounter();

    // Fixed timestep: https://gafferongames.com/post/fix_your_timestep/
    constexpr float FIXED_DT    = 1.0f / 120.0f;
    constexpr float MAX_FRAME   = 1.0f / 20.0f;
    constexpr float TARGET_DT   = 1.0f / 60.0f;
    float           accumulator = 0.0f;

    while (true) {
        Uint64 currentTime = SDL_GetPerformanceCounter();
        float  frameTime =
            static_cast<float>(currentTime - lastTime) / static_cast<float>(frequency);
        lastTime = currentTime;

        // Clamp so a breakpoint / system pause doesn't send the sim flying
        if (frameTime > MAX_FRAME)
            frameTime = MAX_FRAME;

        while (SDL_PollEvent(&E)) {
            if (!manager.HandleEvent(E)) {
                manager.Shutdown();
                FontCache::Clear();
                TTF_Quit();
                SDL_Quit();
                return 0;
            }
        }

        accumulator += frameTime;
        while (accumulator >= FIXED_DT) {
            manager.Update(FIXED_DT, GameWindow);
            accumulator -= FIXED_DT;
        }

        float alpha = accumulator / FIXED_DT;
        manager.Render(GameWindow, alpha);

        // Coarse sleep + busy-spin: SDL_Delay granularity is ~10ms, spin covers the last ~1ms.
        Uint64 frameEnd = SDL_GetPerformanceCounter();
        float  frameCost =
            static_cast<float>(frameEnd - lastTime) / static_cast<float>(frequency);
        float sleepSeconds = TARGET_DT - frameCost - 0.001f;
        if (sleepSeconds > 0.0f)
            SDL_Delay(static_cast<Uint32>(sleepSeconds * 1000.0f));

        while (static_cast<float>(SDL_GetPerformanceCounter() - lastTime) /
                   static_cast<float>(frequency) <
               TARGET_DT) {
        }
    }

    TTF_Quit();
    SDL_Quit();
    return 0;
}
