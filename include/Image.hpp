#pragma once
#include <SDL.h>
#include <SDL_error.h>
#include <SDL_surface.h>
#include <bit>
#include <iostream>
#include <string>

class Image {
  public:
    Image(std::string File, SDL_PixelFormat* PreferredFormat = nullptr)
        : ImageSurface(SDL_LoadBMP(File.c_str())) {
        if (!ImageSurface) {
            std::cout << "Failed to Load Image" << File << " "
                      << SDL_GetError();
        }
        if (PreferredFormat) {
            SDL_Surface* Converted{
                SDL_ConvertSurface(ImageSurface, PreferredFormat, 0)};

            if (Converted) {
                ImageSurface = Converted;
            }
        } else {
            std::cout << "Error converting Surface" << SDL_GetError();
        }
    };
    ~Image() {
        SDL_FreeSurface(ImageSurface);
    }
    Image(const Image&)            = delete; // remove copy constructor
    Image& operator=(const Image&) = delete; // remove copy assignment

    void Render(SDL_Surface* DestinationSurface) {
        if (!hasBeenRead) {
            if (DestinationSurface->format == ImageSurface->format) {
                std::cout << "Matched Format\n";
            } else {
                std::cout << "Reformat Required\n";
            }
        }
        hasBeenRead = true;
        SDL_BlitSurface(ImageSurface, nullptr, DestinationSurface, nullptr);
    }

  private:
    SDL_Surface* ImageSurface{nullptr};
    bool         hasBeenRead{false};
};
