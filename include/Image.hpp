#pragma once
#include <SDL.h>
#include <SDL_surface.h>
#include <string>

enum class FitMode { CONTAIN, COVER, STRETCH, SRCSIZE };

class Image {
  public:
    /**
     * @param File Path to image file (supports BMP, PNG, JPG, etc.)
     * @param PreferredFormat Optional format conversion for optimized blitting
     * (nullptr = no conversion)
     * @param mode How the image should fit in the destination (CONTAIN, COVER,
     * or STRETCH)
     */
    Image(std::string      File,
          SDL_PixelFormat* PreferredFormat = nullptr,
          FitMode          mode            = FitMode::CONTAIN);
    Image(std::string File);
    ~Image();

    void Render(SDL_Surface* DestinationSurface);
    void SetDestinationRectangle(SDL_Rect Requested);

    Image(const Image& Source);
    Image&  operator=(const Image& Source);
    void    SetFitMode(FitMode mode);
    FitMode GetFitMode() const;

    void SaveToFile(std::string Location);

  protected:
    void HandleContain(SDL_Rect& Requested);
    void HandleCover(SDL_Rect& Requested);
    void HandleStretch(SDL_Rect& Requested);
    void HandleSrcSize(SDL_Rect& Requested);

  private:
    int          destHeight{0};
    int          destWidth{0};
    int          originalWidth{0};
    int          originalHeight{0};
    SDL_Surface* mImageSurface{nullptr};
    SDL_Rect     mDestRectangle{0, 0, 0, 0};
    SDL_Rect     mSrcRectangle{0, 0, 0, 0};
    FitMode      fitMode{FitMode::COVER};
    bool         destinationInitialized{false};
};
