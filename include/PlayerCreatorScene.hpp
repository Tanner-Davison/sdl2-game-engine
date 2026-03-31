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

class PlayerCreatorScene : public Scene {
  public:
    void Load(Window& window) override;
    void Unload() override;
    bool HandleEvent(SDL_Event& e) override;
    void Update(float dt) override;
    void Render(Window& window, float alpha = 1.0f) override;
    std::unique_ptr<Scene> NextScene() override;

  private:
    // --- Layout constants ---
    static constexpr int PANEL_PAD     = 14;
    static constexpr int SLOT_ROW_H    = 64;  // 46 base + 18 for SFX strip
    static constexpr int PREVIEW_W     = 160;
    static constexpr int PREVIEW_H     = 160;
    static constexpr int PREVIEW_PAD   = 16;
    static constexpr float ANIM_FPS    = 10.0f;
    static constexpr int HB_HANDLE_SZ  = 14;

    // --- State ---
    bool mGoBack         = false;
    int  mW              = 0;
    int  mH              = 0;
    SDL_Window* mSDLWin  = nullptr;

    // Dirty flag: when true, Render() repaints the CPU surface.
    bool mDirty          = true;
    SDL_Surface* mRenderSurface = nullptr;
    SDL_Texture* mRenderTexture = nullptr;
    void markDirty() { mDirty = true; }

    PlayerProfile mProfile;

    bool        mNameActive    = false;
    std::string mNameError;
    std::string mLoadedName;  // original name when loaded (empty = new character)

    enum class SizeField { None, Width, Height };
    SizeField   mSizeActive   = SizeField::None;
    std::string mWidthStr     = "120";
    std::string mHeightStr    = "160";
    SDL_Rect    mWidthRect{};
    SDL_Rect    mHeightRect{};

    // --- Per-slot FPS stepper ---
    struct SlotFpsButtons {
        SDL_Rect minusRect{};
        SDL_Rect plusRect{};
    };
    std::array<SlotFpsButtons, PLAYER_ANIM_SLOT_COUNT> mFpsBtns;

    // --- Per-file SFX UI ---
    struct SfxFileUI {
        SDL_Rect nameRect{};
        SDL_Rect clearRect{};
        SDL_Rect stretchRect{};
        SDL_Rect sliderRect{};
        SDL_Rect trimSliderRect{};
    };
    std::array<std::vector<SfxFileUI>, PLAYER_ANIM_SLOT_COUNT> mSfxFileUI;
    int  mVolDragSlot   = -1;
    int  mVolDragFile   = -1;
    int  mTrimDragSlot  = -1;
    int  mTrimDragFile  = -1;
    int  mTrimDragHandle = -1;  // 0 = start handle, 1 = end handle

    int  mSelectedSfxFile = 0;
    std::string mPreviewSfxId;
    std::string mPreviewPath;
    int  mPreviewSlot  = -1;
    int  mPreviewFile  = -1;
    float mPreviewTimer = 0.0f;
    bool  mPreviewPlaying = false;
    std::array<SDL_Rect, PLAYER_ANIM_SLOT_COUNT> mSfxDropRect;
    int  mSfxDropHoverSlot = -1;

    static bool isAudioFile(const std::filesystem::path& p) {
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return ext == ".wav" || ext == ".ogg" || ext == ".mp3" || ext == ".flac";
    }

    int  mSelectedSlot = 0;

    SDL_Rect    mDropZone{};
    bool        mDropHover    = false;
    std::string mDropMsg;

    // --- Animation preview ---
    struct SlotPreview {
        std::unique_ptr<SpriteSheet>  sheet;
        std::vector<SDL_Rect>         frames;
        std::vector<std::string>      paths;
        int                           frameW = 0;
        int                           frameH = 0;
    };

    std::array<std::optional<SlotPreview>, PLAYER_ANIM_SLOT_COUNT> mPreviews;

    float mAnimTimer  = 0.0f;
    int   mAnimFrame  = 0;

    void rebuildPreview(int slotIdx);
    void clearPreview(int slotIdx);
    void deleteFrame(int slotIdx, int frameIdx);
    void duplicateFrame(int slotIdx, int frameIdx);

    // --- Frame strip ---
    int  mFrameStripScroll = 0;
    static constexpr int FRAME_THUMB_SZ = 48;
    static constexpr int FRAME_STRIP_H  = 68;
    std::vector<SDL_Rect> mFrameDelRects;
    std::vector<SDL_Rect> mFrameDupRects;

    // --- Hitbox editor ---
    // Hitbox rect is in preview-local coordinates (0,0 = top-left of scaled sprite)
    SDL_Rect mHBRect{};
    bool     mHBEditing = false;
    int      mHBDragHandle = -1; // -1=none, 0=TL, 1=TR, 2=BR, 3=BL, 4=body
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

    // --- Roster ---
    struct RosterEntry {
        std::string           name;
        std::string           path;
        SDL_Rect              loadRect{};
        SDL_Rect              delRect{};
    };
    std::vector<RosterEntry> mRoster;
    int                      mRosterScroll = 0;
    void refreshRoster();
    void loadRosterEntry(int idx);

    // --- Layout rects ---
    SDL_Rect mSlotPanel{};
    SDL_Rect mCenterPanel{};
    SDL_Rect mRosterPanel{};

    SDL_Rect mNameFieldRect{};
    SDL_Rect mSaveBtnRect{};
    SDL_Rect mBackBtnRect{};
    SDL_Rect mClearSlotRect{};

    enum class HoverBtn { None, Save, Back, Clear, SlotRow, FpsMinus, FpsPlus, RosterLoad, RosterDel };
    HoverBtn mHoverBtn   = HoverBtn::None;
    int      mHoverIndex = -1;

    std::vector<SDL_Rect> mSlotRowRects;

    void computeLayout();

    static float defaultFps(PlayerAnimSlot s) {
        switch (s) {
            case PlayerAnimSlot::Walk:   return 24.0f;
            case PlayerAnimSlot::Jump:   return 4.0f;
            case PlayerAnimSlot::Slash:  return 18.0f;
            case PlayerAnimSlot::Crouch: return 12.0f;
            default:                     return 12.0f;
        }
    }

    void startTextInput();
    void stopTextInput();

    // --- Draw helpers ---
    static void fillRect(SDL_Surface* s, SDL_Rect r, SDL_Color c);
    static void outlineRect(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1);
    static void drawText(SDL_Surface* s, const std::string& str,
                         int x, int y, int ptSize,
                         SDL_Color col = {220, 220, 220, 255});
    static void drawTextCentered(SDL_Surface* s, const std::string& str,
                                 SDL_Rect r, int ptSize,
                                 SDL_Color col = {220, 220, 220, 255});
    static void blitScaled(SDL_Surface* dst, SDL_Surface* src, SDL_Rect dstRect);
    static void blitScaledRegion(SDL_Surface* dst, SDL_Surface* src,
                                 const SDL_Rect* srcRect, SDL_Rect dstRect);

    // --- Colours ---
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
    static constexpr SDL_Color BTN_SAVE_H  = {60, 200, 100,  255};
    static constexpr SDL_Color BTN_BACK    = {80,  80, 160,  255};
    static constexpr SDL_Color BTN_BACK_H  = {110, 110, 200,  255};
    static constexpr SDL_Color BTN_DEL     = {160, 50,  50,  255};
    static constexpr SDL_Color BTN_DEL_H   = {200, 70,  70,  255};
    static constexpr SDL_Color BTN_LOAD    = {40, 100, 180,  255};
    static constexpr SDL_Color BTN_LOAD_H  = {60, 130, 220,  255};
    static constexpr SDL_Color BTN_CLR     = {80, 50, 50, 255};
    static constexpr SDL_Color BTN_CLR_H   = {110, 70, 70, 255};

    static SDL_Rect normaliseRect(SDL_Rect r);
};
