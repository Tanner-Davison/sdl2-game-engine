#pragma once
#include "Image.hpp"
#include "PlayerProfile.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SpriteSheet.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ────────────────────────────────────────────────────────────────────────────
// PlayerCreatorScene
//
// Full-screen character-creation UI.  Layout (left → right):
//
//   [Slot List Panel]  [Animation Preview + Hitbox Editor]  [Roster Panel]
//
// Workflow:
//   1. User types a character name in the name field.
//   2. User clicks a slot row to select it.
//   3. User drops a folder of PNGs onto the Drop Zone — slot gets the path.
//   4. The preview starts playing the animation frames at 12 fps.
//   5. User drags the hitbox handles in the preview to define a custom rect.
//   6. User clicks Save → written to players/<name>.json.
//   7. Saved characters appear in the Roster panel on the right.
//   8. Back button → returns to TitleScene.
// ────────────────────────────────────────────────────────────────────────────
class PlayerCreatorScene : public Scene {
  public:
    void Load(Window& window) override;
    void Unload() override;
    bool HandleEvent(SDL_Event& e) override;
    void Update(float dt) override;
    void Render(Window& window) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int PANEL_PAD     = 14;
    static constexpr int SLOT_ROW_H    = 46;
    static constexpr int PREVIEW_W     = 160;  // fallback cell size
    static constexpr int PREVIEW_H     = 160;
    static constexpr int PREVIEW_PAD   = 16;   // padding around sprite in cell
    static constexpr float ANIM_FPS    = 10.0f;
    static constexpr int HB_HANDLE_SZ  = 14;  // hitbox corner handle size in px (render + hit test)

    // ── State flags ───────────────────────────────────────────────────────────
    bool mGoBack         = false;
    int  mW              = 0;
    int  mH              = 0;
    SDL_Window* mSDLWin  = nullptr;

    // ── Active profile being edited ───────────────────────────────────────────
    PlayerProfile mProfile;

    // ── Name field ────────────────────────────────────────────────────────────
    bool        mNameActive    = false;  // true while name text field is focused
    std::string mNameError;
    std::string mLoadedName;             // original name when loaded from roster (empty = new character)

    // ── Sprite size fields ────────────────────────────────────────────────────
    enum class SizeField { None, Width, Height };
    SizeField   mSizeActive   = SizeField::None;
    std::string mWidthStr     = "120";   // display string while editing
    std::string mHeightStr    = "160";
    SDL_Rect    mWidthRect{};
    SDL_Rect    mHeightRect{};

    // ── Per-slot FPS stepper ──────────────────────────────────────────────────
    // Displayed in the slot list rows as  "- 12 fps +"  click buttons
    struct SlotFpsButtons {
        SDL_Rect minusRect{};
        SDL_Rect plusRect{};
    };
    std::array<SlotFpsButtons, PLAYER_ANIM_SLOT_COUNT> mFpsBtns;

    // ── Slot selection ────────────────────────────────────────────────────────
    int  mSelectedSlot = 0;   // which PlayerAnimSlot row is highlighted

    // ── Drop zone ────────────────────────────────────────────────────────────
    SDL_Rect    mDropZone{};
    bool        mDropHover    = false;  // file is being dragged over the zone
    std::string mDropMsg;              // feedback after a drop

    // ── Animation preview ────────────────────────────────────────────────────
    struct SlotPreview {
        std::unique_ptr<SpriteSheet>  sheet;  // stitched sprite sheet for this slot
        std::vector<SDL_Rect>         frames; // frame rects inside sheet->GetSurface()
        int                           frameW = 0;
        int                           frameH = 0;
    };

    // One preview per slot (lazily built when a folder is assigned)
    std::array<std::optional<SlotPreview>, PLAYER_ANIM_SLOT_COUNT> mPreviews;

    float mAnimTimer  = 0.0f;
    int   mAnimFrame  = 0;

    void rebuildPreview(int slotIdx);
    void clearPreview(int slotIdx);

    // ── Hitbox editor ────────────────────────────────────────────────────────
    // The hitbox rect is drawn/edited in *preview-local* coordinates.
    // Preview-local (0,0) = top-left of the scaled sprite frame displayed on screen.
    SDL_Rect mHBRect{};         // current hitbox in preview-local px
    bool     mHBEditing = false; // user is actively dragging a corner/edge
    int      mHBDragHandle = -1; // which handle is being dragged (-1 = none, 0=TL,1=TR,2=BR,3=BL,4=body)
    int      mHBDragStartMX = 0, mHBDragStartMY = 0;
    SDL_Rect mHBDragStartRect{};
    bool     mHBInitialised = false; // has mHBRect been populated for current slot?
    SDL_Rect mPreviewCellRect{};     // fixed PREVIEW_W x PREVIEW_H cell in the centre panel
    SDL_Rect mPreviewRenderRect{};   // actual sprite draw rect (bottom-aligned inside cell)

    void recomputePreviewRect();   // called when spriteW/H changes
    void initHBFromProfile(int slotIdx);
    void commitHBToProfile(int slotIdx);
    int  hitboxHandleAt(int mx, int my) const; // -1 = none
    void renderHitboxOverlay(SDL_Surface* surf) const;

    // ── Roster (right panel) ──────────────────────────────────────────────────
    struct RosterEntry {
        std::string           name;
        std::string           path;
        SDL_Rect              loadRect{};
        SDL_Rect              delRect{};
    };
    std::vector<RosterEntry> mRoster;
    int                      mRosterScroll = 0;  // row offset for scrolling
    void refreshRoster();
    void loadRosterEntry(int idx);

    // ── Layout rects (computed in Load / resize) ──────────────────────────────
    SDL_Rect mSlotPanel{};    // left
    SDL_Rect mCenterPanel{};  // middle
    SDL_Rect mRosterPanel{};  // right

    SDL_Rect mNameFieldRect{};
    SDL_Rect mSaveBtnRect{};
    SDL_Rect mBackBtnRect{};
    SDL_Rect mClearSlotRect{};

    std::vector<SDL_Rect> mSlotRowRects;  // one per slot

    void computeLayout();

    // Returns the engine's built-in FPS for a slot (used as starting point when
    // the user first clicks + on a slot that has no override yet)
    static float defaultFps(PlayerAnimSlot s) {
        switch (s) {
            case PlayerAnimSlot::Walk:   return 24.0f;
            case PlayerAnimSlot::Jump:   return 4.0f;
            case PlayerAnimSlot::Slash:  return 18.0f;
            case PlayerAnimSlot::Crouch: return 12.0f;
            default:                     return 12.0f;
        }
    }

    // ── Text input ───────────────────────────────────────────────────────────
    void startTextInput();
    void stopTextInput();

    // ── Draw helpers (same pattern as TitleScene) ─────────────────────────────
    static void fillRect(SDL_Surface* s, SDL_Rect r, SDL_Color c);
    static void outlineRect(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1);
    static void drawText(SDL_Surface* s, const std::string& str,
                         int x, int y, int ptSize,
                         SDL_Color col = {220, 220, 220, 255});
    static void drawTextCentered(SDL_Surface* s, const std::string& str,
                                 SDL_Rect r, int ptSize,
                                 SDL_Color col = {220, 220, 220, 255});

    // Blit a scaled copy of src into dst rect on surface, keeping aspect ratio
    static void blitScaled(SDL_Surface* dst, SDL_Surface* src, SDL_Rect dstRect);

    // ── Colours ───────────────────────────────────────────────────────────────
    static constexpr SDL_Color BG          = {18,  20,  30,  255};
    static constexpr SDL_Color PANEL_BG    = {28,  32,  50,  255};
    static constexpr SDL_Color PANEL_OUT   = {60,  70, 110,  255};
    static constexpr SDL_Color SEL_BG      = {50,  80, 160,  255};
    static constexpr SDL_Color SLOT_FILLED = {40, 140,  70,  255};
    static constexpr SDL_Color SLOT_EMPTY  = {50,  55,  75,  255};
    static constexpr SDL_Color DROP_IDLE   = {35,  45,  80,  255};
    static constexpr SDL_Color DROP_HOVER  = {50, 100, 200,  255};
    static constexpr SDL_Color HB_COLOR    = {255, 80,  80, 180};
    static constexpr SDL_Color BTN_SAVE    = {40, 160,  80,  255};
    static constexpr SDL_Color BTN_BACK    = {80,  80, 160,  255};
    static constexpr SDL_Color BTN_DEL     = {160, 50,  50,  255};
    static constexpr SDL_Color BTN_LOAD    = {40, 100, 180,  255};

    // ── Clamp helper ──────────────────────────────────────────────────────────
    static SDL_Rect normaliseRect(SDL_Rect r);
};
