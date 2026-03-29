#pragma once
// EditorSurfaceCache.hpp — cached SDL_Surface* resources for the level editor
// (tile surfaces, rotations, badges, destroy-anim thumbnails).

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class EditorSurfaceCache {
  public:
    EditorSurfaceCache() = default;
    ~EditorSurfaceCache();

    EditorSurfaceCache(const EditorSurfaceCache&)            = delete;
    EditorSurfaceCache& operator=(const EditorSurfaceCache&) = delete;
    EditorSurfaceCache(EditorSurfaceCache&&)                 = default;
    EditorSurfaceCache& operator=(EditorSurfaceCache&&)      = default;

    // Returns nullptr on failure. Caller owns the result.
    static SDL_Surface* MakeThumb(SDL_Surface* src, int w, int h);

    // Load a PNG, convert to ARGB8888, set blend mode. Caller owns the result.
    static SDL_Surface* LoadPNG(const std::filesystem::path& p);

    SDL_Surface* FindTileSurface(const std::string& path) const;
    void         InsertTileSurface(const std::string& path, SDL_Surface* surf);
    bool         HasTileSurface(const std::string& path) const;
    void         ClearTileSurfaceCache();

    // Returns cached surface, loading from disk on first call for a given path.
    SDL_Surface* LoadAndCache(const std::string& path);

    void AddExtraTileSurface(SDL_Surface* surf);
    void ClearExtraTileSurfaces();

    // Lazily builds 90/180/270 rotated surfaces for a given path.
    SDL_Surface* GetRotated(const std::string& path, SDL_Surface* src, int deg);

    // Keyed by "text_RRGGBB". Built lazily.
    SDL_Surface* GetBadge(const std::string& text, SDL_Color col);

    // Maps animated tile JSON path -> 48x48 first-frame thumbnail. Built lazily.
    SDL_Surface* GetDestroyAnimThumb(const std::string& jsonPath);

    void Clear();

  private:
    // path -> surface (non-owning for palette items, owning for extra surfaces)
    std::unordered_map<std::string, SDL_Surface*> mTileSurfaceCache;

    std::vector<SDL_Surface*> mExtraTileSurfaces;

    std::unordered_map<std::string, std::array<SDL_Surface*, 3>> mRotCache;

    std::unordered_map<std::string, SDL_Surface*> mBadgeCache;

    std::unordered_map<std::string, SDL_Surface*> mDestroyAnimThumbCache;
};
