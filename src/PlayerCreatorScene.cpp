#include "PlayerCreatorScene.hpp"
#include "GameScene.hpp"
#include "TitleScene.hpp"
#include "audio/AudioEngine.hpp"
#include "audio/AudioEvents.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <filesystem>
#include <print>

namespace fs = std::filesystem;

static bool hit(const SDL_Rect& r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static SDL_Color lerp(SDL_Color a, SDL_Color b, float t) {
    auto cl = [](float v) -> Uint8 { return (Uint8)std::clamp((int)v, 0, 255); };
    return {cl(a.r + (b.r - a.r) * t),
            cl(a.g + (b.g - a.g) * t),
            cl(a.b + (b.b - a.b) * t),
            cl(a.a + (b.a - a.a) * t)};
}

// --- Scene interface ---

void PlayerCreatorScene::Load(Window& window) {
    mW      = window.GetWidth();
    mH      = window.GetHeight();
    mSDLWin = window.GetRaw();

    if (!fs::exists("players"))
        fs::create_directory("players");

    mProfile = PlayerProfile{};
    mProfile.name = "MyCharacter";

    computeLayout();
    recomputePreviewRect();
    refreshRoster();
    initHBFromProfile(mSelectedSlot);
}

void PlayerCreatorScene::Unload() {
    if (Audio() && mPreviewPlaying)
        Audio()->Sfx().StopPreview();
    mPreviewPlaying = false;
    stopTextInput();
    if (mRenderTexture) { SDL_DestroyTexture(mRenderTexture); mRenderTexture = nullptr; }
    if (mRenderSurface) { SDL_DestroySurface(mRenderSurface); mRenderSurface = nullptr; }
    for (auto& [k, s] : mTextCache) if (s) SDL_DestroySurface(s);
    mTextCache.clear();
}

// --- Layout ---

void PlayerCreatorScene::computeLayout() {
    const int PAD   = PANEL_PAD;
    const int TOTAL = mW;
    const int H     = mH;

    const int LEFT_W   = 240;
    const int RIGHT_W  = 260;
    const int MID_W    = TOTAL - LEFT_W - RIGHT_W - PAD * 4;

    mSlotPanel   = {PAD,                     40,  LEFT_W,  H - 80};
    mCenterPanel = {PAD * 2 + LEFT_W,         40,  MID_W,   H - 80};
    mRosterPanel = {PAD * 3 + LEFT_W + MID_W, 40,  RIGHT_W, H - 80};

    mNameFieldRect = {mCenterPanel.x, mCenterPanel.y + 40, MID_W - 2, 36};

    const int sfW = (MID_W - 6) / 2;
    mWidthRect  = {mCenterPanel.x,         mCenterPanel.y + 84, sfW, 32};
    mHeightRect = {mCenterPanel.x + sfW + 6, mCenterPanel.y + 84, sfW, 32};

    mSaveBtnRect = {mCenterPanel.x,            mCenterPanel.y + mCenterPanel.h - 50, 130, 40};
    mBackBtnRect = {mCenterPanel.x + MID_W - 130, mCenterPanel.y + mCenterPanel.h - 50, 130, 40};

    // Initialized to zero; recomputePreviewRect() populates these right after
    mPreviewCellRect   = {};
    mPreviewRenderRect = {};
    mDropZone          = {};
    mClearSlotRect     = {};

    static constexpr int BASE_ROW_H   = 44;
    static constexpr int SFX_NAME_H   = 14;
    static constexpr int SFX_SLIDER_H = 10;
    static constexpr int SFX_TRIM_H   = 10;
    static constexpr int SFX_ENTRY_H  = SFX_NAME_H + SFX_SLIDER_H + SFX_TRIM_H;
    static constexpr int SFX_DROP_H   = 16;
    mSlotRowRects.resize(PLAYER_ANIM_SLOT_COUNT);
    int ry = mSlotPanel.y + 40;
    for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
        int numSfx = (int)mProfile.slots[i].sfx.size();
        int rowH = BASE_ROW_H + numSfx * SFX_ENTRY_H + SFX_DROP_H;

        mSlotRowRects[i] = {mSlotPanel.x + 4, ry, LEFT_W - 8, rowH - 4};

        const int BW = 18, BH = 16;
        const int BY = ry + (BASE_ROW_H - BH) / 2;
        mFpsBtns[i].plusRect  = {mSlotPanel.x + LEFT_W - 8 - BW,      BY, BW, BH};
        mFpsBtns[i].minusRect = {mSlotPanel.x + LEFT_W - 8 - BW*2 - 2, BY, BW, BH};

        const int sfxW   = LEFT_W - 16;
        const int clearW = 14;
        const int tsW    = 16;
        const int btnsW  = clearW + tsW + 3;

        mSfxFileUI[i].clear();
        mSfxFileUI[i].resize(numSfx);
        for (int fi = 0; fi < numSfx; ++fi) {
            int entryY = ry + BASE_ROW_H + fi * SFX_ENTRY_H;
            mSfxFileUI[i][fi].nameRect    = {mSlotPanel.x + 8, entryY, sfxW - btnsW - 2, SFX_NAME_H};
            int bx = mSlotPanel.x + 8 + sfxW - btnsW;
            mSfxFileUI[i][fi].stretchRect = {bx, entryY, tsW, SFX_NAME_H};
            mSfxFileUI[i][fi].clearRect   = {mSlotPanel.x + 8 + sfxW - clearW, entryY, clearW, SFX_NAME_H};
            mSfxFileUI[i][fi].sliderRect     = {mSlotPanel.x + 8, entryY + SFX_NAME_H, sfxW, SFX_SLIDER_H};
            mSfxFileUI[i][fi].trimSliderRect = {mSlotPanel.x + 8, entryY + SFX_NAME_H + SFX_SLIDER_H, sfxW, SFX_TRIM_H};
        }

        int dropY = ry + BASE_ROW_H + numSfx * SFX_ENTRY_H;
        mSfxDropRect[i] = {mSlotPanel.x + 8, dropY, sfxW, SFX_DROP_H};

        ry += rowH;
    }

    recomputePreviewRect();
}

// --- Events ---

bool PlayerCreatorScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) return false;

    // Any event that modifies visible state should call markDirty().
    // For simplicity, mark dirty on any user interaction.
    markDirty();

    if (e.type == SDL_EVENT_DROP_BEGIN) {
        mDropHover = true;
        return true;
    }
    if (e.type == SDL_EVENT_DROP_COMPLETE) {
        mDropHover = false;
        return true;
    }

    if (e.type == SDL_EVENT_DROP_FILE || e.type == SDL_EVENT_DROP_TEXT) {
        mDropHover = false;
        mSfxDropHoverSlot = -1;
        std::string dropped = e.drop.data ? e.drop.data : "";

        if (!dropped.empty()) {
            fs::path p(dropped);

            if (fs::is_regular_file(p) && isAudioFile(p)) {
                float fmx, fmy;
                SDL_GetMouseState(&fmx, &fmy);
                int mx2 = (int)fmx, my2 = (int)fmy;
                int targetSlot = mSelectedSlot;
                for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
                    if (hit(mSfxDropRect[i], mx2, my2) ||
                        hit(mSlotRowRects[i], mx2, my2)) {
                        targetSlot = i;
                        break;
                    }
                }
                auto& sfx = mProfile.slots[targetSlot].sfx;
                bool dup = false;
                for (const auto& e : sfx)
                    if (e.path == dropped) { dup = true; break; }
                if (!dup) sfx.push_back({dropped, 1.0f, false});
                computeLayout();
                auto slotName = PlayerAnimSlotName(static_cast<PlayerAnimSlot>(targetSlot));
                mDropMsg = std::string(slotName) + " SFX " + std::to_string(sfx.size())
                          + ": " + p.filename().string();
                return true;
            }

            std::string dir;
            if (fs::is_directory(p))      dir = dropped;
            else if (fs::is_regular_file(p)) dir = p.parent_path().string();

            if (!dir.empty()) {
                bool hasPng = false;
                try {
                    for (const auto& entry : fs::directory_iterator(dir)) {
                        if (entry.path().extension() == ".png" ||
                            entry.path().extension() == ".PNG") {
                            hasPng = true; break;
                        }
                    }
                } catch (...) {}

                if (hasPng) {
                    mProfile.Slot(static_cast<PlayerAnimSlot>(mSelectedSlot)).folderPath = dir;
                    rebuildPreview(mSelectedSlot);
                    recomputePreviewRect();
                    initHBFromProfile(mSelectedSlot);
                    mDropMsg = "Loaded: " + fs::path(dir).filename().string();
                } else {
                    mDropMsg = "No PNGs found in that folder.";
                }
            } else {
                mDropMsg = "Drop a folder of PNG frames here.";
            }
        }
        return true;
    }

    if (mSizeActive != SizeField::None) {
        std::string& str = (mSizeActive == SizeField::Width) ? mWidthStr : mHeightStr;
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            for (char c : std::string(e.text.text))
                if (std::isdigit((unsigned char)c) && str.size() < 4)
                    str += c;
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_BACKSPACE && !str.empty()) { str.pop_back(); return true; }
            if (e.key.key == SDLK_RETURN || e.key.key == SDLK_ESCAPE) {
                int val = str.empty() ? 0 : std::stoi(str);
                if (mSizeActive == SizeField::Width)  mProfile.spriteW = val;
                else                                  mProfile.spriteH = val;
                mSizeActive = SizeField::None;
                SDL_StopTextInput(mSDLWin);
                recomputePreviewRect();
                initHBFromProfile(mSelectedSlot);
                return true;
            }
        }
        return true;
    }

    if (mNameActive) {
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            for (char c : std::string(e.text.text)) {
                if (std::isalnum((unsigned char)c) || c == '-' || c == '_')
                    mProfile.name += c;
            }
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_BACKSPACE:
                    if (!mProfile.name.empty()) mProfile.name.pop_back();
                    return true;
                case SDLK_RETURN:
                case SDLK_ESCAPE:
                    stopTextInput();
                    return true;
                default: break;
            }
        }
        return true;
    }

    auto ensurePreview = [&](int slot) {
        if (!mPreviews[slot].has_value() && !mProfile.slots[slot].folderPath.empty())
            rebuildPreview(slot);
    };

    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_DOWN || e.key.key == SDLK_S) {
            if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
            mSelectedSlot = (mSelectedSlot + 1) % PLAYER_ANIM_SLOT_COUNT;
            mSelectedSfxFile = 0;
            ensurePreview(mSelectedSlot);
            initHBFromProfile(mSelectedSlot);
            mAnimFrame = 0; mAnimTimer = 0; mFrameStripScroll = 0;
        }
        if (e.key.key == SDLK_UP || e.key.key == SDLK_W) {
            if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
            mSelectedSlot = (mSelectedSlot - 1 + PLAYER_ANIM_SLOT_COUNT) % PLAYER_ANIM_SLOT_COUNT;
            mSelectedSfxFile = 0;
            ensurePreview(mSelectedSlot);
            initHBFromProfile(mSelectedSlot);
            mAnimFrame = 0; mAnimTimer = 0; mFrameStripScroll = 0;
        }
        if (e.key.key == SDLK_ESCAPE) {
            mGoBack = true;
        }
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        int mx = (int)e.motion.x;
        int my = (int)e.motion.y;

        if (mVolDragSlot >= 0 && mVolDragFile >= 0) {
            auto& entry = mProfile.slots[mVolDragSlot].sfx[mVolDragFile];
            const auto& sr = mSfxFileUI[mVolDragSlot][mVolDragFile].sliderRect;
            float t = std::clamp((float)(mx - sr.x) / (float)sr.w, 0.0f, 1.0f);
            entry.volume = t;
            return true;
        }
        if (mTrimDragSlot >= 0 && mTrimDragFile >= 0) {
            auto& entry = mProfile.slots[mTrimDragSlot].sfx[mTrimDragFile];
            const auto& tr = mSfxFileUI[mTrimDragSlot][mTrimDragFile].trimSliderRect;
            float t = std::clamp((float)(mx - tr.x) / (float)tr.w, 0.0f, 1.0f);
            if (mTrimDragHandle == 0)
                entry.trimStart = std::min(t, entry.trimEnd - 0.01f);
            else
                entry.trimEnd = std::max(t, entry.trimStart + 0.01f);
            return true;
        }

        if (mHBEditing && mHBDragHandle >= 0) {
            int dx = mx - mHBDragStartMX;
            int dy = my - mHBDragStartMY;
            SDL_Rect r = mHBDragStartRect;

            switch (mHBDragHandle) {
                case 0: r.x += dx; r.y += dy; r.w -= dx; r.h -= dy; break; // TL
                case 1: r.y += dy; r.w += dx; r.h -= dy; break;             // TR
                case 2: r.w += dx; r.h += dy; break;                        // BR
                case 3: r.x += dx; r.w -= dx; r.h += dy; break;             // BL
                case 4: r.x += dx; r.y += dy; break;                        // body move
            }
            r.w = std::max(r.w, 4);
            r.h = std::max(r.h, 4);
            mHBRect = r;
        }

        mHoverBtn = HoverBtn::None;
        mHoverIndex = -1;
        if (hit(mSaveBtnRect, mx, my))           mHoverBtn = HoverBtn::Save;
        else if (hit(mBackBtnRect, mx, my))       mHoverBtn = HoverBtn::Back;
        else if (hit(mClearSlotRect, mx, my))     mHoverBtn = HoverBtn::Clear;
        else {
            for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
                if (hit(mFpsBtns[i].plusRect, mx, my))  { mHoverBtn = HoverBtn::FpsPlus;  mHoverIndex = i; break; }
                if (hit(mFpsBtns[i].minusRect, mx, my)) { mHoverBtn = HoverBtn::FpsMinus; mHoverIndex = i; break; }
                if (hit(mSlotRowRects[i], mx, my))       { mHoverBtn = HoverBtn::SlotRow;  mHoverIndex = i; break; }
            }
            if (mHoverBtn == HoverBtn::None) {
                for (int i = 0; i < (int)mRoster.size(); ++i) {
                    if (hit(mRoster[i].loadRect, mx, my)) { mHoverBtn = HoverBtn::RosterLoad; mHoverIndex = i; break; }
                    if (hit(mRoster[i].delRect, mx, my))  { mHoverBtn = HoverBtn::RosterDel;  mHoverIndex = i; break; }
                }
            }
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = (int)e.button.x;
        int my = (int)e.button.y;

        for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
            for (int fi = 0; fi < (int)mSfxFileUI[i].size(); ++fi) {
                const auto& ui = mSfxFileUI[i][fi];
                auto& entry = mProfile.slots[i].sfx[fi];
                bool anyHit = hit(ui.nameRect, mx, my) || hit(ui.stretchRect, mx, my) ||
                              hit(ui.sliderRect, mx, my) || hit(ui.trimSliderRect, mx, my) ||
                              hit(ui.clearRect, mx, my);
                if (anyHit && i == mSelectedSlot)
                    mSelectedSfxFile = fi;
                if (hit(ui.stretchRect, mx, my)) {
                    entry.timeStretch = !entry.timeStretch;
                    mDropMsg = fs::path(entry.path).filename().string()
                              + " TS: " + (entry.timeStretch ? "ON" : "OFF");
                    return true;
                }
                if (hit(ui.sliderRect, mx, my)) {
                    mVolDragSlot = i;
                    mVolDragFile = fi;
                    float t = std::clamp((float)(mx - ui.sliderRect.x) / (float)ui.sliderRect.w, 0.0f, 1.0f);
                    entry.volume = t;
                    return true;
                }
                if (hit(ui.trimSliderRect, mx, my)) {
                    mTrimDragSlot = i;
                    mTrimDragFile = fi;
                    float t = std::clamp((float)(mx - ui.trimSliderRect.x) / (float)ui.trimSliderRect.w, 0.0f, 1.0f);
                    float dStart = std::abs(t - entry.trimStart);
                    float dEnd   = std::abs(t - entry.trimEnd);
                    mTrimDragHandle = (dStart <= dEnd) ? 0 : 1;
                    if (mTrimDragHandle == 0)
                        entry.trimStart = std::min(t, entry.trimEnd - 0.01f);
                    else
                        entry.trimEnd = std::max(t, entry.trimStart + 0.01f);
                    return true;
                }
                if (hit(ui.clearRect, mx, my)) {
                    mDropMsg = fs::path(entry.path).filename().string() + " removed";
                    mProfile.slots[i].sfx.erase(mProfile.slots[i].sfx.begin() + fi);
                    if (mSelectedSfxFile >= (int)mProfile.slots[i].sfx.size())
                        mSelectedSfxFile = std::max(0, (int)mProfile.slots[i].sfx.size() - 1);
                    computeLayout();
                    return true;
                }
            }
        }

        // FPS +/- (check before slot row so they don't also trigger selection)
        for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
            auto slot = static_cast<PlayerAnimSlot>(i);
            float& fps = mProfile.Slot(slot).fps;
            if (hit(mFpsBtns[i].plusRect, mx, my)) {
                fps = (fps <= 0.0f ? defaultFps(slot) : fps) + 1.0f;
                return true;
            }
            if (hit(mFpsBtns[i].minusRect, mx, my)) {
                fps = std::max(1.0f, (fps <= 0.0f ? defaultFps(slot) : fps) - 1.0f);
                return true;
            }
        }

        for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
            if (hit(mSlotRowRects[i], mx, my)) {
                if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
                mSelectedSlot = i;
                mSelectedSfxFile = 0;
                ensurePreview(mSelectedSlot);
                initHBFromProfile(mSelectedSlot);
                mAnimFrame = 0; mAnimTimer = 0;
                mFrameStripScroll = 0;
                return true;
            }
        }

        if (hit(mWidthRect, mx, my)) {
            mWidthStr   = (mProfile.spriteW > 0) ? std::to_string(mProfile.spriteW) : "";
            mSizeActive = SizeField::Width;
            if (mNameActive) stopTextInput();
            SDL_StartTextInput(mSDLWin);
            return true;
        }
        if (hit(mHeightRect, mx, my)) {
            mHeightStr  = (mProfile.spriteH > 0) ? std::to_string(mProfile.spriteH) : "";
            mSizeActive = SizeField::Height;
            if (mNameActive) stopTextInput();
            SDL_StartTextInput(mSDLWin);
            return true;
        }

        if (hit(mNameFieldRect, mx, my)) {
            mProfile.name.clear();
            mNameError.clear();
            mSizeActive = SizeField::None;
            startTextInput();
            return true;
        } else {
            if (mNameActive) stopTextInput();
            if (mSizeActive != SizeField::None) {
                auto& str = (mSizeActive == SizeField::Width) ? mWidthStr : mHeightStr;
                int val = str.empty() ? 0 : std::stoi(str);
                if (mSizeActive == SizeField::Width)  mProfile.spriteW = val;
                else                                  mProfile.spriteH = val;
                mSizeActive = SizeField::None;
                SDL_StopTextInput(mSDLWin);
                recomputePreviewRect();
                initHBFromProfile(mSelectedSlot);
            }
        }

        if (hit(mBackBtnRect, mx, my)) {
            if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
            mGoBack = true;
            return true;
        }

        if (hit(mSaveBtnRect, mx, my)) {
            if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
            if (mProfile.name.empty()) {
                mNameError = "Name required!";
            } else {
                if (!fs::exists("players")) fs::create_directory("players");
                // Delete old file if name changed so no stale copy remains
                if (!mLoadedName.empty() && mLoadedName != mProfile.name) {
                    std::error_code ec;
                    fs::remove(PlayerProfilePath(mLoadedName), ec);
                }
                SavePlayerProfile(mProfile, PlayerProfilePath(mProfile.name));
                mLoadedName = mProfile.name;
                refreshRoster();
                mNameError.clear();
                mDropMsg = "Saved as: " + mProfile.name;
            }
            return true;
        }

        if (hit(mClearSlotRect, mx, my)) {
            mProfile.Slot(static_cast<PlayerAnimSlot>(mSelectedSlot)).folderPath.clear();
            mProfile.Slot(static_cast<PlayerAnimSlot>(mSelectedSlot)).hitbox = {};
            clearPreview(mSelectedSlot);
            initHBFromProfile(mSelectedSlot);
            mDropMsg = "Slot cleared.";
            return true;
        }

        if (!mFrameDupRects.empty()) {
            for (int i = 0; i < (int)mFrameDupRects.size(); ++i) {
                if (mFrameDupRects[i].w > 0 && hit(mFrameDupRects[i], mx, my)) {
                    duplicateFrame(mSelectedSlot, i);
                    return true;
                }
            }
        }

        if (!mFrameDelRects.empty()) {
            for (int i = 0; i < (int)mFrameDelRects.size(); ++i) {
                if (mFrameDelRects[i].w > 0 && hit(mFrameDelRects[i], mx, my)) {
                    deleteFrame(mSelectedSlot, i);
                    return true;
                }
            }
        }

        int handle = hitboxHandleAt(mx, my);
        if (handle >= 0 && mHBInitialised) {
            mHBEditing       = true;
            mHBDragHandle    = handle;
            mHBDragStartMX   = mx;
            mHBDragStartMY   = my;
            mHBDragStartRect = mHBRect;
            return true;
        }

        int ry = mRosterPanel.y + 40;
        for (int i = mRosterScroll; i < (int)mRoster.size(); ++i) {
            auto& entry = mRoster[i];
            if (hit(entry.loadRect, mx, my)) {
                loadRosterEntry(i);
                return true;
            }
            if (hit(entry.delRect, mx, my)) {
                try { fs::remove(entry.path); } catch (...) {}
                refreshRoster();
                return true;
            }
            ry += 52;
            if (ry > mRosterPanel.y + mRosterPanel.h - 10) break;
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
        if (mVolDragSlot >= 0) {
            mVolDragSlot = -1;
            mVolDragFile = -1;
        }
        if (mTrimDragSlot >= 0) {
            mTrimDragSlot  = -1;
            mTrimDragFile  = -1;
            mTrimDragHandle = -1;
        }
        if (mHBEditing) {
            mHBEditing    = false;
            mHBDragHandle = -1;
            mHBRect = normaliseRect(mHBRect);
        }
    }

    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        float fmx, fmy;
        SDL_GetMouseState(&fmx, &fmy);
        int mx = (int)fmx, my = (int)fmy;
        if (hit(mRosterPanel, mx, my)) {
            mRosterScroll = std::clamp(mRosterScroll - (int)e.wheel.y,
                                       0, std::max(0, (int)mRoster.size() - 1));
        }
        if (hit(mCenterPanel, mx, my) && !hit(mRosterPanel, mx, my)) {
            const auto& sp = mPreviews[mSelectedSlot];
            if (sp.has_value() && !sp->frames.empty()) {
                int availW = mCenterPanel.w - 8;
                int cols   = std::max(1, availW / (FRAME_THUMB_SZ + 4));
                int totalRows = ((int)sp->frames.size() + cols - 1) / cols;
                int maxScrollRow = std::max(0, totalRows - 3); // 3 = MAX_VIS_ROWS
                mFrameStripScroll = std::clamp(mFrameStripScroll - (int)e.wheel.y,
                                               0, maxScrollRow);
            }
        }
    }

    return true;
}

// --- Update ---

void PlayerCreatorScene::Update(float dt) {
    const auto& prev = mPreviews[mSelectedSlot];
    if (prev.has_value() && !prev->frames.empty()) {
        float slotFps = mProfile.Slot(static_cast<PlayerAnimSlot>(mSelectedSlot)).fps;
        float previewFps = (slotFps > 0.0f) ? slotFps : ANIM_FPS;
        mAnimTimer += dt;
        if (mAnimTimer >= 1.0f / previewFps) {
            mAnimTimer = 0.0f;
            mAnimFrame = (mAnimFrame + 1) % (int)prev->frames.size();
            markDirty();
        }
    }

    if (!Audio()) return;
    auto& sfx = Audio()->Sfx();
    const auto& slotSfx = mProfile.slots[mSelectedSlot].sfx;

    if (slotSfx.empty()) {
        if (mPreviewPlaying) {
            sfx.StopPreview();
            mPreviewPlaying = false;
            mPreviewSfxId.clear();
            mPreviewPath.clear();
            mPreviewSlot = -1;
            mPreviewFile = -1;
        }
        return;
    }

    // Compute animation duration for time-stretch
    float animDur = 0.0f;
    {
        const auto& prev = mPreviews[mSelectedSlot];
        float slotFps = mProfile.Slot(static_cast<PlayerAnimSlot>(mSelectedSlot)).fps;
        float fps = (slotFps > 0.0f) ? slotFps : ANIM_FPS;
        if (prev.has_value() && !prev->frames.empty() && fps > 0.0f)
            animDur = (float)prev->frames.size() / fps;
    }

    auto calcRatio = [&](const PlayerProfile::SfxEntry& e, const std::string& id) -> float {
        if (!e.timeStretch || animDur <= 0.0f) return 1.0f;
        float naturalDur = sfx.GetDuration(id);
        if (naturalDur <= 0.0f) return 1.0f;
        return std::clamp(naturalDur / animDur, 0.25f, 4.0f);
    };

    auto effectiveDur = [&](const PlayerProfile::SfxEntry& e, const std::string& id) -> float {
        float natural = sfx.GetDuration(id);
        if (natural <= 0.0f) return 0.0f;
        if (!e.timeStretch || animDur <= 0.0f) return natural;
        float ratio = std::clamp(natural / animDur, 0.25f, 4.0f);
        return natural / ratio;
    };

    int fileIdx = std::clamp(mSelectedSfxFile, 0, (int)slotSfx.size() - 1);
    const auto& entry = slotSfx[fileIdx];
    std::string wantId = audio::PlayerSlotSfxId(mSelectedSlot, fileIdx);

    if (mPreviewSlot != mSelectedSlot || mPreviewFile != fileIdx ||
        mPreviewSfxId != wantId || mPreviewPath != entry.path) {
        if (mPreviewPlaying)
            sfx.StopPreview();

        if (!sfx.Has(wantId))
            sfx.Load(wantId, entry.path);

        mPreviewSfxId = wantId;
        mPreviewPath  = entry.path;
        mPreviewSlot  = mSelectedSlot;
        mPreviewFile  = fileIdx;
        mPreviewTimer = 0.0f;
        mPreviewPlaying = false;
    }

    float playDur = effectiveDur(entry, mPreviewSfxId);
    if (playDur <= 0.0f) return;

    float naturalDur = sfx.GetDuration(mPreviewSfxId);
    float tStart = entry.trimStart * naturalDur;
    float tEnd   = entry.trimEnd   * naturalDur;
    if (tEnd <= tStart) return;

    mPreviewTimer += dt;

    auto startFile = [&](int fi) {
        const auto& e = slotSfx[fi];
        std::string id = audio::PlayerSlotSfxId(mSelectedSlot, fi);
        if (!sfx.Has(id)) sfx.Load(id, e.path);
        float ratio = calcRatio(e, id);
        sfx.PlayPreview(id, e.volume, ratio);
        mPreviewSfxId   = id;
        mPreviewPath    = e.path;
        mPreviewFile    = fi;
        mPreviewPlaying = true;
        mPreviewTimer   = 0.0f;
    };

    if (!mPreviewPlaying || mPreviewTimer >= playDur) {
        if (mPreviewPlaying && (int)slotSfx.size() > 1) {
            mSelectedSfxFile = (mSelectedSfxFile + 1) % (int)slotSfx.size();
            startFile(mSelectedSfxFile);
        } else {
            startFile(fileIdx);
        }
    } else {
        float ratio = calcRatio(entry, mPreviewSfxId);
        float curNatural = mPreviewTimer * ratio;
        bool inWindow = (curNatural >= tStart && curNatural < tEnd);
        sfx.SetPreviewGain(inWindow ? entry.volume : 0.0f);
    }
}

// --- Render ---

void PlayerCreatorScene::Render(Window& window, float /*alpha*/) {
    window.Render();
    SDL_Renderer* ren = window.GetRenderer();

    // Reuse the cached texture when nothing changed.
    if (!mDirty && mRenderTexture) {
        SDL_RenderTexture(ren, mRenderTexture, nullptr, nullptr);
        window.Update();
        return;
    }
    mDirty = false;

    // Reuse the surface allocation across frames.
    if (!mRenderSurface || mRenderSurface->w != mW || mRenderSurface->h != mH) {
        if (mRenderSurface) SDL_DestroySurface(mRenderSurface);
        mRenderSurface = SDL_CreateSurface(mW, mH, SDL_PIXELFORMAT_ARGB8888);
    }
    SDL_Surface* s = mRenderSurface;
    if (!s) { window.Update(); return; }

    fillRect(s, {0, 0, mW, mH}, BG);
    drawTextCentered(s, "Player Creator", {0, 4, mW, 32}, 24, {200, 210, 255, 255});

    // --- Left: Slot panel ---
    fillRect(s, mSlotPanel, PANEL_BG);
    outlineRect(s, mSlotPanel, PANEL_OUT);
    drawText(s, "Animation Slots", mSlotPanel.x + 6, mSlotPanel.y + 8, 14, {160, 170, 220, 255});

    for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
        auto slot  = static_cast<PlayerAnimSlot>(i);
        bool sel   = (i == mSelectedSlot);
        bool filled = mProfile.HasSlot(slot);

        bool hRow = (!sel && mHoverBtn == HoverBtn::SlotRow && mHoverIndex == i);
        SDL_Color bg = sel ? SEL_BG : hRow ? SDL_Color{60, 65, 95, 255} : (filled ? SLOT_FILLED : SLOT_EMPTY);
        fillRect(s, mSlotRowRects[i], bg);
        outlineRect(s, mSlotRowRects[i], sel ? SDL_Color{120, 160, 255, 255}
                                        : hRow ? SDL_Color{100, 120, 180, 255} : PANEL_OUT);

        drawText(s, PlayerAnimSlotName(slot),
                 mSlotRowRects[i].x + 8,
                 mSlotRowRects[i].y + (SLOT_ROW_H - 20) / 2,
                 16, sel ? SDL_Color{255, 255, 255, 255} : SDL_Color{180, 190, 210, 255});

        const auto& path = mProfile.Slot(slot).folderPath;
        if (!path.empty()) {
            std::string fname = fs::path(path).filename().string();
            if ((int)fname.size() > 16) fname = fname.substr(0, 14) + "..";
            drawText(s, fname,
                     mSlotRowRects[i].x + 90,
                     mSlotRowRects[i].y + (SLOT_ROW_H - 14) / 2,
                     11, {160, 220, 160, 255});
        } else {
            drawText(s, "-- empty --",
                     mSlotRowRects[i].x + 90,
                     mSlotRowRects[i].y + (SLOT_ROW_H - 14) / 2,
                     11, {80, 90, 100, 255});
        }

        {
            float curFps = mProfile.Slot(slot).fps;
            std::string fpsStr = (curFps > 0.0f)
                ? std::to_string((int)curFps) + "fps"
                : "default";
            {
                bool hMinus = (mHoverBtn == HoverBtn::FpsMinus && mHoverIndex == i);
                bool hPlus  = (mHoverBtn == HoverBtn::FpsPlus  && mHoverIndex == i);
                fillRect(s, mFpsBtns[i].minusRect, hMinus ? SDL_Color{70, 70, 120, 255} : SDL_Color{50, 50, 80, 255});
                outlineRect(s, mFpsBtns[i].minusRect, hMinus ? SDL_Color{120, 120, 200, 255} : SDL_Color{80, 80, 130, 255});
                drawTextCentered(s, "-", mFpsBtns[i].minusRect, 13, {200, 200, 255, 255});
                fillRect(s, mFpsBtns[i].plusRect, hPlus ? SDL_Color{70, 70, 120, 255} : SDL_Color{50, 50, 80, 255});
                outlineRect(s, mFpsBtns[i].plusRect, hPlus ? SDL_Color{120, 120, 200, 255} : SDL_Color{80, 80, 130, 255});
                drawTextCentered(s, "+", mFpsBtns[i].plusRect, 13, {200, 200, 255, 255});
            }
            int labelX = mFpsBtns[i].minusRect.x - 2 - (int)fpsStr.size() * 6;
            drawText(s, fpsStr, labelX,
                     mFpsBtns[i].minusRect.y + 2, 10, {160, 170, 200, 255});
        }

        for (int fi = 0; fi < (int)mSfxFileUI[i].size(); ++fi) {
            const auto& ui = mSfxFileUI[i][fi];
            const auto& entry = mProfile.slots[i].sfx[fi];

            bool isSelected = (i == mSelectedSlot && fi == mSelectedSfxFile);
            bool isPlaying  = isSelected && mPreviewPlaying && mPreviewFile == fi;
            SDL_Color bgCol  = isPlaying  ? SDL_Color{35, 75, 50, 255}
                             : isSelected ? SDL_Color{50, 65, 95, 255}
                                          : SDL_Color{35, 55, 80, 255};
            SDL_Color outCol = isPlaying  ? SDL_Color{80, 255, 120, 255}
                             : isSelected ? SDL_Color{200, 140, 40, 255}
                                          : SDL_Color{60, 120, 180, 255};
            fillRect(s, ui.nameRect, bgCol);
            outlineRect(s, ui.nameRect, outCol);
            std::string fname = fs::path(entry.path).filename().string();
            if ((int)fname.size() > 20) fname = fname.substr(0, 18) + "..";
            SDL_Color nameCol = isPlaying ? SDL_Color{120, 255, 160, 255} : SDL_Color{120, 190, 255, 255};
            drawText(s, (isPlaying ? "> " : "") + fname, ui.nameRect.x + 3, ui.nameRect.y + 2, 9, nameCol);

            bool ts = entry.timeStretch;
            SDL_Color tsBg  = ts ? SDL_Color{40, 80, 50, 255}  : SDL_Color{40, 35, 55, 255};
            SDL_Color tsOut = ts ? SDL_Color{80, 180, 100, 255} : SDL_Color{60, 55, 80, 255};
            SDL_Color tsFg  = ts ? SDL_Color{140, 255, 160, 255}: SDL_Color{100, 90, 130, 255};
            fillRect(s, ui.stretchRect, tsBg);
            outlineRect(s, ui.stretchRect, tsOut);
            drawTextCentered(s, "TS", ui.stretchRect, 7, tsFg);

            fillRect(s, ui.clearRect, {120, 40, 40, 255});
            outlineRect(s, ui.clearRect, {180, 60, 60, 255});
            drawTextCentered(s, "x", ui.clearRect, 8, {255, 180, 180, 255});

            int pct = (int)(entry.volume * 100.0f + 0.5f);
            const SDL_Rect& sr = ui.sliderRect;
            fillRect(s, sr, {20, 22, 36, 255});
            outlineRect(s, sr, {50, 55, 80, 255});
            int fillW = (int)(entry.volume * (float)(sr.w - 2));
            if (fillW > 0) {
                SDL_Rect filled = {sr.x + 1, sr.y + 1, fillW, sr.h - 2};
                fillRect(s, filled, {50, 120, 200, 255});
            }
            int knobX = sr.x + 1 + fillW;
            SDL_Rect knob = {knobX - 2, sr.y, 4, sr.h};
            fillRect(s, knob, {180, 200, 255, 255});
            std::string volStr = std::to_string(pct) + "%";
            drawText(s, volStr, sr.x + sr.w / 2 - (int)volStr.size() * 3, sr.y + 1, 7, {200, 210, 230, 255});

            const SDL_Rect& tr = ui.trimSliderRect;
            fillRect(s, tr, {30, 22, 16, 255});
            outlineRect(s, tr, {80, 60, 30, 255});
            int tStartX = tr.x + (int)(entry.trimStart * (float)(tr.w - 2)) + 1;
            int tEndX   = tr.x + (int)(entry.trimEnd   * (float)(tr.w - 2)) + 1;
            if (tEndX > tStartX) {
                SDL_Rect trimFill = {tStartX, tr.y + 1, tEndX - tStartX, tr.h - 2};
                fillRect(s, trimFill, {200, 120, 30, 255});
            }
            SDL_Rect startKnob = {tStartX - 2, tr.y, 4, tr.h};
            SDL_Rect endKnob   = {tEndX - 2,   tr.y, 4, tr.h};
            fillRect(s, startKnob, {255, 200, 100, 255});
            fillRect(s, endKnob,   {255, 200, 100, 255});
        }

        {
            bool sfxHover = (mSfxDropHoverSlot == i);
            SDL_Color dzBg  = sfxHover ? SDL_Color{40, 60, 120, 255} : SDL_Color{25, 30, 45, 255};
            SDL_Color dzOut = sfxHover ? SDL_Color{80, 130, 220, 255} : SDL_Color{45, 50, 70, 255};
            fillRect(s, mSfxDropRect[i], dzBg);
            outlineRect(s, mSfxDropRect[i], dzOut);
            drawText(s, "drop audio file", mSfxDropRect[i].x + 3, mSfxDropRect[i].y + 2, 8, {70, 80, 100, 255});
        }
    }

    // --- Centre panel ---
    fillRect(s, mCenterPanel, PANEL_BG);
    outlineRect(s, mCenterPanel, PANEL_OUT);

    drawText(s, "Character Name:",
             mCenterPanel.x + 4, mCenterPanel.y + 10, 14, {160, 170, 220, 255});
    SDL_Color fieldOutCol = mNameActive ? SDL_Color{100, 150, 255, 255} : PANEL_OUT;
    fillRect(s, mNameFieldRect, {18, 18, 32, 255});
    outlineRect(s, mNameFieldRect, fieldOutCol, 2);
    std::string nameDisplay = mProfile.name + (mNameActive ? "|" : "");
    drawText(s, nameDisplay, mNameFieldRect.x + 8, mNameFieldRect.y + 8, 18);
    if (!mNameError.empty())
        drawText(s, mNameError, mNameFieldRect.x, mNameFieldRect.y + mNameFieldRect.h + 2,
                 12, {255, 80, 80, 255});

    {
        bool wActive = (mSizeActive == SizeField::Width);
        bool hActive = (mSizeActive == SizeField::Height);
        drawText(s, "W:", mWidthRect.x + 4,  mWidthRect.y + 8, 13, {160, 170, 220, 255});
        drawText(s, "H:", mHeightRect.x + 4, mHeightRect.y + 8, 13, {160, 170, 220, 255});
        fillRect(s, mWidthRect, {18, 18, 32, 255});
        outlineRect(s, mWidthRect, wActive ? SDL_Color{100, 150, 255, 255} : PANEL_OUT, 2);
        std::string wDisplay = (wActive ? mWidthStr : (mProfile.spriteW > 0 ? std::to_string(mProfile.spriteW) : "120")) + (wActive ? "|" : "");
        drawText(s, wDisplay, mWidthRect.x + 22, mWidthRect.y + 8, 14);
        fillRect(s, mHeightRect, {18, 18, 32, 255});
        outlineRect(s, mHeightRect, hActive ? SDL_Color{100, 150, 255, 255} : PANEL_OUT, 2);
        std::string hDisplay = (hActive ? mHeightStr : (mProfile.spriteH > 0 ? std::to_string(mProfile.spriteH) : "160")) + (hActive ? "|" : "");
        drawText(s, hDisplay, mHeightRect.x + 22, mHeightRect.y + 8, 14);
    }

    std::string slotLabel = std::string(PlayerAnimSlotName(static_cast<PlayerAnimSlot>(mSelectedSlot)))
                          + " Preview";
    if (mProfile.spriteW > 0 && mProfile.spriteH > 0)
        slotLabel += "  (" + std::to_string(mProfile.spriteW) + "x" + std::to_string(mProfile.spriteH) + "px)";
    drawTextCentered(s, slotLabel,
                     {mPreviewCellRect.x, mPreviewCellRect.y - 24, mPreviewCellRect.w, 20},
                     13, {200, 210, 255, 255});

    fillRect(s, mPreviewCellRect, {10, 12, 22, 255});
    outlineRect(s, mPreviewCellRect, PANEL_OUT);

    const auto& prev = mPreviews[mSelectedSlot];
    if (prev.has_value() && !prev->frames.empty()) {
        SDL_Surface* sheet = prev->sheet->GetSurface();
        const SDL_Rect& fr = prev->frames[std::min(mAnimFrame, (int)prev->frames.size() - 1)];
        blitScaledRegion(s, sheet, &fr, mPreviewRenderRect);

        // Floor line = collider bottom = physics ground
        {
            const SDL_Rect hbNorm = normaliseRect(mHBRect);
            const int colBottom = hbNorm.y + hbNorm.h;
            SDL_Rect floorLine = {mPreviewCellRect.x, colBottom, mPreviewCellRect.w, 2};
            const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
            SDL_FillSurfaceRect(s, &floorLine, SDL_MapRGBA(fmt, nullptr, 80, 200, 80, 200));
            const int spriteBottom = mPreviewRenderRect.y + mPreviewRenderRect.h;
            if (spriteBottom != colBottom) {
                SDL_Rect spriteLine = {mPreviewCellRect.x, spriteBottom, mPreviewCellRect.w, 1};
                SDL_FillSurfaceRect(s, &spriteLine, SDL_MapRGBA(fmt, nullptr, 80, 200, 80, 70));
            }
        }

        renderHitboxOverlay(s);

        drawText(s,
                 std::to_string(mAnimFrame + 1) + "/" + std::to_string(prev->frames.size()),
                 mPreviewRenderRect.x + 4,
                 mPreviewRenderRect.y + mPreviewRenderRect.h - 18,
                 11, {160, 160, 180, 255});
    } else {
        drawTextCentered(s, "Drop a folder of PNGs", mPreviewCellRect, 14, {80, 90, 110, 255});
        drawTextCentered(s, "onto the zone below", {mPreviewCellRect.x, mPreviewCellRect.y + 20, mPreviewCellRect.w, mPreviewCellRect.h}, 13, {60, 70, 90, 255});
        renderHitboxOverlay(s);
    }

    SDL_Color dzCol = mDropHover ? DROP_HOVER : DROP_IDLE;
    fillRect(s, mDropZone, dzCol);
    outlineRect(s, mDropZone, mDropHover ? SDL_Color{100, 180, 255, 255} : PANEL_OUT, 2);
    drawTextCentered(s,
                     mDropMsg.empty() ? "Drop sprite folder here" : mDropMsg,
                     mDropZone, 15,
                     mDropHover ? SDL_Color{200, 230, 255, 255} : SDL_Color{140, 150, 180, 255});

    {
        bool hClr = (mHoverBtn == HoverBtn::Clear);
        fillRect(s, mClearSlotRect, hClr ? BTN_CLR_H : BTN_CLR);
        outlineRect(s, mClearSlotRect, hClr ? SDL_Color{220, 100, 100, 255} : SDL_Color{160, 80, 80, 255});
        drawTextCentered(s, "Clear", mClearSlotRect, 13, {255, 180, 180, 255});
    }

    {
        int hintY = mDropZone.y + mDropZone.h + 8;
        drawText(s, "Hitbox Editor — drag corners/edges of the red box",
                 mCenterPanel.x + 4, hintY, 12, {120, 130, 160, 255});

        // Live sprite-local readout (same math as commitHBToProfile)
        const int srcW = (mProfile.spriteW > 0) ? mProfile.spriteW : PREVIEW_W;
        const int srcH = (mProfile.spriteH > 0) ? mProfile.spriteH : PREVIEW_H;
        SDL_Rect nr = normaliseRect(mHBRect);
        int liveX = nr.x - mPreviewRenderRect.x;
        int liveY = nr.y - mPreviewRenderRect.y;
        int liveW = nr.w;
        int liveH = nr.h;
        liveX = std::clamp(liveX, 0, srcW - 1);
        liveY = std::clamp(liveY, 0, srcH - 1);
        liveW = std::clamp(liveW, 1, srcW - liveX);
        liveH = std::clamp(liveH, 1, srcH - liveY);

        std::string spriteStr = "sprite-local: x:" + std::to_string(liveX)
                              + " y:" + std::to_string(liveY)
                              + " w:" + std::to_string(liveW)
                              + " h:" + std::to_string(liveH);
        drawText(s, spriteStr, mCenterPanel.x + 4, hintY + 16, 12, {100, 220, 120, 255});

        // roffX = -hb.x, roffY = -hb.y  (sprite origin offset from collider origin)
        int gameRoffX = -liveX;
        int gameRoffY = -liveY;
        std::string offStr = "roff: x:" + std::to_string(gameRoffX)
                           + " y:" + std::to_string(gameRoffY)
                           + "   col: " + std::to_string(liveW) + "x" + std::to_string(liveH);
        drawText(s, offStr, mCenterPanel.x + 4, hintY + 30, 12, {180, 160, 100, 255});

        std::string slotName = PlayerAnimSlotName(static_cast<PlayerAnimSlot>(mSelectedSlot));
        std::string hbSizeStr = slotName + " hitbox: "
                              + std::to_string(liveW) + "w x " + std::to_string(liveH) + "h"
                              + "  (sprite: " + std::to_string(srcW) + "x" + std::to_string(srcH) + ")";
        drawText(s, hbSizeStr, mCenterPanel.x + 4, hintY + 44, 13, {255, 200, 80, 255});

        int stripY = hintY + 64;
        const auto& stripPrev = mPreviews[mSelectedSlot];
        if (stripPrev.has_value() && !stripPrev->frames.empty()) {
            const int TH    = FRAME_THUMB_SZ;
            const int PAD   = 4;
            const int DELH  = 14;
            const int CELLW = TH + PAD;
            const int CELLH = TH + DELH + PAD + 2;
            const int MAX_VIS_ROWS = 3;
            int nFrames  = (int)stripPrev->frames.size();
            int availW   = mCenterPanel.w - 8;
            int cols     = std::max(1, availW / CELLW);
            int totalRows = (nFrames + cols - 1) / cols;
            int visRows   = std::min(totalRows, MAX_VIS_ROWS);

            std::string hdr = "Frames (" + std::to_string(nFrames) + ")";
            if (totalRows > MAX_VIS_ROWS)
                hdr += "  scroll for more";
            drawText(s, hdr, mCenterPanel.x + 4, stripY, 11, {140, 150, 180, 255});
            stripY += 16;

            SDL_Rect stripBg = {mCenterPanel.x + 2, stripY, availW + 4, visRows * CELLH + PAD};
            fillRect(s, stripBg, {14, 16, 28, 255});
            outlineRect(s, stripBg, {50, 55, 80, 255});

            mFrameDelRects.clear();
            mFrameDelRects.resize(nFrames);
            mFrameDupRects.clear();
            mFrameDupRects.resize(nFrames);

            SDL_Surface* sheet = stripPrev->sheet->GetSurface();
            int startRow = mFrameStripScroll;
            for (int row = 0; row < visRows; ++row) {
                for (int col = 0; col < cols; ++col) {
                    int fi = (startRow + row) * cols + col;
                    if (fi >= nFrames) break;
                    int fx = mCenterPanel.x + 4 + col * CELLW;
                    int fy = stripY + PAD + row * CELLH;

                    const SDL_Rect& fr = stripPrev->frames[fi];
                    SDL_Rect thumbDst = {fx, fy, TH, TH};
                    blitScaledRegion(s, sheet, &fr, thumbDst);

                    if (fi == mAnimFrame)
                        outlineRect(s, thumbDst, {100, 200, 255, 255}, 2);
                    else
                        outlineRect(s, thumbDst, {40, 50, 70, 255});

                    drawText(s, std::to_string(fi + 1), fx + 2, fy + 1, 8, {180, 180, 200, 255});

                    int btnY = fy + TH + 2;
                    int halfW = TH / 2;

                    SDL_Rect dupBtn = {fx, btnY, halfW, DELH};
                    mFrameDupRects[fi] = dupBtn;
                    fillRect(s, dupBtn, {30, 90, 130, 220});
                    outlineRect(s, dupBtn, {60, 150, 200, 255});
                    drawTextCentered(s, "+", dupBtn, 9, {180, 220, 255, 255});

                    SDL_Rect delBtn = {fx + halfW, btnY, TH - halfW, DELH};
                    mFrameDelRects[fi] = delBtn;
                    fillRect(s, delBtn, {120, 30, 30, 220});
                    outlineRect(s, delBtn, {180, 60, 60, 255});
                    drawTextCentered(s, "x", delBtn, 9, {255, 180, 180, 255});
                }
            }
        }
    }

    {
        bool hSave = (mHoverBtn == HoverBtn::Save);
        fillRect(s, mSaveBtnRect, hSave ? BTN_SAVE_H : BTN_SAVE);
        outlineRect(s, mSaveBtnRect, hSave ? SDL_Color{100, 255, 140, 255} : SDL_Color{60, 200, 100, 255});
        drawTextCentered(s, "Save Character", mSaveBtnRect, 16);
    }
    {
        bool hBack = (mHoverBtn == HoverBtn::Back);
        fillRect(s, mBackBtnRect, hBack ? BTN_BACK_H : BTN_BACK);
        outlineRect(s, mBackBtnRect, hBack ? SDL_Color{150, 150, 255, 255} : SDL_Color{100, 100, 200, 255});
        drawTextCentered(s, "< Back", mBackBtnRect, 16);
    }

    // --- Right: Roster panel ---
    fillRect(s, mRosterPanel, PANEL_BG);
    outlineRect(s, mRosterPanel, PANEL_OUT);
    drawText(s, "Saved Characters",
             mRosterPanel.x + 6, mRosterPanel.y + 8, 14, {200, 180, 120, 255});

    int ry = mRosterPanel.y + 34;
    if (mRoster.empty()) {
        drawText(s, "None yet.", mRosterPanel.x + 10, ry + 10, 13, {80, 90, 110, 255});
    } else {
        for (int i = mRosterScroll; i < (int)mRoster.size(); ++i) {
            auto& entry = mRoster[i];
            if (ry + 44 > mRosterPanel.y + mRosterPanel.h) break;
            fillRect(s, {mRosterPanel.x + 4, ry, mRosterPanel.w - 8, 44}, {36, 40, 60, 255});
            drawText(s, entry.name, mRosterPanel.x + 10, ry + 4, 15, {220, 220, 255, 255});
            entry.loadRect = {mRosterPanel.x + 4,              ry + 24, 80, 18};
            entry.delRect  = {mRosterPanel.x + mRosterPanel.w - 54, ry + 24, 50, 18};
            {
                bool hLoad = (mHoverBtn == HoverBtn::RosterLoad && mHoverIndex == i);
                fillRect(s, entry.loadRect, hLoad ? BTN_LOAD_H : BTN_LOAD);
                outlineRect(s, entry.loadRect, hLoad ? SDL_Color{100, 180, 255, 255} : SDL_Color{60, 120, 200, 255});
                drawTextCentered(s, "Load", entry.loadRect, 11);
            }
            {
                bool hDel = (mHoverBtn == HoverBtn::RosterDel && mHoverIndex == i);
                fillRect(s, entry.delRect, hDel ? BTN_DEL_H : BTN_DEL);
                outlineRect(s, entry.delRect, hDel ? SDL_Color{255, 100, 100, 255} : SDL_Color{180, 60, 60, 255});
                drawTextCentered(s, "Delete", entry.delRect, 11, {255, 200, 200, 255});
            }
            ry += 50;
        }
    }

    // Upload to GPU and cache for reuse on non-dirty frames.
    if (mRenderTexture) SDL_DestroyTexture(mRenderTexture);
    mRenderTexture = SDL_CreateTextureFromSurface(ren, s);
    if (mRenderTexture) {
        SDL_SetTextureScaleMode(mRenderTexture, SDL_SCALEMODE_LINEAR);
        SDL_RenderTexture(ren, mRenderTexture, nullptr, nullptr);
    }
    window.Update();
}

// --- NextScene ---

std::unique_ptr<Scene> PlayerCreatorScene::NextScene() {
    if (mGoBack) {
        mGoBack = false;
        return std::make_unique<TitleScene>();
    }
    return nullptr;
}

// --- Preview building ---

void PlayerCreatorScene::rebuildPreview(int slotIdx) {
    clearPreview(slotIdx);
    const std::string& dir = mProfile.slots[slotIdx].folderPath;
    if (dir.empty()) return;

    std::error_code ec;
    bool isDir = fs::is_directory(dir, ec);
    if (!isDir || ec) {
        mDropMsg = "Folder not found (path may be from another machine): " + fs::path(dir).filename().string();
        mProfile.slots[slotIdx].folderPath.clear();
        return;
    }

    std::vector<fs::path> pngs;
    try {
        for (const auto& e : fs::directory_iterator(dir))
            if (e.path().extension() == ".png" || e.path().extension() == ".PNG")
                pngs.push_back(e.path());
    } catch (const std::exception& ex) {
        mDropMsg = std::string("Cannot read folder: ") + ex.what();
        return;
    }
    if (pngs.empty()) return;
    std::sort(pngs.begin(), pngs.end());

    // Probe dimensions from the first frame without a redundant load.
    // GameScene's raw surface cache may already have this file.
    SDL_Surface* firstSurf = GameScene::GetRawSurface(pngs[0].string());
    if (!firstSurf) {
        mDropMsg = "Failed to load: " + pngs[0].filename().string();
        return;
    }
    int fw = firstSurf->w, fh = firstSurf->h;

    // Auto-fill spriteW/H from raw frame size so the hitbox editor is
    // immediately accurate without requiring manual size entry.
    if (mProfile.spriteW <= 0) {
        mProfile.spriteW = fw;
        mWidthStr = std::to_string(fw);
    }
    if (mProfile.spriteH <= 0) {
        mProfile.spriteH = fh;
        mHeightStr = std::to_string(fh);
    }

    try {
        const int tW = (mProfile.spriteW > 0) ? mProfile.spriteW : 0;
        const int tH = (mProfile.spriteH > 0) ? mProfile.spriteH : 0;
        // Explicit path-list constructor loads PNGs in alphabetical order,
        // matching GameScene's loadSlot behaviour for shared folders.
        std::vector<std::string> pathStrs;
        pathStrs.reserve(pngs.size());
        for (const auto& p : pngs) pathStrs.push_back(p.string());
        auto ss = std::make_unique<SpriteSheet>(pathStrs, tW, tH);
        auto frames = ss->GetAnimation("");
        if (!frames.empty()) {
            SlotPreview p;
            p.frameW = fw;
            p.frameH = fh;
            p.frames = std::move(frames);
            p.paths  = std::move(pathStrs);
            p.sheet  = std::move(ss);
            mPreviews[slotIdx] = std::move(p);
        }
    } catch (const std::exception& ex) {
        mDropMsg = std::string("Load error: ") + ex.what();
    }

    mAnimFrame = 0;
    mAnimTimer = 0;
}

void PlayerCreatorScene::clearPreview(int slotIdx) {
    mPreviews[slotIdx].reset();
    if (mSelectedSlot == slotIdx) {
        mAnimFrame = 0;
        mAnimTimer = 0;
    }
}

void PlayerCreatorScene::deleteFrame(int slotIdx, int frameIdx) {
    auto& prev = mPreviews[slotIdx];
    if (!prev.has_value() || frameIdx < 0 || frameIdx >= (int)prev->paths.size())
        return;

    const std::string& path = prev->paths[frameIdx];
    std::error_code ec;
    fs::remove(path, ec);
    if (ec) {
        mDropMsg = "Failed to delete: " + fs::path(path).filename().string();
        return;
    }
    mDropMsg = "Deleted: " + fs::path(path).filename().string();

    rebuildPreview(slotIdx);
    recomputePreviewRect();
    initHBFromProfile(slotIdx);

    if (mPreviews[slotIdx].has_value()) {
        int n = (int)mPreviews[slotIdx]->frames.size();
        if (mAnimFrame >= n) mAnimFrame = std::max(0, n - 1);
    } else {
        mAnimFrame = 0;
    }
    mFrameStripScroll = 0;
}

void PlayerCreatorScene::duplicateFrame(int slotIdx, int frameIdx) {
    auto& prev = mPreviews[slotIdx];
    if (!prev.has_value() || frameIdx < 0 || frameIdx >= (int)prev->paths.size())
        return;

    const fs::path src(prev->paths[frameIdx]);
    const fs::path dir  = src.parent_path();
    const std::string stem = src.stem().string();
    const std::string ext  = src.extension().string();

    fs::path dst;
    for (int n = 1; ; ++n) {
        dst = dir / (stem + "_dup" + std::to_string(n) + ext);
        if (!fs::exists(dst)) break;
    }

    std::error_code ec;
    fs::copy_file(src, dst, ec);
    if (ec) {
        mDropMsg = "Failed to duplicate: " + src.filename().string();
        return;
    }
    mDropMsg = "Duplicated: " + dst.filename().string();

    rebuildPreview(slotIdx);
    recomputePreviewRect();
    initHBFromProfile(slotIdx);
    mFrameStripScroll = 0;
}

// --- Hitbox editor ---

void PlayerCreatorScene::recomputePreviewRect() {
    const int srcW = (mProfile.spriteW > 0) ? mProfile.spriteW : PREVIEW_W;
    const int srcH = (mProfile.spriteH > 0) ? mProfile.spriteH : PREVIEW_H;

    // Anchor preview top relative to centre panel, not the old (possibly stale) cell rect
    const int previewTop = mCenterPanel.y + 126;
    const int MID_W      = mCenterPanel.w;

    const int cellW = srcW + PREVIEW_PAD * 2;
    const int cellH = srcH + PREVIEW_PAD * 2;
    mPreviewCellRect = {mCenterPanel.x + (MID_W - cellW) / 2, previewTop, cellW, cellH};

    mPreviewRenderRect = {mPreviewCellRect.x + PREVIEW_PAD,
                          mPreviewCellRect.y + PREVIEW_PAD,
                          srcW, srcH};

    mDropZone      = {mCenterPanel.x, mPreviewCellRect.y + cellH + 14, MID_W - 2, 56};
    mClearSlotRect = {mDropZone.x + mDropZone.w - 100, mDropZone.y, 96, mDropZone.h};
}

void PlayerCreatorScene::initHBFromProfile(int slotIdx) {
    // Game model: Transform = collider top-left, roffX = -hb.x, roffY = -hb.y,
    // physics floor = collider bottom. Editor mirrors this at 1:1 scale.

    const int srcW = (mProfile.spriteW > 0) ? mProfile.spriteW : PREVIEW_W;
    const int srcH = (mProfile.spriteH > 0) ? mProfile.spriteH : PREVIEW_H;
    const int cellX = (mPreviewCellRect.w > 0) ? mPreviewCellRect.x : 0;
    const int cellW = (mPreviewCellRect.w > 0) ? mPreviewCellRect.w : PREVIEW_W;
    const int cellH = (mPreviewCellRect.h > 0) ? mPreviewCellRect.h : PREVIEW_H;
    const int floorY = (mPreviewCellRect.h > 0)
                       ? mPreviewCellRect.y + cellH
                       : mPreviewRenderRect.y + mPreviewRenderRect.h;

    const float scale = 1.0f;
    const int   drawW = srcW;
    const int   drawH = srcH;

    const auto& hb = mProfile.slots[slotIdx].hitbox;
    int hbX, hbY, hbW, hbH;
    if (!hb.IsDefault()) {
        hbX = hb.x;  hbY = hb.y;  hbW = hb.w;  hbH = hb.h;
    } else {
        // Proportional frost-knight insets as fallback
        const float baseW = 120.0f, baseH = 160.0f;
        hbX = (int)std::round(32.0f * srcW / baseW);
        hbY = (int)std::round(33.0f * srcH / baseH);
        hbW = (int)std::round(srcW - 2.0f * 32.0f * srcW / baseW);
        hbH = (int)std::round(srcH - 33.0f * srcH / baseH - 26.0f * srcH / baseH);
    }

    const int spriteX = mPreviewCellRect.x + PREVIEW_PAD;
    const int spriteY = mPreviewCellRect.y + PREVIEW_PAD;
    mPreviewRenderRect = {spriteX, spriteY, drawW, drawH};

    const int colW_px  = (int)std::round(hbW * scale);
    const int colH_px  = (int)std::round(hbH * scale);
    const int colScreenX = spriteX + (int)std::round(hbX * scale);
    const int colScreenY = spriteY + (int)std::round(hbY * scale);

    mHBRect = {colScreenX, colScreenY, colW_px, colH_px};
    mHBInitialised = true;
}

void PlayerCreatorScene::commitHBToProfile(int slotIdx) {
    if (!mHBInitialised) return;
    SDL_Rect nr = normaliseRect(mHBRect);
    auto& hb = mProfile.slots[slotIdx].hitbox;

    // mPreviewRenderRect = stable sprite origin (set once by initHBFromProfile,
    // never overwritten by drags or Render). Only the collider box moves.

    const int srcW = (mProfile.spriteW > 0) ? mProfile.spriteW : PREVIEW_W;
    const int srcH = (mProfile.spriteH > 0) ? mProfile.spriteH : PREVIEW_H;
    const float scale    = 1.0f;
    const float invScale = 1.0f;

    int localX = nr.x - mPreviewRenderRect.x;
    int localY = nr.y - mPreviewRenderRect.y;

    hb.x = (int)std::round(localX * invScale);
    hb.y = (int)std::round(localY * invScale);
    hb.w = std::max(1, (int)std::round(nr.w * invScale));
    hb.h = std::max(1, (int)std::round(nr.h * invScale));

    hb.x = std::clamp(hb.x, 0, srcW - 1);
    hb.y = std::clamp(hb.y, 0, srcH - 1);
    hb.w = std::clamp(hb.w, 1, srcW - hb.x);
    hb.h = std::clamp(hb.h, 1, srcH - hb.y);
}

int PlayerCreatorScene::hitboxHandleAt(int mx, int my) const {
    if (!mHBInitialised) return -1;
    SDL_Rect r = normaliseRect(mHBRect);
    const int HSZ = HB_HANDLE_SZ;

    SDL_Rect corners[4] = {
        {r.x - HSZ/2,           r.y - HSZ/2,           HSZ, HSZ},
        {r.x + r.w - HSZ/2,     r.y - HSZ/2,           HSZ, HSZ},
        {r.x + r.w - HSZ/2,     r.y + r.h - HSZ/2,     HSZ, HSZ},
        {r.x - HSZ/2,           r.y + r.h - HSZ/2,     HSZ, HSZ},
    };
    for (int i = 0; i < 4; ++i)
        if (hit(corners[i], mx, my)) return i;

    if (hit(r, mx, my)) return 4;
    return -1;
}

void PlayerCreatorScene::renderHitboxOverlay(SDL_Surface* surf) const {
    if (!mHBInitialised) return;
    SDL_Rect r = normaliseRect(mHBRect);

    // Semi-transparent fill clipped to the cell
    {
        SDL_Rect pr = mPreviewCellRect;
        SDL_Rect fill = r;
        fill.x = std::max(fill.x, pr.x);
        fill.y = std::max(fill.y, pr.y);
        int fillRight  = std::min(r.x + r.w, pr.x + pr.w);
        int fillBottom = std::min(r.y + r.h, pr.y + pr.h);
        fill.w = fillRight  - fill.x;
        fill.h = fillBottom - fill.y;
        if (fill.w > 0 && fill.h > 0) {
            const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(surf->format);
            SDL_Surface* overlay = SDL_CreateSurface(fill.w, fill.h, SDL_PIXELFORMAT_ARGB8888);
            if (overlay) {
                SDL_SetSurfaceBlendMode(overlay, SDL_BLENDMODE_BLEND);
                SDL_FillSurfaceRect(overlay, nullptr,
                    SDL_MapRGBA(fmt, nullptr, HB_COLOR.r, HB_COLOR.g, HB_COLOR.b, 50));
                SDL_BlitSurface(overlay, nullptr, surf, &fill);
                SDL_DestroySurface(overlay);
            }
        }
    }

    outlineRect(surf, r, HB_COLOR, 2);

    {
        int cx = r.x + r.w / 2;
        int cy = r.y + r.h / 2;
        const int CL = 6;
        SDL_Rect hLine = {cx - CL, cy, CL * 2 + 1, 1};
        SDL_Rect vLine = {cx,      cy - CL, 1, CL * 2 + 1};
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(surf->format);
        Uint32 col = SDL_MapRGBA(fmt, nullptr, 255, 255, 80, 220);
        SDL_FillSurfaceRect(surf, &hLine, col);
        SDL_FillSurfaceRect(surf, &vLine, col);
    }

    const int HSZ = HB_HANDLE_SZ;
    SDL_Rect corners[4] = {
        {r.x - HSZ/2,       r.y - HSZ/2,       HSZ, HSZ},
        {r.x + r.w - HSZ/2, r.y - HSZ/2,       HSZ, HSZ},
        {r.x + r.w - HSZ/2, r.y + r.h - HSZ/2, HSZ, HSZ},
        {r.x - HSZ/2,       r.y + r.h - HSZ/2, HSZ, HSZ},
    };
    for (auto& c : corners) {
        fillRect(surf, c, {255, 220, 80, 255});
        outlineRect(surf, c, {180, 150, 40, 255});
    }
}

// --- Roster ---

void PlayerCreatorScene::refreshRoster() {
    mRoster.clear();
    for (const auto& p : ScanPlayerProfiles()) {
        RosterEntry e;
        e.name = p.stem().string();
        e.path = p.string();
        mRoster.push_back(std::move(e));
    }
    mRosterScroll = 0;
}

void PlayerCreatorScene::loadRosterEntry(int idx) {
    if (idx < 0 || idx >= (int)mRoster.size()) return;
    PlayerProfile loaded;
    if (LoadPlayerProfile(mRoster[idx].path, loaded)) {
        if (mHBInitialised && mPreviewCellRect.w > 0)
            commitHBToProfile(mSelectedSlot);
        for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) mPreviews[i].reset();
        mProfile = std::move(loaded);
        mLoadedName  = mProfile.name;
        mWidthStr    = mProfile.spriteW > 0 ? std::to_string(mProfile.spriteW) : "120";
        mHeightStr   = mProfile.spriteH > 0 ? std::to_string(mProfile.spriteH) : "160";
        computeLayout();
        // Only rebuild the selected slot's preview now; others load lazily.
        if (!mProfile.slots[0].folderPath.empty())
            rebuildPreview(0);
        mSelectedSlot = 0;
        mSelectedSfxFile = 0;
        recomputePreviewRect();
        initHBFromProfile(0);
        mAnimFrame = 0; mAnimTimer = 0;
        mDropMsg = "Loaded: " + mRoster[idx].name;
    }
}

// --- Text input ---

void PlayerCreatorScene::startTextInput() {
    if (!mNameActive) {
        mNameActive = true;
        SDL_StartTextInput(mSDLWin);
    }
}

void PlayerCreatorScene::stopTextInput() {
    if (mNameActive) {
        mNameActive = false;
        SDL_StopTextInput(mSDLWin);
    }
}

// --- Draw helpers ---

void PlayerCreatorScene::fillRect(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
    static const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_ARGB8888);
    SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
}

void PlayerCreatorScene::outlineRect(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t) {
    static const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(SDL_PIXELFORMAT_ARGB8888);
    Uint32 col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
    SDL_Rect sides[4] = {{r.x,       r.y,       r.w, t},
                         {r.x,       r.y+r.h-t, r.w, t},
                         {r.x,       r.y,       t,   r.h},
                         {r.x+r.w-t, r.y,       t,   r.h}};
    for (auto& sr : sides) SDL_FillSurfaceRect(s, &sr, col);
}

SDL_Surface* PlayerCreatorScene::getCachedText(const std::string& str, int ptSize, SDL_Color col) {
    Uint32 packed = ((Uint32)col.r << 24) | ((Uint32)col.g << 16) | ((Uint32)col.b << 8) | col.a;
    TextCacheKey key{str, ptSize, packed};
    auto it = mTextCache.find(key);
    if (it != mTextCache.end()) return it->second;
    TTF_Font* font = FontCache::Get(ptSize);
    if (!font) return nullptr;
    SDL_Surface* ts = TTF_RenderText_Blended(font, str.c_str(), 0, col);
    mTextCache[key] = ts;
    return ts;
}

void PlayerCreatorScene::drawText(SDL_Surface* s, const std::string& str,
                                   int x, int y, int ptSize, SDL_Color col) {
    if (str.empty()) return;
    SDL_Surface* ts = getCachedText(str, ptSize, col);
    if (ts) { SDL_Rect dst = {x, y, ts->w, ts->h}; SDL_BlitSurface(ts, nullptr, s, &dst); }
}

void PlayerCreatorScene::drawTextCentered(SDL_Surface* s, const std::string& str,
                                           SDL_Rect r, int ptSize, SDL_Color col) {
    if (str.empty()) return;
    auto [tx, ty] = Text::CenterInRect(str, ptSize, r);
    drawText(s, str, tx, ty, ptSize, col);
}

void PlayerCreatorScene::blitScaled(SDL_Surface* dst, SDL_Surface* src, SDL_Rect dstRect) {
    if (!src || !dst) return;
    blitScaledRegion(dst, src, nullptr, dstRect);
}

void PlayerCreatorScene::blitScaledRegion(SDL_Surface* dst, SDL_Surface* src,
                                          const SDL_Rect* srcRect, SDL_Rect dstRect) {
    if (!src || !dst) return;
    int srcW = srcRect ? srcRect->w : src->w;
    int srcH = srcRect ? srcRect->h : src->h;
    float scaleX = (float)dstRect.w / srcW;
    float scaleY = (float)dstRect.h / srcH;
    float scale  = std::min(scaleX, scaleY);
    int   w      = (int)(srcW * scale);
    int   h      = (int)(srcH * scale);
    int   ox     = dstRect.x + (dstRect.w - w) / 2;
    int   oy     = dstRect.y + (dstRect.h - h) / 2;
    SDL_Rect dst2 = {ox, oy, w, h};
    SDL_BlitSurfaceScaled(src, srcRect, dst, &dst2, SDL_SCALEMODE_PIXELART);
}

SDL_Rect PlayerCreatorScene::normaliseRect(SDL_Rect r) {
    if (r.w < 0) { r.x += r.w; r.w = -r.w; }
    if (r.h < 0) { r.y += r.h; r.h = -r.h; }
    return r;
}
