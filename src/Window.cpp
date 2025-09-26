#include "Window.hpp"
#include "SDL2/SDL_video.h"
#include <SDL2/SDL_log.h>
#include <stdexcept>

Window::Window() {
   SDL_Window* Ptr{SDL_CreateWindow("Smart Pointer Window",
                                    SDL_WINDOWPOS_UNDEFINED,
                                    SDL_WINDOWPOS_UNDEFINED,
                                    700,
                                    300,
                                    SDL_WINDOW_RESIZABLE | SDL_WINDOW_MINIMIZED)};

   SDLWindow = static_cast<UniqueSDLWindow>(Ptr);
}
