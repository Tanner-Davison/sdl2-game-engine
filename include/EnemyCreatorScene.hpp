#pragma once
#include "EnemyProfile.hpp"
#include "Image.hpp"
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
// EnemyCreatorScene
//
// Full-screen enemy-type creation UI.  Layout (left -> right):
//
//   [Slot List Panel]  [Preview + Hitbox Editor + Fields]  [Roster Panel]
//
// Workflow:
//   1. User types an enemy name in the name field.
//   2. User clicks a slot row to select it (Idle, Move, Hit, Dead).
//   3. User drops a folder of PNGs (or a single PNG) onto the Drop Zone.
//   4. The preview starts playing the animation frames.
//   5. User drags the hitbox handles in the preview to define a custom rect.
//   6. User sets speed, health, and sprite size in the fields.
//   7. User clicks Save -> written to enemies/<name>.json.
//   8. Saved enemies appear in the Roster panel on the right.
//   9. Back button -> returns to TitleScene.
// ────────────────────────────────────────────────────────────────────────────
class EnemyCreatorScene : public Scene {
  public:
    void Load(Window& window) override;
    void Unload() override;
    bool HandleEvent(SDL_Event& e) override;
    void Update(float dt) override;
    void Render(Window& window, float alpha = 1.0f) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    // ── Layout constants ──────────────────────────────────────────────────────
    static constexpr int PANEL_PAD     = 14;
    static constexpr int SLOT_ROW_H    = 46;
    static constexpr int PREVIEW_W     = 160;
    static constexpr int PREVIEW_H     = 160;
    static constexpr int PREVIEW_PAD   = 16;
    static constexpr float ANIM_FPS    = 10.0f;
    static constexpr int HB_HANDLE_SZ  = 14;

    // ── State flags ───────────────────────────────────────────────────────────
    bool mGoBack         = false;
    int  mW              = 0;
    int  mH              = 0;
    SDL_Window* mSDLWin  = nullptr;

    // ── Active profile being edited ───────────────────────────────────────────
    EnemyProfile mProfile;

    // ── Name field ────────────────────────────────────────────────────────────
    bool        mNameActive    = false;
    std::string mNameError;
    std::string mLoadedName;

    // ── Numeric fields (sprite size, speed, health) ───────────────────────────
    enum class NumField { None, Width, Height, Speed, Health };
    NumField    mNumFieldActive = NumField::None;
    std::string mWidthStr   = "40";
    std::string mHeightStr  = "40";
    std::string mSpeedStr   = "120";
    std::string mHealthStr  = "30";
    SDL_Rect    mWidthRect{};
    SDL_Rect    mHeightRect{};
    SDL_Rect    mSpeedRect{};
    SDL_Rect    mHealthRect{};

    // ── Per-slot FPS stepper ──────────────────────────────────────────────────
    struct SlotFpsButtons {
        SDL_Rect minusRect{};
        SDL_Rect plusRect{};
    };
    std::array<SlotFpsButtons, ENEMY_ANIM_SLOT_COUNT> mFpsBtns;

    // ── Per-file SFX UI ──────────────────────────────────────────────────────
    struct SfxFileUI {
        SDL_Rect nameRect{};
        SDL_Rect clearRect{};
        SDL_Rect stretchRect{};
        SDL_Rect sliderRect{};
        SDL_Rect trimSliderRect{};
    };
    std::array<std::vector<SfxFileUI>, ENEMY_ANIM_SLOT_COUNT> mSfxFileUI;
    int  mVolDragSlot   = -1;
    int  mVolDragFile   = -1;
    int  mTrimDragSlot  = -1;
    int  mTrimDragFile  = -1;
    int  mTrimDragHandle = -1;

    int  mSelectedSfxFile = 0;
    std::string mPreviewSfxId;
    std::string mPreviewPath;
    int  mPreviewSlot  = -1;
    int  mPreviewFile  = -1;
    float mPreviewTimer = 0.0f;
    bool  mPreviewPlaying = false;
    std::array<SDL_Rect, ENEMY_ANIM_SLOT_COUNT> mSfxDropRect;
    int mSfxDropHoverSlot = -1;

    static bool isAudioFile(const std::filesystem::path& p) {
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac";
    }

    // ── Slot selection ────────────────────────────────────────────────────────
    int  mSelectedSlot = 0;

    // ── Drop zone ─────────────────────────────────────────────────────────────
    SDL_Rect    mDropZone{};
    bool        mDropHover    = false;
    std::string mDropMsg;

    // ── Animation preview ─────────────────────────────────────────────────────
    struct SlotPreview {
        std::unique_ptr<SpriteSheet>  sheet;
        std::vector<SDL_Rect>         frames;
        std::vector<std::string>      paths;
        int                           frameW = 0;
        int                           frameH = 0;
    };

    std::array<std::optional<SlotPreview>, ENEMY_ANIM_SLOT_COUNT> mPreviews;

    float mAnimTimer  = 0.0f;
    int   mAnimFrame  = 0;

    void rebuildPreview(int slotIdx);
    void clearPreview(int slotIdx);
    void deleteFrame(int slotIdx, int frameIdx);

    // ── Frame strip ───────────────────────────────────────────────────────────
    int  mFrameStripScroll = 0;
    static constexpr int FRAME_THUMB_SZ = 48;
    static constexpr int FRAME_STRIP_H  = 68;
    std::vector<SDL_Rect> mFrameDelRects;

    // ── Hitbox editor ─────────────────────────────────────────────────────────
    SDL_Rect mHBRect{};
    bool     mHBEditing = false;
    int      mHBDragHandle = -1;
    int      mHBDragStartMX = 0, mHBDragStartMY = 0;
    SDL_Rect mHBDragStartRect{};
    bool     mHBInitialised = false;
    SDL_Rect mPreviewCellRect{};
    SDL_Rect mPreviewRenderRect{};

    void recomputePreviewRect();
    void initHBFromProfile(int slotIdx);
    void commitHBToProfile(int slotIdx);
    int  hitboxHandleAt(int mx, int my) const;
    void renderHitboxOverlay(SDL_Surface* surf) const;

    // ── Roster (right panel) ──────────────────────────────────────────────────
    struct RosterEntry {
        std::string name;
        std::string path;
        SDL_Rect    loadRect{};
        SDL_Rect    delRect{};
    };
    std::vector<RosterEntry> mRoster;
    int                      mRosterScroll = 0;
    void refreshRoster();
    void loadRosterEntry(int idx);

    // ── Layout rects ──────────────────────────────────────────────────────────
    SDL_Rect mSlotPanel{};
    SDL_Rect mCenterPanel{};
    SDL_Rect mRosterPanel{};

    SDL_Rect mNameFieldRect{};
    SDL_Rect mSaveBtnRect{};
    SDL_Rect mBackBtnRect{};
    SDL_Rect mClearSlotRect{};

    std::vector<SDL_Rect> mSlotRowRects;

    void computeLayout();

    static float defaultFps(EnemyAnimSlot s) {
        switch (s) {
            case EnemyAnimSlot::Move:   return 10.0f;
            case EnemyAnimSlot::Attack: return 10.0f;
            case EnemyAnimSlot::Hurt:   return 8.0f;
            case EnemyAnimSlot::Dead:   return 6.0f;
            default:                    return 10.0f;
        }
    }

    // ── Text input ────────────────────────────────────────────────────────────
    void startTextInput();
    void stopTextInput();

    // ── Draw helpers ──────────────────────────────────────────────────────────
    static void fillRect(SDL_Surface* s, SDL_Rect r, SDL_Color c);
    static void outlineRect(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1);
    static void drawText(SDL_Surface* s, const std::string& str,
                         int x, int y, int ptSize,
                         SDL_Color col = {220, 220, 220, 255});
    static void drawTextCentered(SDL_Surface* s, const std::string& str,
                                 SDL_Rect r, int ptSize,
                                 SDL_Color col = {220, 220, 220, 255});
    static void blitScaled(SDL_Surface* dst, SDL_Surface* src, SDL_Rect dstRect);

    // ── Colours ───────────────────────────────────────────────────────────────
    static constexpr SDL_Color BG          = {18,  20,  30,  255};
    static constexpr SDL_Color PANEL_BG    = {28,  32,  50,  255};
    static constexpr SDL_Color PANEL_OUT   = {60,  70, 110,  255};
    static constexpr SDL_Color SEL_BG      = {160, 50,  50,  255};
    static constexpr SDL_Color SLOT_FILLED = {140, 60,  40,  255};
    static constexpr SDL_Color SLOT_EMPTY  = {50,  55,  75,  255};
    static constexpr SDL_Color DROP_IDLE   = {35,  45,  80,  255};
    static constexpr SDL_Color DROP_HOVER  = {200, 80,  50,  255};
    static constexpr SDL_Color HB_COLOR    = {255, 80,  80, 180};
    static constexpr SDL_Color BTN_SAVE    = {40, 160,  80,  255};
    static constexpr SDL_Color BTN_BACK    = {80,  80, 160,  255};
    static constexpr SDL_Color BTN_DEL     = {160, 50,  50,  255};
    static constexpr SDL_Color BTN_LOAD    = {180, 100, 40,  255};

    // ── Clamp helper ──────────────────────────────────────────────────────────
    static SDL_Rect normaliseRect(SDL_Rect r);
};
