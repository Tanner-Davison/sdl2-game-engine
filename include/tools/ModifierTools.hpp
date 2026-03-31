#pragma once
// ModifierTools.hpp
//
// Single-click toggle tools that modify tile flags. They share the same
// pattern: click a tile to toggle a flag (with mutual-exclusion rules),
// then fall through to base entity-drag for repositioning.

#include "tools/EditorTool.hpp"
#include <string>

// --- PropTool ---

class PropTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Prop"; }

    bool     popupOpen  = false;
    int      popupIdx   = -1;
    SDL_Rect popupRect{};
    SDL_Rect frontRect{};
    SDL_Rect backRect{};

    void OnDeactivate(EditorToolContext& /*ctx*/) override {
        popupOpen = false;
        popupIdx  = -1;
    }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (popupOpen) {
            if (button == SDL_BUTTON_LEFT && popupIdx >= 0 &&
                popupIdx < (int)ctx.level.tiles.size()) {
                auto& t = ctx.level.tiles[popupIdx];
                if (hitR(frontRect, mx, my)) {
                    t.propBehind = false;
                    ctx.SetStatus("Tile " + std::to_string(popupIdx) + " prop -> FRONT (over player)");
                    return ToolResult::Consumed;
                }
                if (hitR(backRect, mx, my)) {
                    t.propBehind = true;
                    ctx.SetStatus("Tile " + std::to_string(popupIdx) + " prop -> BACK (behind player)");
                    return ToolResult::Consumed;
                }
            }
            // Close button
            SDL_Rect closeBtn = {popupRect.x + popupRect.w - 40, popupRect.y + 4, 36, 20};
            if (button == SDL_BUTTON_LEFT && hitR(closeBtn, mx, my)) {
                popupOpen = false;
                popupIdx  = -1;
                return ToolResult::Consumed;
            }
            if (hitR(popupRect, mx, my))
                return ToolResult::Consumed;
            popupOpen = false;
            popupIdx  = -1;
        }

        if (button == SDL_BUTTON_RIGHT) {
            if (my >= ctx.ToolbarH() && mx < ctx.CanvasW()) {
                int ti = ctx.HitTile(mx, my);
                if (ti >= 0 && ctx.level.tiles[ti].prop) {
                    popupIdx  = ti;
                    popupOpen = true;
                    popupRect = {mx, my, 180, 72};
                    if (popupRect.x + popupRect.w > ctx.CanvasW())
                        popupRect.x = ctx.CanvasW() - popupRect.w - 4;
                    ctx.SetStatus("RClick prop #" + std::to_string(ti) + " — choose Front/Back");
                    return ToolResult::Consumed;
                }
            }
        }

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
            } else {
                t.propBehind = false;
            }
            bool isHazard = t.hazard;
            std::string layer = t.propBehind ? "back" : "front";
            ctx.SetStatus(std::string("Tile ") + std::to_string(ti) +
                          (nowProp ? (isHazard ? " -> prop+hazard (walk-through, damages)"
                                               : (" -> prop [" + layer + "] (no collision)  RClick to change"))
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

    void RenderOverlay(EditorToolContext& ctx, SDL_Surface* screen,
                       int /*canvasW*/) override {
        if (!popupOpen || popupIdx < 0 ||
            popupIdx >= (int)ctx.level.tiles.size())
            return;

        const auto& t = ctx.level.tiles[popupIdx];
        bool isBehind = t.propBehind;

        ctx.DrawRect(screen, popupRect, {14, 22, 40, 245});
        ctx.DrawOutline(screen, popupRect, {120, 180, 255, 255}, 2);

        SDL_Surface* title = ctx.GetBadge("Prop Layer", {180, 220, 255, 255});
        if (title) {
            SDL_Rect td = {popupRect.x + 6, popupRect.y + 6, title->w, title->h};
            SDL_BlitSurface(title, nullptr, screen, &td);
        }

        SDL_Rect closeBtn = {popupRect.x + popupRect.w - 40, popupRect.y + 4, 36, 20};
        ctx.DrawRect(screen, closeBtn, {60, 30, 30, 220});
        ctx.DrawOutline(screen, closeBtn, {180, 80, 80, 255});
        SDL_Surface* closeLbl = ctx.GetBadge("close", {220, 140, 140, 255});
        if (closeLbl) {
            SDL_Rect cd = {closeBtn.x + 4, closeBtn.y + 4, closeLbl->w, closeLbl->h};
            SDL_BlitSurface(closeLbl, nullptr, screen, &cd);
        }

        const int optY = popupRect.y + 30;
        const int boxSz = 14;
        const int pad = 8;

        // Front option
        frontRect = {popupRect.x + pad, optY, popupRect.w / 2 - pad, 28};
        SDL_Color fBg = !isBehind ? SDL_Color{30, 60, 100, 255} : SDL_Color{20, 28, 45, 255};
        ctx.DrawRect(screen, frontRect, fBg);
        ctx.DrawOutline(screen, frontRect, !isBehind ? SDL_Color{100, 180, 255, 255}
                                                      : SDL_Color{50, 60, 80, 255});
        SDL_Rect fBox = {frontRect.x + 6, optY + 7, boxSz, boxSz};
        ctx.DrawRect(screen, fBox, {10, 14, 24, 255});
        ctx.DrawOutline(screen, fBox, {80, 120, 180, 255});
        if (!isBehind) {
            SDL_Rect fFill = {fBox.x + 3, fBox.y + 3, boxSz - 6, boxSz - 6};
            ctx.DrawRect(screen, fFill, {100, 200, 255, 255});
        }
        SDL_Surface* fLbl = ctx.GetBadge("Front", !isBehind ? SDL_Color{200, 240, 255, 255}
                                                              : SDL_Color{120, 130, 150, 255});
        if (fLbl) {
            SDL_Rect fl = {fBox.x + boxSz + 4, optY + 8, fLbl->w, fLbl->h};
            SDL_BlitSurface(fLbl, nullptr, screen, &fl);
        }

        // Back option
        backRect = {popupRect.x + popupRect.w / 2 + 2, optY, popupRect.w / 2 - pad - 2, 28};
        SDL_Color bBg = isBehind ? SDL_Color{30, 60, 100, 255} : SDL_Color{20, 28, 45, 255};
        ctx.DrawRect(screen, backRect, bBg);
        ctx.DrawOutline(screen, backRect, isBehind ? SDL_Color{100, 180, 255, 255}
                                                    : SDL_Color{50, 60, 80, 255});
        SDL_Rect bBox = {backRect.x + 6, optY + 7, boxSz, boxSz};
        ctx.DrawRect(screen, bBox, {10, 14, 24, 255});
        ctx.DrawOutline(screen, bBox, {80, 120, 180, 255});
        if (isBehind) {
            SDL_Rect bFill = {bBox.x + 3, bBox.y + 3, boxSz - 6, boxSz - 6};
            ctx.DrawRect(screen, bFill, {100, 200, 255, 255});
        }
        SDL_Surface* bLbl = ctx.GetBadge("Back", isBehind ? SDL_Color{200, 240, 255, 255}
                                                            : SDL_Color{120, 130, 150, 255});
        if (bLbl) {
            SDL_Rect bl = {bBox.x + boxSz + 4, optY + 8, bLbl->w, bLbl->h};
            SDL_BlitSurface(bLbl, nullptr, screen, &bl);
        }
    }

  private:
    static bool hitR(const SDL_Rect& r, int x, int y) {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    }
};

// --- GoalTool ---
// Click a tile to toggle its goal flag. Any tile type can be a goal.

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

// --- LadderTool ---

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

// --- SlopeTool ---

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

// --- HazardTool ---

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

// --- AntiGravTool (Float) ---

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

// --- ShooterTool (Turret) ---
// LClick toggles shooter, RClick cycles side (Top/Right/Bottom/Left).

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

// --- ShieldTool ---
// LClick toggles shield pickup. Automatically adds ActionTag (hitsRequired=1)
// so the tile is slashable to pick up.

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
