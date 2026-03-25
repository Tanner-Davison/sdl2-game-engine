#include "Image.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <print>

// =========================================================================
// Construction / destruction / move
// =========================================================================

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

// =========================================================================
// Private helpers
// =========================================================================

void Image::UploadSurface(SDL_Renderer* renderer) {
    if (!mPendingSurface) return;
    mTexture = SDL_CreateTextureFromSurface(renderer, mPendingSurface);
    if (!mTexture)
        std::print("Image: failed to create texture: {}\n", SDL_GetError());
    else
        SDL_SetTextureBlendMode(mTexture, SDL_BLENDMODE_BLEND);
    SDL_DestroySurface(mPendingSurface);
    mPendingSurface = nullptr;
}

static void GetLogicalSize(SDL_Renderer* renderer, int& w, int& h) {
    // Get the LOGICAL window size — the coordinate space that all
    // SDL_Render* calls operate in when SetRenderLogicalPresentation
    // is active. We query the window directly because
    // SDL_GetCurrentRenderOutputSize may still return physical pixels
    // on some platforms/configurations.
    SDL_Window* win = SDL_GetRenderWindow(renderer);
    if (win) {
        SDL_GetWindowSize(win, &w, &h);
        return;
    }
    // Fallback if no window (software renderer, etc.)
    if (!SDL_GetCurrentRenderOutputSize(renderer, &w, &h))
        SDL_GetRenderOutputSize(renderer, &w, &h);
}

// =========================================================================
// ComputeDest - static (non-scrolling) fit modes
// =========================================================================

SDL_FRect Image::ComputeDest(int vw, int vh) const {
    if (mHasExplicitDest) return mExplicitDest;

    const float imgW = static_cast<float>(mOrigW);
    const float imgH = static_cast<float>(mOrigH);
    const float vpW  = static_cast<float>(vw);
    const float vpH  = static_cast<float>(vh);

    switch (mFitMode) {
        case FitMode::PRESCALED:
        case FitMode::STRETCH:
            return {0.0f, 0.0f, vpW, vpH};

        case FitMode::COVER: {
            float s = std::max(vpW / imgW, vpH / imgH);
            return {(vpW - imgW * s) * 0.5f, (vpH - imgH * s) * 0.5f, imgW * s, imgH * s};
        }

        case FitMode::FILL: {
            float cover = std::max(vpW / imgW, vpH / imgH);
            float s = (imgW >= vpW && imgH >= vpH) ? std::min(cover, 1.0f) : cover;
            return {(vpW - imgW * s) * 0.5f, (vpH - imgH * s) * 0.5f, imgW * s, imgH * s};
        }

        case FitMode::CONTAIN: {
            float s = std::min(vpW / imgW, vpH / imgH);
            return {(vpW - imgW * s) * 0.5f, (vpH - imgH * s) * 0.5f, imgW * s, imgH * s};
        }

        case FitMode::SRCSIZE:
        default:
            return {mExplicitDest.x, mExplicitDest.y, imgW, imgH};
    }
}

// =========================================================================
// Render - static modes
// =========================================================================

void Image::Render(SDL_Renderer* renderer) {
    if (!renderer) return;
    if (!mTexture) UploadSurface(renderer);
    if (!mTexture) return;

    int rw, rh;
    GetLogicalSize(renderer, rw, rh);
    SDL_FRect dst = ComputeDest(rw, rh);

    SDL_FlipMode flip = mFlipH ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
    SDL_RenderTextureRotated(renderer, mTexture, nullptr, &dst, 0.0, nullptr, flip);
}

// =========================================================================
// ScrollHelper - shared logic for SCROLL and SCROLL_WIDE
// =========================================================================
//
// Both scroll modes use the same rendering approach:
//
//   1. Scale the image so it FILLS the viewport (no black bars, ever).
//      This uses COVER-fit: scale = max(vpW/imgW, vpH/imgH).
//
//      - If the image is wider than the viewport ratio: height-fill wins,
//        the width extends past the viewport = horizontal scroll range.
//
//      - If the image is taller than the viewport ratio: width-fill wins,
//        height extends past viewport (excess cropped top/bottom, centred).
//        The width exactly matches the viewport = no horizontal scroll,
//        but the image fills the screen with no black bars.
//
//      - If the image exactly matches the viewport ratio: perfect fit,
//        no scroll, no crop.
//
//   2. Slide the dst rect horizontally based on camera position.
//
//      Gameplay (levelW > vpW):
//        t = cameraX / (levelW - vpW), maps [0..1] to [0..scrollRange]
//
//      Editor (levelW == 0):
//        Map camera world-space to background world-space. The background
//        occupies [0..imgW] in world units. The camera sees vpW/scale
//        world-units of width. So the camera's scrollable world range is
//        imgW - vpW/scale. Map that to the display scroll range.
//
// SCROLL vs SCROLL_WIDE difference:
//   SCROLL:      parallaxFactor = 1.0 (background scrolls at full speed)
//   SCROLL_WIDE: parallaxFactor = 0.5 (background scrolls at half speed,
//                creating a depth/parallax effect for extra-wide images)
//
// =========================================================================

static void ScrollHelper(SDL_Renderer* renderer, SDL_Texture* texture,
                         int origW, int origH, bool flipH, bool repeat,
                         float cameraX, float levelW,
                         float parallaxFactor) {
    int rendW, rendH;
    GetLogicalSize(renderer, rendW, rendH);

    const float vpW  = static_cast<float>(rendW);
    const float vpH  = static_cast<float>(rendH);
    const float imgW = static_cast<float>(origW);
    const float imgH = static_cast<float>(origH);
    if (imgW < 1.f || imgH < 1.f || vpW < 1.f || vpH < 1.f) return;

    // COVER-fit: always fill the viewport. No black bars ever.
    float scale = std::max(vpW / imgW, vpH / imgH);
    float dispW = imgW * scale;
    float dispH = imgH * scale;

    // Centre vertically (crops top/bottom equally for tall images).
    float offsetY = (vpH - dispH) * 0.5f;

    SDL_FlipMode flip = flipH ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;

    if (repeat) {
        // ── Tiling / infinite scroll ──────────────────────────────────────
        // The camera scrolls 1:1 with the background (scaled to display
        // coordinates). We compute which tile index is visible and draw
        // enough copies to fill the viewport.
        float camDisplay = cameraX * scale * parallaxFactor;

        // fmod to get the offset within one tile period.
        // Use fmod that works for negative values too.
        float raw = std::fmod(camDisplay, dispW);
        if (raw < 0.0f) raw += dispW;
        float startX = -raw;

        // Draw tiles from startX until we pass the right edge of the viewport.
        for (float x = startX; x < vpW; x += dispW) {
            SDL_FRect dst = {x, offsetY, dispW, dispH};
            SDL_RenderTextureRotated(renderer, texture, nullptr, &dst,
                                     0.0, nullptr, flip);
        }
    } else {
        // ── Single image, clamped scroll ──────────────────────────────────
        float scrollRange = dispW - vpW;
        float offsetX = 0.0f;

        if (scrollRange > 0.5f) {
            if (levelW > vpW) {
                float t = std::clamp(cameraX / (levelW - vpW), 0.0f, 1.0f);
                offsetX = -t * scrollRange * parallaxFactor;
            } else {
                float worldViewW     = vpW / scale;
                float worldScrollEnd = imgW - worldViewW;
                if (worldScrollEnd > 0.5f) {
                    float t = std::clamp(cameraX / worldScrollEnd, 0.0f, 1.0f);
                    offsetX = -t * scrollRange;
                }
            }
        }

        SDL_FRect dst = {offsetX, offsetY, dispW, dispH};
        SDL_RenderTextureRotated(renderer, texture, nullptr, &dst,
                                 0.0, nullptr, flip);
    }
}

// =========================================================================
// RenderScrolling  (SCROLL mode)
// =========================================================================
// Full-speed scrolling. Background scrolls 1:1 with camera.
// Image fills viewport (COVER-fit). Excess width scrolls horizontally.
// Excess height is cropped and centred vertically.
// =========================================================================

void Image::RenderScrolling(SDL_Renderer* renderer, float cameraX, float levelW) {
    if (!renderer) return;
    if (!mTexture) UploadSurface(renderer);
    if (!mTexture) return;
    ScrollHelper(renderer, mTexture, mOrigW, mOrigH, mFlipH, mRepeat,
                 cameraX, levelW, 1.0f);
}

// =========================================================================
// RenderScrollingWide  (SCROLL_WIDE mode)
// =========================================================================
// Half-speed parallax scrolling for extra-wide panoramic images.
// Image fills viewport (COVER-fit). Excess width scrolls at 50% speed
// relative to camera, creating a depth effect.
// =========================================================================

void Image::RenderScrollingWide(SDL_Renderer* renderer, float cameraX, float levelW) {
    if (!renderer) return;
    if (!mTexture) UploadSurface(renderer);
    if (!mTexture) return;
    ScrollHelper(renderer, mTexture, mOrigW, mOrigH, mFlipH, mRepeat,
                 cameraX, levelW, 0.5f);
}

// =========================================================================
// Setters / getters / utilities
// =========================================================================

void Image::SetDestinationRectangle(SDL_FRect dest) {
    mExplicitDest    = dest;
    mHasExplicitDest = true;
}

void    Image::SetFitMode(FitMode mode)      { mFitMode = mode; }
FitMode Image::GetFitMode() const            { return mFitMode; }
void    Image::SetFlipHorizontal(bool flip)   { mFlipH = flip; }
void    Image::SetRepeat(bool repeat)         { mRepeat = repeat; }
bool    Image::GetRepeat() const              { return mRepeat; }

void Image::SaveToFile(std::string Location) {
    if (mPendingSurface)
        IMG_SavePNG(mPendingSurface, Location.c_str());
}
