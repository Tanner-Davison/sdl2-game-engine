#include "EditorToolbar.hpp"

#include <algorithm>

// ---------------------------------------------------------------------------
// Static button definition table
// ---------------------------------------------------------------------------
// Single source of truth for every button's identity, group, label text,
// label font size, shortcut hint, and whether it uses tool or action width.
// The order here determines left-to-right layout within each group.
const std::array<EditorToolbar::ButtonDef, EditorToolbar::kButtonCount>&
EditorToolbar::Defs() {
    static const std::array<ButtonDef, kButtonCount> kDefs = {{
        // Group 1 - Place tools
        {ButtonId::Coin,        Group::Place,    "Coin",     12, "1", true},
        {ButtonId::Enemy,       Group::Place,    "Enemy",    12, "2", true},
        {ButtonId::Tile,        Group::Place,    "Tile",     12, "3", true},
        {ButtonId::Erase,       Group::Place,    "Erase",    12, "4", true},
        {ButtonId::PlayerStart, Group::Place,    "Player",   12, "5", true},
        {ButtonId::Select,      Group::Place,    "Select",   11, "Q", true},
        {ButtonId::MoveCam,     Group::Place,    "Pan",      11, "T", true},
        // Group 2 - Modifier tools
        {ButtonId::Prop,        Group::Modifier, "Prop",     12, "8", true},
        {ButtonId::Ladder,      Group::Modifier, "Ladder",   12, "9", true},
        {ButtonId::Action,      Group::Modifier, "Action",   12, "0", true},
        {ButtonId::Slope,       Group::Modifier, "Slope",    12, "-", true},
        {ButtonId::Resize,      Group::Modifier, "Resize",   12, "R", true},
        {ButtonId::Hitbox,      Group::Modifier, "Hitbox",   12, "",  true},
        {ButtonId::Hazard,      Group::Modifier, "Hazard",   12, "",  true},
        {ButtonId::AntiGrav,    Group::Modifier, "Float",    11, "",  true},
        {ButtonId::MovingPlat,  Group::Modifier, "MovePlat", 10, "",  true},
        {ButtonId::PowerUp,     Group::Modifier, "PowerUp",  10, "",  true},
        // Group 3 - Action buttons
        {ButtonId::Gravity,     Group::Actions,  "Platform", 11, "",  false},
        {ButtonId::Save,        Group::Actions,  "Save",     12, "",  false},
        {ButtonId::Load,        Group::Actions,  "Load",     12, "",  false},
        {ButtonId::Clear,       Group::Actions,  "Clear",    12, "",  false},
        {ButtonId::Play,        Group::Actions,  "Play",     12, "",  false},
        {ButtonId::Back,        Group::Actions,  "< Menu",   12, "",  false},
    }};
    return kDefs;
}

// ---------------------------------------------------------------------------
// Static metadata lookup
// ---------------------------------------------------------------------------
const std::array<EditorToolbar::ButtonMeta, EditorToolbar::kButtonCount>&
EditorToolbar::AllButtons() {
    static const auto kMeta = []() {
        std::array<ButtonMeta, kButtonCount> out{};
        const auto& defs = Defs();
        for (int i = 0; i < kButtonCount; ++i)
            out[i] = {defs[i].id, defs[i].group, defs[i].isTool};
        return out;
    }();
    return kMeta;
}

EditorToolbar::Group EditorToolbar::GroupOf(ButtonId id) {
    return Defs()[static_cast<int>(id)].group;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
EditorToolbar::EditorToolbar() {
    // Zero-init all rects (off-screen)
    for (auto& r : mRects)
        r = {-200, BTN_Y, BTN_TOOL_W, BTN_H};
    for (auto& p : mPills)
        p = {};
}

// ---------------------------------------------------------------------------
// RebuildLayout
// ---------------------------------------------------------------------------
void EditorToolbar::RebuildLayout() {
    const auto& defs = Defs();

    int x = BTN_GAP;

    // Advance helpers
    auto advance = [&](bool isTool) -> SDL_Rect {
        int w = isTool ? BTN_TOOL_W : BTN_ACT_W;
        SDL_Rect r = {x, BTN_Y, w, BTN_H};
        x += w + BTN_GAP;
        return r;
    };

    auto gap = [&]() { x += GRP_GAP; };

    // Off-screen rect for collapsed buttons
    constexpr SDL_Rect kHidden = {-200, BTN_Y, BTN_TOOL_W, BTN_H};

    // Track group boundaries for pill computation
    Group prevGroup   = Group::COUNT;
    int   groupStartX = x;

    for (int i = 0; i < kButtonCount; ++i) {
        const auto& def = defs[i];

        // Detect group transition
        if (def.group != prevGroup) {
            // Close previous group pill
            if (prevGroup != Group::COUNT) {
                int gi = static_cast<int>(prevGroup);
                mPills[gi] = {groupStartX, STRIP_Y, x - groupStartX, STRIP_H};
                gap();
            }
            prevGroup   = def.group;
            groupStartX = x;

            // If this group is collapsed, emit a single pill-width advance
            if (mCollapsed[static_cast<int>(def.group)]) {
                // Mark all buttons in this group as hidden
                for (int j = i; j < kButtonCount; ++j) {
                    if (defs[j].group != def.group)
                        break;
                    mRects[j] = kHidden;
                }
                x += PILL_W + BTN_GAP;
                // Skip to the end of this group
                while (i + 1 < kButtonCount && defs[i + 1].group == def.group)
                    ++i;
                continue;
            }
        }

        // Special case: gap before Back button in Actions group
        // (Back sits after a divider within group 3)
        if (def.id == ButtonId::Back)
            gap();

        mRects[i] = advance(def.isTool);
    }

    // Close the last group pill
    if (prevGroup != Group::COUNT) {
        int gi = static_cast<int>(prevGroup);
        mPills[gi] = {groupStartX, STRIP_Y, x - groupStartX, STRIP_H};
    }

    // Re-centre all existing labels in their (possibly moved) rects
    for (int i = 0; i < kButtonCount; ++i) {
        const auto& def = defs[i];
        if (mLabels[i]) {
            auto [lx, ly] = Text::CenterInRect(
                std::string(def.label), def.labelSize, mRects[i]);
            mLabels[i]->SetPosition(lx, ly);
        }
        if (mHints[i]) {
            mHints[i]->SetPosition(mRects[i].x + mRects[i].w - 14,
                                   mRects[i].y + mRects[i].h - 13);
        }
    }

    // Rebuild collapse tab labels (+/-)
    for (int g = 0; g < kGroupCount; ++g) {
        mCollapseLabels[g] = std::make_unique<Text>(
            mCollapsed[g] ? "+" : "-",
            SDL_Color{200, 220, 255, 200}, 0, 0, 9);
    }
}

// ---------------------------------------------------------------------------
// CreateLabels
// ---------------------------------------------------------------------------
void EditorToolbar::CreateLabels() {
    const auto& defs = Defs();

    for (int i = 0; i < kButtonCount; ++i) {
        const auto& def = defs[i];
        const auto& r   = mRects[i];

        // Label
        auto [lx, ly] = Text::CenterInRect(std::string(def.label), def.labelSize, r);
        mLabels[i] = std::make_unique<Text>(
            std::string(def.label),
            SDL_Color{255, 255, 255, 255}, lx, ly, def.labelSize);

        // Hint (only if non-empty)
        if (!def.hint.empty()) {
            mHints[i] = std::make_unique<Text>(
                std::string(def.hint),
                SDL_Color{180, 180, 180, 160},
                r.x + r.w - 14, r.y + r.h - 13, 9);
        }
    }

    // Collapse labels are created in RebuildLayout
}

// ---------------------------------------------------------------------------
// SetGravityLabel
// ---------------------------------------------------------------------------
void EditorToolbar::SetGravityLabel(std::string_view label) {
    int idx = static_cast<int>(ButtonId::Gravity);
    auto [gx, gy] = Text::CenterInRect(std::string(label), 11, mRects[idx]);
    mLabels[idx] = std::make_unique<Text>(
        std::string(label), SDL_Color{255, 255, 255, 255}, gx, gy, 11);
}

// ---------------------------------------------------------------------------
// ToggleGroup / SetCollapsed
// ---------------------------------------------------------------------------
void EditorToolbar::ToggleGroup(Group g) {
    int gi = static_cast<int>(g);
    mCollapsed[gi] = !mCollapsed[gi];
    RebuildLayout();
}

void EditorToolbar::SetCollapsed(Group g, bool collapsed) {
    int gi = static_cast<int>(g);
    if (mCollapsed[gi] != collapsed) {
        mCollapsed[gi] = collapsed;
        RebuildLayout();
    }
}

// ---------------------------------------------------------------------------
// HandleClick
// ---------------------------------------------------------------------------
EditorToolbar::ClickResult EditorToolbar::HandleClick(int mx, int my) const {
    // Check collapse pills first (they sit in the strip below buttons)
    for (int g = 0; g < kGroupCount; ++g) {
        const auto& pill = mPills[g];
        if (mx >= pill.x && mx <= pill.x + pill.w &&
            my >= pill.y && my <= pill.y + pill.h) {
            return {ClickResult::Kind::CollapseToggle, ButtonId::COUNT,
                    static_cast<Group>(g)};
        }
    }

    // Check all buttons
    for (int i = 0; i < kButtonCount; ++i) {
        const auto& r = mRects[i];
        if (r.x < 0)
            continue; // off-screen (collapsed group)
        if (mx >= r.x && mx <= r.x + r.w && my >= r.y && my <= r.y + r.h) {
            return {ClickResult::Kind::Button, static_cast<ButtonId>(i), Group::COUNT};
        }
    }

    return {}; // Kind::None
}
