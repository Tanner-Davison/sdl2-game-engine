#pragma once
#include <SDL3/SDL.h>
#include <string>

enum class FitMode {
    CONTAIN,
    COVER,
    STRETCH,
    SRCSIZE,
    PRESCALED // scales the texture dst rect to fill the viewport each frame
};

class Image {
  public:
    // Load from file and optionally upload to GPU immediately
    Image(std::string File, FitMode mode = FitMode::CONTAIN);

    // Take ownership of an existing surface and convert to texture on first render
    Image(SDL_Surface* surface, FitMode mode);

    ~Image();

    // Non-copyable (owns GPU texture)
    Image(const Image&)            = delete;
    Image& operator=(const Image&) = delete;

    // Movable
    Image(Image&&) noexcept;
    Image& operator=(Image&&) noexcept;

    // Render to renderer. For PRESCALED/COVER/CONTAIN the dst fills rendererW x rendererH.
    void Render(SDL_Renderer* renderer);

    // Set an explicit destination rect (used by scenes that place images at fixed positions)
    void SetDestinationRectangle(SDL_FRect dest);

    void    SetFitMode(FitMode mode);
    FitMode GetFitMode() const;
    void    SetFlipHorizontal(bool flip);

    void SaveToFile(std::string Location);

    int GetOriginalWidth()  const { return mOrigW; }
    int GetOriginalHeight() const { return mOrigH; }

  private:
    // Upload mPendingSurface to GPU and store in mTexture. Called lazily on first Render.
    void UploadSurface(SDL_Renderer* renderer);

    // Compute mDestRect from current renderer output size based on fit mode
    SDL_FRect ComputeDest(int rendW, int rendH) const;

    SDL_Surface* mPendingSurface = nullptr; // held until first Render call
    SDL_Texture* mTexture        = nullptr;
    int          mOrigW          = 0;
    int          mOrigH          = 0;
    FitMode      mFitMode        = FitMode::CONTAIN;
    bool         mFlipH          = false;

    // For SRCSIZE / explicit placement
    SDL_FRect mExplicitDest{0, 0, 0, 0};
    bool      mHasExplicitDest = false;
};
