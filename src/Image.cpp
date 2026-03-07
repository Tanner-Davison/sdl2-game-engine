#include "Image.hpp"
#include <SDL3_image/SDL_image.h>
#include <print>

// ── Construction ──────────────────────────────────────────────────────────────

Image::Image(std::string File, FitMode mode)
    : mFitMode(mode) {
    mPendingSurface = IMG_Load(File.c_str());
    if (!mPendingSurface) {
        std::print("Failed to load image: {}\n{}\n", File, SDL_GetError());
        return;
    }
    SDL_SetSurfaceBlendMode(mPendingSurface, SDL_BLENDMODE_BLEND);
    mOrigW = mPendingSurface->w;
    mOrigH = mPendingSurface->h;
}

Image::Image(SDL_Surface* surface, FitMode mode)
    : mPendingSurface(surface)
    , mFitMode(mode) {
    if (mPendingSurface) {
        SDL_SetSurfaceBlendMode(mPendingSurface, SDL_BLENDMODE_BLEND);
        mOrigW = mPendingSurface->w;
        mOrigH = mPendingSurface->h;
    }
}

Image::~Image() {
    if (mTexture)        SDL_DestroyTexture(mTexture);
    if (mPendingSurface) SDL_DestroySurface(mPendingSurface);
}

Image::Image(Image&& o) noexcept
    : mPendingSurface(o.mPendingSurface)
    , mTexture(o.mTexture)
    , mOrigW(o.mOrigW)
    , mOrigH(o.mOrigH)
    , mFitMode(o.mFitMode)
    , mFlipH(o.mFlipH)
    , mExplicitDest(o.mExplicitDest)
    , mHasExplicitDest(o.mHasExplicitDest) {
    o.mPendingSurface = nullptr;
    o.mTexture        = nullptr;
}

Image& Image::operator=(Image&& o) noexcept {
    if (this != &o) {
        if (mTexture)        SDL_DestroyTexture(mTexture);
        if (mPendingSurface) SDL_DestroySurface(mPendingSurface);
        mPendingSurface    = o.mPendingSurface;
        mTexture           = o.mTexture;
        mOrigW             = o.mOrigW;
        mOrigH             = o.mOrigH;
        mFitMode           = o.mFitMode;
        mFlipH             = o.mFlipH;
        mExplicitDest      = o.mExplicitDest;
        mHasExplicitDest   = o.mHasExplicitDest;
        o.mPendingSurface  = nullptr;
        o.mTexture         = nullptr;
    }
    return *this;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void Image::UploadSurface(SDL_Renderer* renderer) {
    if (!mPendingSurface) return;
    mTexture = SDL_CreateTextureFromSurface(renderer, mPendingSurface);
    if (!mTexture)
        std::print("Image: failed to create texture: {}\n", SDL_GetError());
    SDL_DestroySurface(mPendingSurface);
    mPendingSurface = nullptr;
}

SDL_FRect Image::ComputeDest(int rendW, int rendH) const {
    if (mHasExplicitDest) return mExplicitDest;

    switch (mFitMode) {
        case FitMode::PRESCALED:
        case FitMode::STRETCH:
            return {0.0f, 0.0f, (float)rendW, (float)rendH};

        case FitMode::COVER: {
            float srcRatio  = (float)mOrigW / (float)mOrigH;
            float dstRatio  = (float)rendW  / (float)rendH;
            float dw, dh;
            if (dstRatio >= srcRatio) { dw = rendW; dh = rendW / srcRatio; }
            else                      { dh = rendH; dw = rendH * srcRatio; }
            return {(rendW - dw) * 0.5f, (rendH - dh) * 0.5f, dw, dh};
        }

        case FitMode::CONTAIN: {
            float srcRatio = (float)mOrigW / (float)mOrigH;
            float dstRatio = (float)rendW  / (float)rendH;
            float dw, dh;
            if (dstRatio <= srcRatio) { dw = rendW; dh = rendW / srcRatio; }
            else                      { dh = rendH; dw = rendH * srcRatio; }
            return {(rendW - dw) * 0.5f, (rendH - dh) * 0.5f, dw, dh};
        }

        case FitMode::SRCSIZE:
        default:
            return {mExplicitDest.x, mExplicitDest.y,
                    (float)mOrigW,   (float)mOrigH};
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

void Image::Render(SDL_Renderer* renderer) {
    if (!renderer) return;
    if (!mTexture) UploadSurface(renderer);
    if (!mTexture) return;

    int rw, rh;
    SDL_GetRenderOutputSize(renderer, &rw, &rh);
    SDL_FRect dst = ComputeDest(rw, rh);

    SDL_FlipMode flip = mFlipH ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderTextureRotated(renderer, mTexture, nullptr, &dst, 0.0, nullptr, flip);
}

void Image::SetDestinationRectangle(SDL_FRect dest) {
    mExplicitDest      = dest;
    mHasExplicitDest   = true;
}

void Image::SetFitMode(FitMode mode)     { mFitMode = mode; }
FitMode Image::GetFitMode() const        { return mFitMode; }
void Image::SetFlipHorizontal(bool flip) { mFlipH = flip; }

void Image::RenderScrolling(SDL_Renderer* renderer, float cameraX, float levelW) {
    if (!renderer) return;
    if (!mTexture) UploadSurface(renderer);
    if (!mTexture) return;

    int rendW, rendH;
    SDL_GetRenderOutputSize(renderer, &rendW, &rendH);

    // Scale image so its height exactly fills the viewport.
    // The displayed width scales proportionally.
    float scale   = (float)rendH / (float)mOrigH;
    float imgDispW = (float)mOrigW * scale; // how wide the image appears on screen

    // Scroll: map cameraX across the scrollable range of the image.
    // scrollable image pixels = imgDispW - rendW  (how far we can pan)
    // scrollable world pixels = levelW - rendW    (how far the camera can move)
    // If levelW is 0 or image is narrower than viewport, just centre it.
    float offsetX = 0.0f;
    float scrollableImg   = imgDispW - (float)rendW;
    float scrollableWorld = (levelW > 0.0f ? levelW : imgDispW) - (float)rendW;
    if (scrollableImg > 0.0f && scrollableWorld > 0.0f) {
        float t = cameraX / scrollableWorld; // 0..1
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        offsetX = -t * scrollableImg;
    } else {
        // Image fits entirely (or world is tiny): centre it
        offsetX = ((float)rendW - imgDispW) * 0.5f;
    }

    SDL_FRect dst = {offsetX, 0.0f, imgDispW, (float)rendH};
    SDL_FlipMode flip = mFlipH ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderTextureRotated(renderer, mTexture, nullptr, &dst, 0.0, nullptr, flip);
}

void Image::SaveToFile(std::string Location) {
    if (mPendingSurface)
        IMG_SavePNG(mPendingSurface, Location.c_str());
}
