#pragma once
// PlacementTools.hpp
//
// Simple one-click tools: Coin, Enemy, Tile, Erase, PlayerStart.
// These tools place or remove entities on left-click and have no
// complex drag or state-machine behavior beyond the base entity drag.

#include "tools/EditorTool.hpp"
#include "EditorSurfaceCache.hpp"
#include <string>

// ─── Forward declarations for palette query ──────────────────────────────────
// The Tile tool needs to know the selected palette item. Rather than
// including EditorPalette.hpp (which would pull in too many deps), we
// accept a small callback struct that the orchestrator fills in.
struct TilePlacementInfo {
    bool        hasSelection = false;
    bool        isFolder     = false;
    std::string imagePath;
    std::string label;
};

// ═══════════════════════════════════════════════════════════════════════════════
// CoinTool
// ═══════════════════════════════════════════════════════════════════════════════
class CoinTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Coin"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        auto [sx, sy] = ctx.SnapToGrid(mx, my);
        ctx.level.coins.push_back({static_cast<float>(sx), static_cast<float>(sy)});
        ctx.SetStatus("Coin at " + std::to_string(sx) + "," + std::to_string(sy));
        return ToolResult::Consumed;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// EnemyTool
// ═══════════════════════════════════════════════════════════════════════════════
class EnemyTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Enemy"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        auto [sx, sy] = ctx.SnapToGrid(mx, my);
        ctx.level.enemies.push_back(
            {static_cast<float>(sx), static_cast<float>(sy), 120.0f});
        ctx.SetStatus("Enemy at " + std::to_string(sx) + "," + std::to_string(sy));
        return ToolResult::Consumed;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// EraseTool
// ═══════════════════════════════════════════════════════════════════════════════
class EraseTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Erase"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;

        int ci = ctx.HitCoin(mx, my);
        if (ci >= 0) {
            ctx.level.coins.erase(ctx.level.coins.begin() + ci);
            ctx.SetStatus("Erased coin");
            return ToolResult::Consumed;
        }
        int ei = ctx.HitEnemy(mx, my);
        if (ei >= 0) {
            ctx.level.enemies.erase(ctx.level.enemies.begin() + ei);
            ctx.SetStatus("Erased enemy");
            return ToolResult::Consumed;
        }
        int ti = ctx.HitTile(mx, my);
        if (ti >= 0) {
            ctx.level.tiles.erase(ctx.level.tiles.begin() + ti);
            ctx.SetStatus("Erased tile");
            return ToolResult::Consumed;
        }
        return ToolResult::Consumed;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// PlayerStartTool
// ═══════════════════════════════════════════════════════════════════════════════
class PlayerStartTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Player"; }

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;
        auto [sx, sy]  = ctx.SnapToGrid(mx, my);
        ctx.level.player = {static_cast<float>(sx), static_cast<float>(sy)};
        ctx.SetStatus("Player start set");
        return ToolResult::Consumed;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// TileTool
// ═══════════════════════════════════════════════════════════════════════════════
class TileTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Tile"; }

    // Tile size (defaults to GRID, adjustable via scroll)
    int  tileW         = 38;
    int  tileH         = 38;
    int  ghostRotation = 0;
    float scrollAccum  = 0.0f;

    // The orchestrator sets this every frame before dispatch so the tool
    // knows what palette item is selected without holding a palette reference.
    TilePlacementInfo placementInfo;

    ToolResult OnMouseDown(EditorToolContext& ctx, int mx, int my,
                           Uint8 button, SDL_Keymod /*mods*/) override {
        if (button == SDL_BUTTON_RIGHT) {
            // Right-click on a tile: rotate it. On empty space: cycle ghost.
            if (my >= ctx.ToolbarH() && mx < ctx.CanvasW()) {
                int ti = ctx.HitTile(mx, my);
                if (ti >= 0) {
                    int& rot = ctx.level.tiles[ti].rotation;
                    rot = (rot + 90) % 360;
                    ctx.SetStatus("Tile " + std::to_string(ti) + " rotated to " +
                                  std::to_string(rot) + "deg");
                } else {
                    ghostRotation = (ghostRotation + 90) % 360;
                    ctx.SetStatus("Ghost rotation: " + std::to_string(ghostRotation) +
                                  "deg  (RClick to cycle)");
                }
                return ToolResult::Consumed;
            }
            return ToolResult::Ignored;
        }

        if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
        if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;

        if (!placementInfo.hasSelection || placementInfo.isFolder)
            return ToolResult::Consumed;

        auto [sx, sy] = ctx.SnapToGrid(mx, my);
        auto newTile  = TileSpawn{static_cast<float>(sx), static_cast<float>(sy),
                                  tileW, tileH, placementInfo.imagePath};
        newTile.rotation = ghostRotation;
        ctx.level.tiles.push_back(std::move(newTile));
        ctx.SetStatus("Tile: " + placementInfo.label +
                      (ghostRotation ? "  rot=" + std::to_string(ghostRotation) : ""));
        return ToolResult::Consumed;
    }

    ToolResult OnScroll(EditorToolContext& ctx, float wheelY,
                        int /*mx*/, int /*my*/, SDL_Keymod /*mods*/) override {
        scrollAccum += wheelY;
        int steps = static_cast<int>(scrollAccum);
        if (steps != 0) {
            scrollAccum -= steps;
            tileW = std::max(ctx.Grid(), tileW + steps * ctx.Grid());
            tileH = tileW;
            ctx.SetStatus("Tile size: " + std::to_string(tileW));
        }
        return ToolResult::Consumed;
    }

    void OnActivate(EditorToolContext& /*ctx*/) override {
        ghostRotation = 0;
        scrollAccum   = 0.0f;
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// MoveCamTool -- does nothing on click (pan is handled at the orchestrator
// level via middle-mouse / Ctrl+left / MoveCam left-click).
// ═══════════════════════════════════════════════════════════════════════════════
class MoveCamTool final : public EditorTool {
  public:
    [[nodiscard]] const char* Name() const override { return "Pan"; }

    ToolResult OnMouseDown(EditorToolContext& /*ctx*/, int /*mx*/, int /*my*/,
                           Uint8 /*button*/, SDL_Keymod /*mods*/) override {
        // Pan is handled by the orchestrator before tool dispatch.
        return ToolResult::Ignored;
    }
};
