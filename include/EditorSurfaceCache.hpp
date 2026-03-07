#pragma once
// EditorSurfaceCache.hpp
// ---------------------------------------------------------------------------
// Owns and manages all cached SDL_Surface* resources used by the level editor:
//   - Tile surface cache (path -> surface for rendering tiles on canvas)
//   - Extra tile surfaces (loaded from subdirs not in current palette view)
//   - Rotation cache (path -> {90, 180, 270} pre-rotated surfaces)
//   - Badge cache (text+colour key -> pre-rendered badge surface)
//   - Destroy-anim thumbnail cache (JSON path -> 48x48 first-frame thumb)
//
// Also provides static utility functions for loading/scaling surfaces that
// are used by the palette, editor, and other subsystems.
// ---------------------------------------------------------------------------

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

    // Non-copyable, movable
    EditorSurfaceCache(const EditorSurfaceCache&)            = delete;
    EditorSurfaceCache& operator=(const EditorSurfaceCache&) = delete;
    EditorSurfaceCache(EditorSurfaceCache&&)                 = default;
    EditorSurfaceCache& operator=(EditorSurfaceCache&&)      = default;

    // ── Static utility functions ──────────────────────────────────────────────

    // Build a w x h thumbnail from a full-res SDL_Surface.
    // Returns nullptr on failure. Caller owns the result.
    static SDL_Surface* MakeThumb(SDL_Surface* src, int w, int h);

    // Load a PNG from disk, convert to ARGB8888, return it (caller owns).
    // Blend mode is set to BLEND for correct compositing.
    static SDL_Surface* LoadPNG(const std::filesystem::path& p);

    // ── Tile surface cache ────────────────────────────────────────────────────
    // Fast path->surface lookup for rendering tiles on the canvas.

    SDL_Surface* FindTileSurface(const std::string& path) const;
    void         InsertTileSurface(const std::string& path, SDL_Surface* surf);
    bool         HasTileSurface(const std::string& path) const;
    void         ClearTileSurfaceCache();

    // ── Extra tile surfaces ───────────────────────────────────────────────────
    // Surfaces loaded for level tiles from subdirs not in the current palette.
    // Owned here, freed on clear/destruction.

    void AddExtraTileSurface(SDL_Surface* surf);
    void ClearExtraTileSurfaces();

    // ── Rotation cache ────────────────────────────────────────────────────────
    // For each image path, pre-built surfaces for 90/180/270 degrees.
    // Built lazily on first use.

    SDL_Surface* GetRotated(const std::string& path, SDL_Surface* src, int deg);

    // ── Badge cache ───────────────────────────────────────────────────────────
    // Pre-rendered text badges (P, L, A, F, H, slope markers, etc.)
    // Keyed by "text_RRGGBB". Built lazily on first use.

    SDL_Surface* GetBadge(const std::string& text, SDL_Color col);

    // ── Destroy-anim thumbnail cache ──────────────────────────────────────────
    // Maps animated tile JSON path -> 48x48 first-frame thumbnail.
    // Built lazily on first use.

    SDL_Surface* GetDestroyAnimThumb(const std::string& jsonPath);

    // ── Bulk cleanup ──────────────────────────────────────────────────────────

    // Frees ALL cached surfaces. Called by LevelEditorScene::Unload().
    void Clear();

  private:
    // path -> surface (non-owning for palette items, owning for extra surfaces)
    std::unordered_map<std::string, SDL_Surface*> mTileSurfaceCache;

    // Owned surfaces loaded for level tiles from subdirs
    std::vector<SDL_Surface*> mExtraTileSurfaces;

    // path -> {90, 180, 270} rotated surfaces
    std::unordered_map<std::string, std::array<SDL_Surface*, 3>> mRotCache;

    // "text_RRGGBB" -> pre-rendered badge surface
    std::unordered_map<std::string, SDL_Surface*> mBadgeCache;

    // JSON path -> 48x48 thumbnail surface
    std::unordered_map<std::string, SDL_Surface*> mDestroyAnimThumbCache;
};
