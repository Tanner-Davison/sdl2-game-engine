#pragma once
#include <SDL3/SDL.h>
#include <memory>
#include <string>

struct SDLWindowDeleter {
    void operator()(SDL_Window* Ptr) const {
        if (Ptr && SDL_WasInit(SDL_INIT_VIDEO))
            SDL_DestroyWindow(Ptr);
    }
};
struct SDLRendererDeleter {
    void operator()(SDL_Renderer* Ptr) const {
        if (Ptr) SDL_DestroyRenderer(Ptr);
    }
};

using UniqueSDLWindow   = std::unique_ptr<SDL_Window,   SDLWindowDeleter>;
using UniqueSDLRenderer = std::unique_ptr<SDL_Renderer, SDLRendererDeleter>;

class Window {
  public:
    Window();

    SDL_Window*   GetRaw()      const;
    SDL_Renderer* GetRenderer() const;

    void Render();   // clear the back buffer
    void Update();   // present / flip

    int  GetWidth()  const;  // logical points — use for UI layout and hit-testing
    int  GetHeight() const;  // logical points — use for UI layout and hit-testing
    int  GetPhysicalWidth()  const; // physical pixels — use for render targets
    int  GetPhysicalHeight() const; // physical pixels — use for render targets

    void TakeScreenshot(std::string Location);

    // Toggle borderless-fullscreen (F11)
    void ToggleFullscreen();

  private:
    UniqueSDLWindow   SDLWindow{nullptr};
    UniqueSDLRenderer SDLRenderer{nullptr};

    int mWidth{0};         // logical points (window coordinate space)
    int mHeight{0};        // logical points
    int mPhysicalWidth{0}; // physical pixels (render output)
    int mPhysicalHeight{0};
};
