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
    // Hint SDL to use the best available GPU backend and enable low-latency
    // presentation. On WSL this can force OpenGL instead of software rendering.
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

    // Open the audio device early so every scene has access.
    if (!manager.InitAudio()) {
        std::print("Warning: audio device failed to open -- running without sound\n");
    }

    // Auto-open any gamepads already connected at startup.
    // SDL3 also fires SDL_EVENT_GAMEPAD_ADDED for hot-plug so HandleEvent
    // in GameScene picks up controllers connected after launch.
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

    // ── Fixed timestep accumulator ─────────────────────────────────────────
    // Physics always advances in fixed FIXED_DT steps regardless of how long
    // the previous frame took to render. This fully decouples simulation from
    // frame rate so gameplay feels identical on WSL (variable dt) and macOS
    // (stable VSync). Any leftover time carries into the next frame via the
    // accumulator — nothing is ever lost or rounded.
    // Reference: https://gafferongames.com/post/fix_your_timestep/
    constexpr float FIXED_DT    = 1.0f / 120.0f; // physics tick rate (120 Hz)
    constexpr float MAX_FRAME   = 1.0f / 20.0f;  // max real dt before spiral-of-death clamp
    constexpr float TARGET_DT   = 1.0f / 60.0f;  // render / sleep target
    float           accumulator = 0.0f;

    while (true) {
        // ── Measure real elapsed time ────────────────────────────────────
        Uint64 currentTime = SDL_GetPerformanceCounter();
        float  frameTime =
            static_cast<float>(currentTime - lastTime) / static_cast<float>(frequency);
        lastTime = currentTime;

        // Clamp so a breakpoint / system pause doesn't send the sim flying
        if (frameTime > MAX_FRAME)
            frameTime = MAX_FRAME;

        // ── Events ──────────────────────────────────────────────────────
        while (SDL_PollEvent(&E)) {
            if (!manager.HandleEvent(E)) {
                manager.Shutdown();
                FontCache::Clear();
                TTF_Quit();
                SDL_Quit();
                return 0;
            }
        }

        // ── Fixed-step physics ───────────────────────────────────────────
        // Drain the accumulator in FIXED_DT chunks. Each tick advances the
        // full simulation by exactly the same amount every time — gravity,
        // velocity integration, collision, animation, camera — all deterministic.
        accumulator += frameTime;
        while (accumulator >= FIXED_DT) {
            manager.Update(FIXED_DT, GameWindow);
            accumulator -= FIXED_DT;
        }

        // ── Render ──────────────────────────────────────────────────────
        // alpha is the fraction of a physics step that has accumulated but
        // not yet been simulated. RenderSystem lerps each entity's draw
        // position between PrevTransform and Transform by this factor, so
        // motion appears perfectly smooth regardless of frame rate variance.
        float alpha = accumulator / FIXED_DT;
        manager.Render(GameWindow, alpha);

        // ── Hybrid frame limiter ─────────────────────────────────────────
        // Coarse sleep eats most of the idle time cheaply; busy-spin covers
        // the last ~1ms where SDL_Delay's ~10ms granularity would overshoot.
        Uint64 frameEnd = SDL_GetPerformanceCounter();
        float  frameCost =
            static_cast<float>(frameEnd - lastTime) / static_cast<float>(frequency);
        float sleepSeconds = TARGET_DT - frameCost - 0.001f;
        if (sleepSeconds > 0.0f)
            SDL_Delay(static_cast<Uint32>(sleepSeconds * 1000.0f));

        while (static_cast<float>(SDL_GetPerformanceCounter() - lastTime) /
                   static_cast<float>(frequency) <
               TARGET_DT) {
            // busy-spin the last sub-millisecond for precise frame delivery
        }
    }

    TTF_Quit();
    SDL_Quit();
    return 0;
}
