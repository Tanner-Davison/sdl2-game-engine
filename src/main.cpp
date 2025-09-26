/*Copyright (c) 2025 Tanner Davison. All Rights Reserved.*/

#include "Window.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_video.h>

int main(int argc, char** argv) {
   SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER);

   Window gameWindow;

   while (true) {
      SDL_PumpEvents();
   }
   SDL_Quit();
   return 0;
}
