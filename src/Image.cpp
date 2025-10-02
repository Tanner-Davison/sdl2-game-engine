#include "Image.hpp"
#include <iostream>

Image::Image(std::string File, SDL_PixelFormat* PreferredFormat)
    : ImageSurface{SDL_LoadBMP(File.c_str())} {
    if (!ImageSurface) {
        std::cout << "Failed to load image: " << File << ":\n"
                  << SDL_GetError();
    } else {
        SrcRectangle.w = ImageSurface->w;
        SrcRectangle.h = ImageSurface->h;
        SetDestinationRectangle({0, 0, 600, 300});
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

Image::~Image() {
    if (ImageSurface) {
        SDL_FreeSurface(ImageSurface);
    }
}

void Image::Render(SDL_Surface* DestinationSurface) {
    if (destWidth != DestinationSurface->w ||
        destHeight != DestinationSurface->h) {
        destWidth       = DestinationSurface->w;
        destHeight      = DestinationSurface->h;
        DestRectangle.w = destWidth;
        DestRectangle.h = destHeight;
        SetDestinationRectangle(DestRectangle);
    }
    SDL_BlitScaled(
        ImageSurface, &SrcRectangle, DestinationSurface, &DestRectangle);
    // destinationRectangle.x = ImageSurface->w;
    // destinationRectangle.y = ImageSurface->h;
    // SDL_BlitSurface(ImageSurface, &srcRect, DestinationSurface,
    // &destRect);
}

void Image::SetDestinationRectangle(SDL_Rect Requested) {
    float TargetRatio{SrcRectangle.w / static_cast<float>(SrcRectangle.h)};
    float RequestedRatio{Requested.w / static_cast<float>(Requested.h)};
    DestRectangle = Requested;

    if (RequestedRatio < TargetRatio) {
        // Reduce Height
        DestRectangle.h = static_cast<int>(Requested.w / TargetRatio);
    } else {
        // Reduce width as before
        DestRectangle.w = static_cast<int>(Requested.h * TargetRatio);
    }
    // center image
    DestRectangle.x = (Requested.w - DestRectangle.w) / 2;
    DestRectangle.y = (Requested.h - DestRectangle.h) / 2;

    float AppliedRatio{DestRectangle.w /
                       static_cast<float>(DestRectangle.h)}; // for logging
}
