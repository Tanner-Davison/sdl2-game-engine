#include "Image.hpp"
#include <SDL_image.h>
#include <SDL_surface.h>
#include <iostream>

Image::Image(std::string File, SDL_PixelFormat* PreferredFormat, FitMode mode)
    : mImageSurface{IMG_Load(File.c_str())}
    , fitMode{mode} {
    if (!mImageSurface) {
        std::cout << "Failed to load image: " << File << ":\n"
                  << SDL_GetError();
    } else {
        SDL_SetSurfaceBlendMode(mImageSurface, SDL_BLENDMODE_BLEND);
        originalWidth   = mImageSurface->w;
        originalHeight  = mImageSurface->h;
        mSrcRectangle.w = originalWidth;
        mSrcRectangle.h = originalHeight;
        SetDestinationRectangle({0, 0, 600, 300});
        // std::cout << "Original Format: "
        //           << SDL_GetPixelFormatName(ImageSurface->format->format)
        //           << "\n";
        // std::cout << "Has alpha: " << (ImageSurface->format->Amask != 0)
        //           << "\n";
    }
    if (PreferredFormat) {
        Uint32 targetFormat = (PreferredFormat && PreferredFormat->Amask != 0)
                                  ? PreferredFormat->format
                                  : SDL_PIXELFORMAT_RGBA8888;

        SDL_Surface* Converted =
            SDL_ConvertSurfaceFormat(mImageSurface, targetFormat, 0);
        if (Converted) {
            SDL_SetSurfaceBlendMode(Converted, SDL_BLENDMODE_BLEND);
            SDL_FreeSurface(mImageSurface);
            mImageSurface = Converted;
        } else {
            std::cout << "Error converting surface: " << SDL_GetError();
        }
    }
}

// Copy  constructor
Image::Image(const Image& Source)
    : destHeight{Source.destHeight}
    , destWidth{Source.destWidth}
    , originalWidth{Source.originalWidth}
    , originalHeight{Source.originalHeight}
    , mImageSurface{nullptr}
    , mDestRectangle{Source.mDestRectangle}
    , mSrcRectangle{Source.mSrcRectangle}
    , fitMode{Source.fitMode}
    , destinationInitialized{Source.destinationInitialized} {
    if (Source.mImageSurface) {
        mImageSurface = SDL_ConvertSurface(
            Source.mImageSurface, Source.mImageSurface->format, 0);
        if (mImageSurface) {
            SDL_SetSurfaceBlendMode(mImageSurface, SDL_BLENDMODE_BLEND);
        }
    }
}

// copy assaignment operator
Image& Image::operator=(const Image& Source) {
    // Self-assignment check
    if (this == &Source) {
        return *this;
    }

    // Free current resources
    SDL_FreeSurface(mImageSurface);

    // Copy surface
    if (Source.mImageSurface) {
        mImageSurface = SDL_ConvertSurface(
            Source.mImageSurface, Source.mImageSurface->format, 0);
        if (mImageSurface) {
            SDL_SetSurfaceBlendMode(mImageSurface, SDL_BLENDMODE_BLEND);
        }
    } else {
        mImageSurface = nullptr;
    }

    // Copy ALL member variables
    destHeight             = Source.destHeight;
    destWidth              = Source.destWidth;
    originalWidth          = Source.originalWidth;
    originalHeight         = Source.originalHeight;
    mDestRectangle         = Source.mDestRectangle;
    mSrcRectangle          = Source.mSrcRectangle;
    fitMode                = Source.fitMode;
    destinationInitialized = Source.destinationInitialized;

    return *this;
}
Image::~Image() {
    if (mImageSurface) {
        SDL_FreeSurface(mImageSurface);
    }
}

void Image::Render(SDL_Surface* DestinationSurface) {
    if (destWidth != DestinationSurface->w ||
        destHeight != DestinationSurface->h || !destinationInitialized) {
        destWidth        = DestinationSurface->w;
        destHeight       = DestinationSurface->h;
        mDestRectangle.w = destWidth;
        mDestRectangle.h = destHeight;
        SetDestinationRectangle(mDestRectangle);
        destinationInitialized = true;
    }
    if (fitMode == FitMode::SRCSIZE) {
        SDL_BlitSurface(
            mImageSurface, &mSrcRectangle, DestinationSurface, &mDestRectangle);
    } else {
        SDL_BlitScaled(
            mImageSurface, &mSrcRectangle, DestinationSurface, &mDestRectangle);
    }
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
        case FitMode::STRETCH:
            HandleStretch(Requested);
            break;
        case FitMode::SRCSIZE:
            HandleSrcSize(Requested);
    }
}

void Image::HandleContain(SDL_Rect& Requested) {
    // Use originalWidth/Height instead of current SrcRectangle
    float SourceRatio    = originalWidth / static_cast<float>(originalHeight);
    float RequestedRatio = Requested.w / static_cast<float>(Requested.h);

    // Reset source to full image
    mSrcRectangle.x = 0;
    mSrcRectangle.y = 0;
    mSrcRectangle.w = originalWidth;
    mSrcRectangle.h = originalHeight;

    mDestRectangle = Requested;

    if (RequestedRatio < SourceRatio) {
        mDestRectangle.h = static_cast<int>(Requested.w / SourceRatio);
    } else {
        mDestRectangle.w = static_cast<int>(Requested.h * SourceRatio);
    }

    // Center image
    mDestRectangle.x = (Requested.w - mDestRectangle.w) / 2;
    mDestRectangle.y = (Requested.h - mDestRectangle.h) / 2;
}
void Image::HandleCover(SDL_Rect& Requested) {
    if (originalWidth <= 0 || originalHeight <= 0) {
        return;
    }
    float sourceRatio    = originalWidth / static_cast<float>(originalHeight);
    float requestedRatio = Requested.w / static_cast<float>(Requested.h);

    mDestRectangle = Requested;

    // Reset source to original
    mSrcRectangle.x = 0;
    mSrcRectangle.y = 0;
    mSrcRectangle.w = originalWidth;
    mSrcRectangle.h = originalHeight;

    if (requestedRatio < sourceRatio) {
        // Crop width
        int newSrcWidth = static_cast<int>(originalHeight * requestedRatio);
        mSrcRectangle.x = (originalWidth - newSrcWidth) / 2;
        mSrcRectangle.w = newSrcWidth;
    } else {
        // Crop height
        int newSrcHeight = static_cast<int>(originalWidth / requestedRatio);
        mSrcRectangle.y  = (originalHeight - newSrcHeight) / 2;
        mSrcRectangle.h  = newSrcHeight;
    }
}
void Image::HandleStretch(SDL_Rect& Requested) {
    // Reset source to full original image
    mSrcRectangle.x = 0;
    mSrcRectangle.y = 0;
    mSrcRectangle.w = originalWidth;
    mSrcRectangle.h = originalHeight;

    // Destination fills entire requested area (no aspect ratio preservation)
    mDestRectangle = Requested;
}

void Image::HandleSrcSize(SDL_Rect& Requested) {
    // Reset source to full image
    mSrcRectangle.x = 0;
    mSrcRectangle.y = 0;
    mSrcRectangle.w = originalWidth;
    mSrcRectangle.h = originalHeight;

    // Center image
    mDestRectangle.x = (Requested.w - mDestRectangle.w) / 2;
    mDestRectangle.y = (Requested.h - mDestRectangle.h) / 2;
}

void Image::SaveToFile(std::string Location) {
    IMG_SavePNG(mImageSurface, Location.c_str());
}
