#pragma once
// EditorToolbar.hpp — toolbar layout, button rects, labels, hints, group
// collapse state, and hit-testing for the level editor.

#include "Text.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

class EditorToolbar {
  public:
    // --- Layout constants ---
    static constexpr int TOOLBAR_H  = 86;
    static constexpr int BTN_H      = 56;
    static constexpr int BTN_Y      = 8;
    static constexpr int BTN_TOOL_W = 68;
    static constexpr int BTN_ACT_W  = 72;
    static constexpr int BTN_GAP    = 4;
    static constexpr int GRP_GAP    = 16;
    static constexpr int PILL_W     = 28;
    static constexpr int STRIP_Y    = BTN_Y + BTN_H + 2; // 66
    static constexpr int STRIP_H    = 14;

    // --- Button identifiers ---
    // Ordering within each group is significant for layout.
    enum class ButtonId : uint8_t {
        // Group 1 - Place tools
        Goal,
        Enemy,
        Tile,
        Erase,
        PlayerStart,
        Select,
        MoveCam,
        // Group 2 - Modifier tools
        Prop,
        Ladder,
        Action,
        Slope,
        Resize,
        Hitbox,
        Hazard,
        AntiGrav,
        MovingPlat,
        PowerUp,
        Shooter,
        Shield,
        // Group 3 - Action buttons
        Gravity,
        Save,
        Load,
        Clear,
        Play,
        Back,
        // Sentinel
        COUNT
    };

    static constexpr int kButtonCount = static_cast<int>(ButtonId::COUNT);

    enum class Group : uint8_t { Place = 0, Modifier = 1, Actions = 2, COUNT };
    static constexpr int kGroupCount = static_cast<int>(Group::COUNT);

    // --- Click result ---
    struct ClickResult {
        enum class Kind { None, Button, CollapseToggle };
        Kind kind = Kind::None;
        ButtonId button = ButtonId::COUNT;
        Group group = Group::COUNT;
    };

    EditorToolbar();
    ~EditorToolbar() = default;

    // Non-copyable (labels are unique_ptr<Text>), movable
    EditorToolbar(const EditorToolbar&)            = delete;
    EditorToolbar& operator=(const EditorToolbar&) = delete;
    EditorToolbar(EditorToolbar&&)                 = default;
    EditorToolbar& operator=(EditorToolbar&&)      = default;

    // --- Layout ---

    /// Recompute all button rects based on current collapse state.
    void RebuildLayout();

    /// Create all label + hint Text objects. Call once after first RebuildLayout().
    void CreateLabels();

    void SetGravityLabel(std::string_view label);

    // --- Queries ---

    [[nodiscard]] const SDL_Rect& Rect(ButtonId id) const {
        return mRects[static_cast<int>(id)];
    }

    [[nodiscard]] Text* Label(ButtonId id) const {
        return mLabels[static_cast<int>(id)].get();
    }

    [[nodiscard]] Text* Hint(ButtonId id) const {
        return mHints[static_cast<int>(id)].get();
    }

    [[nodiscard]] Text* CollapseTabLabel(Group g) const {
        return mCollapseLabels[static_cast<int>(g)].get();
    }

    [[nodiscard]] const SDL_Rect& PillRect(Group g) const {
        return mPills[static_cast<int>(g)];
    }

    [[nodiscard]] bool IsCollapsed(Group g) const {
        return mCollapsed[static_cast<int>(g)];
    }

    [[nodiscard]] static constexpr int Height() { return TOOLBAR_H; }

    // --- Mutation ---

    void ToggleGroup(Group g);
    void SetCollapsed(Group g, bool collapsed);

    // --- Hit testing ---

    [[nodiscard]] ClickResult HandleClick(int mx, int my) const;

    [[nodiscard]] bool IsInToolbar(int /*mx*/, int my) const {
        return my < TOOLBAR_H;
    }

    // --- Iteration helpers ---

    struct ButtonMeta {
        ButtonId     id;
        Group        group;
        bool         isTool;
    };

    static const std::array<ButtonMeta, kButtonCount>& AllButtons();
    [[nodiscard]] static Group GroupOf(ButtonId id);

  private:
    std::array<SDL_Rect, kButtonCount>                mRects{};
    std::array<std::unique_ptr<Text>, kButtonCount>   mLabels{};
    std::array<std::unique_ptr<Text>, kButtonCount>   mHints{};

    std::array<bool, kGroupCount>                     mCollapsed{false, false, false};
    std::array<SDL_Rect, kGroupCount>                 mPills{};
    std::array<std::unique_ptr<Text>, kGroupCount>    mCollapseLabels{};

    struct ButtonDef {
        ButtonId         id;
        Group            group;
        std::string_view label;
        int              labelSize;
        std::string_view hint;
        bool             isTool;
    };

    /// Single source of truth for button ordering, labels, hints, and sizes.
    static const std::array<ButtonDef, kButtonCount>& Defs();
};
