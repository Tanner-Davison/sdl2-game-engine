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
                t.action.reset();
                t.slope.reset();
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
// GoalTool
// Click a tile to toggle its goal flag. Goal tiles must be collected by
// the player to complete the level. Any tile type can be a goal.
// ═══════════════════════════════════════════════════════════════════════════════
class GoalTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Goal"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        int ti = ctx.HitTile(mx, my);
        if (ti >= 0) {
            auto& t     = ctx.level.tiles[ti];
            bool nowGoal = !t.goal;
            t.goal       = nowGoal;
            ctx.SetStatus(std::string("Tile ") + std::to_string(ti) +
                          (nowGoal ? " -> goal (collect to complete level)"
                                   : " -> goal removed"));
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
                t.action.reset();
                t.slope.reset();
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
            SlopeType   curType = t.GetSlopeType();
            std::string label;
            if (curType == SlopeType::None) {
                t.slope = SlopeData{SlopeType::DiagUpRight, 1.0f};
                label = "DiagUpRight (rises left->right)";
            } else if (curType == SlopeType::DiagUpRight) {
                t.slope->type = SlopeType::DiagUpLeft;
                label = "DiagUpLeft  (rises right->left)";
            } else {
                t.slope.reset();
                label = "slope removed";
            }
            if (t.HasSlope()) {
                t.prop   = false;
                t.ladder = false;
                t.action.reset();
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
        if (hovSlope >= 0 && ctx.level.tiles[hovSlope].HasSlope()) {
            float& frac = ctx.level.tiles[hovSlope].slope->heightFrac;
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
                t.action.reset();
                t.slope.reset();
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

// ═══════════════════════════════════════════════════════════════════════════════
// ShooterTool (Turret)
// Left-click a tile to toggle shooter, right-click to cycle side (Top/Right/Bottom/Left).
// ═══════════════════════════════════════════════════════════════════════════════
class ShooterTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Turret"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;

        int ti = ctx.HitTile(mx, my);
        if (ti < 0) {
            if (button == SDL_BUTTON_LEFT)
                return StartEntityDrag(ctx, mx, my);
            return ToolResult::Ignored;
        }

        auto& t = ctx.level.tiles[ti];

        if (button == SDL_BUTTON_RIGHT) {
            if (!t.HasShooter()) return ToolResult::Consumed;
            int s = (static_cast<int>(t.shooter->side) + 1) % 4;
            t.shooter->side = static_cast<ShooterSide>(s);
            static const char* kNames[] = {"Top","Right","Bottom","Left"};
            ctx.SetStatus("Tile " + std::to_string(ti) + " shooter side -> " + kNames[s]);
            return ToolResult::Consumed;
        }

        if (button == SDL_BUTTON_LEFT) {
            if (t.HasShooter()) {
                t.shooter.reset();
                ctx.SetStatus("Tile " + std::to_string(ti) + " -> shooter removed");
            } else {
                t.shooter = ShooterData{};
                ctx.SetStatus("Tile " + std::to_string(ti) + " -> shooter (RClick to cycle side)");
            }
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
                         Uint8 button, SDL_Keymod /*mods*/) override {
        (void)button;
        StopEntityDrag();
        return ToolResult::Consumed;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// ShieldTool
// Left-click a tile to toggle shield pickup. Automatically adds ActionTag
// (hitsRequired=1) so the tile is slashable to pick up.
// ═══════════════════════════════════════════════════════════════════════════════
class ShieldTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Shield"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        int ti = ctx.HitTile(mx, my);
        if (ti >= 0) {
            auto& t = ctx.level.tiles[ti];
            if (t.HasShield()) {
                t.shield.reset();
                if (t.HasAction() && t.action->hitsRequired == 1 &&
                    t.action->destroyAnimPath.empty() && t.action->group == 0)
                    t.action.reset();
                ctx.SetStatus("Tile " + std::to_string(ti) + " -> shield removed");
            } else {
                t.shield = ShieldData{};
                if (!t.HasAction())
                    t.action = ActionData{0, 1, "", false};
                ctx.SetStatus("Tile " + std::to_string(ti) +
                              " -> shield pickup (slash to collect)");
            }
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
                         Uint8 button, SDL_Keymod /*mods*/) override {
        (void)button;
        StopEntityDrag();
        return ToolResult::Consumed;
    }
};
