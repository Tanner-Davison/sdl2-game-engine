#pragma once
// HitboxTool.hpp -- 8-handle hitbox editor for per-tile collision rectangles.

#include "tools/EditorTool.hpp"
#include "Text.hpp"
#include <algorithm>
#include <string>

class HitboxTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Hitbox"; }

    enum class Handle {
        None, Left, Right, Top, Bottom, TopLeft, TopRight, BotLeft, BotRight
    };

    void OnActivate(EditorToolContext& /*ctx*/) override {
        mTileIdx  = -1;
        mDragging = false;
        mHoverHdl = Handle::None;
    }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;

        // Priority 1: handle drag on already-selected tile
        if (mTileIdx >= 0 && mHoverHdl != Handle::None) {
            auto& t       = ctx.level.tiles[mTileIdx];
            mDragging     = true;
            mHandle       = mHoverHdl;
            mDragX        = mx; mDragY = my;
            mOrigOffX     = t.hitbox->offX; mOrigOffY = t.hitbox->offY;
            mOrigW        = t.hitbox->w;    mOrigH    = t.hitbox->h;
            return ToolResult::Consumed;
        }
        // Priority 2: keep current tile if click is inside its visual bounds
        bool keptCurrent = false;
        if (mTileIdx >= 0 && mTileIdx < static_cast<int>(ctx.level.tiles.size())) {
            const auto& ct = ctx.level.tiles[mTileIdx];
            auto [csx, csy] = ctx.camera.WorldToScreen(ct.x, ct.y);
            int csw = static_cast<int>(ct.w * ctx.Zoom());
            int csh = static_cast<int>(ct.h * ctx.Zoom());
            if (mx >= csx && mx <= csx + csw && my >= csy && my <= csy + csh)
                keptCurrent = true;
        }
        if (!keptCurrent) {
            int ti = ctx.HitTile(mx, my);
            if (ti >= 0) {
                mTileIdx = ti;
                auto& t  = ctx.level.tiles[ti];
                if (!t.HasHitbox()) {
                    t.hitbox = HitboxData{0, 0, t.w, t.h};
                }
                ctx.SetStatus("Hitbox: tile " + std::to_string(ti) + "  drag edges to adjust");
            } else {
                mTileIdx = -1;
            }
        }
        return ToolResult::Consumed;
    }

    ToolResult OnMouseMove(EditorToolContext& ctx, int mx, int my) override {
        if (mDragging && mTileIdx >= 0 && mTileIdx < static_cast<int>(ctx.level.tiles.size())) {
            auto& t = ctx.level.tiles[mTileIdx];
            int dx = mx - mDragX, dy = my - mDragY;
            constexpr int MIN_SIDE = 4;
            switch (mHandle) {
                case Handle::Left: {
                    int nOff = std::min(mOrigOffX + dx, mOrigOffX + mOrigW - MIN_SIDE);
                    t.hitbox->w = std::max(MIN_SIDE, mOrigW - (nOff - mOrigOffX));
                    t.hitbox->offX = nOff; break;
                }
                case Handle::Right:
                    t.hitbox->w = std::clamp(mOrigW + dx, MIN_SIDE, t.w - t.hitbox->offX); break;
                case Handle::Top: {
                    int nOff = std::min(mOrigOffY + dy, mOrigOffY + mOrigH - MIN_SIDE);
                    t.hitbox->h = std::max(MIN_SIDE, mOrigH - (nOff - mOrigOffY));
                    t.hitbox->offY = nOff; break;
                }
                case Handle::Bottom:
                    t.hitbox->h = std::clamp(mOrigH + dy, MIN_SIDE, t.h - t.hitbox->offY); break;
                case Handle::TopLeft: {
                    int nox = std::min(mOrigOffX + dx, mOrigOffX + mOrigW - MIN_SIDE);
                    int noy = std::min(mOrigOffY + dy, mOrigOffY + mOrigH - MIN_SIDE);
                    t.hitbox->w = std::max(MIN_SIDE, mOrigW - (nox - mOrigOffX));
                    t.hitbox->h = std::max(MIN_SIDE, mOrigH - (noy - mOrigOffY));
                    t.hitbox->offX = nox; t.hitbox->offY = noy; break;
                }
                case Handle::TopRight: {
                    int noy = std::min(mOrigOffY + dy, mOrigOffY + mOrigH - MIN_SIDE);
                    t.hitbox->w = std::max(MIN_SIDE, mOrigW + dx);
                    t.hitbox->h = std::max(MIN_SIDE, mOrigH - (noy - mOrigOffY));
                    t.hitbox->offY = noy; break;
                }
                case Handle::BotLeft: {
                    int nox = std::min(mOrigOffX + dx, mOrigOffX + mOrigW - MIN_SIDE);
                    t.hitbox->w = std::max(MIN_SIDE, mOrigW - (nox - mOrigOffX));
                    t.hitbox->h = std::max(MIN_SIDE, mOrigH + dy);
                    t.hitbox->offX = nox; break;
                }
                case Handle::BotRight:
                    t.hitbox->w = std::max(MIN_SIDE, mOrigW + dx);
                    t.hitbox->h = std::max(MIN_SIDE, mOrigH + dy); break;
                default: break;
            }
            t.hitbox->offX = std::max(0, std::min(t.hitbox->offX, t.w - MIN_SIDE));
            t.hitbox->offY = std::max(0, std::min(t.hitbox->offY, t.h - MIN_SIDE));
            ctx.SetStatus("Hitbox: off(" + std::to_string(t.hitbox->offX) + "," +
                          std::to_string(t.hitbox->offY) + ") size(" +
                          std::to_string(t.hitbox->w) + "x" + std::to_string(t.hitbox->h) + ")");
            return ToolResult::Consumed;
        }
        if (mTileIdx >= 0 && !mDragging && mTileIdx < static_cast<int>(ctx.level.tiles.size())) {
            mHoverHdl = Handle::None;
            const auto& t = ctx.level.tiles[mTileIdx];
            auto [htsx, htsy] = ctx.camera.WorldToScreen(t.x + t.hitbox->offX, t.y + t.hitbox->offY);
            int hw = static_cast<int>(t.hitbox->w * ctx.Zoom());
            int hh = static_cast<int>(t.hitbox->h * ctx.Zoom());
            constexpr int H = 10;
            bool nL = (mx >= htsx - H && mx <= htsx + H);
            bool nR = (mx >= htsx + hw - H && mx <= htsx + hw + H);
            bool nT = (my >= htsy - H && my <= htsy + H);
            bool nB = (my >= htsy + hh - H && my <= htsy + hh + H);
            bool inH = (mx >= htsx && mx <= htsx + hw);
            bool inV = (my >= htsy && my <= htsy + hh);
            if (nL && nT) mHoverHdl = Handle::TopLeft;
            else if (nR && nT) mHoverHdl = Handle::TopRight;
            else if (nL && nB) mHoverHdl = Handle::BotLeft;
            else if (nR && nB) mHoverHdl = Handle::BotRight;
            else if (nL && inV) mHoverHdl = Handle::Left;
            else if (nR && inV) mHoverHdl = Handle::Right;
            else if (nT && inH) mHoverHdl = Handle::Top;
            else if (nB && inH) mHoverHdl = Handle::Bottom;
        }
        return ToolResult::Ignored;
    }

    ToolResult OnMouseUp(EditorToolContext& ctx, int /*mx*/, int /*my*/,
                         Uint8 /*button*/, SDL_Keymod /*mods*/) override {
        if (mDragging) {
            mDragging = false;
            mHandle   = Handle::None;
            if (mTileIdx >= 0 && mTileIdx < static_cast<int>(ctx.level.tiles.size())) {
                auto& t = ctx.level.tiles[mTileIdx];
                ctx.SetStatus("Hitbox: off(" + std::to_string(t.hitbox->offX) + "," +
                              std::to_string(t.hitbox->offY) + ") size(" +
                              std::to_string(t.hitbox->w) + "x" + std::to_string(t.hitbox->h) + ")");
            }
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    void RenderOverlay(EditorToolContext& ctx, SDL_Surface* screen, int /*canvasW*/) override {
        if (mTileIdx < 0 || mTileIdx >= static_cast<int>(ctx.level.tiles.size())) return;
        const auto& t = ctx.level.tiles[mTileIdx];
        float camX = ctx.CamX(), camY = ctx.CamY(), zoom = ctx.Zoom();

        int tileX = static_cast<int>((t.x - camX) * zoom);
        int tileY = static_cast<int>((t.y - camY) * zoom);
        int tileW = static_cast<int>(t.w * zoom);
        int tileH = static_cast<int>(t.h * zoom);

        int hx = static_cast<int>((t.x - camX + t.hitbox->offX) * zoom);
        int hy = static_cast<int>((t.y - camY + t.hitbox->offY) * zoom);
        int hw = static_cast<int>(t.hitbox->w * zoom);
        int hh = static_cast<int>(t.hitbox->h * zoom);

        EditorToolContext::DrawRectAlpha(screen, {tileX, tileY, tileW, tileH}, {20, 20, 30, 80});
        EditorToolContext::DrawOutline(screen, {tileX, tileY, tileW, tileH}, {255, 255, 255, 80}, 1);
        EditorToolContext::DrawRectAlpha(screen, {hx, hy, hw, hh}, {60, 140, 255, 30});
        EditorToolContext::DrawOutline(screen, {hx, hy, hw, hh}, {80, 180, 255, 255}, 2);

        constexpr int HS = 10;
        constexpr SDL_Color hcNorm = {80, 180, 255, 220};
        constexpr SDL_Color hcHov  = {255, 220, 80, 255};
        auto col = [&](Handle h) -> SDL_Color { return (mHoverHdl == h) ? hcHov : hcNorm; };
        auto drawH = [&](int cx, int cy, Handle h) {
            EditorToolContext::DrawRect(screen, {cx - HS/2, cy - HS/2, HS, HS}, col(h));
            EditorToolContext::DrawOutline(screen, {cx - HS/2, cy - HS/2, HS, HS}, {20, 20, 40, 255});
        };
        int cx = hx + hw / 2, cy = hy + hh / 2;
        drawH(hx, hy, Handle::TopLeft);     drawH(cx, hy, Handle::Top);      drawH(hx + hw, hy, Handle::TopRight);
        drawH(hx, cy, Handle::Left);                                          drawH(hx + hw, cy, Handle::Right);
        drawH(hx, hy + hh, Handle::BotLeft); drawH(cx, hy + hh, Handle::Bottom); drawH(hx + hw, hy + hh, Handle::BotRight);

        std::string info = "HB: " + std::to_string(hw) + "x" + std::to_string(hh) + " @(" +
                           std::to_string(t.hitbox->offX) + "," + std::to_string(t.hitbox->offY) + ")";
        EditorToolContext::DrawRect(screen, {hx, hy - 16, static_cast<int>(info.size()) * 7, 14}, {10, 20, 50, 200});
        Text infoT(info, SDL_Color{180, 220, 255, 255}, hx + 2, hy - 15, 10);
        infoT.RenderToSurface(screen);
    }

  private:
    int    mTileIdx  = -1;
    bool   mDragging = false;
    Handle mHandle   = Handle::None;
    Handle mHoverHdl = Handle::None;
    int    mDragX = 0, mDragY = 0;
    int    mOrigOffX = 0, mOrigOffY = 0;
    int    mOrigW = 0, mOrigH = 0;
};
