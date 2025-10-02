#include "Window.hpp"
#include "ErrorHandling.hpp"
#include <SDL2/SDL_shape.h>
#include <SDL2/SDL_video.h>
#include <iostream>
#include <stdexcept>

Window::Window() {
    SDL_Window* Ptr = {SDL_CreateWindow("Smart Pointer Window",
                                        SDL_WINDOWPOS_UNDEFINED,
                                        SDL_WINDOWPOS_UNDEFINED,
                                        1440,
                                        1080,
                                        SDL_WINDOW_RESIZABLE)};
    CheckSDLError("Creating Window");

    if (!Ptr) {
#ifdef ERROR_LOGGING
        CheckSDLError("No Window Created Check if Minimized");
#else
        throw std::runtime_error(std::string("Failed to create Window") +
                                 SDL_Error());
#endif
    }

    SDLWindow.reset(Ptr);

    Fmt = GetSurface()->format;
    CheckSDLError("Getting Surface");

    DarkGreen = SDL_MapRGB(Fmt, 0, 150, 100);
    Yellow    = SDL_MapRGB(Fmt, 255, 255, 0);
    Green     = SDL_MapRGB(Fmt, 0, 255, 0);
    Red       = SDL_MapRGB(Fmt, 255, 0, 0);
    Blue      = SDL_MapRGB(Fmt, 0, 0, 255);
    Black     = SDL_MapRGB(Fmt, 0, 0, 1);
    Gray      = SDL_MapRGB(Fmt, 134, 149, 149);
}

SDL_Window* Window::GetRaw() const {
    return SDLWindow.get();
}

SDL_Surface* Window::GetSurface() const {
    return SDLWindow ? SDL_GetWindowSurface(SDLWindow.get()) : nullptr;
}
void Window::Render() {
    SDL_FillRect(GetSurface(), nullptr, Black);
};

void Window::Update() {
    SDL_UpdateWindowSurface(SDLWindow.get());
};

int Window::GetWidth() const {
    return 700;
};
int Window::GetHeight() const {
    return 700;
};
