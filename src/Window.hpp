#pragma once
#include <SDL2/SDL_video.h>
#include <memory>
#define SDL_MAIN_HANDLED
// window.hpp
#ifdef __linux__
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

struct SDLWindowDeleter {
   void operator()(SDL_Window* Ptr) const {
      if (Ptr && SDL_WasInit(SDL_INIT_VIDEO)) {
         SDL_DestroyWindow(Ptr);
      }
   }
};

using UniqueSDLWindow = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

class Window {
 public:
   Window();

   // Window(const Window&)            = delete;
   // Window& operator=(const Window&) = delete;  -- No longer needed when using a unique_ptr for SDLWindow

 private:
   UniqueSDLWindow SDLWindow{nullptr};
};
