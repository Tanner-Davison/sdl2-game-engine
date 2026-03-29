#pragma once
// Image.hpp — GPU-backed image with multiple viewport fit modes.
//
// Fit modes: CONTAIN (fit inside, letterbox), COVER (fill, crop excess),
// STRETCH (ignore aspect), SRCSIZE (native dims at explicit pos),
// PRESCALED (legacy alias for "tile"), FILL (cover but never over-zoom 1:1),
// SCROLL (height-fill + horizontal camera scroll),
// SCROLL_WIDE (half-speed parallax for extra-wide panoramas).

#include <SDL3/SDL.h>
#include <string>
#include <string_view>

enum class FitMode {
    CONTAIN,
    COVER,
    STRETCH,
    SRCSIZE,
    PRESCALED,
    SCROLL,
    SCROLL_WIDE,
    FILL,
};

inline FitMode FitModeFromString(const std::string& s) {
    if (s == "contain")     return FitMode::CONTAIN;
    if (s == "cover")       return FitMode::COVER;
    if (s == "stretch")     return FitMode::STRETCH;
    if (s == "tile")        return FitMode::PRESCALED;
    if (s == "scroll")      return FitMode::SCROLL;
    if (s == "scroll_wide") return FitMode::SCROLL_WIDE;
    if (s == "fill")        return FitMode::FILL;
    return FitMode::FILL; // default
}

inline const char* FitModeToString(FitMode m) {
    switch (m) {
        case FitMode::CONTAIN:     return "contain";
        case FitMode::COVER:       return "cover";
        case FitMode::STRETCH:     return "stretch";
        case FitMode::PRESCALED:   return "tile";
        case FitMode::SCROLL:      return "scroll";
        case FitMode::SCROLL_WIDE: return "scroll_wide";
        case FitMode::FILL:        return "fill";
        case FitMode::SRCSIZE:     return "srcsize";
    }
    return "fill";
}

class Image {
  public:
    Image(std::string File, FitMode mode = FitMode::CONTAIN);
    Image(SDL_Surface* surface, FitMode mode);

    ~Image();

    Image(const Image&)            = delete;
    Image& operator=(const Image&) = delete;

    Image(Image&&) noexcept;
    Image& operator=(Image&&) noexcept;

    void Render(SDL_Renderer* renderer);
    void RenderScrolling(SDL_Renderer* renderer, float cameraX, float levelW = 0.0f);
    void RenderScrollingWide(SDL_Renderer* renderer, float cameraX, float levelW = 0.0f);

    void SetDestinationRectangle(SDL_FRect dest);

    void    SetFitMode(FitMode mode);
    FitMode GetFitMode() const;
    void    SetFlipHorizontal(bool flip);
    void    SetRepeat(bool repeat);
    bool    GetRepeat() const;

    void SaveToFile(std::string Location);

    int GetOriginalWidth()  const { return mOrigW; }
    int GetOriginalHeight() const { return mOrigH; }

  private:
    void      UploadSurface(SDL_Renderer* renderer);
    SDL_FRect ComputeDest(int rendW, int rendH) const;

    SDL_Surface* mPendingSurface = nullptr;
    SDL_Texture* mTexture        = nullptr;
    int          mOrigW          = 0;
    int          mOrigH          = 0;
    FitMode      mFitMode        = FitMode::CONTAIN;
    bool         mFlipH          = false;

    SDL_FRect mExplicitDest{0, 0, 0, 0};
    bool      mHasExplicitDest = false;
    bool      mRepeat          = false;
};
