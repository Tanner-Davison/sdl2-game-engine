/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/

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
   bool      shouldContinue{true};
   SDL_Event event;
   while (shouldContinue) {
      // 1. Process Events
      SDL_Event E;
      while (SDL_PollEvent(&E)) {
         // MOUSE MOTION
         if (E.type == SDL_MOUSEMOTION) {
            HandleMotion(E.motion, GameWindow);

            // WINDOW EVENTS
         } else if (E.type == SDL_WINDOWEVENT) {
            HandleWindowEvent(E.window);

            // BUTTON EVENTS
         } else if (E.type == SDL_MOUSEBUTTONDOWN || E.type == SDL_MOUSEBUTTONUP) {
            HandleButtonEvent(E.button);
            // QUIT
         } else if (E.type == SDL_QUIT) {
            shouldContinue = false;
         }
         GameWindow.Render();
         // render everything else
         GameWindow.Update();
      };
      // 2. Update Objects
      // 3. Render Changes
   }

   SDL_Quit();
   return 0;
}
