#pragma once
#include <Components.hpp>
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <entt/entt.hpp>
#include <vector>

// camX/camY: viewport top-left. vw/vh: viewport size (0 = auto-query).
// sortedTiles: pre-sorted from Spawn(); nullptr = build inline.
// sortedFrontProps: pre-sorted front-prop list; nullptr = build inline.
// alpha: fixed-step interpolation factor for smooth rendering.
inline void RenderSystem(entt::registry& reg, SDL_Renderer* renderer,
                         float camX = 0.0f, float camY = 0.0f,
                         int vw = 0, int vh = 0,
                         const std::vector<entt::entity>* sortedTiles = nullptr,
                         float alpha = 1.0f,
                         const std::vector<entt::entity>* sortedFrontProps = nullptr) {
    if (vw == 0 || vh == 0)
        SDL_GetRenderOutputSize(renderer, &vw, &vh);

    auto culled = [&](float wx, float wy, int w, int h) -> bool {
        return wx + w  <= camX      ||
               wx      >= camX + vw ||
               wy + h  <= camY      ||
               wy      >= camY + vh;
    };

    // Helper: render a sorted entity list (tiles / props)
    auto renderTileList = [&](const std::vector<entt::entity>& list) {
        for (auto entity : list) {
            if (!reg.valid(entity)) continue;
            auto* rp   = reg.try_get<Renderable>(entity);
            auto* tp   = reg.try_get<Transform>(entity);
            auto* anip = reg.try_get<AnimationState>(entity);
            if (!rp || !tp || !anip) continue;

            auto& t    = *tp;
            auto& r    = *rp;
            auto& anim = *anip;
            if (!r.sheet || r.frames.empty()) continue;

            int tFrameIdx = anim.currentFrame;
            if (tFrameIdx >= (int)r.frames.size()) tFrameIdx = 0;
            const SDL_Rect& src = r.frames[tFrameIdx];
            const int tDrawW = (r.renderW > 0) ? r.renderW : src.w;
            const int tDrawH = (r.renderH > 0) ? r.renderH : src.h;
            if (culled(t.x, t.y, tDrawW, tDrawH)) continue;

            SDL_FRect dst = {t.x - camX, t.y - camY, (float)tDrawW, (float)tDrawH};

            double angle = 0.0;
            if (const auto* fs = reg.try_get<FloatState>(entity))
                angle = (double)fs->spinAngle;

            SDL_FRect srcF = {(float)src.x, (float)src.y, (float)src.w, (float)src.h};
            SDL_RenderTextureRotated(renderer, r.sheet, &srcF, &dst, angle, nullptr, SDL_FLIP_NONE);

            if (const auto* hf = reg.try_get<HitFlash>(entity)) {
                float frac  = hf->timer / hf->duration;
                Uint8 a     = (Uint8)(frac * 160.0f);
                SDL_SetRenderDrawColor(renderer, 220, 30, 30, a);
                SDL_RenderFillRect(renderer, &dst);
            }
        }
    };

    // --- Pass 1: tiles + ladders + behind-props in spawn order ---
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
        renderTileList(*tiles);
    }

    // --- Pass 2: player, enemies, coins ---
    auto interpPos = [&](entt::entity e, const Transform& t) -> std::pair<float,float> {
        if (const auto* p = reg.try_get<PrevTransform>(e))
            return { p->x + (t.x - p->x) * alpha,
                     p->y + (t.y - p->y) * alpha };
        return { t.x, t.y };
    };

    auto view = reg.view<Transform, Renderable, AnimationState>(
        entt::exclude<TileTag, LadderTag, PropTag, PropFrontTag>);

    view.each([&](entt::entity entity, const Transform& t, Renderable& r,
                  const AnimationState& anim) {
        if (!r.sheet || r.frames.empty()) return;

        // Clamp frame index against animation/state system ordering race.
        int frameIdx = anim.currentFrame;
        if (frameIdx >= (int)r.frames.size()) frameIdx = 0;
        const SDL_Rect& src = r.frames[frameIdx];
        // Use the intended render size when set; fall back to source frame dims.
        const int drawW = (r.renderW > 0) ? r.renderW : src.w;
        const int drawH = (r.renderH > 0) ? r.renderH : src.h;
        if (culled(t.x, t.y, drawW, drawH)) return;

        auto [ix, iy] = interpPos(entity, t);

        auto* g    = reg.try_get<GravityState>(entity);
        auto* inv  = reg.try_get<InvincibilityTimer>(entity);
        auto* hz   = reg.try_get<HazardState>(entity);
        auto* hf   = reg.try_get<HitFlash>(entity);
        auto* col  = reg.try_get<Collider>(entity);
        auto* roff = reg.try_get<RenderOffset>(entity);

        bool colorModded = false;
        if (hf && hf->timer > 0.0f) {
            SDL_SetTextureColorMod(r.sheet, 255, 60, 60);
            colorModded = true;
        } else if (inv && inv->isInvincible && (int)(inv->remaining * 10.0f) % 2 == 0) {
            SDL_SetTextureColorMod(r.sheet, 255, 0, 0);
            colorModded = true;
        } else if (hz && hz->active && (int)(hz->flashTimer * 8.0f) % 2 == 0) {
            SDL_SetTextureColorMod(r.sheet, 255, 80, 80);
            colorModded = true;
        }

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

        float rx = ix - camX;
        float ry = iy - camY;

        if (g && g->active && col) {
            switch (g->direction) {
                case GravityDir::DOWN:
                    if (roff) {
                        if (r.flipH) {
                            rx = (ix - camX) - (drawW - col->w) - roff->x;
                        } else {
                            rx += roff->x;
                        }
                        ry += roff->y;
                    }
                    break;
                case GravityDir::UP:
                    rx += roff ? roff->x : -(drawW - col->w) / 2;
                    ry += roff ? roff->y : 0;
                    break;
                case GravityDir::LEFT:
                    rx += roff ? roff->y : 0;
                    ry += roff ? roff->x : -(drawH - col->h) / 2;
                    break;
                case GravityDir::RIGHT:
                    rx  = (ix - camX) + col->h - drawW - (roff ? roff->y : 0);
                    ry += roff ? roff->x : -(drawH - col->h) / 2;
                    break;
            }
        } else {
            // Inactive gravity (ladder, open-world) — still apply flip-aware offset.
            if (roff && col) {
                if (r.flipH) {
                    rx = (ix - camX) - (drawW - col->w) - roff->x;
                } else {
                    rx += roff->x;
                }
                ry += roff->y;
            } else if (roff) {
                rx += roff->x;
                ry += roff->y;
            }
        }

        SDL_FRect srcF = {(float)src.x, (float)src.y, (float)src.w, (float)src.h};
        SDL_FRect dst  = {rx, ry, (float)drawW, (float)drawH};
        SDL_RenderTextureRotated(renderer, r.sheet, &srcF, &dst, angle, nullptr, flip);

        if (colorModded)
            SDL_SetTextureColorMod(r.sheet, 255, 255, 255);
    });

    // --- Pass 3: front-props (rendered over the player) ---
    {
        std::vector<entt::entity> localFrontProps;
        const std::vector<entt::entity>* frontProps = sortedFrontProps;
        if (!frontProps) {
            auto fpView = reg.view<Transform, Renderable, AnimationState, PropFrontTag>();
            localFrontProps.reserve(fpView.size_hint());
            for (auto e : fpView) localFrontProps.push_back(e);
            std::sort(localFrontProps.begin(), localFrontProps.end());
            frontProps = &localFrontProps;
        }
        renderTileList(*frontProps);
    }

    // --- Pass 4: enemy health bars ---
    {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        auto hpView = reg.view<EnemyTag, Transform, Health, Collider>(
            entt::exclude<DeadTag>);

        hpView.each([&](entt::entity e, const Transform& t, const Health& hp,
                        const Collider& c) {
            if (hp.current >= hp.max) return;
            if (culled(t.x, t.y, c.w, c.h)) return;

            auto [ix, iy] = interpPos(e, t);

            float frac   = std::clamp(hp.current / hp.max, 0.0f, 1.0f);
            int   barW   = std::max((int)(c.w * 0.8f), 24);
            int   barH   = 4;
            int   padY   = 6;
            float barX   = (ix - camX) + c.w * 0.5f - barW * 0.5f;
            float barY   = (iy - camY) - padY - barH;

            SDL_FRect bg   = {barX - 1, barY - 1, (float)(barW + 2), (float)(barH + 2)};
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
            SDL_RenderFillRect(renderer, &bg);

            Uint8 r = (Uint8)((1.0f - frac) * 220);
            Uint8 g = (Uint8)(frac * 200);
            SDL_FRect fill = {barX, barY, barW * frac, (float)barH};
            SDL_SetRenderDrawColor(renderer, r, g, 40, 230);
            SDL_RenderFillRect(renderer, &fill);

            SDL_FRect border = {barX - 1, barY - 1, (float)(barW + 2), (float)(barH + 2)};
            SDL_SetRenderDrawColor(renderer, 20, 20, 20, 200);
            SDL_RenderRect(renderer, &border);
        });
    }
}
