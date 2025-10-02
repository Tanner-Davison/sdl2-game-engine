#pragma once
#include <SDL2/SDL_stdinc.h>
#include <memory>
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

    SDL_Window*  GetRaw() const;
    SDL_Surface* GetSurface() const;
    void         Render();
    void         Update();
    int          GetWidth() const;
    int          GetHeight() const;

    SDL_PixelFormat* Fmt;
    Uint32           Red;
    Uint32           Green;
    Uint32           DarkGreen;
    Uint32           Blue;
    Uint32           Yellow;
    Uint32           Black;
    Uint32           Gray;

  private:
    UniqueSDLWindow SDLWindow{nullptr};
};
