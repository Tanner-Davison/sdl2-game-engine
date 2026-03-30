#include "EnemyCreatorScene.hpp"
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

// --- Scene interface ---

void EnemyCreatorScene::Load(Window& window) {
    mW      = window.GetWidth();
    mH      = window.GetHeight();
    mSDLWin = window.GetRaw();

    if (!fs::exists("enemies"))
        fs::create_directory("enemies");

    mProfile = EnemyProfile{};
    mProfile.name = "MyEnemy";

    computeLayout();
    recomputePreviewRect();
    refreshRoster();
    initHBFromProfile(mSelectedSlot);
}

void EnemyCreatorScene::Unload() {
    if (Audio() && mPreviewPlaying)
        Audio()->Sfx().StopPreview();
    mPreviewPlaying = false;
    stopTextInput();
}

// --- Layout ---

void EnemyCreatorScene::computeLayout() {
    const int PAD   = PANEL_PAD;
    const int TOTAL = mW;
    const int H     = mH;

    const int LEFT_W   = 240;
    const int RIGHT_W  = 260;
    const int MID_W    = TOTAL - LEFT_W - RIGHT_W - PAD * 4;

    mSlotPanel   = {PAD,                     40,  LEFT_W,  H - 80};
    mCenterPanel = {PAD * 2 + LEFT_W,        40,  MID_W,   H - 80};
    mRosterPanel = {PAD * 3 + LEFT_W + MID_W, 40, RIGHT_W, H - 80};

    mNameFieldRect = {mCenterPanel.x, mCenterPanel.y + 40, MID_W - 2, 36};

    const int sfW = (MID_W - 6) / 2;
    mWidthRect  = {mCenterPanel.x,           mCenterPanel.y + 96, sfW, 32};
    mHeightRect = {mCenterPanel.x + sfW + 6, mCenterPanel.y + 96, sfW, 32};

    mSpeedRect  = {mCenterPanel.x,           mCenterPanel.y + 148, sfW, 32};
    mHealthRect = {mCenterPanel.x + sfW + 6, mCenterPanel.y + 148, sfW, 32};

    mSaveBtnRect = {mCenterPanel.x,                mCenterPanel.y + mCenterPanel.h - 50, 130, 40};
    mBackBtnRect = {mCenterPanel.x + MID_W - 130,  mCenterPanel.y + mCenterPanel.h - 50, 130, 40};

    mPreviewCellRect   = {};
    mPreviewRenderRect = {};
    mDropZone          = {};
    mClearSlotRect     = {};

    mSlotRowRects.resize(ENEMY_ANIM_SLOT_COUNT);
    int ry = mSlotPanel.y + 40;
    for (int i = 0; i < ENEMY_ANIM_SLOT_COUNT; ++i) {
        mSlotRowRects[i] = {mSlotPanel.x + 4, ry, LEFT_W - 8, SLOT_ROW_H - 4};
        const int BW = 18;
        const int BH = 16;
        const int BY = ry + (SLOT_ROW_H - 4 - BH) / 2;
        mFpsBtns[i].plusRect  = {mSlotPanel.x + LEFT_W - 8 - BW,      BY, BW, BH};
        mFpsBtns[i].minusRect = {mSlotPanel.x + LEFT_W - 8 - BW*2 - 2, BY, BW, BH};

        static constexpr int SFX_NAME_H   = 14;
        static constexpr int SFX_SLIDER_H = 10;
        static constexpr int SFX_TRIM_H   = 10;
        static constexpr int SFX_ENTRY_H  = SFX_NAME_H + SFX_SLIDER_H + SFX_TRIM_H;
        static constexpr int SFX_DROP_H   = 16;
        static constexpr int BASE_H       = 44;
        const int sfxW   = LEFT_W - 16;
        const int clearW = 14;
        const int tsW    = 16;
        const int btnsW  = clearW + tsW + 3;

        int numSfx = (int)mProfile.slots[i].sfx.size();
        int rowH   = BASE_H + numSfx * SFX_ENTRY_H + SFX_DROP_H;
        mSlotRowRects[i].h = rowH - 4;

        mSfxFileUI[i].clear();
        mSfxFileUI[i].resize(numSfx);
        for (int fi = 0; fi < numSfx; ++fi) {
            int entryY = ry + BASE_H + fi * SFX_ENTRY_H;
            mSfxFileUI[i][fi].nameRect    = {mSlotPanel.x + 8, entryY, sfxW - btnsW - 2, SFX_NAME_H};
            int bx2 = mSlotPanel.x + 8 + sfxW - btnsW;
            mSfxFileUI[i][fi].stretchRect = {bx2, entryY, tsW, SFX_NAME_H};
            mSfxFileUI[i][fi].clearRect   = {mSlotPanel.x + 8 + sfxW - clearW, entryY, clearW, SFX_NAME_H};
            mSfxFileUI[i][fi].sliderRect     = {mSlotPanel.x + 8, entryY + SFX_NAME_H, sfxW, SFX_SLIDER_H};
            mSfxFileUI[i][fi].trimSliderRect = {mSlotPanel.x + 8, entryY + SFX_NAME_H + SFX_SLIDER_H, sfxW, SFX_TRIM_H};
        }
        int dropY = ry + BASE_H + numSfx * SFX_ENTRY_H;
        mSfxDropRect[i] = {mSlotPanel.x + 8, dropY, sfxW, SFX_DROP_H};

        ry += rowH;
    }

    recomputePreviewRect();
}

// --- Events ---

bool EnemyCreatorScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) return false;

    if (e.type == SDL_EVENT_DROP_BEGIN) { mDropHover = true; return true; }
    if (e.type == SDL_EVENT_DROP_COMPLETE) { mDropHover = false; return true; }

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
                for (int i = 0; i < ENEMY_ANIM_SLOT_COUNT; ++i) {
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
                auto slotName = EnemyAnimSlotName(static_cast<EnemyAnimSlot>(targetSlot));
                mDropMsg = std::string(slotName) + " SFX " + std::to_string(sfx.size())
                          + ": " + p.filename().string();
                return true;
            }

            std::string dir;

            if (fs::is_directory(p)) {
                dir = dropped;
            } else if (fs::is_regular_file(p)) {
                auto ext = p.extension().string();
                if (ext == ".png" || ext == ".PNG") {
                    dir = p.parent_path().string();
                }
            }

            if (!dir.empty()) {
                bool hasPng = false;
                try {
                    for (const auto& entry : fs::directory_iterator(dir)) {
                        auto ext = entry.path().extension().string();
                        if (ext == ".png" || ext == ".PNG") { hasPng = true; break; }
                    }
                } catch (...) {}

                if (hasPng) {
                    mProfile.Slot(static_cast<EnemyAnimSlot>(mSelectedSlot)).folderPath = dir;
                    rebuildPreview(mSelectedSlot);
                    recomputePreviewRect();
                    initHBFromProfile(mSelectedSlot);
                    mDropMsg = "Loaded: " + fs::path(dir).filename().string();
                } else {
                    mDropMsg = "No PNGs found in that folder.";
                }
            } else {
                mDropMsg = "Drop a PNG or folder of PNGs here.";
            }
        }
        return true;
    }

    if (mNumFieldActive != NumField::None) {
        std::string* str = nullptr;
        switch (mNumFieldActive) {
            case NumField::Width:  str = &mWidthStr;  break;
            case NumField::Height: str = &mHeightStr; break;
            case NumField::Speed:  str = &mSpeedStr;  break;
            case NumField::Health: str = &mHealthStr; break;
            default: break;
        }
        if (!str) { mNumFieldActive = NumField::None; return true; }

        if (e.type == SDL_EVENT_TEXT_INPUT) {
            for (char c : std::string(e.text.text))
                if ((std::isdigit((unsigned char)c) || c == '.') && str->size() < 6)
                    str->push_back(c);
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_BACKSPACE && !str->empty()) { str->pop_back(); return true; }
            if (e.key.key == SDLK_RETURN || e.key.key == SDLK_ESCAPE) {
                float val = str->empty() ? 0.0f : std::stof(*str);
                switch (mNumFieldActive) {
                    case NumField::Width:  mProfile.spriteW = (int)val; break;
                    case NumField::Height: mProfile.spriteH = (int)val; break;
                    case NumField::Speed:  mProfile.speed   = val;      break;
                    case NumField::Health: mProfile.health  = val;      break;
                    default: break;
                }
                mNumFieldActive = NumField::None;
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
            for (char c : std::string(e.text.text))
                if (std::isalnum((unsigned char)c) || c == '-' || c == '_')
                    mProfile.name += c;
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

    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_DOWN || e.key.key == SDLK_S) {
            if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
            mSelectedSlot = (mSelectedSlot + 1) % ENEMY_ANIM_SLOT_COUNT;
            mSelectedSfxFile = 0;
            initHBFromProfile(mSelectedSlot);
            mAnimFrame = 0; mAnimTimer = 0; mFrameStripScroll = 0;
        }
        if (e.key.key == SDLK_UP || e.key.key == SDLK_W) {
            if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
            mSelectedSlot = (mSelectedSlot - 1 + ENEMY_ANIM_SLOT_COUNT) % ENEMY_ANIM_SLOT_COUNT;
            mSelectedSfxFile = 0;
            initHBFromProfile(mSelectedSlot);
            mAnimFrame = 0; mAnimTimer = 0; mFrameStripScroll = 0;
        }
        if (e.key.key == SDLK_ESCAPE) { mGoBack = true; }
    }

    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        int mx = (int)e.motion.x, my = (int)e.motion.y;
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
            int dx = mx - mHBDragStartMX, dy = my - mHBDragStartMY;
            SDL_Rect r = mHBDragStartRect;
            switch (mHBDragHandle) {
                case 0: r.x += dx; r.y += dy; r.w -= dx; r.h -= dy; break;
                case 1: r.y += dy; r.w += dx; r.h -= dy; break;
                case 2: r.w += dx; r.h += dy; break;
                case 3: r.x += dx; r.w -= dx; r.h += dy; break;
                case 4: r.x += dx; r.y += dy; break;
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
            for (int i = 0; i < ENEMY_ANIM_SLOT_COUNT; ++i) {
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
        int mx = (int)e.button.x, my = (int)e.button.y;

        for (int i = 0; i < ENEMY_ANIM_SLOT_COUNT; ++i) {
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

        for (int i = 0; i < ENEMY_ANIM_SLOT_COUNT; ++i) {
            auto slot = static_cast<EnemyAnimSlot>(i);
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

        for (int i = 0; i < ENEMY_ANIM_SLOT_COUNT; ++i) {
            if (hit(mSlotRowRects[i], mx, my)) {
                if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
                mSelectedSlot = i;
                mSelectedSfxFile = 0;
                initHBFromProfile(mSelectedSlot);
                mAnimFrame = 0; mAnimTimer = 0; mFrameStripScroll = 0;
                return true;
            }
        }

        auto activateNumField = [&](NumField f, const SDL_Rect& r, std::string& s, auto getter) {
            if (hit(r, mx, my)) {
                s = std::to_string((int)getter());
                mNumFieldActive = f;
                if (mNameActive) stopTextInput();
                SDL_StartTextInput(mSDLWin);
                return true;
            }
            return false;
        };
        if (activateNumField(NumField::Width,  mWidthRect,  mWidthStr,  [&]{ return (float)mProfile.spriteW; })) return true;
        if (activateNumField(NumField::Height, mHeightRect, mHeightStr, [&]{ return (float)mProfile.spriteH; })) return true;
        if (hit(mSpeedRect, mx, my)) {
            mSpeedStr = std::to_string((int)mProfile.speed);
            mNumFieldActive = NumField::Speed;
            if (mNameActive) stopTextInput();
            SDL_StartTextInput(mSDLWin);
            return true;
        }
        if (hit(mHealthRect, mx, my)) {
            mHealthStr = std::to_string((int)mProfile.health);
            mNumFieldActive = NumField::Health;
            if (mNameActive) stopTextInput();
            SDL_StartTextInput(mSDLWin);
            return true;
        }

        if (hit(mNameFieldRect, mx, my)) {
            mProfile.name.clear();
            mNameError.clear();
            mNumFieldActive = NumField::None;
            startTextInput();
            return true;
        } else {
            if (mNameActive) stopTextInput();
            if (mNumFieldActive != NumField::None) {
                std::string* str = nullptr;
                switch (mNumFieldActive) {
                    case NumField::Width:  str = &mWidthStr;  break;
                    case NumField::Height: str = &mHeightStr; break;
                    case NumField::Speed:  str = &mSpeedStr;  break;
                    case NumField::Health: str = &mHealthStr; break;
                    default: break;
                }
                if (str) {
                    float val = str->empty() ? 0.0f : std::stof(*str);
                    switch (mNumFieldActive) {
                        case NumField::Width:  mProfile.spriteW = (int)val; break;
                        case NumField::Height: mProfile.spriteH = (int)val; break;
                        case NumField::Speed:  mProfile.speed   = val;      break;
                        case NumField::Health: mProfile.health  = val;      break;
                        default: break;
                    }
                }
                mNumFieldActive = NumField::None;
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
                if (!fs::exists("enemies")) fs::create_directory("enemies");
                if (!mLoadedName.empty() && mLoadedName != mProfile.name) {
                    std::error_code ec;
                    fs::remove(EnemyProfilePath(mLoadedName), ec);
                }
                SaveEnemyProfile(mProfile, EnemyProfilePath(mProfile.name));
                mLoadedName = mProfile.name;
                refreshRoster();
                mNameError.clear();
                mDropMsg = "Saved as: " + mProfile.name;
            }
            return true;
        }

        if (hit(mClearSlotRect, mx, my)) {
            auto& slot = mProfile.Slot(static_cast<EnemyAnimSlot>(mSelectedSlot));
            slot.folderPath.clear();
            slot.hitbox = {};
            slot.sfx.clear();
            clearPreview(mSelectedSlot);
            initHBFromProfile(mSelectedSlot);
            mDropMsg = "Slot cleared.";
            return true;
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

        for (int i = mRosterScroll; i < (int)mRoster.size(); ++i) {
            auto& entry = mRoster[i];
            if (hit(entry.loadRect, mx, my)) { loadRosterEntry(i); return true; }
            if (hit(entry.delRect, mx, my)) {
                try { fs::remove(entry.path); } catch (...) {}
                refreshRoster();
                return true;
            }
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
        if (mVolDragSlot >= 0) {
            mVolDragSlot = -1;
            mVolDragFile = -1;
        }
        if (mTrimDragSlot >= 0) {
            mTrimDragSlot   = -1;
            mTrimDragFile   = -1;
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
                int maxScrollRow = std::max(0, totalRows - 3);
                mFrameStripScroll = std::clamp(mFrameStripScroll - (int)e.wheel.y,
                                               0, maxScrollRow);
            }
        }
    }

    return true;
}

// --- Update ---

void EnemyCreatorScene::Update(float dt) {
    const auto& prev = mPreviews[mSelectedSlot];
    if (prev.has_value() && !prev->frames.empty()) {
        float slotFps = mProfile.Slot(static_cast<EnemyAnimSlot>(mSelectedSlot)).fps;
        float previewFps = (slotFps > 0.0f) ? slotFps : ANIM_FPS;
        mAnimTimer += dt;
        if (mAnimTimer >= 1.0f / previewFps) {
            mAnimTimer = 0.0f;
            mAnimFrame = (mAnimFrame + 1) % (int)prev->frames.size();
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

    int fileIdx = std::clamp(mSelectedSfxFile, 0, (int)slotSfx.size() - 1);
    const auto& entry = slotSfx[fileIdx];
    std::string wantId = audio::EnemySfxId(mProfile.name, mSelectedSlot, fileIdx);

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

    float totalDur = sfx.GetDuration(mPreviewSfxId);
    if (totalDur <= 0.0f) return;

    float tStart = entry.trimStart * totalDur;
    float tEnd   = entry.trimEnd   * totalDur;
    if (tEnd <= tStart) return;

    mPreviewTimer += dt;

    if (!mPreviewPlaying || mPreviewTimer >= totalDur) {
        sfx.PlayPreview(mPreviewSfxId, entry.volume);
        mPreviewPlaying = true;
        mPreviewTimer = 0.0f;
    }

    bool inWindow = (mPreviewTimer >= tStart && mPreviewTimer < tEnd);
    sfx.SetPreviewGain(inWindow ? entry.volume : 0.0f);
}

// --- Render ---

void EnemyCreatorScene::Render(Window& window, float /*alpha*/) {
    window.Render();
    SDL_Renderer* ren = window.GetRenderer();
    SDL_Surface* s = SDL_CreateSurface(mW, mH, SDL_PIXELFORMAT_ARGB8888);
    if (!s) { window.Update(); return; }

    fillRect(s, {0, 0, mW, mH}, BG);
    drawTextCentered(s, "Enemy Creator", {0, 4, mW, 32}, 24, {255, 180, 160, 255});

    // --- Left: Slot panel ---
    fillRect(s, mSlotPanel, PANEL_BG);
    outlineRect(s, mSlotPanel, PANEL_OUT);
    drawText(s, "Animation Slots", mSlotPanel.x + 6, mSlotPanel.y + 8, 14, {220, 160, 140, 255});

    for (int i = 0; i < ENEMY_ANIM_SLOT_COUNT; ++i) {
        auto slot  = static_cast<EnemyAnimSlot>(i);
        bool sel   = (i == mSelectedSlot);
        bool filled = mProfile.HasSlot(slot);

        bool hRow = (!sel && mHoverBtn == HoverBtn::SlotRow && mHoverIndex == i);
        SDL_Color bg = sel ? SEL_BG : hRow ? SDL_Color{70, 60, 65, 255} : (filled ? SLOT_FILLED : SLOT_EMPTY);
        fillRect(s, mSlotRowRects[i], bg);
        outlineRect(s, mSlotRowRects[i], sel ? SDL_Color{255, 120, 100, 255}
                                        : hRow ? SDL_Color{180, 100, 80, 255} : PANEL_OUT);

        drawText(s, EnemyAnimSlotName(slot),
                 mSlotRowRects[i].x + 8,
                 mSlotRowRects[i].y + (SLOT_ROW_H - 20) / 2,
                 16, sel ? SDL_Color{255, 255, 255, 255} : SDL_Color{210, 180, 170, 255});

        const auto& path = mProfile.Slot(slot).folderPath;
        if (!path.empty()) {
            std::string fname = fs::path(path).filename().string();
            if ((int)fname.size() > 16) fname = fname.substr(0, 14) + "..";
            drawText(s, fname, mSlotRowRects[i].x + 90,
                     mSlotRowRects[i].y + (SLOT_ROW_H - 14) / 2, 11, {220, 180, 120, 255});
        } else {
            drawText(s, "-- empty --", mSlotRowRects[i].x + 90,
                     mSlotRowRects[i].y + (SLOT_ROW_H - 14) / 2, 11, {80, 90, 100, 255});
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
            drawText(s, fpsStr, labelX, mFpsBtns[i].minusRect.y + 2, 10, {160, 170, 200, 255});
        }

        for (int fi = 0; fi < (int)mSfxFileUI[i].size(); ++fi) {
            const auto& ui = mSfxFileUI[i][fi];
            const auto& entry = mProfile.slots[i].sfx[fi];

            bool isSelected = (i == mSelectedSlot && fi == mSelectedSfxFile);
            fillRect(s, ui.nameRect, isSelected ? SDL_Color{50, 65, 95, 255} : SDL_Color{35, 55, 80, 255});
            outlineRect(s, ui.nameRect, isSelected ? SDL_Color{200, 140, 40, 255} : SDL_Color{60, 100, 160, 255});
            std::string fname = fs::path(entry.path).filename().string();
            if ((int)fname.size() > 20) fname = fname.substr(0, 18) + "..";
            drawText(s, fname, ui.nameRect.x + 3, ui.nameRect.y + 2, 9, {120, 190, 255, 255});

            bool ts = entry.timeStretch;
            SDL_Color tsBg  = ts ? SDL_Color{40, 80, 50, 255}  : SDL_Color{40, 35, 55, 255};
            SDL_Color tsOut = ts ? SDL_Color{80, 180, 100, 255} : SDL_Color{60, 55, 80, 255};
            SDL_Color tsFg  = ts ? SDL_Color{140, 255, 160, 255}: SDL_Color{100, 90, 130, 255};
            fillRect(s, ui.stretchRect, tsBg);
            outlineRect(s, ui.stretchRect, tsOut);
            drawTextCentered(s, "TS", ui.stretchRect, 7, tsFg);

            fillRect(s, ui.clearRect, {80, 30, 30, 255});
            outlineRect(s, ui.clearRect, {140, 50, 50, 255});
            drawTextCentered(s, "x", ui.clearRect, 8, {255, 150, 150, 255});

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
            drawText(s, "drop sfx", mSfxDropRect[i].x + 3, mSfxDropRect[i].y + 2, 8, {60, 65, 85, 255});
        }
    }

    // --- Centre panel ---
    fillRect(s, mCenterPanel, PANEL_BG);
    outlineRect(s, mCenterPanel, PANEL_OUT);

    drawText(s, "Enemy Name:", mCenterPanel.x + 4, mCenterPanel.y + 10, 14, {220, 160, 140, 255});
    SDL_Color fieldOutCol = mNameActive ? SDL_Color{255, 120, 100, 255} : PANEL_OUT;
    fillRect(s, mNameFieldRect, {18, 18, 32, 255});
    outlineRect(s, mNameFieldRect, fieldOutCol, 2);
    std::string nameDisplay = mProfile.name + (mNameActive ? "|" : "");
    drawText(s, nameDisplay, mNameFieldRect.x + 8, mNameFieldRect.y + 8, 18);
    if (!mNameError.empty())
        drawText(s, mNameError, mNameFieldRect.x, mNameFieldRect.y + mNameFieldRect.h + 2,
                 12, {255, 80, 80, 255});

    {
        bool wActive = (mNumFieldActive == NumField::Width);
        bool hActive = (mNumFieldActive == NumField::Height);
        drawText(s, "Sprite Width", mWidthRect.x + 2, mWidthRect.y - 16, 11, {180, 140, 120, 255});
        drawText(s, "Sprite Height", mHeightRect.x + 2, mHeightRect.y - 16, 11, {180, 140, 120, 255});
        fillRect(s, mWidthRect, {18, 18, 32, 255});
        outlineRect(s, mWidthRect, wActive ? SDL_Color{255, 120, 100, 255} : PANEL_OUT, 2);
        std::string wDisplay = (wActive ? mWidthStr : std::to_string(mProfile.spriteW)) + (wActive ? "|" : " px");
        drawText(s, wDisplay, mWidthRect.x + 8, mWidthRect.y + 8, 14);
        fillRect(s, mHeightRect, {18, 18, 32, 255});
        outlineRect(s, mHeightRect, hActive ? SDL_Color{255, 120, 100, 255} : PANEL_OUT, 2);
        std::string hDisplay = (hActive ? mHeightStr : std::to_string(mProfile.spriteH)) + (hActive ? "|" : " px");
        drawText(s, hDisplay, mHeightRect.x + 8, mHeightRect.y + 8, 14);
    }

    {
        bool sActive = (mNumFieldActive == NumField::Speed);
        bool hlActive = (mNumFieldActive == NumField::Health);
        drawText(s, "Move Speed (px/s)", mSpeedRect.x + 2, mSpeedRect.y - 16, 11, {180, 140, 120, 255});
        drawText(s, "Health (HP)", mHealthRect.x + 2, mHealthRect.y - 16, 11, {180, 140, 120, 255});
        fillRect(s, mSpeedRect, {18, 18, 32, 255});
        outlineRect(s, mSpeedRect, sActive ? SDL_Color{255, 120, 100, 255} : PANEL_OUT, 2);
        std::string sDisplay = (sActive ? mSpeedStr : std::to_string((int)mProfile.speed)) + (sActive ? "|" : "");
        drawText(s, sDisplay, mSpeedRect.x + 8, mSpeedRect.y + 8, 14);
        fillRect(s, mHealthRect, {18, 18, 32, 255});
        outlineRect(s, mHealthRect, hlActive ? SDL_Color{255, 120, 100, 255} : PANEL_OUT, 2);
        std::string hlDisplay = (hlActive ? mHealthStr : std::to_string((int)mProfile.health)) + (hlActive ? "|" : "");
        drawText(s, hlDisplay, mHealthRect.x + 8, mHealthRect.y + 8, 14);
    }

    std::string slotLabel = std::string(EnemyAnimSlotName(static_cast<EnemyAnimSlot>(mSelectedSlot)))
                          + " Preview";
    if (mProfile.spriteW > 0 && mProfile.spriteH > 0)
        slotLabel += "  (" + std::to_string(mProfile.spriteW) + "x" + std::to_string(mProfile.spriteH) + "px)";
    drawTextCentered(s, slotLabel,
                     {mPreviewCellRect.x, mPreviewCellRect.y - 24, mPreviewCellRect.w, 20},
                     13, {255, 200, 180, 255});

    fillRect(s, mPreviewCellRect, {10, 12, 22, 255});
    outlineRect(s, mPreviewCellRect, PANEL_OUT);

    const auto& prev = mPreviews[mSelectedSlot];
    if (prev.has_value() && !prev->frames.empty()) {
        SDL_Surface* sheet = prev->sheet->GetSurface();
        const SDL_Rect& fr = prev->frames[std::min(mAnimFrame, (int)prev->frames.size() - 1)];
        if (sheet && mPreviewRenderRect.w > 0 && mPreviewRenderRect.h > 0) {
            SDL_Rect src = fr;
            SDL_Rect dst = mPreviewRenderRect;
            SDL_ScaleMode sm = (dst.w < src.w || dst.h < src.h)
                             ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_PIXELART;
            SDL_BlitSurfaceScaled(sheet, &src, s, &dst, sm);
        }
    } else {
        drawTextCentered(s, "No sprite", mPreviewCellRect, 14, {80, 80, 100, 255});
    }

    renderHitboxOverlay(s);

    {
        SDL_Color dzBg = mDropHover ? DROP_HOVER : DROP_IDLE;
        fillRect(s, mDropZone, dzBg);
        outlineRect(s, mDropZone, {100, 60, 40, 255});
        std::string dzLabel = mDropMsg.empty() ? "Drop PNG / folder here" : mDropMsg;
        drawTextCentered(s, dzLabel,
                         {mDropZone.x, mDropZone.y, mDropZone.w - 104, mDropZone.h},
                         13, {200, 180, 160, 255});

        {
            bool hClr = (mHoverBtn == HoverBtn::Clear);
            fillRect(s, mClearSlotRect, hClr ? BTN_CLR_H : BTN_CLR);
            outlineRect(s, mClearSlotRect, hClr ? SDL_Color{220, 100, 100, 255} : SDL_Color{200, 80, 80, 255});
            drawTextCentered(s, "Clear Slot", mClearSlotRect, 12, {255, 200, 200, 255});
        }
    }

    if (prev.has_value() && !prev->frames.empty()) {
        int stripY   = mDropZone.y + mDropZone.h + 8;
        int availW   = mCenterPanel.w - 8;
        int cols     = std::max(1, availW / (FRAME_THUMB_SZ + 4));
        int maxVisRows = 3;
        int firstFrame = mFrameStripScroll * cols;
        mFrameDelRects.clear();
        mFrameDelRects.resize(prev->frames.size(), {0, 0, 0, 0});
        for (int idx = firstFrame; idx < (int)prev->frames.size(); ++idx) {
            int visIdx = idx - firstFrame;
            int row = visIdx / cols;
            if (row >= maxVisRows) break;
            int col = visIdx % cols;
            int tx = mCenterPanel.x + 4 + col * (FRAME_THUMB_SZ + 4);
            int ty = stripY + row * (FRAME_THUMB_SZ + 16);
            SDL_Rect dst = {tx, ty, FRAME_THUMB_SZ, FRAME_THUMB_SZ};
            bool isCurrent = (idx == mAnimFrame);
            if (isCurrent)
                outlineRect(s, {tx - 1, ty - 1, FRAME_THUMB_SZ + 2, FRAME_THUMB_SZ + 2},
                            {255, 220, 80, 255}, 2);
            SDL_Surface* sheet = prev->sheet->GetSurface();
            if (sheet) {
                SDL_Rect src = prev->frames[idx];
                SDL_BlitSurfaceScaled(sheet, &src, s, &dst, SDL_SCALEMODE_LINEAR);
            }
            outlineRect(s, dst, {60, 60, 80, 255});
            SDL_Rect delBtn = {tx + FRAME_THUMB_SZ - 12, ty, 12, 12};
            fillRect(s, delBtn, {180, 40, 40, 220});
            drawTextCentered(s, "x", delBtn, 9, {255, 200, 200, 255});
            mFrameDelRects[idx] = delBtn;
        }
    }

    {
        bool hSave = (mHoverBtn == HoverBtn::Save);
        fillRect(s, mSaveBtnRect, hSave ? BTN_SAVE_H : BTN_SAVE);
        outlineRect(s, mSaveBtnRect, hSave ? SDL_Color{100, 255, 140, 255} : SDL_Color{60, 200, 100, 255});
        drawTextCentered(s, "Save Enemy", mSaveBtnRect, 16);
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
    drawText(s, "Saved Enemies", mRosterPanel.x + 6, mRosterPanel.y + 8, 14, {220, 160, 100, 255});

    int ry = mRosterPanel.y + 34;
    if (mRoster.empty()) {
        drawText(s, "None yet.", mRosterPanel.x + 10, ry + 10, 13, {80, 90, 110, 255});
    } else {
        for (int i = mRosterScroll; i < (int)mRoster.size(); ++i) {
            auto& entry = mRoster[i];
            if (ry + 44 > mRosterPanel.y + mRosterPanel.h) break;
            fillRect(s, {mRosterPanel.x + 4, ry, mRosterPanel.w - 8, 44}, {50, 36, 30, 255});
            drawText(s, entry.name, mRosterPanel.x + 10, ry + 4, 15, {255, 220, 200, 255});
            entry.loadRect = {mRosterPanel.x + 4,                    ry + 24, 80, 18};
            entry.delRect  = {mRosterPanel.x + mRosterPanel.w - 54,  ry + 24, 50, 18};
            {
                bool hLoad = (mHoverBtn == HoverBtn::RosterLoad && mHoverIndex == i);
                fillRect(s, entry.loadRect, hLoad ? BTN_LOAD_H : BTN_LOAD);
                outlineRect(s, entry.loadRect, hLoad ? SDL_Color{255, 180, 100, 255} : SDL_Color{200, 120, 60, 255});
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

    // LINEAR keeps anti-aliased text smooth on HiDPI
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, s);
    SDL_DestroySurface(s);
    if (tex) {
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
        SDL_RenderTexture(ren, tex, nullptr, nullptr);
        SDL_DestroyTexture(tex);
    }
    window.Update();
}

// --- NextScene ---

std::unique_ptr<Scene> EnemyCreatorScene::NextScene() {
    if (mGoBack) {
        mGoBack = false;
        return std::make_unique<TitleScene>();
    }
    return nullptr;
}

// --- Preview building ---

void EnemyCreatorScene::rebuildPreview(int slotIdx) {
    clearPreview(slotIdx);
    const std::string& dir = mProfile.slots[slotIdx].folderPath;
    if (dir.empty()) return;

    std::error_code ec;
    bool isDir = fs::is_directory(dir, ec);
    if (!isDir || ec) {
        mDropMsg = "Folder not found: " + fs::path(dir).filename().string();
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

    SDL_Surface* first = IMG_Load(pngs[0].string().c_str());
    if (!first) {
        mDropMsg = "Failed to load: " + pngs[0].filename().string();
        return;
    }
    int fw = first->w, fh = first->h;
    SDL_DestroySurface(first);

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

void EnemyCreatorScene::clearPreview(int slotIdx) {
    mPreviews[slotIdx].reset();
    if (mSelectedSlot == slotIdx) {
        mAnimFrame = 0;
        mAnimTimer = 0;
    }
}

void EnemyCreatorScene::deleteFrame(int slotIdx, int frameIdx) {
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

// --- Hitbox editor ---

void EnemyCreatorScene::recomputePreviewRect() {
    const int srcW = (mProfile.spriteW > 0) ? mProfile.spriteW : PREVIEW_W;
    const int srcH = (mProfile.spriteH > 0) ? mProfile.spriteH : PREVIEW_H;

    const int previewTop = mCenterPanel.y + 192;
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

void EnemyCreatorScene::initHBFromProfile(int slotIdx) {
    const int srcW = (mProfile.spriteW > 0) ? mProfile.spriteW : PREVIEW_W;
    const int srcH = (mProfile.spriteH > 0) ? mProfile.spriteH : PREVIEW_H;

    const auto& hb = mProfile.slots[slotIdx].hitbox;
    int hbX, hbY, hbW, hbH;
    if (!hb.IsDefault()) {
        hbX = hb.x;  hbY = hb.y;  hbW = hb.w;  hbH = hb.h;
    } else {
        // Default hitbox: slight inset from sprite edges
        hbX = srcW / 8;
        hbY = srcH / 8;
        hbW = srcW - srcW / 4;
        hbH = srcH - srcH / 4;
    }

    const int spriteX = mPreviewCellRect.x + PREVIEW_PAD;
    const int spriteY = mPreviewCellRect.y + PREVIEW_PAD;
    mPreviewRenderRect = {spriteX, spriteY, srcW, srcH};

    const int colScreenX = spriteX + hbX;
    const int colScreenY = spriteY + hbY;

    mHBRect = {colScreenX, colScreenY, hbW, hbH};
    mHBInitialised = true;
}

void EnemyCreatorScene::commitHBToProfile(int slotIdx) {
    if (!mHBInitialised) return;
    SDL_Rect nr = normaliseRect(mHBRect);
    auto& hb = mProfile.slots[slotIdx].hitbox;

    const int srcW = (mProfile.spriteW > 0) ? mProfile.spriteW : PREVIEW_W;
    const int srcH = (mProfile.spriteH > 0) ? mProfile.spriteH : PREVIEW_H;

    int localX = nr.x - mPreviewRenderRect.x;
    int localY = nr.y - mPreviewRenderRect.y;

    hb.x = std::clamp(localX, 0, srcW - 1);
    hb.y = std::clamp(localY, 0, srcH - 1);
    hb.w = std::clamp(nr.w, 1, srcW - hb.x);
    hb.h = std::clamp(nr.h, 1, srcH - hb.y);
}

int EnemyCreatorScene::hitboxHandleAt(int mx, int my) const {
    if (!mHBInitialised) return -1;
    SDL_Rect r = normaliseRect(mHBRect);
    const int HSZ = HB_HANDLE_SZ;
    SDL_Rect corners[4] = {
        {r.x - HSZ/2,       r.y - HSZ/2,       HSZ, HSZ},
        {r.x + r.w - HSZ/2, r.y - HSZ/2,       HSZ, HSZ},
        {r.x + r.w - HSZ/2, r.y + r.h - HSZ/2, HSZ, HSZ},
        {r.x - HSZ/2,       r.y + r.h - HSZ/2, HSZ, HSZ},
    };
    for (int i = 0; i < 4; ++i)
        if (hit(corners[i], mx, my)) return i;
    if (hit(r, mx, my)) return 4;
    return -1;
}

void EnemyCreatorScene::renderHitboxOverlay(SDL_Surface* surf) const {
    if (!mHBInitialised) return;
    SDL_Rect r = normaliseRect(mHBRect);

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
        int cx = r.x + r.w / 2, cy = r.y + r.h / 2;
        const int CL = 6;
        SDL_Rect hLine = {cx - CL, cy, CL * 2 + 1, 1};
        SDL_Rect vLine = {cx, cy - CL, 1, CL * 2 + 1};
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

void EnemyCreatorScene::refreshRoster() {
    mRoster.clear();
    for (const auto& p : ScanEnemyProfiles()) {
        RosterEntry e;
        e.name = p.stem().string();
        e.path = p.string();
        mRoster.push_back(std::move(e));
    }
    mRosterScroll = 0;
}

void EnemyCreatorScene::loadRosterEntry(int idx) {
    if (idx < 0 || idx >= (int)mRoster.size()) return;
    EnemyProfile loaded;
    if (LoadEnemyProfile(mRoster[idx].path, loaded)) {
        if (mHBInitialised && mPreviewCellRect.w > 0)
            commitHBToProfile(mSelectedSlot);
        for (int i = 0; i < ENEMY_ANIM_SLOT_COUNT; ++i) mPreviews[i].reset();
        mProfile = std::move(loaded);
        mLoadedName = mProfile.name;
        mWidthStr   = std::to_string(mProfile.spriteW);
        mHeightStr  = std::to_string(mProfile.spriteH);
        mSpeedStr   = std::to_string((int)mProfile.speed);
        mHealthStr  = std::to_string((int)mProfile.health);
        computeLayout();
        for (int i = 0; i < ENEMY_ANIM_SLOT_COUNT; ++i)
            rebuildPreview(i);
        mSelectedSlot = 0;
        mSelectedSfxFile = 0;
        recomputePreviewRect();
        initHBFromProfile(0);
        mAnimFrame = 0; mAnimTimer = 0;
        mDropMsg = "Loaded: " + mRoster[idx].name;
    }
}

// --- Text input ---

void EnemyCreatorScene::startTextInput() {
    if (!mNameActive) {
        mNameActive = true;
        SDL_StartTextInput(mSDLWin);
    }
}

void EnemyCreatorScene::stopTextInput() {
    if (mNameActive) {
        mNameActive = false;
        SDL_StopTextInput(mSDLWin);
    }
}

// --- Draw helpers ---

void EnemyCreatorScene::fillRect(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
    SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
}

void EnemyCreatorScene::outlineRect(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t) {
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
    Uint32 col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
    SDL_Rect sides[4] = {{r.x, r.y, r.w, t}, {r.x, r.y+r.h-t, r.w, t},
                         {r.x, r.y, t, r.h}, {r.x+r.w-t, r.y, t, r.h}};
    for (auto& sr : sides) SDL_FillSurfaceRect(s, &sr, col);
}

void EnemyCreatorScene::drawText(SDL_Surface* s, const std::string& str,
                                  int x, int y, int ptSize, SDL_Color col) {
    if (str.empty()) return;
    TTF_Font* font = FontCache::Get(ptSize);
    if (!font) return;
    SDL_Surface* ts = TTF_RenderText_Blended(font, str.c_str(), 0, col);
    if (ts) {
        SDL_Rect dst = {x, y, ts->w, ts->h};
        SDL_BlitSurface(ts, nullptr, s, &dst);
        SDL_DestroySurface(ts);
    }
}

void EnemyCreatorScene::drawTextCentered(SDL_Surface* s, const std::string& str,
                                          SDL_Rect r, int ptSize, SDL_Color col) {
    if (str.empty()) return;
    auto [tx, ty] = Text::CenterInRect(str, ptSize, r);
    drawText(s, str, tx, ty, ptSize, col);
}

void EnemyCreatorScene::blitScaled(SDL_Surface* dst, SDL_Surface* src, SDL_Rect dstRect) {
    if (!src || !dst) return;
    float scaleX = (float)dstRect.w / src->w;
    float scaleY = (float)dstRect.h / src->h;
    float scale  = std::min(scaleX, scaleY);
    int   w      = (int)(src->w * scale);
    int   h      = (int)(src->h * scale);
    int   ox     = dstRect.x + (dstRect.w - w) / 2;
    int   oy     = dstRect.y + (dstRect.h - h) / 2;
    SDL_Rect dst2 = {ox, oy, w, h};
    SDL_BlitSurfaceScaled(src, nullptr, dst, &dst2, SDL_SCALEMODE_PIXELART);
}

SDL_Rect EnemyCreatorScene::normaliseRect(SDL_Rect r) {
    if (r.w < 0) { r.x += r.w; r.w = -r.w; }
    if (r.h < 0) { r.y += r.h; r.h = -r.h; }
    return r;
}
