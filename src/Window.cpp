#include "Window.hpp"
#include <SDL2/SDL_video.h>
#include <iostream>
#include <stdexcept>

Window::Window() {
   SDL_Window* Ptr = {SDL_CreateWindow("Smart Pointer Window",
                                       SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED,
                                       700,
                                       300,
                                       SDL_WINDOW_RESIZABLE | SDL_WINDOW_MINIMIZED)};
   if (!Ptr) {
      throw std::runtime_error(std::string("Failed to create Window: ") + SDL_GetError());
   }
   SDLWindow.reset(Ptr);

   Fmt = GetSurface()->format;
   if (!Fmt) {
      throw std::runtime_error("Failed to **GetSurace() --SDL_Surface**");
   }
   Red       = SDL_MapRGB(Fmt, 255, 0, 0);
   Green     = SDL_MapRGB(Fmt, 0, 255, 0);
   DarkGreen = SDL_MapRGB(Fmt, 0, 150, 100);
   Blue      = SDL_MapRGB(Fmt, 0, 0, 255);
   Yellow    = SDL_MapRGB(Fmt, 255, 255, 0);

   SDL_FillRect(GetSurface(), nullptr, DarkGreen);
   SDL_UpdateWindowSurface(SDLWindow.get());
}

SDL_Window* Window::GetRaw() const {
   return SDLWindow.get();
}

SDL_Surface* Window::GetSurface() const {
   return SDL_GetWindowSurface(SDLWindow.get());
}
