#pragma once
// SelectTool.hpp
//
// Rubber-band marquee selection + multi-entity drag-move.
// Supports both tiles and enemies as selectable, movable entities.

#include "tools/EditorTool.hpp"
#include "EnemyProfile.hpp"
#include <algorithm>
#include <climits>
#include <vector>

class SelectTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Select"; }

    std::vector<int>                      selIndices;
    std::vector<int>                      selEnemyIndices;
    bool                                  selBoxing      = false;
    int                                   selBoxX0 = 0, selBoxY0 = 0;
    int                                   selBoxX1 = 0, selBoxY1 = 0;
    bool                                  selDragging    = false;

    void OnActivate(EditorToolContext& /*ctx*/) override {
        selBoxing   = false;
        selDragging = false;
    }

    void OnDeactivate(EditorToolContext& /*ctx*/) override {
        selIndices.clear();
        selEnemyIndices.clear();
        selBoxing   = false;
        selDragging = false;
    }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod mods) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;

        auto [wx, wy] = ctx.ScreenToWorld(mx, my);

        int  ti       = ctx.HitTile(mx, my);
        int  ei       = ctx.HitEnemy(mx, my);

        bool hitSelTile  = (ti >= 0 && Contains(selIndices, ti));
        bool hitSelEnemy = (ei >= 0 && Contains(selEnemyIndices, ei));

        if (hitSelTile || hitSelEnemy) {
            selDragging     = true;
            mSelDragStartWX = wx;
            mSelDragStartWY = wy;
            mSelOrigPositions.clear();
            mSelEnemyOrigPositions.clear();
            for (int idx : selIndices)
                mSelOrigPositions.push_back({ctx.level.tiles[idx].x, ctx.level.tiles[idx].y});
            for (int idx : selEnemyIndices)
                mSelEnemyOrigPositions.push_back({ctx.level.enemies[idx].x, ctx.level.enemies[idx].y});
        } else if (ei >= 0) {
            if (mods & SDL_KMOD_SHIFT) {
                ToggleIn(selEnemyIndices, ei);
            } else {
                selIndices.clear();
                selEnemyIndices = {ei};
            }
            ReportSelection(ctx);
        } else if (ti >= 0) {
            if (mods & SDL_KMOD_SHIFT) {
                ToggleIn(selIndices, ti);
            } else {
                selEnemyIndices.clear();
                selIndices = {ti};
            }
            ReportSelection(ctx);
        } else {
            if (!(mods & SDL_KMOD_SHIFT)) { selIndices.clear(); selEnemyIndices.clear(); }
            selBoxing = true;
            selBoxX0 = selBoxX1 = wx;
            selBoxY0 = selBoxY1 = wy;
        }
        return ToolResult::Consumed;
    }

    ToolResult OnMouseMove(EditorToolContext& ctx, int mx, int my) override {
        if (selBoxing) {
            auto [wx, wy] = ctx.ScreenToWorld(mx, my);
            selBoxX1 = wx;
            selBoxY1 = wy;
            return ToolResult::Consumed;
        }
        if (selDragging) {
            auto [wx, wy] = ctx.ScreenToWorld(mx, my);
            int dx   = wx - mSelDragStartWX;
            int dy   = wy - mSelDragStartWY;
            int grid = ctx.Grid();
            for (int i = 0; i < (int)selIndices.size(); ++i) {
                int idx = selIndices[i];
                if (idx < (int)ctx.level.tiles.size()) {
                    float rawX = mSelOrigPositions[i].first + dx;
                    float rawY = mSelOrigPositions[i].second + dy;
                    ctx.level.tiles[idx].x = (float)((int)(rawX) / grid * grid);
                    ctx.level.tiles[idx].y = (float)((int)(rawY) / grid * grid);
                }
            }
            for (int i = 0; i < (int)selEnemyIndices.size(); ++i) {
                int idx = selEnemyIndices[i];
                if (idx < (int)ctx.level.enemies.size()) {
                    float rawX = mSelEnemyOrigPositions[i].first + dx;
                    float rawY = mSelEnemyOrigPositions[i].second + dy;
                    ctx.level.enemies[idx].x = (float)((int)(rawX) / grid * grid);
                    ctx.level.enemies[idx].y = (float)((int)(rawY) / grid * grid);
                }
            }
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    ToolResult OnMouseUp(EditorToolContext& ctx, int /*mx*/, int /*my*/,
                         Uint8 /*button*/, SDL_Keymod mods) override {
        if (selBoxing) {
            selBoxing = false;
            int rx0 = std::min(selBoxX0, selBoxX1), ry0 = std::min(selBoxY0, selBoxY1);
            int rx1 = std::max(selBoxX0, selBoxX1), ry1 = std::max(selBoxY0, selBoxY1);
            if (!(mods & SDL_KMOD_SHIFT)) { selIndices.clear(); selEnemyIndices.clear(); }

            for (int i = 0; i < (int)ctx.level.tiles.size(); ++i) {
                const auto& t = ctx.level.tiles[i];
                if ((int)t.x + t.w > rx0 && (int)t.x < rx1 &&
                    (int)t.y + t.h > ry0 && (int)t.y < ry1) {
                    if (!Contains(selIndices, i))
                        selIndices.push_back(i);
                }
            }

            for (int i = 0; i < (int)ctx.level.enemies.size(); ++i) {
                const auto& en = ctx.level.enemies[i];
                auto [ew, eh] = GetEnemySize(en, ctx.Grid());
                if ((int)en.x + ew > rx0 && (int)en.x < rx1 &&
                    (int)en.y + eh > ry0 && (int)en.y < ry1) {
                    if (!Contains(selEnemyIndices, i))
                        selEnemyIndices.push_back(i);
                }
            }

            ReportSelection(ctx);
        }
        if (selDragging) {
            selDragging = false;
            int total = (int)selIndices.size() + (int)selEnemyIndices.size();
            ctx.SetStatus("Moved " + std::to_string(total) + " entity(s)");
        }
        return ToolResult::Consumed;
    }

    ToolResult OnKeyDown(EditorToolContext& ctx, SDL_Keycode key, SDL_Keymod /*mods*/) override {
        if (key == SDLK_DELETE || key == SDLK_BACKSPACE) {
            int deleted = 0;
            if (!selIndices.empty()) {
                std::sort(selIndices.begin(), selIndices.end(), std::greater<int>());
                for (int idx : selIndices)
                    if (idx >= 0 && idx < (int)ctx.level.tiles.size())
                        ctx.level.tiles.erase(ctx.level.tiles.begin() + idx);
                deleted += (int)selIndices.size();
                selIndices.clear();
            }
            if (!selEnemyIndices.empty()) {
                std::sort(selEnemyIndices.begin(), selEnemyIndices.end(), std::greater<int>());
                for (int idx : selEnemyIndices)
                    if (idx >= 0 && idx < (int)ctx.level.enemies.size())
                        ctx.level.enemies.erase(ctx.level.enemies.begin() + idx);
                deleted += (int)selEnemyIndices.size();
                selEnemyIndices.clear();
            }
            if (deleted > 0)
                ctx.SetStatus("Deleted " + std::to_string(deleted) + " entity(s)");
            return ToolResult::Consumed;
        }
        if (key == SDLK_ESCAPE) {
            selIndices.clear();
            selEnemyIndices.clear();
            selBoxing   = false;
            selDragging = false;
            ctx.SetStatus("Selection cleared");
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    void RenderOverlay(EditorToolContext& ctx, SDL_Surface* screen, int canvasW) override {
        (void)canvasW;
        float camX = ctx.CamX(), camY = ctx.CamY(), zoom = ctx.Zoom();

        // Highlight selected tiles
        for (int idx : selIndices) {
            if (idx < 0 || idx >= (int)ctx.level.tiles.size()) continue;
            const auto& t = ctx.level.tiles[idx];
            int sx = (int)((t.x - camX) * zoom);
            int sy = (int)((t.y - camY) * zoom);
            int sw = (int)(t.w * zoom);
            int sh = (int)(t.h * zoom);
            EditorToolContext::DrawRectAlpha(screen, {sx, sy, sw, sh}, {0, 220, 220, 40});
            EditorToolContext::DrawOutline(screen, {sx, sy, sw, sh}, {0, 255, 255, 220}, 2);
        }

        // Highlight selected enemies
        for (int idx : selEnemyIndices) {
            if (idx < 0 || idx >= (int)ctx.level.enemies.size()) continue;
            const auto& en = ctx.level.enemies[idx];
            auto [ew, eh] = GetEnemySize(en, ctx.Grid());
            int sx = (int)((en.x - camX) * zoom);
            int sy = (int)((en.y - camY) * zoom);
            int sw = (int)(ew * zoom);
            int sh = (int)(eh * zoom);
            EditorToolContext::DrawRectAlpha(screen, {sx, sy, sw, sh}, {255, 140, 60, 50});
            EditorToolContext::DrawOutline(screen, {sx, sy, sw, sh}, {255, 160, 80, 230}, 2);

            std::string label = en.enemyType.empty() ? "slime" : en.enemyType;
            label += " " + std::to_string(ew) + "x" + std::to_string(eh);
            SDL_Surface* badge = ctx.GetBadge(label, {255, 200, 140, 255});
            if (badge) {
                SDL_Rect bd = {sx, sy - badge->h - 2, badge->w, badge->h};
                SDL_BlitSurface(badge, nullptr, screen, &bd);
            }
        }

        // Bounding box around all selected entities
        if (!selIndices.empty() || !selEnemyIndices.empty()) {
            int bx0 = INT_MAX, by0 = INT_MAX, bx1 = INT_MIN, by1 = INT_MIN;
            for (int idx : selIndices) {
                if (idx < 0 || idx >= (int)ctx.level.tiles.size()) continue;
                const auto& t = ctx.level.tiles[idx];
                bx0 = std::min(bx0, (int)t.x);
                by0 = std::min(by0, (int)t.y);
                bx1 = std::max(bx1, (int)t.x + t.w);
                by1 = std::max(by1, (int)t.y + t.h);
            }
            for (int idx : selEnemyIndices) {
                if (idx < 0 || idx >= (int)ctx.level.enemies.size()) continue;
                const auto& en = ctx.level.enemies[idx];
                auto [ew, eh] = GetEnemySize(en, ctx.Grid());
                bx0 = std::min(bx0, (int)en.x);
                by0 = std::min(by0, (int)en.y);
                bx1 = std::max(bx1, (int)en.x + ew);
                by1 = std::max(by1, (int)en.y + eh);
            }
            if (bx0 < bx1 && by0 < by1) {
                int sbx  = (int)((bx0 - camX) * zoom);
                int sby  = (int)((by0 - camY) * zoom);
                int sbbw = (int)((bx1 - bx0) * zoom);
                int sbbh = (int)((by1 - by0) * zoom);
                EditorToolContext::DrawOutline(screen, {sbx - 2, sby - 2, sbbw + 4, sbbh + 4},
                                              {0, 255, 255, 180}, 1);
                int total = (int)selIndices.size() + (int)selEnemyIndices.size();
                std::string selLabel = std::to_string(total) + " selected";
                EditorToolContext::DrawRect(screen,
                    {sbx, sby - 16, (int)selLabel.size() * 7 + 4, 14},
                    {0, 60, 70, 200});
                Text selT(selLabel, SDL_Color{0, 255, 255, 255}, sbx + 2, sby - 15, 10);
                selT.RenderToSurface(screen);
            }
        }

        // Rubber-band marquee
        if (selBoxing) {
            int rx0 = (int)((std::min(selBoxX0, selBoxX1) - camX) * zoom);
            int ry0 = (int)((std::min(selBoxY0, selBoxY1) - camY) * zoom);
            int rx1 = (int)((std::max(selBoxX0, selBoxX1) - camX) * zoom);
            int ry1 = (int)((std::max(selBoxY0, selBoxY1) - camY) * zoom);
            int rw = rx1 - rx0, rh = ry1 - ry0;
            if (rw > 0 && rh > 0) {
                EditorToolContext::DrawRectAlpha(screen, {rx0, ry0, rw, rh}, {100, 220, 255, 20});
                EditorToolContext::DrawOutline(screen, {rx0, ry0, rw, rh}, {100, 220, 255, 180}, 1);
            }
        }
    }

  private:
    int mSelDragStartWX = 0, mSelDragStartWY = 0;
    std::vector<std::pair<float, float>> mSelOrigPositions;
    std::vector<std::pair<float, float>> mSelEnemyOrigPositions;

    static bool Contains(const std::vector<int>& v, int val) {
        return std::find(v.begin(), v.end(), val) != v.end();
    }

    static void ToggleIn(std::vector<int>& v, int val) {
        auto it = std::find(v.begin(), v.end(), val);
        if (it != v.end()) v.erase(it);
        else v.push_back(val);
    }

    static std::pair<int,int> GetEnemySize(const EnemySpawn& en, int grid) {
        int ew = grid, eh = grid;
        if (!en.enemyType.empty()) {
            EnemyProfile prof;
            if (LoadEnemyProfile("enemies/" + en.enemyType + ".json", prof)) {
                ew = (prof.spriteW > 0) ? prof.spriteW : grid;
                eh = (prof.spriteH > 0) ? prof.spriteH : grid;
            }
        }
        return {ew, eh};
    }

    void ReportSelection(EditorToolContext& ctx) {
        int total = (int)selIndices.size() + (int)selEnemyIndices.size();
        std::string msg = "Selected " + std::to_string(total) + " (";
        msg += std::to_string(selIndices.size()) + " tiles, ";
        msg += std::to_string(selEnemyIndices.size()) + " enemies)";
        ctx.SetStatus(msg);
    }
};
