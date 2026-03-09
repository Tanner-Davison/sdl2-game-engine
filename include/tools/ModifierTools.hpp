#pragma once
// ModifierTools.hpp
//
// Single-click toggle tools that modify tile flags: Prop, Ladder, Slope,
// Hazard, AntiGrav. They all share the same pattern: click a tile to toggle
// a boolean flag, with mutual-exclusion rules between certain flags.
// After toggling, they fall through to the base entity-drag so the user
// can reposition the entity while the modifier tool is active.

#include "tools/EditorTool.hpp"
#include <string>

// ═══════════════════════════════════════════════════════════════════════════════
// PropTool
// ═══════════════════════════════════════════════════════════════════════════════
class PropTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Prop"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        int ti = ctx.HitTile(mx, my);
        if (ti >= 0) {
            auto& t      = ctx.level.tiles[ti];
            bool nowProp  = !t.prop;
            t.prop        = nowProp;
            if (nowProp) {
                t.ladder = false;
                t.action = false;
                t.slope  = SlopeType::None;
            }
            bool isHazard = t.hazard;
            ctx.SetStatus(std::string("Tile ") + std::to_string(ti) +
                          (nowProp ? (isHazard ? " -> prop+hazard (walk-through, damages)"
                                               : " -> prop (no collision)")
                                   : " -> solid (collision on)"));
            return ToolResult::Consumed;
        }
        return StartEntityDrag(ctx, mx, my);
    }

    ToolResult OnMouseMove(EditorToolContext& ctx, int mx, int my) override {
        if (mIsDragging && my >= ctx.ToolbarH() && mx < ctx.CanvasW()) {
            UpdateEntityDrag(ctx, mx, my);
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    ToolResult OnMouseUp(EditorToolContext& /*ctx*/, int /*mx*/, int /*my*/,
                         Uint8 /*button*/, SDL_Keymod /*mods*/) override {
        StopEntityDrag();
        return ToolResult::Consumed;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// LadderTool
// ═══════════════════════════════════════════════════════════════════════════════
class LadderTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Ladder"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        int ti = ctx.HitTile(mx, my);
        if (ti >= 0) {
            auto& t        = ctx.level.tiles[ti];
            bool nowLadder  = !t.ladder;
            t.ladder        = nowLadder;
            if (nowLadder) {
                t.prop   = false;
                t.action = false;
                t.slope  = SlopeType::None;
            }
            ctx.SetStatus(std::string("Tile ") + std::to_string(ti) +
                          (nowLadder ? " -> ladder (climbable)"
                                     : " -> solid (ladder removed)"));
            return ToolResult::Consumed;
        }
        return StartEntityDrag(ctx, mx, my);
    }

    ToolResult OnMouseMove(EditorToolContext& ctx, int mx, int my) override {
        if (mIsDragging && my >= ctx.ToolbarH() && mx < ctx.CanvasW()) {
            UpdateEntityDrag(ctx, mx, my);
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    ToolResult OnMouseUp(EditorToolContext& /*ctx*/, int /*mx*/, int /*my*/,
                         Uint8 /*button*/, SDL_Keymod /*mods*/) override {
        StopEntityDrag();
        return ToolResult::Consumed;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// SlopeTool
// ═══════════════════════════════════════════════════════════════════════════════
class SlopeTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Slope"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        int ti = ctx.HitTile(mx, my);
        if (ti >= 0) {
            auto&       t = ctx.level.tiles[ti];
            SlopeType   next;
            std::string label;
            if (t.slope == SlopeType::None) {
                next  = SlopeType::DiagUpRight;
                label = "DiagUpRight (rises left->right)";
            } else if (t.slope == SlopeType::DiagUpRight) {
                next  = SlopeType::DiagUpLeft;
                label = "DiagUpLeft  (rises right->left)";
            } else {
                next  = SlopeType::None;
                label = "slope removed";
            }
            t.slope = next;
            if (next != SlopeType::None) {
                t.prop   = false;
                t.ladder = false;
                t.action = false;
            }
            ctx.SetStatus(std::string("Tile ") + std::to_string(ti) + " -> " + label);
            return ToolResult::Consumed;
        }
        return StartEntityDrag(ctx, mx, my);
    }

    ToolResult OnScroll(EditorToolContext& ctx, float wheelY,
                        int mx, int my, SDL_Keymod /*mods*/) override {
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        int hovSlope = ctx.HitTile(mx, my);
        if (hovSlope >= 0 && ctx.level.tiles[hovSlope].slope != SlopeType::None) {
            float& frac = ctx.level.tiles[hovSlope].slopeHeightFrac;
            frac = std::clamp(frac + wheelY * 0.05f, 0.05f, 1.0f);
            frac = std::round(frac * 20.0f) / 20.0f;
            ctx.SetStatus("Slope height: " + std::to_string(static_cast<int>(frac * 100)) +
                          "%  (scroll to adjust)");
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    ToolResult OnMouseMove(EditorToolContext& ctx, int mx, int my) override {
        if (mIsDragging && my >= ctx.ToolbarH() && mx < ctx.CanvasW()) {
            UpdateEntityDrag(ctx, mx, my);
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    ToolResult OnMouseUp(EditorToolContext& /*ctx*/, int /*mx*/, int /*my*/,
                         Uint8 /*button*/, SDL_Keymod /*mods*/) override {
        StopEntityDrag();
        return ToolResult::Consumed;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// HazardTool
// ═══════════════════════════════════════════════════════════════════════════════
class HazardTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Hazard"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        int ti = ctx.HitTile(mx, my);
        if (ti >= 0) {
            auto& t        = ctx.level.tiles[ti];
            bool nowHazard  = !t.hazard;
            t.hazard        = nowHazard;
            if (nowHazard) {
                t.ladder = false;
                t.action = false;
                t.slope  = SlopeType::None;
            }
            bool isProp = t.prop;
            ctx.SetStatus(std::string("Tile ") + std::to_string(ti) +
                          (nowHazard ? (isProp ? " -> hazard+prop (walk-through, damages)"
                                               : " -> hazard (solid, 30 HP/sec)")
                                     : " -> solid (hazard removed)"));
            return ToolResult::Consumed;
        }
        return StartEntityDrag(ctx, mx, my);
    }

    ToolResult OnMouseMove(EditorToolContext& ctx, int mx, int my) override {
        if (mIsDragging && my >= ctx.ToolbarH() && mx < ctx.CanvasW()) {
            UpdateEntityDrag(ctx, mx, my);
            return ToolResult::Consumed;
        }
        return ToolResult::Ignored;
    }

    ToolResult OnMouseUp(EditorToolContext& /*ctx*/, int /*mx*/, int /*my*/,
                         Uint8 /*button*/, SDL_Keymod /*mods*/) override {
        StopEntityDrag();
        return ToolResult::Consumed;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// AntiGravTool (Float)
// ═══════════════════════════════════════════════════════════════════════════════
class AntiGravTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Float"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        int ti = ctx.HitTile(mx, my);
        if (ti >= 0) {
            bool now                         = !ctx.level.tiles[ti].antiGravity;
            ctx.level.tiles[ti].antiGravity  = now;
            ctx.SetStatus("Tile " + std::to_string(ti) +
                          (now ? " -> floating (anti-gravity)" : " -> normal gravity"));
            return ToolResult::Consumed;
        }
        int ei = ctx.HitEnemy(mx, my);
        if (ei >= 0) {
            bool now                           = !ctx.level.enemies[ei].antiGravity;
            ctx.level.enemies[ei].antiGravity  = now;
            ctx.SetStatus("Enemy " + std::to_string(ei) +
                          (now ? " -> floating" : " -> normal gravity"));
            return ToolResult::Consumed;
        }
        return ToolResult::Consumed;
    }
};
