#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <entt/entt.hpp>
#include <vector>

// camX/camY: world-space top-left of the viewport.
// All world positions are offset by (-camX, -camY) before rendering.
// Entities whose bounding rect lies fully outside the viewport are culled.
//
// vw/vh: logical viewport dimensions. Pass Window::GetWidth()/GetHeight() to
//        avoid an SDL driver query every frame. Leave at 0 to auto-query.
//
// sortedTiles: pre-sorted tile entity list built once in Spawn() and stored on
//        GameScene. Passing it eliminates the per-frame heap alloc + sort for
//        Pass 1. Pass nullptr to fall back to building the list inline (used by
//        scenes that don't maintain their own sorted list, e.g. the editor).
inline void RenderSystem(entt::registry& reg, SDL_Renderer* renderer,
                         float camX = 0.0f, float camY = 0.0f,
                         int vw = 0, int vh = 0,
                         const std::vector<entt::entity>* sortedTiles = nullptr) {
    if (vw == 0 || vh == 0)
        SDL_GetRenderOutputSize(renderer, &vw, &vh);

    auto culled = [&](float wx, float wy, int w, int h) -> bool {
        return wx + w  <= camX      ||
               wx      >= camX + vw ||
               wy + h  <= camY      ||
               wy      >= camY + vh;
    };

    // Pass 1: tiles in strict spawn order
    // Uses the pre-sorted list from GameScene::Spawn() when available to avoid
    // a per-frame allocation + sort. Falls back to building inline when called
    // from scenes that don't maintain a sorted list (e.g. editor previews).
    {
        std::vector<entt::entity> localTiles;
        const std::vector<entt::entity>* tiles = sortedTiles;
        if (!tiles) {
            auto tileView   = reg.view<Transform, Renderable, AnimationState, TileTag>();
            auto ladderView = reg.view<Transform, Renderable, AnimationState, LadderTag>();
            auto propView   = reg.view<Transform, Renderable, AnimationState, PropTag>();
            localTiles.reserve(tileView.size_hint() + ladderView.size_hint() + propView.size_hint());
            for (auto e : tileView)   localTiles.push_back(e);
            for (auto e : ladderView) localTiles.push_back(e);
            for (auto e : propView)   localTiles.push_back(e);
            std::sort(localTiles.begin(), localTiles.end());
            tiles = &localTiles;
        }

        for (auto entity : *tiles) {
            // Guard: action tiles can be destroyed mid-level; the sorted list
            // isn't pruned until the next Spawn(), so we must validate here.
            if (!reg.valid(entity)) continue;

            auto* rp   = reg.try_get<Renderable>(entity);
            auto* tp   = reg.try_get<Transform>(entity);
            auto* anip = reg.try_get<AnimationState>(entity);
            if (!rp || !tp || !anip) continue;

            auto& t    = *tp;
            auto& r    = *rp;
            auto& anim = *anip;
            if (!r.sheet || r.frames.empty()) continue;

            const SDL_Rect& src = r.frames[anim.currentFrame];
            if (culled(t.x, t.y, src.w, src.h)) continue;

            SDL_FRect dst = {t.x - camX, t.y - camY, (float)src.w, (float)src.h};

            // Spin rotation for floating tiles
            double angle = 0.0;
            if (const auto* fs = reg.try_get<FloatState>(entity))
                angle = (double)fs->spinAngle;

            SDL_FRect srcF = {(float)src.x, (float)src.y, (float)src.w, (float)src.h};
            SDL_RenderTextureRotated(renderer, r.sheet, &srcF, &dst, angle, nullptr, SDL_FLIP_NONE);

            // HitFlash overlay
            if (const auto* hf = reg.try_get<HitFlash>(entity)) {
                float frac  = hf->timer / hf->duration;
                Uint8 alpha = (Uint8)(frac * 160.0f);
                SDL_SetRenderDrawColor(renderer, 220, 30, 30, alpha);
                SDL_RenderFillRect(renderer, &dst);
            }
        }
    }

    // Pass 2: player, enemies, coins
    auto view = reg.view<Transform, Renderable, AnimationState>(
        entt::exclude<TileTag, LadderTag, PropTag>);

    view.each([&](entt::entity entity, const Transform& t, Renderable& r,
                  const AnimationState& anim) {
        if (!r.sheet || r.frames.empty()) return;

        const SDL_Rect& src = r.frames[anim.currentFrame];
        if (culled(t.x, t.y, src.w, src.h)) return;

        auto* g    = reg.try_get<GravityState>(entity);
        auto* inv  = reg.try_get<InvincibilityTimer>(entity);
        auto* hz   = reg.try_get<HazardState>(entity);
        auto* col  = reg.try_get<Collider>(entity);
        auto* roff = reg.try_get<RenderOffset>(entity);

        // Colour mod for invincibility / hazard flash
        bool colorModded = false;
        if (inv && inv->isInvincible && (int)(inv->remaining * 10.0f) % 2 == 0) {
            SDL_SetTextureColorMod(r.sheet, 255, 0, 0);
            colorModded = true;
        } else if (hz && hz->active && (int)(hz->flashTimer * 8.0f) % 2 == 0) {
            SDL_SetTextureColorMod(r.sheet, 255, 80, 80);
            colorModded = true;
        }

        // Flip / rotation flags
        SDL_FlipMode flip  = r.flipH ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE;
        double       angle = 0.0;

        if (g && g->active) {
            switch (g->direction) {
                case GravityDir::DOWN:  break;
                case GravityDir::UP:    angle = 180.0; break;
                case GravityDir::RIGHT: angle = 90.0;  break;
                case GravityDir::LEFT:  angle = -90.0; break;
            }
        }

        // Render position
        float rx = t.x - camX;
        float ry = t.y - camY;

        if (g && g->active && col) {
            switch (g->direction) {
                case GravityDir::DOWN:
                    if (roff) { rx += roff->x; ry += roff->y; }
                    break;
                case GravityDir::UP:
                    rx += roff ? roff->x : -(src.w - col->w) / 2;
                    ry += roff ? roff->y : 0;
                    break;
                case GravityDir::LEFT:
                    rx += roff ? roff->y : 0;
                    ry += roff ? roff->x : -(src.h - col->h) / 2;
                    break;
                case GravityDir::RIGHT:
                    rx  = (t.x - camX) + col->h - src.w - (roff ? roff->y : 0);
                    ry += roff ? roff->x : -(src.h - col->h) / 2;
                    break;
            }
        } else {
            if (roff) { rx += roff->x; ry += roff->y; }
        }

        SDL_FRect srcF = {(float)src.x, (float)src.y, (float)src.w, (float)src.h};
        SDL_FRect dst  = {rx, ry, (float)src.w, (float)src.h};
        SDL_RenderTextureRotated(renderer, r.sheet, &srcF, &dst, angle, nullptr, flip);

        if (colorModded)
            SDL_SetTextureColorMod(r.sheet, 255, 255, 255);
    });
}
