#pragma once
// EditorTool.hpp
//
// Abstract base for all level-editor tools. Each tool encapsulates its own
// state machine (mouse-down / move / up, key-down, scroll) and its own
// canvas overlay rendering. The LevelEditorScene orchestrator owns the
// active tool via std::unique_ptr<EditorTool> and forwards events to it.
//
// Design goals:
//   - Zero virtual overhead for hot methods -- OnMouseMove is the only one
//     called per-pixel and it returns quickly when idle.
//   - Tools never include LevelEditorScene.hpp. They interact with shared
//     state exclusively through EditorToolContext.
//   - Render overlay is optional (default = no-op). Only tools that need
//     visual feedback (Select, Resize, Hitbox, MovingPlat) override it.

#include "tools/EditorToolContext.hpp"
#include <SDL3/SDL.h>
#include <string>

// Enumerates every tool the editor supports. The orchestrator maps toolbar
// buttons and hotkeys to these values. Tools themselves don't need to know
// their own enum -- the orchestrator picks the right subclass.
enum class ToolId {
    Coin,
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
};

// Return value from event handlers: tells the orchestrator whether the
// event was consumed by this tool.
enum class ToolResult {
    Consumed,  // Event handled; don't propagate further.
    Ignored,   // Tool didn't care; let the orchestrator try other handlers.
};

class EditorTool {
  public:
    virtual ~EditorTool() = default;

    // Human-readable name shown in the toolbar status label.
    [[nodiscard]] virtual const char* Name() const = 0;

    // Called once when this tool becomes the active tool. Override to reset
    // any per-activation state (e.g. clear selection, reset drag flags).
    virtual void OnActivate(EditorToolContext& /*ctx*/) {}

    // Called when another tool replaces this one. Override to commit or
    // cancel any in-progress operation (e.g. stop text input).
    virtual void OnDeactivate(EditorToolContext& /*ctx*/) {}

    // ── Event handlers ───────────────────────────────────────────────────────
    // Return ToolResult::Consumed to swallow the event.

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

    // ── Render overlay ───────────────────────────────────────────────────────
    // Called after the main canvas is rendered but before the toolbar/palette.
    // Tools that need visual feedback (selection marquee, resize handles,
    // hitbox handles, platform path lines) override this.
    virtual void RenderOverlay(EditorToolContext& /*ctx*/, SDL_Surface* /*screen*/,
                               int /*canvasW*/) {}

    // ── Drag helpers (generic entity drag, shared by modifier tools) ─────────
    // These can be overridden by tools that need custom drag behavior.
    [[nodiscard]] bool IsDragging() const { return mIsDragging; }

  protected:
    // Generic entity drag state -- used by modifier tools that don't have
    // their own specialized drag (e.g. Prop, Ladder, Hazard, AntiGrav).
    bool mIsDragging  = false;
    int  mDragIndex   = -1;
    bool mDragIsCoin  = false;
    bool mDragIsTile  = false;

    // Start dragging the entity at screen coords (mx, my). Returns Consumed
    // if an entity was found, Ignored otherwise.
    ToolResult StartEntityDrag(EditorToolContext& ctx, int mx, int my) {
        int ti = ctx.HitTile(mx, my);
        if (ti >= 0) {
            mIsDragging = true; mDragIndex = ti;
            mDragIsTile = true; mDragIsCoin = false;
            return ToolResult::Consumed;
        }
        int ci = ctx.HitCoin(mx, my);
        if (ci >= 0) {
            mIsDragging = true; mDragIndex = ci;
            mDragIsCoin = true; mDragIsTile = false;
            return ToolResult::Consumed;
        }
        int ei = ctx.HitEnemy(mx, my);
        if (ei >= 0) {
            mIsDragging = true; mDragIndex = ei;
            mDragIsCoin = false; mDragIsTile = false;
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    // Update entity position during drag.
    void UpdateEntityDrag(EditorToolContext& ctx, int mx, int my) {
        if (!mIsDragging || mDragIndex < 0) return;
        auto [sx, sy] = ctx.SnapToGrid(mx, my);
        if (mDragIsTile && mDragIndex < static_cast<int>(ctx.level.tiles.size())) {
            ctx.level.tiles[mDragIndex].x = static_cast<float>(sx);
            ctx.level.tiles[mDragIndex].y = static_cast<float>(sy);
        } else if (mDragIsCoin && mDragIndex < static_cast<int>(ctx.level.coins.size())) {
            ctx.level.coins[mDragIndex].x = static_cast<float>(sx);
            ctx.level.coins[mDragIndex].y = static_cast<float>(sy);
        } else if (!mDragIsCoin && !mDragIsTile &&
                   mDragIndex < static_cast<int>(ctx.level.enemies.size())) {
            ctx.level.enemies[mDragIndex].x = static_cast<float>(sx);
            ctx.level.enemies[mDragIndex].y = static_cast<float>(sy);
        }
    }

    void StopEntityDrag() { mIsDragging = false; }
};
