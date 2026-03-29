#include "EditorCanvasRenderer.hpp"
#include "AnimatedTile.hpp"
#include "EnemyProfile.hpp"
#include "tools/PlacementTools.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cmath>
#include <print>
#include <string>

namespace {
void DrawRectS(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
    const auto* fmt = SDL_GetPixelFormatDetails(s->format);
    SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
}
void DrawRectAlphaS(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
    if (r.w <= 0 || r.h <= 0) return;
    SDL_Surface* ov = SDL_CreateSurface(r.w, r.h, SDL_PIXELFORMAT_ARGB8888);
    if (!ov) return;
    SDL_SetSurfaceBlendMode(ov, SDL_BLENDMODE_BLEND);
    const auto* fmt = SDL_GetPixelFormatDetails(ov->format);
    SDL_FillSurfaceRect(ov, nullptr, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
    SDL_BlitSurface(ov, nullptr, s, &r);
    SDL_DestroySurface(ov);
}
void DrawOutlineS(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1) {
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
} // namespace

void EditorCanvasRenderer::DrawRect(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
    DrawRectS(s, r, c);
}
void EditorCanvasRenderer::DrawRectAlpha(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
    DrawRectAlphaS(s, r, c);
}
void EditorCanvasRenderer::DrawOutline(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t) {
    DrawOutlineS(s, r, c, t);
}
void EditorCanvasRenderer::BlitBadge(SDL_Surface* screen, SDL_Surface* badge, int bx, int by) {
    if (!badge) return;
    SDL_Rect d = {bx, by, badge->w, badge->h};
    SDL_BlitSurface(badge, nullptr, screen, &d);
}
SDL_Surface* EditorCanvasRenderer::Badge(EditorSurfaceCache& cache,
                                         const std::string& text, SDL_Color col) {
    return cache.GetBadge(text, col);
}

// --- Render ---
void EditorCanvasRenderer::Render(
    Window&              window,
    SDL_Surface*         screen,
    int                  canvasW,
    int                  toolbarH,
    int                  grid,
    const Level&         level,
    const EditorCamera&  camera,
    EditorSurfaceCache&  cache,
    const EditorPalette& palette,
    Image*               background,
    SpriteSheet*         enemySheet,
    ToolId               activeToolId,
    EditorTool*          activeTool,
    EditorToolContext    toolCtx,
    int                  actionAnimDropHover,
    const MovPlatState&  movPlat,
    const std::vector<std::unique_ptr<Image>>* parallaxImages,
    const std::vector<float>* parallaxFactors)
{
    SDL_Renderer* ren  = window.GetRenderer();
    int           winH = window.GetHeight();

    if (background) {
        if (background->GetFitMode() == FitMode::SCROLL)
            background->RenderScrolling(ren, camera.X(), 0.0f);
        else if (background->GetFitMode() == FitMode::SCROLL_WIDE)
            background->RenderScrollingWide(ren, camera.X(), 0.0f);
        else
            background->Render(ren);
    }

    if (parallaxImages && parallaxFactors) {
        for (size_t i = 0; i < parallaxImages->size(); ++i) {
            auto& img = (*parallaxImages)[i];
            if (!img) continue;
            float factor = (i < parallaxFactors->size()) ? (*parallaxFactors)[i] : 0.5f;
            img->RenderScrolling(ren, camera.X() * factor, 0.0f);
        }
    }

    RenderGrid(screen, canvasW, toolbarH, winH, camera, grid);
    RenderTiles(screen, canvasW, toolbarH, winH, level, camera, cache,
                activeToolId, actionAnimDropHover);

    if (activeToolId == ToolId::MovingPlat) {
        RenderMovingPlatOverlay(screen, canvasW, toolbarH, level, camera, cache, grid, movPlat);
        if (movPlat.popupOpen)
            RenderMovPlatPopup(screen, canvasW, toolbarH, cache, movPlat);
    } else {
        for (int ti = 0; ti < (int)level.tiles.size(); ti++) {
            const auto& t = level.tiles[ti];
            if (!t.HasMoving()) continue;
            int tsx = (int)((t.x - camera.X()) * camera.Zoom());
            int tsy = (int)((t.y - camera.Y()) * camera.Zoom());
            int tsw = (int)(t.w * camera.Zoom());
            int tsh = (int)(t.h * camera.Zoom());
            if (tsx + tsw <= 0 || tsx >= canvasW || tsy + tsh <= toolbarH || tsy >= winH)
                continue;
            DrawRect(screen, {tsx + 2, tsy + 2, 14, 14}, {0, 160, 180, 200});
            BlitBadge(screen, Badge(cache, "M", {255, 255, 255, 255}), tsx + 4, tsy + 2);
        }
    }

    RenderEntities(screen, canvasW, toolbarH, winH, level, camera, cache, enemySheet, grid);
    RenderPlayerMarker(screen, level, camera);

    if (activeTool) {
        activeTool->RenderOverlay(toolCtx, screen, canvasW);
    }

    RenderGhost(screen, canvasW, toolbarH, camera, palette, cache, activeToolId, activeTool, grid);
}

// --- Grid ---
void EditorCanvasRenderer::RenderGrid(SDL_Surface* screen, int canvasW, int toolbarH,
                                      int winH, const EditorCamera& cam, int grid)
{
    const auto* fmt    = SDL_GetPixelFormatDetails(screen->format);
    const float zoom   = cam.Zoom();
    const float camX   = cam.X();
    const float camY   = cam.Y();

    // Alpha fades as you zoom out so the grid doesn't overwhelm the canvas
    float zoomT = std::clamp(
        (zoom - EditorCamera::ZOOM_MIN) / (1.0f - EditorCamera::ZOOM_MIN), 0.0f, 1.0f);
    Uint8  gridAlpha = static_cast<Uint8>(4.0f + zoomT * 16.0f);
    Uint32 gridCol   = SDL_MapRGBA(fmt, nullptr, 255, 255, 255, gridAlpha);

    int firstCol = (int)std::floor(camX / grid);
    int firstRow = (int)std::floor(camY / grid);
    int numCols  = (int)std::ceil(canvasW / (grid * zoom)) + 2;
    int numRows  = (int)std::ceil(winH    / (grid * zoom)) + 2;

    for (int i = 0; i < numCols; i++) {
        float worldX = (firstCol + i) * (float)grid;
        int   sx     = (int)std::round((worldX - camX) * zoom);
        if (sx < 0 || sx >= canvasW) continue;
        SDL_Rect l = {sx, toolbarH, 1, winH - toolbarH};
        SDL_FillSurfaceRect(screen, &l, gridCol);
    }
    for (int i = 0; i < numRows; i++) {
        float worldY = (firstRow + i) * (float)grid;
        int   sy     = (int)std::round((worldY - camY) * zoom);
        if (sy < toolbarH || sy >= winH) continue;
        SDL_Rect l = {0, sy, canvasW, 1};
        SDL_FillSurfaceRect(screen, &l, gridCol);
    }
}

// --- Tiles ---
void EditorCanvasRenderer::RenderTiles(SDL_Surface* screen, int canvasW, int toolbarH,
                                       int winH, const Level& level,
                                       const EditorCamera& cam,
                                       EditorSurfaceCache& cache, ToolId activeToolId,
                                       int actionAnimDropHover)
{
    const float zoom = cam.Zoom();
    const float camX = cam.X();
    const float camY = cam.Y();

    auto blitBadge = [&](SDL_Surface* badge, int bx, int by) {
        BlitBadge(screen, badge, bx, by);
    };
    auto badge = [&](const std::string& text, SDL_Color col) -> SDL_Surface* {
        return cache.GetBadge(text, col);
    };

    for (int ti = 0; ti < (int)level.tiles.size(); ti++) {
        const auto& t   = level.tiles[ti];
        int tsx = (int)((t.x - camX) * zoom);
        int tsy = (int)((t.y - camY) * zoom);
        int tsw = (int)(t.w * zoom);
        int tsh = (int)(t.h * zoom);
        if (tsx + tsw <= 0 || tsx >= canvasW || tsy + tsh <= toolbarH || tsy >= winH)
            continue;

        SDL_Surface* ts  = cache.FindTileSurface(t.imagePath);
        SDL_Rect     dst = {tsx, tsy, tsw, tsh};

        if (ts) {
            SDL_Surface* draw = (t.rotation != 0)
                ? cache.GetRotated(t.imagePath, ts, t.rotation) : ts;
            if (t.prop)   SDL_SetSurfaceColorMod(draw, 120, 255, 120);
            if (t.ladder) SDL_SetSurfaceColorMod(draw, 120, 220, 255);
            if (t.HasAction()) SDL_SetSurfaceColorMod(draw, 255, 160,  80);
            if (t.hazard) SDL_SetSurfaceColorMod(draw, 255,  80,  80);
            if (t.HasShooter()) SDL_SetSurfaceColorMod(draw, 255, 160, 60);
            if (t.HasShield()) SDL_SetSurfaceColorMod(draw, 100, 180, 255);
            SDL_ScaleMode tileScale = (dst.w < draw->w || dst.h < draw->h)
                                    ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_PIXELART;
            SDL_BlitSurfaceScaled(draw, nullptr, screen, &dst, tileScale);
            if (t.prop || t.ladder || t.HasAction() || t.hazard || t.HasShooter() || t.HasShield())
                SDL_SetSurfaceColorMod(draw, 255, 255, 255);
        } else {
            DrawRect(screen, dst, {80, 80, 120, 200});
        }

        SDL_Color outlineCol = t.ladder       ? SDL_Color{0, 220, 220, 255}
                             : t.prop         ? SDL_Color{80, 255, 80, 255}
                             : t.HasAction()  ? SDL_Color{255, 160, 60, 255}
                             : t.hazard       ? SDL_Color{255, 60, 60, 255}
                             : t.HasShooter() ? SDL_Color{255, 140, 40, 255}
                             : t.HasShield()  ? SDL_Color{60, 160, 255, 255}
                                              : SDL_Color{100, 180, 255, 255};
        DrawOutline(screen, dst, outlineCol);

        if (t.HasShooter()) {
            int cx = tsx + tsw / 2;
            int cy = tsy + tsh / 2;
            int side = static_cast<int>(t.shooter->side);

            // Base: dark circle-ish square in the center
            int baseW = std::max(6, tsw / 4);
            int baseH = std::max(6, tsh / 4);
            SDL_Rect base = {cx - baseW / 2, cy - baseH / 2, baseW, baseH};
            DrawRect(screen, base, {20, 20, 20, 220});
            DrawOutline(screen, base, {60, 60, 60, 255});

            // Barrel: rectangle extending from the base toward the firing side
            int barrelLen = std::max(4, std::min(tsw, tsh) / 3);
            int barrelThk = std::max(3, std::min(baseW, baseH) / 2);
            SDL_Rect barrel{};
            switch (side) {
                case 0: // Top
                    barrel = {cx - barrelThk / 2, cy - baseH / 2 - barrelLen, barrelThk, barrelLen};
                    break;
                case 1: // Right
                    barrel = {cx + baseW / 2, cy - barrelThk / 2, barrelLen, barrelThk};
                    break;
                case 2: // Bottom
                    barrel = {cx - barrelThk / 2, cy + baseH / 2, barrelThk, barrelLen};
                    break;
                case 3: // Left
                    barrel = {cx - baseW / 2 - barrelLen, cy - barrelThk / 2, barrelLen, barrelThk};
                    break;
            }
            DrawRect(screen, barrel, {15, 15, 15, 230});
            DrawOutline(screen, barrel, {80, 80, 80, 255});

            // Muzzle tip: bright orange dot at the end of the barrel
            int muzzleSz = std::max(2, barrelThk - 2);
            SDL_Rect muzzle{};
            switch (side) {
                case 0: muzzle = {cx - muzzleSz / 2, barrel.y,                     muzzleSz, muzzleSz}; break;
                case 1: muzzle = {barrel.x + barrel.w - muzzleSz, cy - muzzleSz/2, muzzleSz, muzzleSz}; break;
                case 2: muzzle = {cx - muzzleSz / 2, barrel.y + barrel.h - muzzleSz, muzzleSz, muzzleSz}; break;
                case 3: muzzle = {barrel.x,           cy - muzzleSz / 2,            muzzleSz, muzzleSz}; break;
            }
            DrawRect(screen, muzzle, {255, 140, 30, 255});
        }

        {
            int           bx = tsx + 2;
            constexpr int BH = 14, BW = 14, GAP = 2;
            auto drawBadge = [&](const char* label, SDL_Color bg, SDL_Color fg) {
                DrawRect(screen, {bx, tsy + 2, BW, BH}, bg);
                blitBadge(badge(label, fg), bx + 2, tsy + 2);
                bx += BW + GAP;
            };
            if (t.prop)   drawBadge("P", {0, 180, 0, 210},       {255, 255, 255, 255});
            if (t.ladder) drawBadge("L", {0, 160, 180, 210},      {255, 255, 255, 255});
            if (t.HasAction()) {
                std::string ab = "A";
                if (t.action->group > 0)  ab += std::to_string(t.action->group);
                if (t.action->hitsRequired > 1) ab += "x" + std::to_string(t.action->hitsRequired);
                int abw = (int)ab.size() * 6 + 4;
                DrawRect(screen, {bx, tsy + 2, abw, BH}, {200, 100, 0, 200});
                blitBadge(badge(ab, {255, 255, 255, 255}), bx + 2, tsy + 2);
                bx += abw + GAP;
            }
            if (t.hazard)    drawBadge("H", {200, 0, 0, 220},    {255, 255, 255, 255});
            if (t.antiGravity) drawBadge("F", {0, 180, 200, 220},{255, 255, 255, 255});
            if (t.HasPowerUp()) {
                std::string pb  = t.powerUp->type.empty() ? "PU" : t.powerUp->type.substr(0, 2);
                if (!t.powerUp->sfxPath.empty()) pb += " \xe2\x99\xaa";
                int         pw  = (int)pb.size() * 6 + 4;
                DrawRect(screen, {bx, tsy + 2, pw, BH}, {180, 0, 220, 220});
                DrawOutline(screen, {bx, tsy + 2, pw, BH}, {255, 80, 255, 255});
                blitBadge(badge(pb, {255, 255, 255, 255}), bx + 2, tsy + 2);
                bx += pw + GAP;
            }
            if (t.HasSlope()) {
                std::string sb = (t.slope->type == SlopeType::DiagUpRight) ? "/" : "\\";
                if (t.slope->heightFrac < 0.99f)
                    sb += std::to_string((int)std::round(t.slope->heightFrac * 100)) + "%";
                int bw = (int)sb.size() * 6 + 4;
                DrawRect(screen, {bx, tsy + 2, bw, BH}, {160, 120, 0, 200});
                blitBadge(badge(sb, {255, 255, 255, 255}), bx + 2, tsy + 2);
                bx += bw + GAP;
            }
            if (t.goal)  drawBadge("G", {0, 200, 80, 220},     {255, 255, 255, 255});
            if (t.HasShooter()) {
                static const char* kArrows[] = {"\xe2\x96\xb2","\xe2\x96\xb6","\xe2\x96\xbc","\xe2\x97\x80"}; // ▲▶▼◀
                char rateBuf[16];
                std::snprintf(rateBuf, sizeof(rateBuf), "%.1f", t.shooter->fireRate);
                std::string sb = std::string("T") + kArrows[static_cast<int>(t.shooter->side)]
                               + " " + rateBuf;
                if (!t.shooter->sfxPath.empty()) sb += " \xe2\x99\xaa";
                int sw = (int)sb.size() * 4 + 6;
                DrawRect(screen, {bx, tsy + 2, sw, BH}, {180, 80, 0, 220});
                DrawOutline(screen, {bx, tsy + 2, sw, BH}, {255, 140, 40, 255});
                blitBadge(badge(sb, {255, 255, 255, 255}), bx + 2, tsy + 2);
                bx += sw + GAP;
            }
            if (t.HasShield()) drawBadge("S", {30, 100, 200, 220}, {255, 255, 255, 255});
            (void)bx;
        }

        if (t.HasAction()) {
            constexpr int ANIM_SZ = 16;
            int abx = tsx + tsw - ANIM_SZ - 2;
            int aby = tsy + tsh - ANIM_SZ - 2;
            if (abx > tsx + 14 + 2 && aby > tsy + 14) {
                if (!t.action->destroyAnimPath.empty()) {
                    DrawRect(screen, {abx-1, aby-1, ANIM_SZ+2, ANIM_SZ+2}, {120, 0, 200, 200});
                    SDL_Surface* animThumb = cache.GetDestroyAnimThumb(t.action->destroyAnimPath);
                    if (animThumb) {
                        SDL_Rect d2 = {abx, aby, ANIM_SZ, ANIM_SZ};
                        SDL_BlitSurfaceScaled(animThumb, nullptr, screen, &d2, SDL_SCALEMODE_LINEAR);
                    }
                    DrawOutline(screen, {abx-1, aby-1, ANIM_SZ+2, ANIM_SZ+2}, {200, 80, 255, 255});
                } else if (activeToolId == ToolId::Action && tsw >= 24 && tsh >= 24) {
                    DrawRect(screen, {abx, aby, ANIM_SZ, ANIM_SZ}, {60, 20, 80, 140});
                    DrawOutline(screen, {abx, aby, ANIM_SZ, ANIM_SZ}, {140, 60, 180, 160});
                    blitBadge(badge("+", {180, 100, 220, 200}), abx + 4, aby + 3);
                }
                if (activeToolId == ToolId::Action && tsw >= 40) {
                    std::string gs  = (t.action->group == 0) ? "G-" : ("G" + std::to_string(t.action->group));
                    SDL_Color   gc  = (t.action->group == 0)
                                    ? SDL_Color{120, 120, 140, 200}
                                    : SDL_Color{255, 220,  60, 255};
                    SDL_Surface* gb = badge(gs, gc);
                    if (gb) {
                        int gx = abx - gb->w - 3;
                        int gy = aby + ANIM_SZ / 2 - gb->h / 2;
                        blitBadge(gb, gx, gy);
                    }
                }
            }
            // Drop-hover highlight
            if (ti == actionAnimDropHover) {
                DrawRect(screen, {tsx, tsy, tsw, tsh}, {140, 0, 220, 70});
                DrawOutline(screen, {tsx, tsy, tsw, tsh}, {200, 80, 255, 255}, 2);
                blitBadge(badge("Drop anim here", {255, 200, 255, 255}),
                          tsx + tsw / 2 - 42, tsy + tsh / 2 - 5);
            }
        }

        if (t.HasSlope()) {
            int riseH = (int)(t.h * t.slope->heightFrac);
            int highY = tsy, lowY = tsy + riseH;
            int lx0, ly0, lx1, ly1;
            if (t.slope->type == SlopeType::DiagUpLeft) {
                lx0 = tsx;      ly0 = lowY;
                lx1 = tsx + tsw; ly1 = highY;
            } else {
                lx0 = tsx;       ly0 = highY;
                lx1 = tsx + tsw; ly1 = lowY;
            }
            int ddx = lx1 - lx0, ddy = ly1 - ly0;
            int steps = std::max(std::abs(ddx), std::abs(ddy));
            if (steps > 0) {
                float ssx = (float)ddx / steps, ssy = (float)ddy / steps;
                float ccx = (float)lx0,         ccy = (float)ly0;
                for (int s = 0; s <= steps; ++s) {
                    DrawRect(screen, {(int)ccx, (int)ccy, 2, 2}, {255, 220, 50, 220});
                    ccx += ssx; ccy += ssy;
                }
            }
        }

        if (t.rotation != 0) {
            std::string rb  = std::to_string(t.rotation);
            int         rbw = 22;
            int         rbx = tsx + tsw - rbw - 2;
            int         rby = tsy + tsh - 14 - 2;
            DrawRect(screen, {rbx, rby, rbw, 14}, {60, 60, 180, 200});
            blitBadge(badge(rb, {200, 220, 255, 255}), rbx + 2, rby + 1);
        }
    }
}

// --- Moving-platform overlay ---
void EditorCanvasRenderer::RenderMovingPlatOverlay(
    SDL_Surface* screen, int canvasW, int toolbarH,
    const Level& level, const EditorCamera& cam,
    EditorSurfaceCache& cache, int grid, const MovPlatState& mp)
{
    const float zoom = cam.Zoom();
    const float camX = cam.X();
    const float camY = cam.Y();

    auto blitBadge = [&](SDL_Surface* b, int bx, int by) { BlitBadge(screen, b, bx, by); };
    auto badge = [&](const std::string& t, SDL_Color c) -> SDL_Surface* {
        return cache.GetBadge(t, c);
    };

    for (int ti = 0; ti < (int)level.tiles.size(); ti++) {
        const auto& t = level.tiles[ti];
        if (!t.HasMoving()) continue;
        int tsx = (int)((t.x - camX) * zoom), tsy = (int)((t.y - camY) * zoom);
        int tsw = (int)(t.w * zoom),           tsh = (int)(t.h * zoom);

        bool inCur = mp.indices &&
            std::find(mp.indices->begin(), mp.indices->end(), ti) != mp.indices->end();

        SDL_Color fill   = inCur ? SDL_Color{0, 200, 200, 60} : SDL_Color{160, 80, 220, 40};
        SDL_Color border = inCur ? SDL_Color{0, 255, 255, 220}: SDL_Color{180, 100, 255, 160};
        DrawRect(screen, {tsx, tsy, tsw, tsh}, fill);
        DrawOutline(screen, {tsx, tsy, tsw, tsh}, border, 2);

        int cx = tsx + tsw / 2, cy = tsy + tsh / 2;
        int sw = (int)(t.w * zoom), sh = (int)(t.h * zoom);

        int lineStartX = 0, lineEndX = 0, lineStartY = 0, lineEndY = 0;

        const auto& mv = *t.moving;
        if (mv.horiz) {
            lineStartX = tsx;
            int endWXSnapped = ((int)(t.x + mv.range) / grid) * grid;
            lineEndX = (int)std::round((endWXSnapped - camX) * zoom);
            DrawRect(screen, {lineStartX, cy - 1, lineEndX - lineStartX, 2}, {0,255,255,100});
        } else {
            lineStartY = tsy;
            int endWYSnapped = ((int)(t.y + mv.range) / grid) * grid;
            lineEndY = (int)std::round((endWYSnapped - camY) * zoom);
            DrawRect(screen, {cx-1, lineStartY, 2, lineEndY-lineStartY}, {0,255,255,100});
        }

        // Direction arrow
        if (mv.horiz) {
            int acy = tsy + tsh / 2;
            int dir = mv.loop ? mv.loopDir : 1;
            int tipX  = cx + dir * (tsw / 2 + 10);
            for (int row = 0; row < 7; row++) {
                int half = row;
                int rx   = (dir > 0) ? tipX + row - 7 : tipX - row + 1;
                DrawRect(screen, {rx, acy - half, 1, half * 2 + 1}, {255,220,0,220});
            }
            if (mv.range > 0.0f) {
                DrawRectAlpha(screen, {tsx, tsy, sw, sh}, {0,200,80,60});
                DrawOutline(screen, {tsx, tsy, sw, sh}, {0,255,100,200}, 2);
                blitBadge(badge("S", {0,255,120,255}), tsx+2, tsy+2);
                int endWXSnapped = ((int)(t.x + mv.range) / grid) * grid;
                int endSX = (int)std::round((endWXSnapped - camX) * zoom);
                DrawRectAlpha(screen, {endSX, tsy, sw, sh}, {220,60,60,60});
                DrawOutline(screen, {endSX, tsy, sw, sh}, {255,80,80,200}, 2);
                blitBadge(badge("E", {255,100,100,255}), endSX+2, tsy+2);
                lineEndX = endSX;
                int phasePx = tsx + (int)((lineEndX - tsx) * mv.phase);
                DrawRect(screen, {phasePx-1, cy-12, 2, 24}, {255,255,0,200});
                blitBadge(badge(std::to_string((int)(mv.phase*100))+"%",{255,255,180,255}),
                          phasePx-8, cy-24);
            }
        } else {
            int acx = tsx + tsw / 2;
            int dir = mv.loop ? mv.loopDir : 1;
            int tipY = cy + dir * (tsh / 2 + 10);
            for (int row = 0; row < 7; row++) {
                int half = row;
                int ry   = (dir > 0) ? tipY + row - 7 : tipY - row + 1;
                DrawRect(screen, {acx - half, ry, half*2+1, 1}, {255,220,0,220});
            }
            if (mv.range > 0.0f) {
                DrawRectAlpha(screen, {tsx, tsy, sw, sh}, {0,200,80,60});
                DrawOutline(screen, {tsx, tsy, sw, sh}, {0,255,100,200}, 2);
                blitBadge(badge("S", {0,255,120,255}), tsx+2, tsy+2);
                int endWYSnapped = ((int)(t.y + mv.range) / grid) * grid;
                int endSY = (int)std::round((endWYSnapped - camY) * zoom);
                DrawRectAlpha(screen, {tsx, endSY, sw, sh}, {220,60,60,60});
                DrawOutline(screen, {tsx, endSY, sw, sh}, {255,80,80,200}, 2);
                blitBadge(badge("E", {255,100,100,255}), tsx+2, endSY+2);
                lineEndY = endSY;
                int phasePy = tsy + (int)((lineEndY - tsy) * mv.phase);
                DrawRect(screen, {acx-12, phasePy-1, 24, 2}, {255,255,0,200});
                blitBadge(badge(std::to_string((int)(mv.phase*100))+"%",{255,255,180,255}),
                          acx+8, phasePy-6);
            }
        }

        blitBadge(badge("M", {0,255,255,255}), tsx+2, tsy+2);
        if (mv.groupId != 0)
            blitBadge(badge(std::to_string(mv.groupId), {200,255,255,255}), tsx+12, tsy+2);
    }

    // Cursor preview of travel path before placing tiles
    {
        float fmx, fmy;
        SDL_GetMouseState(&fmx, &fmy);
        int mx = (int)fmx, my = (int)fmy;
        if (my >= toolbarH && mx < canvasW) {
            int       r           = (int)mp.range;
            SDL_Color previewCol  = {0, 255, 200, 160};
            if (mp.horiz) {
                DrawRect(screen, {mx-r, my-1, r*2, 2}, previewCol);
                DrawRect(screen, {mx-r-4, my-4, 4, 8}, previewCol);
                DrawRect(screen, {mx+r, my-4, 4, 8}, previewCol);
            } else {
                DrawRect(screen, {mx-1, my-r, 2, r*2}, previewCol);
                DrawRect(screen, {mx-4, my-r-4, 8, 4}, previewCol);
                DrawRect(screen, {mx-4, my+r, 8, 4}, previewCol);
            }
        }
    }

    // Param bar
    std::string paramStr =
        "MovePlat  " + std::string(mp.horiz ? "Horiz" : "Vert") +
        "  range=" + std::to_string((int)mp.range) +
        "  spd=" + std::to_string((int)mp.speed) +
        (mp.loop    ? "  LOOP"  : "") +
        (mp.trigger ? "  TOUCH" : "") +
        "  grp=" + std::to_string(mp.curGroupId) +
        "  tiles=" + std::to_string(mp.indices ? (int)mp.indices->size() : 0) +
        "  LClick=add  RClick=cycle preset";
    DrawRect(screen, {0, toolbarH + 20, canvasW, 18}, {10, 30, 50, 210});
    BlitBadge(screen, badge(paramStr, {0, 230, 230, 255}), 6, toolbarH + 23);
}

// --- MovPlatPopup ---
void EditorCanvasRenderer::RenderMovPlatPopup(SDL_Surface* screen, int canvasW,
                                               int toolbarH, EditorSurfaceCache& cache,
                                               const MovPlatState& mp)
{
    constexpr int PW      = 280;
    constexpr int PAD     = 8;
    constexpr int ROW_H   = 26;
    constexpr int TITLE_H = 30;
    constexpr int ROWS    = 4;
    constexpr int PH      = TITLE_H + ROWS * (ROW_H + PAD) + PAD;

    int px = canvasW - PW - 8;
    int py = toolbarH + 42;
    mMovPlatPopupRect = {px, py, PW, PH};

    auto blitBadge = [&](SDL_Surface* b, int bx, int by) { BlitBadge(screen, b, bx, by); };
    auto badge = [&](const std::string& t, SDL_Color c) -> SDL_Surface* {
        return cache.GetBadge(t, c);
    };

    DrawRect(screen, {px, py, PW, PH}, {14, 22, 40, 245});
    DrawOutline(screen, {px, py, PW, PH}, {0, 200, 160, 255}, 2);
    DrawRect(screen, {px+1, py+1, PW-2, 3}, {0, 200, 160, 255});

    {
        std::string title = "Platform Config  (grp " + std::to_string(mp.curGroupId) + ")";
        auto [tx, ty] = Text::CenterInRect(title, 12, {px, py+4, PW, TITLE_H-4});
        Text tt(title, {0,230,200,255}, tx, ty, 12);
        tt.RenderToSurface(screen);
    }

    int ry = py + TITLE_H;

    // Row 0: Speed field
    {
        DrawRect(screen, {px+PAD, ry, PW-PAD*2, ROW_H}, {20,32,55,200});
        blitBadge(badge("Speed (px/s)", {160,200,220,255}), px+PAD+4, ry+(ROW_H-10)/2);
        int      fw      = PW - PAD*2 - 90 - 44;
        SDL_Rect field   = {px+PAD+90, ry+(ROW_H-20)/2, fw, 20};
        SDL_Color fBg    = mp.speedInput ? SDL_Color{40,80,160,255} : SDL_Color{25,40,70,255};
        SDL_Color fBd    = mp.speedInput ? SDL_Color{100,180,255,255} : SDL_Color{60,100,140,255};
        DrawRect(screen, field, fBg);
        DrawOutline(screen, field, fBd);
        std::string disp = mp.speedInput ? mp.speedStr + "|" : std::to_string((int)mp.speed);
        blitBadge(badge(disp, {220,240,255,255}), field.x+4, field.y+(20-10)/2);
        SDL_Rect closeBtn = {px+PW-PAD-36, ry+(ROW_H-20)/2, 36, 20};
        DrawRect(screen, closeBtn, {60,30,30,220});
        DrawOutline(screen, closeBtn, {180,80,80,255});
        blitBadge(badge("close",{220,140,140,255}), closeBtn.x+4, closeBtn.y+4);
    }
    ry += ROW_H + PAD;

    // Row 1: Direction
    {
        DrawRect(screen, {px+PAD, ry, PW-PAD*2, ROW_H}, {20,32,55,200});
        blitBadge(badge("Direction",{160,200,220,255}), px+PAD+4, ry+(ROW_H-10)/2);
        SDL_Rect btnH = {px+PAD+90, ry+2, 48, ROW_H-4};
        SDL_Rect btnV = {px+PAD+90+54, ry+2, 48, ROW_H-4};
        DrawRect(screen, btnH, mp.horiz ? SDL_Color{0,160,255,255} : SDL_Color{30,40,60,220});
        DrawRect(screen, btnV, !mp.horiz? SDL_Color{0,160,255,255} : SDL_Color{30,40,60,220});
        DrawOutline(screen, btnH, {0,120,200,255});
        DrawOutline(screen, btnV, {0,120,200,255});
        {
            auto [hx,hy] = Text::CenterInRect("Horiz",10,btnH);
            blitBadge(badge("Horiz", mp.horiz?SDL_Color{255,255,255,255}:SDL_Color{120,160,200,255}),hx,hy);
            auto [vx,vy] = Text::CenterInRect("Vert",10,btnV);
            blitBadge(badge("Vert", !mp.horiz?SDL_Color{255,255,255,255}:SDL_Color{120,160,200,255}),vx,vy);
        }
    }
    ry += ROW_H + PAD;

    // Row 2: Loop checkbox
    {
        DrawRect(screen, {px+PAD, ry, PW-PAD*2, ROW_H}, {20,32,55,200});
        blitBadge(badge("Ping-pong loop",{160,200,220,255}), px+PAD+4, ry+(ROW_H-10)/2);
        SDL_Rect cb = {px+PAD+90, ry+(ROW_H-16)/2, 16, 16};
        DrawRect(screen, cb, mp.loop?SDL_Color{0,180,120,255}:SDL_Color{30,40,60,255});
        DrawOutline(screen, cb, {0,160,100,255});
        if (mp.loop) blitBadge(badge("x",{255,255,255,255}), cb.x+3, cb.y+2);
    }
    ry += ROW_H + PAD;

    // Row 3: Move on touch
    {
        DrawRect(screen, {px+PAD, ry, PW-PAD*2, ROW_H}, {20,32,55,200});
        blitBadge(badge("Move on touch",{160,200,220,255}), px+PAD+4, ry+(ROW_H-10)/2);
        SDL_Rect cb = {px+PAD+90, ry+(ROW_H-16)/2, 16, 16};
        DrawRect(screen, cb, mp.trigger?SDL_Color{255,160,0,255}:SDL_Color{30,40,60,255});
        DrawOutline(screen, cb, {200,140,0,255});
        if (mp.trigger) blitBadge(badge("x",{255,255,255,255}), cb.x+3, cb.y+2);
        blitBadge(badge("(waits for player to land)",
                        mp.trigger?SDL_Color{255,200,80,255}:SDL_Color{80,100,120,255}),
                  px+PAD+112, ry+(ROW_H-10)/2);
    }
}

// --- Entities ---
void EditorCanvasRenderer::RenderEntities(SDL_Surface* screen, int canvasW, int toolbarH,
                                          int winH, const Level& level,
                                          const EditorCamera& cam,
                                          EditorSurfaceCache& cache,
                                          SpriteSheet* enemySheet,
                                          int grid)
{
    const float zoom  = cam.Zoom();
    const float camX  = cam.X();
    const float camY  = cam.Y();

    struct EnemyRenderInfo {
        std::string previewPath;
        int spriteW = 40, spriteH = 40;
    };
    static std::unordered_map<std::string, EnemyRenderInfo> sEnemyRenderCache;

    std::vector<SDL_Rect> slimeFrames;
    if (enemySheet)
        slimeFrames = enemySheet->GetAnimation("slimeWalk");

    for (const auto& en : level.enemies) {
        // Determine render size: use profile spriteW/H if available
        int enW = 40, enH = 40;
        std::string previewImg;

        if (!en.enemyType.empty()) {
            auto cacheIt = sEnemyRenderCache.find(en.enemyType);
            if (cacheIt != sEnemyRenderCache.end()) {
                enW = cacheIt->second.spriteW;
                enH = cacheIt->second.spriteH;
                previewImg = cacheIt->second.previewPath;
            } else {
                EnemyProfile prof;
                if (LoadEnemyProfile("enemies/" + en.enemyType + ".json", prof)) {
                    enW = (prof.spriteW > 0) ? prof.spriteW : 40;
                    enH = (prof.spriteH > 0) ? prof.spriteH : 40;
                    previewImg = EnemyPreviewImagePath(prof);
                    sEnemyRenderCache[en.enemyType] = {previewImg, enW, enH};
                }
            }
        }

        int drawW = std::max(4, (int)(enW * zoom));
        int drawH = std::max(4, (int)(enH * zoom));
        int ex = (int)((en.x - camX) * zoom);
        int ey = (int)((en.y - camY) * zoom);

        if (ex + drawW <= 0 || ex >= canvasW || ey + drawH <= toolbarH || ey >= winH)
            continue;

        SDL_Rect d = {ex, ey, drawW, drawH};
        bool rendered = false;

        if (!previewImg.empty()) {
            SDL_Surface* thumb = cache.LoadAndCache(previewImg);
            if (thumb) {
                SDL_ScaleMode esm = (d.w < thumb->w || d.h < thumb->h)
                                   ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_PIXELART;
                SDL_BlitSurfaceScaled(thumb, nullptr, screen, &d, esm);
                rendered = true;
            }
        }

        if (!rendered && enemySheet && !slimeFrames.empty()) {
            SDL_Rect s = slimeFrames[0];
            SDL_ScaleMode esm = (d.w < s.w || d.h < s.h)
                               ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_PIXELART;
            SDL_BlitSurfaceScaled(enemySheet->GetSurface(), &s, screen, &d, esm);
        }

        SDL_Color ec = en.antiGravity ? SDL_Color{0,220,220,255} : SDL_Color{255,80,80,255};
        DrawOutline(screen, d, ec);

        // Speed + direction badge (bottom-left)
        {
            std::string dir = en.startLeft ? "<" : ">";
            std::string spdStr = dir + std::to_string((int)en.speed);
            BlitBadge(screen, cache.GetBadge(spdStr, {255, 200, 120, 255}),
                      ex + 2, ey + drawH - 12);
        }

        // Enemy type name badge (above sprite)
        if (!en.enemyType.empty()) {
            std::string typeName = en.enemyType;
            if ((int)typeName.size() > 8) typeName = typeName.substr(0, 6) + "..";
            BlitBadge(screen, cache.GetBadge(typeName, {255, 160, 100, 255}),
                      ex + 2, ey - 12);
        }

        if (en.antiGravity) {
            DrawRect(screen, {ex+drawW/2-4, ey-10, 8, 8}, {0,200,220,220});
            Text fb("F", {255,255,255,255}, ex+drawW/2-3, ey-11, 8);
            fb.RenderToSurface(screen);
        }
    }
}

// --- Player marker ---
void EditorCanvasRenderer::RenderPlayerMarker(SDL_Surface* screen, const Level& level,
                                               const EditorCamera& cam)
{
    int pmx = (int)((level.player.x - cam.X()) * cam.Zoom());
    int pmy = (int)((level.player.y - cam.Y()) * cam.Zoom());
    int pmw = (int)(PLAYER_STAND_WIDTH  * cam.Zoom());
    int pmh = (int)(PLAYER_STAND_HEIGHT * cam.Zoom());
    DrawRect(screen, {pmx, pmy, pmw, pmh}, {0, 200, 80, 180});
    DrawOutline(screen, {pmx, pmy, pmw, pmh}, {0, 255, 100, 255}, 2);
}

// --- Tile ghost ---
void EditorCanvasRenderer::RenderGhost(SDL_Surface* screen, int canvasW, int toolbarH,
                                        const EditorCamera& cam,
                                        const EditorPalette& palette,
                                        EditorSurfaceCache& cache, ToolId activeToolId,
                                        EditorTool* activeTool, int grid)
{
    if (activeToolId != ToolId::Tile) return;
    const auto* selItem = palette.SelectedItem();
    if (!selItem || selItem->isFolder) return;

    float fmx, fmy;
    SDL_GetMouseState(&fmx, &fmy);
    int mx = (int)fmx, my = (int)fmy;
    if (my < toolbarH || mx >= canvasW) return;

    // Query tile dimensions from TileTool if available
    int tileW = grid, tileH = grid, ghostRot = 0;
    if (activeTool) {
        if (auto* tt = dynamic_cast<TileTool*>(activeTool)) {
            tileW    = tt->tileW;
            tileH    = tt->tileH;
            ghostRot = tt->ghostRotation;
        }
    }

    SDL_Point snapped = cam.SnapToGrid(mx, my, grid);
    int gsx = (int)((snapped.x - cam.X()) * cam.Zoom());
    int gsy = (int)((snapped.y - cam.Y()) * cam.Zoom());
    int gsw = (int)(tileW * cam.Zoom());
    int gsh = (int)(tileH * cam.Zoom());
    SDL_Rect ghostDst = {gsx, gsy, gsw, gsh};

    SDL_Surface* ghostSurf = cache.FindTileSurface(selItem->path);
    if (!ghostSurf) ghostSurf = selItem->full;
    if (ghostSurf) {
        SDL_Surface* drawSurf = (ghostRot != 0)
            ? cache.GetRotated(selItem->path, ghostSurf, ghostRot)
            : ghostSurf;
        if (!drawSurf) drawSurf = ghostSurf;
        SDL_SetSurfaceAlphaMod(drawSurf, 140);
        SDL_ScaleMode gsm = (ghostDst.w < drawSurf->w || ghostDst.h < drawSurf->h)
                          ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_PIXELART;
        SDL_BlitSurfaceScaled(drawSurf, nullptr, screen, &ghostDst, gsm);
        SDL_SetSurfaceAlphaMod(drawSurf, 255);
    } else {
        DrawRectAlpha(screen, ghostDst, {100, 180, 255, 60});
    }

    if (ghostRot != 0) {
        std::string  rb  = std::to_string(ghostRot) + "\xc2\xb0";
        SDL_Surface* rbs = cache.GetBadge(rb, {255, 220, 80, 255});
        if (rbs) {
            SDL_Rect bd = {gsx + gsw - rbs->w - 3, gsy + 3, rbs->w, rbs->h};
            SDL_BlitSurface(rbs, nullptr, screen, &bd);
        }
    }
    DrawOutline(screen, ghostDst, {100, 180, 255, 200});
}
