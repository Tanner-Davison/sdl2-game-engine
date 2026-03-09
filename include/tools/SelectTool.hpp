#pragma once
// SelectTool.hpp
//
// Rubber-band marquee selection + multi-tile drag-move.
// State: selected indices, rubber-band rect, drag origin/positions.

#include "tools/EditorTool.hpp"
#include <algorithm>
#include <climits>
#include <vector>

class SelectTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Select"; }

    // ── Public state accessible by the orchestrator for Render overlays ──────
    std::vector<int>                      selIndices;
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
        selBoxing   = false;
        selDragging = false;
    }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod mods) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;

        auto [wx, wy] = ctx.ScreenToWorld(mx, my);
        int  ti       = ctx.HitTile(mx, my);
        bool hitSel   = (ti >= 0 && std::find(selIndices.begin(), selIndices.end(), ti) !=
                                        selIndices.end());
        if (hitSel) {
            selDragging    = true;
            mSelDragStartWX = wx;
            mSelDragStartWY = wy;
            mSelOrigPositions.clear();
            for (int idx : selIndices)
                mSelOrigPositions.push_back({ctx.level.tiles[idx].x, ctx.level.tiles[idx].y});
        } else if (ti >= 0) {
            if (mods & SDL_KMOD_SHIFT) {
                auto it = std::find(selIndices.begin(), selIndices.end(), ti);
                if (it != selIndices.end()) selIndices.erase(it);
                else selIndices.push_back(ti);
            } else {
                selIndices = {ti};
            }
            ctx.SetStatus("Selected " + std::to_string(selIndices.size()) + " tile(s)");
        } else {
            if (!(mods & SDL_KMOD_SHIFT)) selIndices.clear();
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
        if (selDragging && !mSelOrigPositions.empty()) {
            auto [wx, wy] = ctx.ScreenToWorld(mx, my);
            int dx = wx - mSelDragStartWX;
            int dy = wy - mSelDragStartWY;
            int grid = ctx.Grid();
            for (int i = 0; i < static_cast<int>(selIndices.size()); ++i) {
                int idx = selIndices[i];
                if (idx < static_cast<int>(ctx.level.tiles.size())) {
                    float rawX = mSelOrigPositions[i].first + dx;
                    float rawY = mSelOrigPositions[i].second + dy;
                    ctx.level.tiles[idx].x = static_cast<float>((static_cast<int>(rawX) / grid) * grid);
                    ctx.level.tiles[idx].y = static_cast<float>((static_cast<int>(rawY) / grid) * grid);
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
            if (!(mods & SDL_KMOD_SHIFT)) selIndices.clear();
            for (int i = 0; i < static_cast<int>(ctx.level.tiles.size()); ++i) {
                const auto& t = ctx.level.tiles[i];
                if (static_cast<int>(t.x) + t.w > rx0 && static_cast<int>(t.x) < rx1 &&
                    static_cast<int>(t.y) + t.h > ry0 && static_cast<int>(t.y) < ry1) {
                    if (std::find(selIndices.begin(), selIndices.end(), i) == selIndices.end())
                        selIndices.push_back(i);
                }
            }
            ctx.SetStatus("Selected " + std::to_string(selIndices.size()) + " tile(s)");
        }
        if (selDragging) {
            selDragging = false;
            ctx.SetStatus("Moved " + std::to_string(selIndices.size()) + " tile(s)");
        }
        return ToolResult::Consumed;
    }

    ToolResult OnKeyDown(EditorToolContext& ctx, SDL_Keycode key, SDL_Keymod /*mods*/) override {
        if (key == SDLK_DELETE || key == SDLK_BACKSPACE) {
            if (!selIndices.empty()) {
                std::sort(selIndices.begin(), selIndices.end(), std::greater<int>());
                for (int idx : selIndices)
                    if (idx >= 0 && idx < static_cast<int>(ctx.level.tiles.size()))
                        ctx.level.tiles.erase(ctx.level.tiles.begin() + idx);
                ctx.SetStatus("Deleted " + std::to_string(selIndices.size()) + " tile(s)");
                selIndices.clear();
            }
            return ToolResult::Consumed;
        }
        if (key == SDLK_ESCAPE) {
            selIndices.clear();
            selBoxing   = false;
            selDragging = false;
            ctx.SetStatus("Selection cleared");
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    void RenderOverlay(EditorToolContext& ctx, SDL_Surface* screen, int canvasW) override {
        float camX = ctx.CamX(), camY = ctx.CamY(), zoom = ctx.Zoom();

        // Highlight selected tiles
        if (!selIndices.empty()) {
            for (int idx : selIndices) {
                if (idx < 0 || idx >= static_cast<int>(ctx.level.tiles.size())) continue;
                const auto& t = ctx.level.tiles[idx];
                int sx = static_cast<int>((t.x - camX) * zoom);
                int sy = static_cast<int>((t.y - camY) * zoom);
                int sw = static_cast<int>(t.w * zoom);
                int sh = static_cast<int>(t.h * zoom);
                EditorToolContext::DrawRectAlpha(screen, {sx, sy, sw, sh}, {0, 220, 220, 40});
                EditorToolContext::DrawOutline(screen, {sx, sy, sw, sh}, {0, 255, 255, 220}, 2);
            }
            // Bounding box
            int bx0 = INT_MAX, by0 = INT_MAX, bx1 = INT_MIN, by1 = INT_MIN;
            for (int idx : selIndices) {
                if (idx < 0 || idx >= static_cast<int>(ctx.level.tiles.size())) continue;
                const auto& t = ctx.level.tiles[idx];
                bx0 = std::min(bx0, static_cast<int>(t.x));
                by0 = std::min(by0, static_cast<int>(t.y));
                bx1 = std::max(bx1, static_cast<int>(t.x) + t.w);
                by1 = std::max(by1, static_cast<int>(t.y) + t.h);
            }
            int sbx = static_cast<int>((bx0 - camX) * zoom);
            int sby = static_cast<int>((by0 - camY) * zoom);
            int sbbw = static_cast<int>((bx1 - bx0) * zoom);
            int sbbh = static_cast<int>((by1 - by0) * zoom);
            EditorToolContext::DrawOutline(screen, {sbx - 2, sby - 2, sbbw + 4, sbbh + 4},
                                          {0, 255, 255, 180}, 1);
            // Count label
            std::string selLabel = std::to_string(selIndices.size()) + " selected";
            EditorToolContext::DrawRect(screen,
                {sbx, sby - 16, static_cast<int>(selLabel.size()) * 7 + 4, 14},
                {0, 60, 70, 200});
            Text selT(selLabel, SDL_Color{0, 255, 255, 255}, sbx + 2, sby - 15, 10);
            selT.RenderToSurface(screen);
        }

        // Rubber-band marquee
        if (selBoxing) {
            int rx0 = static_cast<int>((std::min(selBoxX0, selBoxX1) - camX) * zoom);
            int ry0 = static_cast<int>((std::min(selBoxY0, selBoxY1) - camY) * zoom);
            int rx1 = static_cast<int>((std::max(selBoxX0, selBoxX1) - camX) * zoom);
            int ry1 = static_cast<int>((std::max(selBoxY0, selBoxY1) - camY) * zoom);
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
};
