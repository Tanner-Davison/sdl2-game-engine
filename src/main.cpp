/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/
#include "Rectangle.hpp"
#include "Window.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <iostream>

void HandleMotion(SDL_MouseMotionEvent& E, Window& gameWindow) {
    int DistanceFromLeftEdge{E.x};
    int DistanceFromTopEdge{E.y};
    int DistanceFromRightEdge{gameWindow.GetWidth() - E.x};
    int DistanceFromBottomEdge(gameWindow.GetHeight() - E.y);
}
void HandleWindowEvent(SDL_WindowEvent& E) {
    if (E.event == SDL_WINDOWEVENT_ENTER) {
        std::cout << "Mouse Entered Window\n";
    } else if (E.event == SDL_WINDOWEVENT_LEAVE) {
        std::cout << "Mouse Left Window\n";
    }
}
void HandleButtonEvent(SDL_MouseButtonEvent& E) {
    if (E.state == SDL_PRESSED) {
        // Button Pressed
        std::cout << "Button Pressed\n";
    } else if (E.state == SDL_RELEASED) {
        // Button Released
        std::cout << "Button Released\n";
    }
}

int main(int argc, char** argv) {
    SDL_Init(SDL_INIT_VIDEO);
    Window    GameWindow;
    Rectangle Rect{SDL_Rect{50, 50, 50, 50}};
    SDL_Event event;
    while (true) {
        // 1. Process Events
        SDL_Event E;
        while (SDL_PollEvent(&E)) {
            /// Process Events
            Rect.HandleEvent(E);
            if (E.type == SDL_QUIT) {
                SDL_Quit();
                return 0;
            }

            GameWindow.Render(); /// Render Background Color

            Rect.Render(GameWindow.GetSurface());

            GameWindow.Update(); /// Swap Buffers
        };
        // 2. Update Objects
        // 3. Render Changes
    }

    return 0;
}
