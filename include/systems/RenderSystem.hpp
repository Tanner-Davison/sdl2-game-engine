#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <SurfaceUtils.hpp>
#include <algorithm>
#include <cmath>
#include <entt/entt.hpp>
#include <vector>

// camX/camY: world-space position of the top-left corner of the viewport.
// All world positions are offset by (-camX, -camY) before blitting.
// Entities whose bounding rect lies fully outside the viewport are culled.
inline void RenderSystem(entt::registry& reg, SDL_Surface* screen,
                         float camX = 0.0f, float camY = 0.0f) {
    const int vw = screen->w;
    const int vh = screen->h;

    // Cache format details once per frame — same for all entities
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(screen->format);

    // Returns true when a world-space rect is completely off-screen.
    auto culled = [&](float wx, float wy, int w, int h) -> bool {
        return wx + w  <= camX      ||
               wx      >= camX + vw ||
               wy + h  <= camY      ||
               wy      >= camY + vh;
    };

    // ── Pass 1: tiles in strict spawn order ──────────────────────────────────
    {
        auto tileView   = reg.view<Transform, Renderable, AnimationState, TileTag>();
        auto ladderView = reg.view<Transform, Renderable, AnimationState, LadderTag>();
        auto propView   = reg.view<Transform, Renderable, AnimationState, PropTag>();

        std::vector<entt::entity> tiles;
        tiles.reserve(tileView.size_hint() + ladderView.size_hint() + propView.size_hint());
        for (auto e : tileView)   tiles.push_back(e);
        for (auto e : ladderView) tiles.push_back(e);
        for (auto e : propView)   tiles.push_back(e);
        std::sort(tiles.begin(), tiles.end());

        for (auto entity : tiles) {
            auto& t    = reg.get<Transform>(entity);
            auto& r    = reg.get<Renderable>(entity);
            auto& anim = reg.get<AnimationState>(entity);
            if (r.frames.empty()) continue;
            if (culled(t.x, t.y, r.frames[anim.currentFrame].w,
                                  r.frames[anim.currentFrame].h)) continue;

            const SDL_Rect& src  = r.frames[anim.currentFrame];
            int sx = (int)(t.x - camX);
            int sy = (int)(t.y - camY);
            SDL_Rect dest = {sx, sy, src.w, src.h};

            if (const auto* fs = reg.try_get<FloatState>(entity);
                fs && std::abs(fs->spinAngle) > 0.1f) {
                SDL_Surface* rotated = RotateSurfaceDeg(r.sheet, (int)fs->spinAngle);
                if (rotated) {
                    SDL_BlitSurfaceScaled(rotated, nullptr, screen, &dest, SDL_SCALEMODE_LINEAR);
                    SDL_DestroySurface(rotated);
                } else {
                    SDL_BlitSurface(r.sheet, &src, screen, &dest);
                }
            } else {
                SDL_BlitSurface(r.sheet, &src, screen, &dest);
            }

            // HitFlash: overlay a pulsing transparent red rect on struck action tiles
            if (const auto* hf = reg.try_get<HitFlash>(entity)) {
                // Pulse: fade from ~160 alpha at hit to 0 at expiry
                float frac = hf->timer / hf->duration;  // 1.0 -> 0.0
                Uint8 alpha = static_cast<Uint8>(frac * 160.0f);
                SDL_Surface* ov = SDL_CreateSurface(dest.w, dest.h, SDL_PIXELFORMAT_ARGB8888);
                if (ov) {
                    SDL_SetSurfaceBlendMode(ov, SDL_BLENDMODE_BLEND);
                    const SDL_PixelFormatDetails* ovf = SDL_GetPixelFormatDetails(ov->format);
                    SDL_FillSurfaceRect(ov, nullptr, SDL_MapRGBA(ovf, nullptr, 220, 30, 30, alpha));
                    SDL_BlitSurface(ov, nullptr, screen, &dest);
                    SDL_DestroySurface(ov);
                }
            }
        }
    }

    // ── Pass 2: player, enemies, coins — always on top of tiles ─────────────
    auto view = reg.view<Transform, Renderable, AnimationState>(
        entt::exclude<TileTag, LadderTag, PropTag>);

    view.each([&](entt::entity entity, const Transform& t, Renderable& r,
                  const AnimationState& anim) {
        if (r.frames.empty()) return;

        const SDL_Rect& src = r.frames[anim.currentFrame];

        // Sprite size for culling — use the raw src rect; rotated frames are the same area
        if (culled(t.x, t.y, src.w, src.h)) return;

        auto* g   = reg.try_get<GravityState>(entity);
        auto* inv = reg.try_get<InvincibilityTimer>(entity);
        auto* col = reg.try_get<Collider>(entity);

        bool needsFlip     = r.flipH;
        bool needsRotation = g && g->active && g->direction != GravityDir::DOWN;

        if (!needsFlip && !needsRotation) {
            auto* hz   = reg.try_get<HazardState>(entity);
            if (inv && inv->isInvincible) {
                if (static_cast<int>(inv->remaining * 10.0f) % 2 == 0)
                    SDL_SetSurfaceColorMod(r.sheet, 255, 0, 0);
            } else if (hz && hz->active) {
                if (static_cast<int>(hz->flashTimer * 8.0f) % 2 == 0)
                    SDL_SetSurfaceColorMod(r.sheet, 255, 80, 80);
            }

            auto* roff = reg.try_get<RenderOffset>(entity);
            int   rx   = (int)(t.x - camX) + (roff ? roff->x : 0);
            int   ry   = (int)(t.y - camY) + (roff ? roff->y : 0);
            SDL_Rect dest = {rx, ry, src.w, src.h};
            SDL_BlitSurface(r.sheet, &src, screen, &dest);
            SDL_SetSurfaceColorMod(r.sheet, 255, 255, 255);
            return;
        }

        // Slow path: flip / rotation
        SDL_Surface* frame    = SDL_CreateSurface(src.w, src.h, r.sheet->format);
        bool         ownFrame = true;
        SDL_SetSurfaceBlendMode(frame, SDL_BLENDMODE_BLEND);
        SDL_BlitSurface(r.sheet, &src, frame, nullptr);

        if (r.flipH) {
            auto* cache = reg.try_get<FlipCache>(entity);
            if (cache && static_cast<int>(cache->frames.size()) != anim.totalFrames) {
                for (auto* s : cache->frames) if (s) SDL_DestroySurface(s);
                cache->frames.assign(anim.totalFrames, nullptr);
            }
            if (!cache) {
                reg.emplace<FlipCache>(entity);
                cache = reg.try_get<FlipCache>(entity);
                cache->frames.resize(anim.totalFrames, nullptr);
            }
            int idx = anim.currentFrame;
            if (!cache->frames[idx]) {
                SDL_Surface* flipped = SDL_CreateSurface(frame->w, frame->h, frame->format);
                SDL_SetSurfaceBlendMode(flipped, SDL_BLENDMODE_BLEND);
                SDL_LockSurface(frame);
                SDL_LockSurface(flipped);
                for (int py = 0; py < frame->h; py++)
                    for (int px = 0; px < frame->w; px++) {
                        *reinterpret_cast<Uint32*>(static_cast<Uint8*>(flipped->pixels) +
                                                   py * flipped->pitch +
                                                   (frame->w - 1 - px) * 4) =
                            *reinterpret_cast<Uint32*>(static_cast<Uint8*>(frame->pixels) +
                                                       py * frame->pitch + px * 4);
                    }
                SDL_UnlockSurface(frame);
                SDL_UnlockSurface(flipped);
                cache->frames[idx] = flipped;
            }
            SDL_DestroySurface(frame);
            frame    = cache->frames[idx];
            ownFrame = false;
        }

        if (g && g->active) {
            SDL_Surface* rotated = nullptr;
            switch (g->direction) {
                case GravityDir::DOWN:  break;
                case GravityDir::UP:    rotated = RotateSurface180(frame);   break;
                case GravityDir::RIGHT: rotated = RotateSurface90CCW(frame); break;
                case GravityDir::LEFT:  rotated = RotateSurface90CW(frame);  break;
            }
            if (rotated) {
                if (ownFrame) SDL_DestroySurface(frame);
                frame    = rotated;
                ownFrame = true;
            }
        }

        int   renderX = (int)(t.x - camX);
        int   renderY = (int)(t.y - camY);
        auto* roff    = reg.try_get<RenderOffset>(entity);

        if (g && g->active) {
            if (col) {
                int cx = roff ? roff->x : -(frame->w - col->w) / 2;
                int fy = roff ? roff->y : 0;
                switch (g->direction) {
                    case GravityDir::DOWN:
                        if (roff) { renderX += roff->x; renderY += roff->y; }
                        break;
                    case GravityDir::UP:
                        renderX += cx;
                        renderY += fy;
                        break;
                    case GravityDir::LEFT:
                        renderX += fy;
                        renderY += cx;
                        break;
                    case GravityDir::RIGHT:
                        renderX = (int)(t.x - camX) + col->h - frame->w - fy;
                        renderY += cx;
                        break;
                }
            }
        } else {
            if (roff) { renderX += roff->x; renderY += roff->y; }
        }

        {
            auto* hz2 = reg.try_get<HazardState>(entity);
            if (inv && inv->isInvincible) {
                if (static_cast<int>(inv->remaining * 10.0f) % 2 == 0)
                    SDL_SetSurfaceColorMod(frame, 255, 0, 0);
            } else if (hz2 && hz2->active) {
                if (static_cast<int>(hz2->flashTimer * 8.0f) % 2 == 0)
                    SDL_SetSurfaceColorMod(frame, 255, 80, 80);
            }
        }

        SDL_Rect dest = {renderX, renderY, frame->w, frame->h};
        SDL_BlitSurface(frame, nullptr, screen, &dest);
        SDL_SetSurfaceColorMod(frame, 255, 255, 255);
        if (ownFrame) SDL_DestroySurface(frame);
    });
}
