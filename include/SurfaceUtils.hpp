#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <cmath>
#include <string>

/// Rotates an SDL_Surface 90 degrees clockwise.
/// Returns a new surface — caller is responsible for freeing it.
inline SDL_Surface* RotateSurface90CW(SDL_Surface* src) {
    SDL_Surface* dst = SDL_CreateSurface(src->h, src->w, src->format);
    SDL_SetSurfaceBlendMode(dst, SDL_BLENDMODE_BLEND);
    SDL_LockSurface(src);
    SDL_LockSurface(dst);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            Uint32* srcPx = reinterpret_cast<Uint32*>(static_cast<Uint8*>(src->pixels) +
                                                      y * src->pitch + x * 4);
            Uint32* dstPx = reinterpret_cast<Uint32*>(static_cast<Uint8*>(dst->pixels) +
                                                      x * dst->pitch + (src->h - 1 - y) * 4);
            *dstPx        = *srcPx;
        }
    }
    SDL_UnlockSurface(src);
    SDL_UnlockSurface(dst);
    return dst;
}

/// Rotates an SDL_Surface 90 degrees counter-clockwise.
/// Returns a new surface — caller is responsible for freeing it.
inline SDL_Surface* RotateSurface90CCW(SDL_Surface* src) {
    SDL_Surface* dst = SDL_CreateSurface(src->h, src->w, src->format);
    SDL_SetSurfaceBlendMode(dst, SDL_BLENDMODE_BLEND);
    SDL_LockSurface(src);
    SDL_LockSurface(dst);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            Uint32* srcPx = reinterpret_cast<Uint32*>(static_cast<Uint8*>(src->pixels) +
                                                      y * src->pitch + x * 4);
            Uint32* dstPx = reinterpret_cast<Uint32*>(static_cast<Uint8*>(dst->pixels) +
                                                      (src->w - 1 - x) * dst->pitch + y * 4);
            *dstPx        = *srcPx;
        }
    }
    SDL_UnlockSurface(src);
    SDL_UnlockSurface(dst);
    return dst;
}

/// Rotates an SDL_Surface 180 degrees.
/// Returns a new surface — caller is responsible for freeing it.
inline SDL_Surface* RotateSurface180(SDL_Surface* src) {
    SDL_Surface* dst = SDL_CreateSurface(src->w, src->h, src->format);
    SDL_SetSurfaceBlendMode(dst, SDL_BLENDMODE_BLEND);
    SDL_LockSurface(src);
    SDL_LockSurface(dst);
    for (int y = 0; y < src->h; y++) {
        for (int x = 0; x < src->w; x++) {
            Uint32* srcPx = reinterpret_cast<Uint32*>(static_cast<Uint8*>(src->pixels) +
                                                      y * src->pitch + x * 4);
            Uint32* dstPx = reinterpret_cast<Uint32*>(static_cast<Uint8*>(dst->pixels) +
                                                      (src->h - 1 - y) * dst->pitch +
                                                      (src->w - 1 - x) * 4);
            *dstPx        = *srcPx;
        }
    }
    SDL_UnlockSurface(src);
    SDL_UnlockSurface(dst);
    return dst;
}

/// Applies a 0/90/180/270-degree clockwise rotation to a surface.
/// Returns a new owned surface for non-zero rotations, nullptr for 0.
/// Caller owns the returned surface.
/// NOTE: must appear after all three helpers above.
inline SDL_Surface* RotateSurfaceDeg(SDL_Surface* src, int degrees) {
    switch (((degrees % 360) + 360) % 360) {
        case 90:  return RotateSurface90CW(src);
        case 180: return RotateSurface180(src);
        case 270: return RotateSurface90CCW(src);
        default:  return nullptr; // 0 — use original
    }
}

/// Rotates an SDL_Surface by an arbitrary angle (degrees, clockwise).
/// The output surface is the same size as the input; pixels outside the
/// rotated bounds are transparent.  Uses nearest-neighbour sampling.
/// Returns a new surface — caller is responsible for freeing it.
inline SDL_Surface* RotateSurfaceAngle(SDL_Surface* src, float angleDeg) {
    const float rad  = angleDeg * 3.14159265f / 180.0f;
    const float cosA = std::cos(-rad);
    const float sinA = std::sin(-rad);
    const int   w    = src->w;
    const int   h    = src->h;
    const float cx   = w * 0.5f;
    const float cy   = h * 0.5f;

    SDL_Surface* dst = SDL_CreateSurface(w, h, src->format);
    SDL_SetSurfaceBlendMode(dst, SDL_BLENDMODE_BLEND);
    SDL_FillSurfaceRect(dst, nullptr, SDL_MapRGBA(
        SDL_GetPixelFormatDetails(dst->format), nullptr, 0, 0, 0, 0));

    SDL_LockSurface(src);
    SDL_LockSurface(dst);
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            float fx = (dx - cx) * cosA - (dy - cy) * sinA + cx;
            float fy = (dx - cx) * sinA + (dy - cy) * cosA + cy;
            int   sx = (int)(fx + 0.5f);
            int   sy = (int)(fy + 0.5f);
            if (sx < 0 || sx >= w || sy < 0 || sy >= h) continue;
            Uint32* sp = reinterpret_cast<Uint32*>(
                static_cast<Uint8*>(src->pixels) + sy * src->pitch + sx * 4);
            Uint32* dp = reinterpret_cast<Uint32*>(
                static_cast<Uint8*>(dst->pixels) + dy * dst->pitch + dx * 4);
            *dp = *sp;
        }
    }
    SDL_UnlockSurface(src);
    SDL_UnlockSurface(dst);
    return dst;
}

/// Returns the x position needed to horizontally center text within a container.
inline int CenterTextX(TTF_Font*          font,
                       const std::string& text,
                       int                containerX,
                       int                containerW) {
    int w = 0;
    TTF_GetStringSize(font, text.c_str(), 0, &w, nullptr);
    return containerX + (containerW - w) / 2;
}

/// Returns the y position needed to vertically center text within a container.
inline int CenterTextY(TTF_Font* font, int containerY, int containerH) {
    int h = 0;
    TTF_GetStringSize(font, "A", 0, nullptr, &h);
    return containerY + (containerH - h) / 2;
}

/// Returns both x and y to center text within a rectangle.
inline SDL_Point CenterTextInRect(TTF_Font*          font,
                                  const std::string& text,
                                  const SDL_Rect&    rect) {
    int w = 0, h = 0;
    TTF_GetStringSize(font, text.c_str(), 0, &w, &h);
    return {rect.x + (rect.w - w) / 2, rect.y + (rect.h - h) / 2};
}

/// Returns an SDL_Rect centered within a container rect.
inline SDL_Rect CenterRect(const SDL_Rect& container, int w, int h, int offsetY = 0) {
    return {container.x + (container.w - w) / 2,
            container.y + (container.h - h) / 2 + offsetY,
            w,
            h};
}
