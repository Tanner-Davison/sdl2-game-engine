#pragma once
#include <SDL.h>
#include <iostream>
#include <string>

class Image {
  public:
    Image(std::string File, SDL_PixelFormat* PreferredFormat = nullptr)
        : ImageSurface{SDL_LoadBMP(File.c_str())} {
        if (!ImageSurface) {
            std::cout << "Failed to load image: " << File << ":\n"
                      << SDL_GetError();
        } else {
            srcRect = {0, 0, ImageSurface->w, ImageSurface->h};
        }
        if (PreferredFormat) {
            SDL_Surface* Converted{
                SDL_ConvertSurface(ImageSurface, PreferredFormat, 0)};
            if (Converted) {
                SDL_FreeSurface(ImageSurface);
                ImageSurface = Converted;
            } else {
                std::cout << "Error converting surface: " << SDL_GetError();
            }
        }
    }

    void Render(SDL_Surface* DestinationSurface) {
        destWidth  = DestinationSurface->w;
        destHeight = DestinationSurface->h;
        destRect.x = (destWidth / 2) - (ImageSurface->w / 2);  // Center X
        destRect.y = (destHeight / 2) - (ImageSurface->h / 2); // Center Y
        SDL_BlitSurface(ImageSurface, &srcRect, DestinationSurface, &destRect);
    }

    ~Image() {
        if (ImageSurface) {
            SDL_FreeSurface(ImageSurface);
        }
    }
    Image(const Image&)            = delete;
    Image& operator=(const Image&) = delete;

  private:
    int          destHeight{0};
    int          destWidth{0};
    SDL_Surface* ImageSurface{nullptr};
    SDL_Rect     destRect;
    SDL_Rect     srcRect{0, 0, 0, 0};
};
