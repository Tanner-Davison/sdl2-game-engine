#include "Image.hpp"
#include <SDL_image.h>
#include <iostream>

// bmp files
Image::Image(std::string File, SDL_PixelFormat* PreferredFormat)
    : ImageSurface{IMG_Load(File.c_str())} {
    if (!ImageSurface) {
        std::cout << "Failed to load image: " << File << ":\n"
                  << SDL_GetError();
    } else {
        originalWidth  = ImageSurface->w;
        originalHeight = ImageSurface->h;
        SrcRectangle.w = originalWidth;
        SrcRectangle.h = originalHeight;
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
void Image::SetFitMode(FitMode mode) {
    fitMode = mode;
};

FitMode Image::GetFitMode() const {
    return fitMode;
};

void Image::SetDestinationRectangle(SDL_Rect Requested) {
    switch (fitMode) {
        case FitMode::CONTAIN:
            HandleContain(Requested);
            break;
        case FitMode::COVER:
            HandleCover(Requested);
            break;
    }
}

void Image::HandleContain(SDL_Rect& Requested) {
    // Use originalWidth/Height instead of current SrcRectangle
    float SourceRatio    = originalWidth / static_cast<float>(originalHeight);
    float RequestedRatio = Requested.w / static_cast<float>(Requested.h);

    // Reset source to full image
    SrcRectangle.x = 0;
    SrcRectangle.y = 0;
    SrcRectangle.w = originalWidth;
    SrcRectangle.h = originalHeight;

    DestRectangle = Requested;

    if (RequestedRatio < SourceRatio) {
        DestRectangle.h = static_cast<int>(Requested.w / SourceRatio);
    } else {
        DestRectangle.w = static_cast<int>(Requested.h * SourceRatio);
    }

    // Center image
    DestRectangle.x = (Requested.w - DestRectangle.w) / 2;
    DestRectangle.y = (Requested.h - DestRectangle.h) / 2;
}
void Image::HandleCover(SDL_Rect& Requested) {
    if (originalWidth <= 0 || originalHeight <= 0) {
        return;
    }
    float sourceRatio    = originalWidth / static_cast<float>(originalHeight);
    float requestedRatio = Requested.w / static_cast<float>(Requested.h);

    DestRectangle = Requested;

    // Reset source to original
    SrcRectangle.x = 0;
    SrcRectangle.y = 0;
    SrcRectangle.w = originalWidth;
    SrcRectangle.h = originalHeight;

    if (requestedRatio < sourceRatio) {
        // Crop width
        int newSrcWidth = static_cast<int>(originalHeight * requestedRatio);
        SrcRectangle.x  = (originalWidth - newSrcWidth) / 2;
        SrcRectangle.w  = newSrcWidth;
    } else {
        // Crop height
        int newSrcHeight = static_cast<int>(originalWidth / requestedRatio);
        SrcRectangle.y   = (originalHeight - newSrcHeight) / 2;
        SrcRectangle.h   = newSrcHeight;
    }
}
