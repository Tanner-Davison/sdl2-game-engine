/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/

#include "Window.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <iostream>

int main(int argc, char** argv) {
   SDL_Init(SDL_INIT_VIDEO);
   Window gameWindow;

   bool shouldContinue{true};
   while (shouldContinue) {
      // 1. Process Events

      SDL_Event Event;
      while (SDL_PollEvent(&Event)) {
         if (Event.type == SDL_QUIT) {
            shouldContinue = false;
         }
      };

      // 2. Update Objects

      // 3. Render Changes
   }

   SDL_Quit();
   return 0;
}
