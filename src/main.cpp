/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
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
    Image     ExampleImg{"game_assets/example.bmp",
                     GameWindow.GetSurface()->format}; // image blitting
    UI        UIManager;
    SDL_Event E;
    // Uint64    frequency = SDL_GetPerformanceFrequency();
    while (true) {
        while (SDL_PollEvent(&E)) {
            UIManager.HandleEvent(E);
            if (E.type == SDL_QUIT) {
                SDL_Quit();
                return 0;
            }
        };

        /*UNCOMMENT Uint64 PERFOMANCE RENDERING*/
        // Uint64 Start{SDL_GetPerformanceCounter()};

        GameWindow.Render(); /// Render Background Color

        ExampleImg.Render(GameWindow.GetSurface());

        UIManager.Render(GameWindow.GetSurface());
        GameWindow.Update(); /// Swap Buffers

        /*UNCOMMENT FOR PERFOMANCE RENDERING*/
        // Uint64 Delta{SDL_GetPerformanceCounter()};
        // double elapsedMs = ((Delta - Start) * 1000.0) / frequency;
        // std::cout << "Render time: " << elapsedMs << " ms" << std::endl;
    };

    return 0;
}
