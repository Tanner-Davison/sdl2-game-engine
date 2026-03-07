#pragma once
// EditorToolbar.hpp
// ---------------------------------------------------------------------------
// Owns the toolbar layout for the level editor: button rects, labels, hint
// text, group collapse state, and pill rects. Provides methods to:
//   - Rebuild layout after collapse toggles
//   - Create/destroy all label Text objects
//   - Hit-test toolbar clicks and return a typed result
//
// Extracted from LevelEditorScene as part of the modular refactor (Prompt #4).
// The orchestrator (LevelEditorScene) owns an EditorToolbar instance and
// delegates toolbar queries/mutations through it.
// ---------------------------------------------------------------------------

#include "Text.hpp"
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

// ---------------------------------------------------------------------------
// EditorToolbar
// ---------------------------------------------------------------------------
class EditorToolbar {
  public:
    // ── Layout constants ───────────────────────────────────────────────────
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

    // ── Button identifiers ─────────────────────────────────────────────────
    // Flat enum so callers can switch on the result of HandleClick().
    // The ordering within each group is significant for layout.
    enum class ButtonId : uint8_t {
        // Group 1 - Place tools
        Coin,
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

    // Which group a button belongs to
    enum class Group : uint8_t { Place = 0, Modifier = 1, Actions = 2, COUNT };
    static constexpr int kGroupCount = static_cast<int>(Group::COUNT);

    // ── Click result ───────────────────────────────────────────────────────
    // Returned by HandleClick to tell the caller what was hit.
    struct ClickResult {
        enum class Kind { None, Button, CollapseToggle };
        Kind kind = Kind::None;
        // Valid when kind == Button
        ButtonId button = ButtonId::COUNT;
        // Valid when kind == CollapseToggle — which group was toggled
        Group group = Group::COUNT;
    };

    // ── Construction ───────────────────────────────────────────────────────
    EditorToolbar();
    ~EditorToolbar() = default;

    // Non-copyable (labels are unique_ptr<Text>), movable
    EditorToolbar(const EditorToolbar&)            = delete;
    EditorToolbar& operator=(const EditorToolbar&) = delete;
    EditorToolbar(EditorToolbar&&)                 = default;
    EditorToolbar& operator=(EditorToolbar&&)      = default;

    // ── Layout ─────────────────────────────────────────────────────────────

    /// Recompute all button rects and re-centre labels based on current
    /// collapse state.  Called once in Load() and again whenever a group is
    /// toggled.
    void RebuildLayout();

    /// Create all label + hint Text objects for the first time.
    /// Must be called once during Load() AFTER the first RebuildLayout().
    void CreateLabels();

    /// Update the gravity button label to reflect the current mode string.
    void SetGravityLabel(std::string_view label);

    // ── Queries ────────────────────────────────────────────────────────────

    /// Get the screen-space rect for a specific button.
    [[nodiscard]] const SDL_Rect& Rect(ButtonId id) const {
        return mRects[static_cast<int>(id)];
    }

    /// Get the label Text for a specific button (may be null before CreateLabels).
    [[nodiscard]] Text* Label(ButtonId id) const {
        return mLabels[static_cast<int>(id)].get();
    }

    /// Get the hint Text for a specific button (may be null if button has no hint).
    [[nodiscard]] Text* Hint(ButtonId id) const {
        return mHints[static_cast<int>(id)].get();
    }

    /// Get the collapse tab label for a group (+/-).
    [[nodiscard]] Text* CollapseTabLabel(Group g) const {
        return mCollapseLabels[static_cast<int>(g)].get();
    }

    /// Get the pill rect for a group (used for collapse toggle click detection).
    [[nodiscard]] const SDL_Rect& PillRect(Group g) const {
        return mPills[static_cast<int>(g)];
    }

    /// Is a group currently collapsed?
    [[nodiscard]] bool IsCollapsed(Group g) const {
        return mCollapsed[static_cast<int>(g)];
    }

    /// Get the total height of the toolbar area.
    [[nodiscard]] static constexpr int Height() { return TOOLBAR_H; }

    // ── Mutation ───────────────────────────────────────────────────────────

    /// Toggle collapse state for a group and rebuild layout.
    void ToggleGroup(Group g);

    /// Set collapse state directly.
    void SetCollapsed(Group g, bool collapsed);

    // ── Hit testing ────────────────────────────────────────────────────────

    /// Test if a screen-space point falls on any toolbar button or collapse
    /// pill.  Returns a ClickResult describing what was hit.
    [[nodiscard]] ClickResult HandleClick(int mx, int my) const;

    /// Returns true if the point is within the toolbar strip (y < TOOLBAR_H).
    [[nodiscard]] bool IsInToolbar(int /*mx*/, int my) const {
        return my < TOOLBAR_H;
    }

    // ── Iteration helpers (for rendering in LevelEditorScene::Render) ──────

    /// Button metadata — returned by ButtonInfo() for rendering.
    struct ButtonMeta {
        ButtonId     id;
        Group        group;
        bool         isTool; // true = tool button (BTN_TOOL_W), false = action (BTN_ACT_W)
    };

    /// Get metadata for all buttons (static, computed once).
    static const std::array<ButtonMeta, kButtonCount>& AllButtons();

    /// Which group does a button belong to?
    [[nodiscard]] static Group GroupOf(ButtonId id);

  private:
    // ── Per-button storage ─────────────────────────────────────────────────
    std::array<SDL_Rect, kButtonCount>                mRects{};
    std::array<std::unique_ptr<Text>, kButtonCount>   mLabels{};
    std::array<std::unique_ptr<Text>, kButtonCount>   mHints{};

    // ── Per-group storage ──────────────────────────────────────────────────
    std::array<bool, kGroupCount>                     mCollapsed{false, false, false};
    std::array<SDL_Rect, kGroupCount>                 mPills{};
    std::array<std::unique_ptr<Text>, kGroupCount>    mCollapseLabels{};

    // ── Static metadata tables ─────────────────────────────────────────────

    struct ButtonDef {
        ButtonId         id;
        Group            group;
        std::string_view label;
        int              labelSize; // font size for label
        std::string_view hint;     // shortcut hint text, empty = no hint
        bool             isTool;   // true = tool (BTN_TOOL_W), false = action (BTN_ACT_W)
    };

    /// Canonical definition table — single source of truth for button ordering,
    /// labels, hints, and sizes.
    static const std::array<ButtonDef, kButtonCount>& Defs();
};
