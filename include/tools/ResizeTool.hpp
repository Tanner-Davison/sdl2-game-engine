#pragma once
// ResizeTool.hpp
//
// Drag tile edges/corner to change tile dimensions. Shows handle highlights
// on hover and renders a size label during drag.

#include "tools/EditorTool.hpp"
#include <algorithm>
#include <string>

class ResizeTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Resize"; }

    enum class Edge { None, Right, Bottom, Corner };

    void OnActivate(EditorToolContext& /*ctx*/) override {
        mIsResizing   = false;
        mHoverEdge    = Edge::None;
        mHoverTileIdx = -1;
    }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        if (mHoverTileIdx >= 0 && mHoverEdge != Edge::None) {
            mIsResizing    = true;
            mResizeTileIdx = mHoverTileIdx;
            mResizeEdge    = mHoverEdge;
            mResizeDragX   = mx;
            mResizeDragY   = my;
            mResizeOrigW   = ctx.level.tiles[mHoverTileIdx].w;
            mResizeOrigH   = ctx.level.tiles[mHoverTileIdx].h;
            ctx.SetStatus("Resizing tile " + std::to_string(mHoverTileIdx));
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    ToolResult OnMouseMove(EditorToolContext& ctx, int mx, int my) override {
        if (mIsResizing && mResizeTileIdx >= 0 &&
            mResizeTileIdx < static_cast<int>(ctx.level.tiles.size())) {
            auto& t  = ctx.level.tiles[mResizeTileIdx];
            int   dx = mx - mResizeDragX, dy = my - mResizeDragY;
            int   grid = ctx.Grid();
            if (mResizeEdge == Edge::Right || mResizeEdge == Edge::Corner)
                t.w = std::max(grid, ((mResizeOrigW + dx + grid / 2) / grid) * grid);
            if (mResizeEdge == Edge::Bottom || mResizeEdge == Edge::Corner)
                t.h = std::max(grid, ((mResizeOrigH + dy + grid / 2) / grid) * grid);
            ctx.SetStatus("Resize: " + std::to_string(t.w) + "x" + std::to_string(t.h));
            return ToolResult::Consumed;
        }

        // Hover detection
        if (my >= ctx.ToolbarH() && mx < ctx.CanvasW()) {
            mHoverEdge    = Edge::None;
            mHoverTileIdx = -1;
            for (int i = static_cast<int>(ctx.level.tiles.size()) - 1; i >= 0; --i) {
                Edge edge = DetectEdge(ctx, i, mx, my);
                if (edge != Edge::None) {
                    mHoverEdge    = edge;
                    mHoverTileIdx = i;
                    break;
                }
            }
        }
        return ToolResult::Ignored;
    }

    ToolResult OnMouseUp(EditorToolContext& ctx, int /*mx*/, int /*my*/,
                         Uint8 /*button*/, SDL_Keymod /*mods*/) override {
        if (mIsResizing) {
            mIsResizing    = false;
            mResizeTileIdx = -1;
            ctx.SetStatus("Resize committed");
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    void RenderOverlay(EditorToolContext& ctx, SDL_Surface* screen, int /*canvasW*/) override {
        constexpr SDL_Color handleCol = {255, 180, 0, 220};
        constexpr int       HS        = 10;
        float camX = ctx.CamX(), camY = ctx.CamY(), zoom = ctx.Zoom();

        if (mHoverTileIdx >= 0 && !mIsResizing &&
            mHoverTileIdx < static_cast<int>(ctx.level.tiles.size())) {
            const auto& t = ctx.level.tiles[mHoverTileIdx];
            int rx = static_cast<int>((t.x - camX) * zoom);
            int ry = static_cast<int>((t.y - camY) * zoom);
            int rw = static_cast<int>(t.w * zoom);
            int rh = static_cast<int>(t.h * zoom);
            if (mHoverEdge == Edge::Right || mHoverEdge == Edge::Corner)
                EditorToolContext::DrawRect(screen, {rx + rw - HS, ry + HS, HS, rh - HS * 2}, handleCol);
            if (mHoverEdge == Edge::Bottom || mHoverEdge == Edge::Corner)
                EditorToolContext::DrawRect(screen, {rx + HS, ry + rh - HS, rw - HS * 2, HS}, handleCol);
            if (mHoverEdge == Edge::Corner)
                EditorToolContext::DrawRect(screen, {rx + rw - HS, ry + rh - HS, HS, HS}, handleCol);
        }

        if (mIsResizing && mResizeTileIdx >= 0 &&
            mResizeTileIdx < static_cast<int>(ctx.level.tiles.size())) {
            const auto& t = ctx.level.tiles[mResizeTileIdx];
            int rx = static_cast<int>((t.x - camX) * zoom);
            int ry = static_cast<int>((t.y - camY) * zoom);
            int rw = static_cast<int>(t.w * zoom);
            int rh = static_cast<int>(t.h * zoom);
            EditorToolContext::DrawOutline(screen, {rx, ry, rw, rh}, {255, 220, 80, 255}, 2);
            int grid = ctx.Grid();
            Text szT(std::to_string(t.w / grid) + "x" + std::to_string(t.h / grid) + " tiles",
                     {255, 220, 80, 180}, rx + 4, ry + 4, 12);
            szT.RenderToSurface(screen);
        }
    }

  private:
    Edge DetectEdge(const EditorToolContext& ctx, int idx, int mx, int my) const {
        if (idx < 0 || idx >= static_cast<int>(ctx.level.tiles.size())) return Edge::None;
        const auto& t = ctx.level.tiles[idx];
        auto [wx, wy] = ctx.camera.ScreenToWorld(mx, my);
        int rx = static_cast<int>(t.x), ry = static_cast<int>(t.y);
        int rw = t.w, rh = t.h;
        if (wx < rx || wx > rx + rw || wy < ry || wy > ry + rh) return Edge::None;
        constexpr int HANDLE = 10;
        bool nearR = (wx >= rx + rw - HANDLE);
        bool nearB = (wy >= ry + rh - HANDLE);
        if (nearR && nearB) return Edge::Corner;
        if (nearR) return Edge::Right;
        if (nearB) return Edge::Bottom;
        return Edge::None;
    }

    Edge mHoverEdge    = Edge::None;
    int  mHoverTileIdx = -1;
    bool mIsResizing   = false;
    int  mResizeTileIdx = -1;
    Edge mResizeEdge    = Edge::None;
    int  mResizeDragX   = 0, mResizeDragY   = 0;
    int  mResizeOrigW   = 0, mResizeOrigH   = 0;
};
