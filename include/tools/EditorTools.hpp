#pragma once
// EditorTools.hpp
//
// Convenience header: includes every tool and provides a factory function
// that creates the right tool subclass for a given ToolId.

#include "tools/EditorTool.hpp"
#include "tools/PlacementTools.hpp"
#include "tools/ModifierTools.hpp"
#include "tools/SelectTool.hpp"
#include "tools/ResizeTool.hpp"
#include "tools/HitboxTool.hpp"
#include <memory>

// ─── Factory ─────────────────────────────────────────────────────────────────
// Creates a new tool instance for the given ToolId.
// The ActionTool, PowerUpTool, and MovingPlatTool are complex tools that
// remain in LevelEditorScene for now (they have heavy popup/state coupling).
// This factory returns nullptr for those IDs, signaling the orchestrator
// to keep handling them inline.
inline std::unique_ptr<EditorTool> MakeEditorTool(ToolId id) {
    switch (id) {
        case ToolId::Coin:        return std::make_unique<CoinTool>();
        case ToolId::Enemy:       return std::make_unique<EnemyTool>();
        case ToolId::Tile:        return std::make_unique<TileTool>();
        case ToolId::Erase:       return std::make_unique<EraseTool>();
        case ToolId::PlayerStart: return std::make_unique<PlayerStartTool>();
        case ToolId::MoveCam:     return std::make_unique<MoveCamTool>();
        case ToolId::Select:      return std::make_unique<SelectTool>();
        case ToolId::Resize:      return std::make_unique<ResizeTool>();
        case ToolId::Hitbox:      return std::make_unique<HitboxTool>();
        case ToolId::Prop:        return std::make_unique<PropTool>();
        case ToolId::Ladder:      return std::make_unique<LadderTool>();
        case ToolId::Slope:       return std::make_unique<SlopeTool>();
        case ToolId::Hazard:      return std::make_unique<HazardTool>();
        case ToolId::AntiGrav:    return std::make_unique<AntiGravTool>();

        // Complex tools -- handled inline by the orchestrator for now.
        case ToolId::Action:
        case ToolId::PowerUp:
        case ToolId::MovingPlat:
            return nullptr;
    }
    return nullptr;
}
