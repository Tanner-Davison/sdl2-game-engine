#include "Window.hpp"
#include "ErrorHandling.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <stdexcept>

Window::Window() {
    SDL_Rect usable{};
    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    if (primary == 0 || !SDL_GetDisplayUsableBounds(primary, &usable))
        usable = {0, 0, 1280, 800};

    int winW = std::min(usable.w, 1600);
    int winH = std::min(usable.h, 1050);

    SDL_Window* winPtr = SDL_CreateWindow("SDL3 Sandbox", winW, winH,
                                          SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!winPtr)
        throw std::runtime_error(std::string("Failed to create Window: ") + SDL_GetError());

    SDLWindow.reset(winPtr);
    SDL_SetWindowPosition(winPtr, usable.x, usable.y);

    // GPU-accelerated renderer
    SDL_Renderer* renPtr = SDL_CreateRenderer(winPtr, nullptr);
    if (!renPtr)
        throw std::runtime_error(std::string("Failed to create Renderer: ") + SDL_GetError());

    SDLRenderer.reset(renPtr);
    SDL_SetRenderDrawBlendMode(renPtr, SDL_BLENDMODE_BLEND);

    // Logical size: what UI, input, and hit-testing use (window points)
    SDL_GetWindowSize(winPtr, &mWidth, &mHeight);
    // Physical size: what the renderer actually draws to (pixels, 2x on Retina)
    SDL_GetRenderOutputSize(renPtr, &mPhysicalWidth, &mPhysicalHeight);

    // Lock the renderer coordinate space to logical window points.
    // This ensures all SDL_Render* calls use the same coordinate space as
    // mouse events — on Retina displays the GPU handles the 2x upscaling.
    SDL_SetRenderLogicalPresentation(renPtr, mWidth, mHeight,
                                     SDL_LOGICAL_PRESENTATION_STRETCH);
}

SDL_Window*   Window::GetRaw()      const { return SDLWindow.get(); }
SDL_Renderer* Window::GetRenderer() const { return SDLRenderer.get(); }

void Window::Render() {
    SDL_SetRenderDrawColor(SDLRenderer.get(), 0, 0, 1, 255);
    SDL_RenderClear(SDLRenderer.get());
}

void Window::Update() {
    SDL_RenderPresent(SDLRenderer.get());
    int prevW = mWidth, prevH = mHeight;
    SDL_GetWindowSize(SDLWindow.get(), &mWidth, &mHeight);
    SDL_GetRenderOutputSize(SDLRenderer.get(), &mPhysicalWidth, &mPhysicalHeight);
    // Re-apply logical presentation if the window was resized
    if (mWidth != prevW || mHeight != prevH)
        SDL_SetRenderLogicalPresentation(SDLRenderer.get(), mWidth, mHeight,
                                         SDL_LOGICAL_PRESENTATION_STRETCH);
}

int Window::GetWidth()  const { return mWidth; }
int Window::GetHeight() const { return mHeight; }
int Window::GetPhysicalWidth()  const { return mPhysicalWidth; }
int Window::GetPhysicalHeight() const { return mPhysicalHeight; }

void Window::TakeScreenshot(std::string Location) {
    SDL_Surface* surf = SDL_RenderReadPixels(SDLRenderer.get(), nullptr);
    if (surf) {
        IMG_SavePNG(surf, Location.c_str());
        SDL_DestroySurface(surf);
    }
}

void Window::ToggleFullscreen() {
    SDL_Window* w    = SDLWindow.get();
    Uint32      flags = SDL_GetWindowFlags(w);
    if (flags & SDL_WINDOW_FULLSCREEN) {
        SDL_SetWindowFullscreen(w, false);
    } else {
        SDL_SetWindowFullscreen(w, true);
    }
    SDL_SyncWindow(w);
    SDL_GetWindowSize(SDLWindow.get(), &mWidth, &mHeight);
    SDL_GetRenderOutputSize(SDLRenderer.get(), &mPhysicalWidth, &mPhysicalHeight);
    SDL_SetRenderLogicalPresentation(SDLRenderer.get(), mWidth, mHeight,
                                     SDL_LOGICAL_PRESENTATION_STRETCH);
}
