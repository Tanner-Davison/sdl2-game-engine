#include "EditorSurfaceCache.hpp"
#include "AnimatedTile.hpp"
#include "SurfaceUtils.hpp"
#include "Text.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Destructor — frees all owned surfaces
// ---------------------------------------------------------------------------
EditorSurfaceCache::~EditorSurfaceCache() { Clear(); }

// ---------------------------------------------------------------------------
// Static utilities
// ---------------------------------------------------------------------------

SDL_Surface* EditorSurfaceCache::MakeThumb(SDL_Surface* src, int w, int h) {
    SDL_Surface* t = SDL_CreateSurface(w, h, SDL_PIXELFORMAT_ARGB8888);
    if (!t)
        return nullptr;
    SDL_SetSurfaceBlendMode(t, SDL_BLENDMODE_NONE);
    // Set source to BLENDMODE_NONE so raw RGBA (including alpha) is copied as-is.
    // Without this SDL composites src onto dst and bakes alpha to 255, making
    // transparent tiles render as opaque in the editor canvas.
    SDL_BlendMode srcMode;
    SDL_GetSurfaceBlendMode(src, &srcMode);
    SDL_SetSurfaceBlendMode(src, SDL_BLENDMODE_NONE);
    SDL_Rect sr = {0, 0, src->w, src->h};
    SDL_Rect dr = {0, 0, w, h};
    SDL_BlitSurfaceScaled(src, &sr, t, &dr, SDL_SCALEMODE_LINEAR);
    SDL_SetSurfaceBlendMode(src, srcMode);
    SDL_SetSurfaceBlendMode(t, SDL_BLENDMODE_BLEND);
    return t;
}

SDL_Surface* EditorSurfaceCache::LoadPNG(const fs::path& p) {
    SDL_Surface* raw = IMG_Load(p.string().c_str());
    if (!raw)
        return nullptr;
    SDL_Surface* c = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(raw);
    if (c)
        SDL_SetSurfaceBlendMode(c, SDL_BLENDMODE_BLEND);
    return c;
}

// ---------------------------------------------------------------------------
// Tile surface cache
// ---------------------------------------------------------------------------

SDL_Surface* EditorSurfaceCache::FindTileSurface(const std::string& path) const {
    auto it = mTileSurfaceCache.find(path);
    return (it != mTileSurfaceCache.end()) ? it->second : nullptr;
}

void EditorSurfaceCache::InsertTileSurface(const std::string& path, SDL_Surface* surf) {
    mTileSurfaceCache[path] = surf;
}

bool EditorSurfaceCache::HasTileSurface(const std::string& path) const {
    return mTileSurfaceCache.count(path) > 0;
}

void EditorSurfaceCache::ClearTileSurfaceCache() { mTileSurfaceCache.clear(); }

// ---------------------------------------------------------------------------
// Extra tile surfaces
// ---------------------------------------------------------------------------

void EditorSurfaceCache::AddExtraTileSurface(SDL_Surface* surf) {
    mExtraTileSurfaces.push_back(surf);
}

void EditorSurfaceCache::ClearExtraTileSurfaces() {
    for (auto* s : mExtraTileSurfaces)
        if (s)
            SDL_DestroySurface(s);
    mExtraTileSurfaces.clear();
}

// ---------------------------------------------------------------------------
// Rotation cache
// ---------------------------------------------------------------------------

SDL_Surface* EditorSurfaceCache::GetRotated(const std::string& path,
                                            SDL_Surface*       src,
                                            int                deg) {
    int   slot  = (deg / 90) - 1; // 90->0, 180->1, 270->2
    auto& entry = mRotCache[path]; // default-init: {nullptr,nullptr,nullptr}
    if (!entry[slot])
        entry[slot] = RotateSurfaceDeg(src, deg);
    return entry[slot];
}

// ---------------------------------------------------------------------------
// Badge cache
// ---------------------------------------------------------------------------

SDL_Surface* EditorSurfaceCache::GetBadge(const std::string& text, SDL_Color col) {
    char key[64];
    SDL_snprintf(key, sizeof(key), "%s_%02x%02x%02x", text.c_str(), col.r, col.g, col.b);
    auto it = mBadgeCache.find(key);
    if (it != mBadgeCache.end())
        return it->second;
    Text         t(text, col, 0, 0, 10);
    SDL_Surface* surf = t.GetSurface();
    if (!surf) {
        mBadgeCache[key] = nullptr;
        return nullptr;
    }
    SDL_Surface* owned = SDL_DuplicateSurface(surf);
    mBadgeCache[key]   = owned;
    return owned;
}

// ---------------------------------------------------------------------------
// Destroy-anim thumbnail cache
// ---------------------------------------------------------------------------

SDL_Surface* EditorSurfaceCache::GetDestroyAnimThumb(const std::string& jsonPath) {
    auto it = mDestroyAnimThumbCache.find(jsonPath);
    if (it != mDestroyAnimThumbCache.end())
        return it->second;

    AnimatedTileDef def;
    if (!LoadAnimatedTileDef(jsonPath, def) || def.framePaths.empty()) {
        mDestroyAnimThumbCache[jsonPath] = nullptr;
        return nullptr;
    }
    SDL_Surface* result = nullptr;
    for (const auto& fp : def.framePaths) {
        SDL_Surface* raw = IMG_Load(fp.c_str());
        if (!raw)
            continue;
        SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        if (!conv)
            continue;
        SDL_Surface* thumb = SDL_CreateSurface(48, 48, SDL_PIXELFORMAT_ARGB8888);
        if (thumb) {
            SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_NONE);
            SDL_BlitSurfaceScaled(conv, nullptr, thumb, nullptr, SDL_SCALEMODE_LINEAR);
            SDL_SetSurfaceBlendMode(thumb, SDL_BLENDMODE_BLEND);
        }
        SDL_DestroySurface(conv);
        result = thumb;
        break;
    }
    mDestroyAnimThumbCache[jsonPath] = result;
    return result;
}

// ---------------------------------------------------------------------------
// Bulk cleanup
// ---------------------------------------------------------------------------

void EditorSurfaceCache::Clear() {
    // Rotation cache
    for (auto& [path, arr] : mRotCache)
        for (auto* s : arr)
            if (s)
                SDL_DestroySurface(s);
    mRotCache.clear();

    // Badge cache
    for (auto& [key, s] : mBadgeCache)
        if (s)
            SDL_DestroySurface(s);
    mBadgeCache.clear();

    // Destroy-anim thumbnail cache
    for (auto& [path, s] : mDestroyAnimThumbCache)
        if (s)
            SDL_DestroySurface(s);
    mDestroyAnimThumbCache.clear();

    // Extra tile surfaces (owned)
    ClearExtraTileSurfaces();

    // Tile surface cache (non-owning pointers — just clear the map)
    mTileSurfaceCache.clear();
}
