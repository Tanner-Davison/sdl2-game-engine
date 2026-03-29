#pragma once
// EditorTool.hpp
//
// Abstract base for all level-editor tools. Tools interact with shared state
// exclusively through EditorToolContext (never include LevelEditorScene.hpp).

#include "tools/EditorToolContext.hpp"
#include <SDL3/SDL.h>
#include <string>

enum class ToolId {
    Goal,
    Enemy,
    Erase,
    PlayerStart,
    Tile,
    Resize,
    Prop,
    Ladder,
    Action,
    Slope,
    Hitbox,
    Hazard,
    AntiGrav,
    MovingPlat,
    Select,
    MoveCam,
    PowerUp,
    Shooter,
    Shield,
};

enum class ToolResult {
    Consumed,
    Ignored,
};

class EditorTool {
  public:
    virtual ~EditorTool() = default;

    [[nodiscard]] virtual const char* Name() const = 0;

    virtual void OnActivate(EditorToolContext& /*ctx*/) {}
    virtual void OnDeactivate(EditorToolContext& /*ctx*/) {}

    virtual ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                                   Uint8 button, SDL_Keymod mods) = 0;

    virtual ToolResult OnMouseUp(EditorToolContext& ctx, int mx, int my,
                                 Uint8 button, SDL_Keymod mods) {
        (void)ctx; (void)mx; (void)my; (void)button; (void)mods;
        return ToolResult::Ignored;
    }

    virtual ToolResult OnMouseMove(EditorToolContext& ctx, int mx, int my) {
        (void)ctx; (void)mx; (void)my;
        return ToolResult::Ignored;
    }

    virtual ToolResult OnKeyDown(EditorToolContext& ctx, SDL_Keycode key,
                                 SDL_Keymod mods) {
        (void)ctx; (void)key; (void)mods;
        return ToolResult::Ignored;
    }

    virtual ToolResult OnScroll(EditorToolContext& ctx, float wheelY,
                                int mx, int my, SDL_Keymod mods) {
        (void)ctx; (void)wheelY; (void)mx; (void)my; (void)mods;
        return ToolResult::Ignored;
    }

    // Called after main canvas render but before toolbar/palette.
    virtual void RenderOverlay(EditorToolContext& /*ctx*/, SDL_Surface* /*screen*/,
                               int /*canvasW*/) {}

    [[nodiscard]] bool IsDragging() const { return mIsDragging; }

  protected:
    bool mIsDragging  = false;
    int  mDragIndex   = -1;
    bool mDragIsTile  = false;

    ToolResult StartEntityDrag(EditorToolContext& ctx, int mx, int my) {
        int ti = ctx.HitTile(mx, my);
        if (ti >= 0) {
            mIsDragging = true; mDragIndex = ti;
            mDragIsTile = true;
            return ToolResult::Consumed;
        }
        int ei = ctx.HitEnemy(mx, my);
        if (ei >= 0) {
            mIsDragging = true; mDragIndex = ei;
            mDragIsTile = false;
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    void UpdateEntityDrag(EditorToolContext& ctx, int mx, int my) {
        if (!mIsDragging || mDragIndex < 0) return;
        auto [sx, sy] = ctx.SnapToGrid(mx, my);
        if (mDragIsTile && mDragIndex < static_cast<int>(ctx.level.tiles.size())) {
            ctx.level.tiles[mDragIndex].x = static_cast<float>(sx);
            ctx.level.tiles[mDragIndex].y = static_cast<float>(sy);
        } else if (!mDragIsTile &&
                   mDragIndex < static_cast<int>(ctx.level.enemies.size())) {
            ctx.level.enemies[mDragIndex].x = static_cast<float>(sx);
            ctx.level.enemies[mDragIndex].y = static_cast<float>(sy);
        }
    }

    void StopEntityDrag() { mIsDragging = false; }
};
