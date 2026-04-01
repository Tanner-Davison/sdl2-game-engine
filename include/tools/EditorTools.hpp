#pragma once
// EditorTools.hpp -- convenience header + factory for all editor tools.

#include "tools/EditorTool.hpp"
#include "tools/PlacementTools.hpp"
#include "tools/ModifierTools.hpp"
#include "tools/SelectTool.hpp"
#include "tools/ResizeTool.hpp"
#include "tools/HitboxTool.hpp"
#include <memory>

// Returns nullptr for Action/PowerUp/MovingPlat (handled inline by orchestrator).
inline std::unique_ptr<EditorTool> MakeEditorTool(ToolId id) {
    switch (id) {
        case ToolId::Goal:        return std::make_unique<GoalTool>();
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
        case ToolId::Shield:      return std::make_unique<ShieldTool>();

        case ToolId::Action:
        case ToolId::PowerUp:
        case ToolId::Shooter:  // merged into PowerUp popup
        case ToolId::MovingPlat:
            return nullptr;
    }
    return nullptr;
}
