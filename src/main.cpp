/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
// #include "Image.h"
#include "Image.hpp"
#include "Rectangle.hpp"
#include "UI.hpp"
#include "Window.hpp"
#include <SDL_filesystem.h>
#include <SDL_timer.h>
#ifdef __linux__
    #include <SDL2/SDL.h>
#else
    #include <SDL.h>
#endif
#include <iostream>

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    Window    GameWindow;
    UI        UIManager;
    Image     Example{"game_assets/example.bmp",
                  GameWindow.GetSurface()->format}; // image blitting
    SDL_Event E;
    // std::cout << SDL_GetBasePath();
    std::cout << "Game Window Surface is storing "
                 "It's Pixel data in: "
              << GameWindow.GetSurface()->pixels;
    Uint64 frequency = SDL_GetPerformanceFrequency();
    while (true) {
        // 1. Process Events
        while (SDL_PollEvent(&E)) {
            /// Process Events
            // UIManager.HandleEvent(E);
            if (E.type == SDL_QUIT) {
                SDL_Quit();
                return 0;
            }
        };

        GameWindow.Render(); /// Render Background Color

        Uint64 Start{SDL_GetPerformanceCounter()};
        Example.Render(GameWindow.GetSurface());
        Uint64 Delta{SDL_GetPerformanceCounter()};

        double elapsedMs = ((Delta - Start) * 1000.0) / frequency;
        std::cout << "Render time: " << elapsedMs << " ms" << std::endl;

        // UIManager.Render(GameWindow.GetSurface());
        //
        GameWindow.Update(); /// Swap Buffers

        // 2. Update Objects
        // 3. Render Changes
    };

    SDL_Quit();
    return 0;
}
