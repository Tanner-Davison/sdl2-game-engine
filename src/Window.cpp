#include "Window.hpp"
#include "ErrorHandling.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <stdexcept>

Window::Window() {
    // Query the usable display area (excludes taskbar/dock) on the primary
    // display so the window fits correctly on any screen size or DPI.
    SDL_Rect usable{};
    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    if (primary == 0 || !SDL_GetDisplayUsableBounds(primary, &usable)) {
        // Fallback if the query fails
        usable = {0, 0, 1280, 800};
    }

    // Cap at a comfortable maximum so the window isn’t absurdly large on
    // ultra-wide monitors, while still filling smaller laptop screens fully.
    int winW = std::min(usable.w, 1600);
    int winH = std::min(usable.h, 1050);

    SDL_Window* Ptr = SDL_CreateWindow("SDL3 Sandbox", winW, winH,
                                       SDL_WINDOW_RESIZABLE);
    CheckSDLError("Creating Window");

    if (!Ptr) {
        throw std::runtime_error(std::string("Failed to create Window: ") +
                                 SDL_GetError());
    }

    SDLWindow.reset(Ptr);

    // Position the window at the top-left of the usable area so it sits
    // neatly inside the dock/taskbar boundaries on every platform.
    SDL_SetWindowPosition(Ptr, usable.x, usable.y);

    // Note: SDL3 enables drop file events by default — no opt-in call needed.
    // Note: do not call SDL_CreateRenderer on this window.
    // SDL_GetWindowSurface and SDL_CreateRenderer are mutually exclusive on Mac.
    // This engine uses surface-based rendering throughout.

    SDL_Surface* surf = GetSurface();
    if (surf) {
        const SDL_PixelFormatDetails* details =
            SDL_GetPixelFormatDetails(surf->format);
        DarkGreen = SDL_MapRGB(details, nullptr, 0, 150, 100);
        Yellow    = SDL_MapRGB(details, nullptr, 255, 255, 0);
        Green     = SDL_MapRGB(details, nullptr, 0, 255, 0);
        Red       = SDL_MapRGB(details, nullptr, 255, 0, 0);
        Blue      = SDL_MapRGB(details, nullptr, 0, 0, 255);
        Black     = SDL_MapRGB(details, nullptr, 0, 0, 1);
        Gray      = SDL_MapRGB(details, nullptr, 134, 149, 149);
    }
}

SDL_Window* Window::GetRaw() const {
    return SDLWindow.get();
}

SDL_Surface* Window::GetSurface() const {
    return SDLWindow ? SDL_GetWindowSurface(SDLWindow.get()) : nullptr;
}

void Window::Render() {
    SDL_Surface* surf = GetSurface();
    if (surf) {
        const SDL_PixelFormatDetails* details =
            SDL_GetPixelFormatDetails(surf->format);
        SDL_FillSurfaceRect(surf, nullptr,
                            SDL_MapRGB(details, nullptr, 0, 0, 1));
    }
}

void Window::Update() {
    SDL_UpdateWindowSurface(SDLWindow.get());
}

int Window::GetWidth() const {
    // Use the surface pixel dimensions so all rendering math works in actual
    // pixels on every platform — including Retina/HiDPI Macs where the logical
    // window size differs from the surface size.
    SDL_Surface* surf = GetSurface();
    return surf ? surf->w : 0;
}

int Window::GetHeight() const {
    SDL_Surface* surf = GetSurface();
    return surf ? surf->h : 0;
}

void Window::TakeScreenshot(std::string Location) {
    IMG_SavePNG(GetSurface(), Location.c_str());
}
