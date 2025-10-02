#pragma once
#include <SDL.h>
#include <SDL_surface.h>
#include <string>

class Image {
  public:
    Image(std::string File, SDL_PixelFormat* PreferredFormat = nullptr);
    ~Image();

    void Render(SDL_Surface* DestinationSurface);
    void SetDestinationRectangle(SDL_Rect Requested);

    Image(const Image&)            = delete;
    Image& operator=(const Image&) = delete;

  private:
    int          destHeight{0};
    int          destWidth{0};
    SDL_Surface* ImageSurface{nullptr};
    SDL_Rect     DestRectangle{0, 0, 0, 0};
    SDL_Rect     SrcRectangle{0, 0, 0, 0};
};
