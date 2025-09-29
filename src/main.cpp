/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
#include "Rectangle.hpp"
#include "UI.hpp"
#include "Window.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <iostream>

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    Window    GameWindow;
    UI        UIManager;
    SDL_Event E;

    while (true) {
        // 1. Process Events
        while (SDL_PollEvent(&E)) {
            /// Process Events
            UIManager.HandleEvent(E);
            if (E.type == SDL_QUIT) {
                SDL_Quit();
                return 0;
            }
        };

        GameWindow.Render(); /// Render Background Color

        UIManager.Render(GameWindow.GetSurface());

        GameWindow.Update(); /// Swap Buffers

        // 2. Update Objects
        // 3. Render Changes
    };

    return 0;
}
