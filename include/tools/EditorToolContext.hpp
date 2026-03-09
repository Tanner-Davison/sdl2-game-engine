#pragma once
// EditorToolContext.hpp
//
// Non-owning view of the shared editor state that every tool needs.
// Passed by reference into tool methods so tools can read/write level data,
// query camera transforms, update the status bar, and access caches
// without knowing about LevelEditorScene directly.
//
// Design: tools never include LevelEditorScene.hpp -- they only see this
// lightweight context. This breaks the circular dependency chain and keeps
// each tool's compilation unit small.

#include "EditorCamera.hpp"
#include "EditorSurfaceCache.hpp"
#include "Level.hpp"
#include "Text.hpp"
#include <SDL3/SDL.h>
#include <functional>
#include <memory>
#include <string>

struct EditorToolContext {
    // ── Level data (read/write) ──────────────────────────────────────────────
    Level& level;

    // ── Camera (read/write for pan, read for coord transforms) ───────────────
    EditorCamera& camera;

    // ── Surface cache (read for thumbnails, badges, rotations) ───────────────
    EditorSurfaceCache& surfaceCache;

    // ── Layout constants ─────────────────────────────────────────────────────
    int grid     = 38;
    int toolbarH = 48;

    // ── Status callback — tool calls this to update the bottom status bar ────
    std::function<void(const std::string&)> setStatus;

    // ── Canvas width callback — depends on palette collapsed state ────────────
    std::function<int()> canvasW;

    // ── SDL window handle — needed for SDL_StartTextInput / SDL_StopTextInput ─
    SDL_Window* sdlWindow = nullptr;

    // ── Convenience wrappers ─────────────────────────────────────────────────

    [[nodiscard]] SDL_Point ScreenToWorld(int sx, int sy) const {
        return camera.ScreenToWorld(sx, sy);
    }
    [[nodiscard]] SDL_Point WorldToScreen(float wx, float wy) const {
        return camera.WorldToScreen(wx, wy);
    }
    [[nodiscard]] SDL_Point SnapToGrid(int sx, int sy) const {
        return camera.SnapToGrid(sx, sy, grid);
    }
    [[nodiscard]] float Zoom() const { return camera.Zoom(); }
    [[nodiscard]] float CamX() const { return camera.X(); }
    [[nodiscard]] float CamY() const { return camera.Y(); }
    [[nodiscard]] int   CanvasW() const { return canvasW ? canvasW() : 800; }
    [[nodiscard]] int   ToolbarH() const { return toolbarH; }
    [[nodiscard]] int   Grid() const { return grid; }

    void SetStatus(const std::string& msg) {
        if (setStatus) setStatus(msg);
    }

    // ── Hit-testing helpers ──────────────────────────────────────────────────

    [[nodiscard]] static bool HitTest(const SDL_Rect& r, int x, int y) {
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    }

    [[nodiscard]] int HitTile(int sx, int sy) const {
        auto [wx, wy] = ScreenToWorld(sx, sy);
        for (int i = static_cast<int>(level.tiles.size()) - 1; i >= 0; --i) {
            const auto& t = level.tiles[i];
            SDL_Rect    r = {static_cast<int>(t.x), static_cast<int>(t.y), t.w, t.h};
            if (HitTest(r, wx, wy)) return i;
        }
        return -1;
    }

    [[nodiscard]] int HitCoin(int sx, int sy) const {
        auto [wx, wy] = ScreenToWorld(sx, sy);
        for (int i = 0; i < static_cast<int>(level.coins.size()); ++i) {
            SDL_Rect r = {static_cast<int>(level.coins[i].x),
                          static_cast<int>(level.coins[i].y), grid, grid};
            if (HitTest(r, wx, wy)) return i;
        }
        return -1;
    }

    [[nodiscard]] int HitEnemy(int sx, int sy) const {
        auto [wx, wy] = ScreenToWorld(sx, sy);
        for (int i = 0; i < static_cast<int>(level.enemies.size()); ++i) {
            SDL_Rect r = {static_cast<int>(level.enemies[i].x),
                          static_cast<int>(level.enemies[i].y), grid, grid};
            if (HitTest(r, wx, wy)) return i;
        }
        return -1;
    }

    // ── Surface drawing helpers (thin wrappers so tools can render overlays) ─

    static void DrawRect(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
        const auto* fmt = SDL_GetPixelFormatDetails(s->format);
        SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
    }

    static void DrawRectAlpha(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
        if (r.w <= 0 || r.h <= 0) return;
        SDL_Surface* ov = SDL_CreateSurface(r.w, r.h, SDL_PIXELFORMAT_ARGB8888);
        if (!ov) return;
        SDL_SetSurfaceBlendMode(ov, SDL_BLENDMODE_BLEND);
        const auto* fmt = SDL_GetPixelFormatDetails(ov->format);
        SDL_FillSurfaceRect(ov, nullptr, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
        SDL_BlitSurface(ov, nullptr, s, &r);
        SDL_DestroySurface(ov);
    }

    static void DrawOutline(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1) {
        const auto* fmt = SDL_GetPixelFormatDetails(s->format);
        Uint32      col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
        SDL_Rect    rects[4] = {
            {r.x, r.y, r.w, t},
            {r.x, r.y + r.h, r.w, t},
            {r.x, r.y, t, r.h},
            {r.x + r.w, r.y, t, r.h}
        };
        for (auto& rr : rects) SDL_FillSurfaceRect(s, &rr, col);
    }

    SDL_Surface* GetBadge(const std::string& text, SDL_Color col) {
        return surfaceCache.GetBadge(text, col);
    }
};
