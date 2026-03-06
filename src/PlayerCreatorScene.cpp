#include "PlayerCreatorScene.hpp"
#include "TitleScene.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <filesystem>
#include <print>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// Small inline helpers
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// Scene interface
// ─────────────────────────────────────────────────────────────────────────────

void PlayerCreatorScene::Load(Window& window) {
    mW      = window.GetWidth();
    mH      = window.GetHeight();
    mSDLWin = window.GetRaw();

    // Ensure players/ directory exists
    if (!fs::exists("players"))
        fs::create_directory("players");

    // Default profile
    mProfile = PlayerProfile{};
    mProfile.name = "MyCharacter";

    computeLayout();
    recomputePreviewRect();
    refreshRoster();
    initHBFromProfile(mSelectedSlot);
}

void PlayerCreatorScene::Unload() {
    stopTextInput();
}

// ─────────────────────────────────────────────────────────────────────────────
// Layout
// ─────────────────────────────────────────────────────────────────────────────

void PlayerCreatorScene::computeLayout() {
    const int PAD   = PANEL_PAD;
    const int TOTAL = mW;
    const int H     = mH;

    // Three columns:
    //  slot panel  = 240 px wide
    //  center      = flexible
    //  roster      = 250 px wide
    const int LEFT_W   = 240;
    const int RIGHT_W  = 260;
    const int MID_W    = TOTAL - LEFT_W - RIGHT_W - PAD * 4;

    mSlotPanel   = {PAD,                     40,  LEFT_W,  H - 80};
    mCenterPanel = {PAD * 2 + LEFT_W,         40,  MID_W,   H - 80};
    mRosterPanel = {PAD * 3 + LEFT_W + MID_W, 40,  RIGHT_W, H - 80};

    // Name field — top of centre panel
    mNameFieldRect = {mCenterPanel.x, mCenterPanel.y + 40, MID_W - 2, 36};

    // Sprite size fields — one row below name field, side by side
    const int sfW = (MID_W - 6) / 2;
    mWidthRect  = {mCenterPanel.x,         mCenterPanel.y + 84, sfW, 32};
    mHeightRect = {mCenterPanel.x + sfW + 6, mCenterPanel.y + 84, sfW, 32};

    // Save & Back buttons — bottom of centre panel
    mSaveBtnRect = {mCenterPanel.x,            mCenterPanel.y + mCenterPanel.h - 50, 130, 40};
    mBackBtnRect = {mCenterPanel.x + MID_W - 130, mCenterPanel.y + mCenterPanel.h - 50, 130, 40};

    // Preview cell, drop zone, and clear button are all computed by
    // recomputePreviewRect() which runs immediately after computeLayout().
    // Initialize to zero here so they're never used uninitialized.
    mPreviewCellRect   = {};
    mPreviewRenderRect = {};
    mDropZone          = {};
    mClearSlotRect     = {};

    // Slot rows — left panel
    mSlotRowRects.resize(PLAYER_ANIM_SLOT_COUNT);
    int ry = mSlotPanel.y + 40;
    for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
        mSlotRowRects[i] = {mSlotPanel.x + 4, ry, LEFT_W - 8, SLOT_ROW_H - 4};
        // FPS +/- buttons sit on the right side of each row
        const int BW = 18;  // button width
        const int BH = 16;  // button height
        const int BY = ry + (SLOT_ROW_H - 4 - BH) / 2;  // vertically centred
        mFpsBtns[i].plusRect  = {mSlotPanel.x + LEFT_W - 8 - BW,      BY, BW, BH};
        mFpsBtns[i].minusRect = {mSlotPanel.x + LEFT_W - 8 - BW*2 - 2, BY, BW, BH};
        ry += SLOT_ROW_H;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Events
// ─────────────────────────────────────────────────────────────────────────────

bool PlayerCreatorScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) return false;

    // ── File drag enter/hover ─────────────────────────────────────────────────
    if (e.type == SDL_EVENT_DROP_BEGIN) {
        mDropHover = true;
        return true;
    }
    if (e.type == SDL_EVENT_DROP_COMPLETE) {
        mDropHover = false;
        return true;
    }

    // ── File / directory drop ─────────────────────────────────────────────────
    if (e.type == SDL_EVENT_DROP_FILE || e.type == SDL_EVENT_DROP_TEXT) {
        mDropHover = false;
        std::string dropped = e.drop.data ? e.drop.data : "";

        if (!dropped.empty()) {
            fs::path p(dropped);
            std::string dir;

            // If it's a file, use its parent directory
            if (fs::is_directory(p))      dir = dropped;
            else if (fs::is_regular_file(p)) dir = p.parent_path().string();

            if (!dir.empty()) {
                // Check it has at least one PNG
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
                    rebuildPreview(mSelectedSlot);    // may auto-fill spriteW/H
                    recomputePreviewRect();            // resize cell to match new sprite dims
                    initHBFromProfile(mSelectedSlot); // reposition hitbox in updated cell
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

    // ── Keyboard ─────────────────────────────────────────────────────────────
    // Size field input (digits only, max 4 chars)
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

    if (e.type == SDL_EVENT_KEY_DOWN) {
        // Arrow keys to cycle slots
        if (e.key.key == SDLK_DOWN || e.key.key == SDLK_S) {
            if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
            mSelectedSlot = (mSelectedSlot + 1) % PLAYER_ANIM_SLOT_COUNT;
            initHBFromProfile(mSelectedSlot);
            mAnimFrame = 0; mAnimTimer = 0;
        }
        if (e.key.key == SDLK_UP || e.key.key == SDLK_W) {
            if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
            mSelectedSlot = (mSelectedSlot - 1 + PLAYER_ANIM_SLOT_COUNT) % PLAYER_ANIM_SLOT_COUNT;
            initHBFromProfile(mSelectedSlot);
            mAnimFrame = 0; mAnimTimer = 0;
        }
        if (e.key.key == SDLK_ESCAPE) {
            mGoBack = true;
        }
    }

    // ── Mouse ─────────────────────────────────────────────────────────────────
    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        int mx = (int)e.motion.x;
        int my = (int)e.motion.y;

        // Hitbox drag
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
            // Clamp to preview area
            r.w = std::max(r.w, 4);
            r.h = std::max(r.h, 4);
            mHBRect = r;
        }
    }

    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = (int)e.button.x;
        int my = (int)e.button.y;

        // FPS +/- buttons (check before slot row so they don't also select)
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

        // Slot row click
        for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
            if (hit(mSlotRowRects[i], mx, my)) {
                if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
                mSelectedSlot = i;
                initHBFromProfile(mSelectedSlot);
                mAnimFrame = 0; mAnimTimer = 0;
                return true;
            }
        }

        // Size field clicks
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
        // Commit size fields on click-away (handled below in blur logic)

        // Name field click — clear the name so the user types a fresh one
        if (hit(mNameFieldRect, mx, my)) {
            mProfile.name.clear();
            mNameError.clear();
            mSizeActive = SizeField::None;
            startTextInput();
            return true;
        } else {
            if (mNameActive) stopTextInput();
            if (mSizeActive != SizeField::None) {
                // Commit on blur
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

        // Back button
        if (hit(mBackBtnRect, mx, my)) {
            if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
            mGoBack = true;
            return true;
        }

        // Save button
        if (hit(mSaveBtnRect, mx, my)) {
            if (mHBInitialised && mPreviewCellRect.w > 0) commitHBToProfile(mSelectedSlot);
            if (mProfile.name.empty()) {
                mNameError = "Name required!";
            } else {
                if (!fs::exists("players")) fs::create_directory("players");
                // If the name changed, delete the old file so no stale copy remains
                if (!mLoadedName.empty() && mLoadedName != mProfile.name) {
                    std::error_code ec;
                    fs::remove(PlayerProfilePath(mLoadedName), ec);
                }
                SavePlayerProfile(mProfile, PlayerProfilePath(mProfile.name));
                mLoadedName = mProfile.name;  // update so a second save doesn't re-delete
                refreshRoster();
                mNameError.clear();
                mDropMsg = "Saved as: " + mProfile.name;
            }
            return true;
        }

        // Clear slot button
        if (hit(mClearSlotRect, mx, my)) {
            mProfile.Slot(static_cast<PlayerAnimSlot>(mSelectedSlot)).folderPath.clear();
            mProfile.Slot(static_cast<PlayerAnimSlot>(mSelectedSlot)).hitbox = {};
            clearPreview(mSelectedSlot);
            initHBFromProfile(mSelectedSlot);
            mDropMsg = "Slot cleared.";
            return true;
        }

        // Hitbox handle hit test — always active once initialised, no sprite required
        int handle = hitboxHandleAt(mx, my);
        if (handle >= 0 && mHBInitialised) {
            mHBEditing       = true;
            mHBDragHandle    = handle;
            mHBDragStartMX   = mx;
            mHBDragStartMY   = my;
            mHBDragStartRect = mHBRect;
            return true;
        }

        // Roster entry buttons
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
        if (mHBEditing) {
            mHBEditing    = false;
            mHBDragHandle = -1;
            mHBRect = normaliseRect(mHBRect);
        }
    }

    // Scroll roster
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        float fmx, fmy;
        SDL_GetMouseState(&fmx, &fmy);
        int mx = (int)fmx, my = (int)fmy;
        if (hit(mRosterPanel, mx, my)) {
            mRosterScroll = std::clamp(mRosterScroll - (int)e.wheel.y,
                                       0, std::max(0, (int)mRoster.size() - 1));
        }
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Update
// ─────────────────────────────────────────────────────────────────────────────

void PlayerCreatorScene::Update(float dt) {
    // Advance animation preview for selected slot.
    // Use the slot's authored FPS if set, otherwise fall back to ANIM_FPS.
    // This means the preview immediately reflects any +/- FPS button press.
    const auto& prev = mPreviews[mSelectedSlot];
    if (prev.has_value() && !prev->frames.empty()) {
        float slotFps = mProfile.Slot(static_cast<PlayerAnimSlot>(mSelectedSlot)).fps;
        float previewFps = (slotFps > 0.0f) ? slotFps : ANIM_FPS;
        mAnimTimer += dt;
        if (mAnimTimer >= 1.0f / previewFps) {
            mAnimTimer = 0.0f;
            mAnimFrame = (mAnimFrame + 1) % (int)prev->frames.size();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Render
// ─────────────────────────────────────────────────────────────────────────────

void PlayerCreatorScene::Render(Window& window) {
    window.Render();
    SDL_Renderer* ren = window.GetRenderer();
    // Render this scene to an intermediate surface, then upload to renderer.
    // This preserves all pixel-exact surface-based hitbox drawing.
    SDL_Surface* s = SDL_CreateSurface(mW, mH, SDL_PIXELFORMAT_ARGB8888);
    if (!s) { window.Update(); return; }

    // Background
    fillRect(s, {0, 0, mW, mH}, BG);

    // ── Title bar ─────────────────────────────────────────────────────────────
    drawTextCentered(s, "Player Creator", {0, 4, mW, 32}, 24, {200, 210, 255, 255});

    // ── LEFT: Slot panel ──────────────────────────────────────────────────────
    fillRect(s, mSlotPanel, PANEL_BG);
    outlineRect(s, mSlotPanel, PANEL_OUT);
    drawText(s, "Animation Slots", mSlotPanel.x + 6, mSlotPanel.y + 8, 14, {160, 170, 220, 255});

    for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
        auto slot  = static_cast<PlayerAnimSlot>(i);
        bool sel   = (i == mSelectedSlot);
        bool filled = mProfile.HasSlot(slot);

        SDL_Color bg = sel ? SEL_BG : (filled ? SLOT_FILLED : SLOT_EMPTY);
        fillRect(s, mSlotRowRects[i], bg);
        outlineRect(s, mSlotRowRects[i], sel ? SDL_Color{120, 160, 255, 255} : PANEL_OUT);

        // Slot name
        drawText(s, PlayerAnimSlotName(slot),
                 mSlotRowRects[i].x + 8,
                 mSlotRowRects[i].y + (SLOT_ROW_H - 20) / 2,
                 16, sel ? SDL_Color{255, 255, 255, 255} : SDL_Color{180, 190, 210, 255});

        // Folder name (truncated)
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

        // FPS stepper: "- 12 +" on the right side of each row
        {
            float curFps = mProfile.Slot(slot).fps;
            std::string fpsStr = (curFps > 0.0f)
                ? std::to_string((int)curFps) + "fps"
                : "default";
            // minus button
            fillRect(s, mFpsBtns[i].minusRect, {50, 50, 80, 255});
            outlineRect(s, mFpsBtns[i].minusRect, {80, 80, 130, 255});
            drawTextCentered(s, "-", mFpsBtns[i].minusRect, 13, {200, 200, 255, 255});
            // plus button
            fillRect(s, mFpsBtns[i].plusRect, {50, 50, 80, 255});
            outlineRect(s, mFpsBtns[i].plusRect, {80, 80, 130, 255});
            drawTextCentered(s, "+", mFpsBtns[i].plusRect, 13, {200, 200, 255, 255});
            // value label between them
            int labelX = mFpsBtns[i].minusRect.x - 2 - (int)fpsStr.size() * 6;
            drawText(s, fpsStr, labelX,
                     mFpsBtns[i].minusRect.y + 2, 10, {160, 170, 200, 255});
        }
    }

    // ── CENTRE panel ──────────────────────────────────────────────────────────
    fillRect(s, mCenterPanel, PANEL_BG);
    outlineRect(s, mCenterPanel, PANEL_OUT);

    // Character name label + field
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

    // Sprite size fields
    {
        bool wActive = (mSizeActive == SizeField::Width);
        bool hActive = (mSizeActive == SizeField::Height);
        // Labels
        drawText(s, "W:", mWidthRect.x + 4,  mWidthRect.y + 8, 13, {160, 170, 220, 255});
        drawText(s, "H:", mHeightRect.x + 4, mHeightRect.y + 8, 13, {160, 170, 220, 255});
        // Width field
        fillRect(s, mWidthRect, {18, 18, 32, 255});
        outlineRect(s, mWidthRect, wActive ? SDL_Color{100, 150, 255, 255} : PANEL_OUT, 2);
        std::string wDisplay = (wActive ? mWidthStr : (mProfile.spriteW > 0 ? std::to_string(mProfile.spriteW) : "120")) + (wActive ? "|" : "");
        drawText(s, wDisplay, mWidthRect.x + 22, mWidthRect.y + 8, 14);
        // Height field
        fillRect(s, mHeightRect, {18, 18, 32, 255});
        outlineRect(s, mHeightRect, hActive ? SDL_Color{100, 150, 255, 255} : PANEL_OUT, 2);
        std::string hDisplay = (hActive ? mHeightStr : (mProfile.spriteH > 0 ? std::to_string(mProfile.spriteH) : "160")) + (hActive ? "|" : "");
        drawText(s, hDisplay, mHeightRect.x + 22, mHeightRect.y + 8, 14);
    }

    // Slot label above preview — show actual pixel size so you can verify it
    std::string slotLabel = std::string(PlayerAnimSlotName(static_cast<PlayerAnimSlot>(mSelectedSlot)))
                          + " Preview";
    if (mProfile.spriteW > 0 && mProfile.spriteH > 0)
        slotLabel += "  (" + std::to_string(mProfile.spriteW) + "x" + std::to_string(mProfile.spriteH) + "px)";
    drawTextCentered(s, slotLabel,
                     {mPreviewCellRect.x, mPreviewCellRect.y - 24, mPreviewCellRect.w, 20},
                     13, {200, 210, 255, 255});

    // Preview area — draw the fixed cell background, not the sprite draw rect
    fillRect(s, mPreviewCellRect, {10, 12, 22, 255});
    outlineRect(s, mPreviewCellRect, PANEL_OUT);

    const auto& prev = mPreviews[mSelectedSlot];
    if (prev.has_value() && !prev->frames.empty()) {
        // ── Game-accurate preview ─────────────────────────────────────────────
        // The game bottom-aligns the sprite to the collider bottom (floor).
        // We replicate that here: fix a floor Y at the preview bottom, scale the
        // sprite to fit the preview width while keeping aspect ratio, anchor its
        // bottom to that floor, then derive the hitbox rect from sprite-local
        // coords using the same scale — so the overlay matches the game exactly.

        // mPreviewRenderRect = sprite draw rect (set by initHBFromProfile, collider-first).
        // Just blit the current animation frame into it directly.
        SDL_Surface* sheet = prev->sheet->GetSurface();
        const SDL_Rect& fr = prev->frames[std::min(mAnimFrame, (int)prev->frames.size() - 1)];
        // Extract just this frame into a temp surface, then scale-blit into
        // mPreviewRenderRect. This handles the case where the raw PNG is a
        // different size than spriteW/H (e.g. new character with no size set yet).
        SDL_Surface* frameSurf = SDL_CreateSurface(fr.w, fr.h, sheet->format);
        if (frameSurf) {
            SDL_BlitSurface(sheet, &fr, frameSurf, nullptr);
            blitScaled(s, frameSurf, mPreviewRenderRect);
            SDL_DestroySurface(frameSurf);
        }

        // Floor line = collider bottom = where physics plants the player on a tile.
        // This is the ground line in-game: the sprite may extend below it.
        {
            const SDL_Rect hbNorm = normaliseRect(mHBRect);
            const int colBottom = hbNorm.y + hbNorm.h;
            SDL_Rect floorLine = {mPreviewCellRect.x, colBottom, mPreviewCellRect.w, 2};
            const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
            SDL_FillSurfaceRect(s, &floorLine, SDL_MapRGBA(fmt, nullptr, 80, 200, 80, 200));
            // Dim line at sprite bottom so you can see any sprite overflow below the floor
            const int spriteBottom = mPreviewRenderRect.y + mPreviewRenderRect.h;
            if (spriteBottom != colBottom) {
                SDL_Rect spriteLine = {mPreviewCellRect.x, spriteBottom, mPreviewCellRect.w, 1};
                SDL_FillSurfaceRect(s, &spriteLine, SDL_MapRGBA(fmt, nullptr, 80, 200, 80, 70));
            }
        }

        // Render() never overwrites mPreviewRenderRect or mHBRect.
        // Both are owned by initHBFromProfile (collider-first layout).
        renderHitboxOverlay(s);

        // Frame counter at bottom-left of sprite
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

    // Drop zone
    SDL_Color dzCol = mDropHover ? DROP_HOVER : DROP_IDLE;
    fillRect(s, mDropZone, dzCol);
    outlineRect(s, mDropZone, mDropHover ? SDL_Color{100, 180, 255, 255} : PANEL_OUT, 2);
    drawTextCentered(s,
                     mDropMsg.empty() ? "Drop sprite folder here" : mDropMsg,
                     mDropZone, 15,
                     mDropHover ? SDL_Color{200, 230, 255, 255} : SDL_Color{140, 150, 180, 255});

    // Clear slot button (overlaps right side of drop zone)
    fillRect(s, mClearSlotRect, {80, 50, 50, 255});
    outlineRect(s, mClearSlotRect, {160, 80, 80, 255});
    drawTextCentered(s, "Clear", mClearSlotRect, 13, {255, 180, 180, 255});

    // Hitbox editor hint + live sprite-local readout
    {
        int hintY = mDropZone.y + mDropZone.h + 8;
        drawText(s, "Hitbox Editor — drag corners/edges of the red box",
                 mCenterPanel.x + 4, hintY, 12, {120, 130, 160, 255});

        // Compute live sprite-local values from the current screen rect
        // (same math as commitHBToProfile so what you see = what gets saved)
        const int srcW = (mProfile.spriteW > 0) ? mProfile.spriteW : PREVIEW_W;
        const int srcH = (mProfile.spriteH > 0) ? mProfile.spriteH : PREVIEW_H;
        // 1:1 scale: screen px == sprite px, no conversion needed
        SDL_Rect nr = normaliseRect(mHBRect);
        int liveX = nr.x - mPreviewRenderRect.x;
        int liveY = nr.y - mPreviewRenderRect.y;
        int liveW = nr.w;
        int liveH = nr.h;
        liveX = std::clamp(liveX, 0, srcW - 1);
        liveY = std::clamp(liveY, 0, srcH - 1);
        liveW = std::clamp(liveW, 1, srcW - liveX);
        liveH = std::clamp(liveH, 1, srcH - liveY);

        // Line 1: sprite-local coords (what gets saved to JSON)
        std::string spriteStr = "sprite-local: x:" + std::to_string(liveX)
                              + " y:" + std::to_string(liveY)
                              + " w:" + std::to_string(liveW)
                              + " h:" + std::to_string(liveH);
        drawText(s, spriteStr, mCenterPanel.x + 4, hintY + 16, 12, {100, 220, 120, 255});

        // Line 2: render offset GameScene will use
        // roffX = -hb.x, roffY = -hb.y  (sprite origin offset from collider origin)
        int gameRoffX = -liveX;
        int gameRoffY = -liveY;
        std::string offStr = "roff: x:" + std::to_string(gameRoffX)
                           + " y:" + std::to_string(gameRoffY)
                           + "   col: " + std::to_string(liveW) + "x" + std::to_string(liveH);
        drawText(s, offStr, mCenterPanel.x + 4, hintY + 30, 12, {180, 160, 100, 255});
    }

    // Save & Back buttons
    fillRect(s, mSaveBtnRect, BTN_SAVE);
    outlineRect(s, mSaveBtnRect, {60, 200, 100, 255});
    drawTextCentered(s, "Save Character", mSaveBtnRect, 16);

    fillRect(s, mBackBtnRect, BTN_BACK);
    outlineRect(s, mBackBtnRect, {100, 100, 200, 255});
    drawTextCentered(s, "< Back", mBackBtnRect, 16);

    // ── RIGHT: Roster panel ───────────────────────────────────────────────────
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
            fillRect(s, entry.loadRect, BTN_LOAD);
            drawTextCentered(s, "Load", entry.loadRect, 11);
            fillRect(s, entry.delRect, BTN_DEL);
            drawTextCentered(s, "Delete", entry.delRect, 11, {255, 200, 200, 255});
            ry += 50;
        }
    }

    // Upload the completed surface to a texture and render it
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, s);
    SDL_DestroySurface(s);
    if (tex) {
        SDL_RenderTexture(ren, tex, nullptr, nullptr);
        SDL_DestroyTexture(tex);
    }
    window.Update();
}

// ─────────────────────────────────────────────────────────────────────────────
// NextScene
// ─────────────────────────────────────────────────────────────────────────────

std::unique_ptr<Scene> PlayerCreatorScene::NextScene() {
    if (mGoBack) {
        mGoBack = false;
        return std::make_unique<TitleScene>();
    }
    return nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// Preview building
// ─────────────────────────────────────────────────────────────────────────────

void PlayerCreatorScene::rebuildPreview(int slotIdx) {
    clearPreview(slotIdx);
    const std::string& dir = mProfile.slots[slotIdx].folderPath;
    if (dir.empty()) return;

    // Guard against stale/missing/non-directory paths (e.g. moved folders, cross-machine paths)
    std::error_code ec;
    bool isDir = fs::is_directory(dir, ec);
    if (!isDir || ec) {
        mDropMsg = "Folder not found (path may be from another machine): " + fs::path(dir).filename().string();
        mProfile.slots[slotIdx].folderPath.clear(); // clear so it doesn't crash on next load
        return;
    }

    // Collect sorted PNGs
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

    // Load first frame to get dimensions
    SDL_Surface* first = IMG_Load(pngs[0].string().c_str());
    if (!first) {
        mDropMsg = "Failed to load: " + pngs[0].filename().string();
        return;
    }
    int fw = first->w, fh = first->h;
    SDL_DestroySurface(first);

    int frameCount = (int)pngs.size();
    // Determine prefix and pad digits from first filename
    std::string stem = pngs[0].stem().string();
    int padDigits = 0;
    {
        int k = (int)stem.size() - 1;
        while (k >= 0 && std::isdigit((unsigned char)stem[k])) { ++padDigits; --k; }
    }
    std::string prefix = stem;
    while (!prefix.empty() && std::isdigit((unsigned char)prefix.back()))
        prefix.pop_back();

    // Extract the actual start index from the first filename so SpriteSheet
    // loads the right range regardless of padding or zero-vs-one origin.
    int startIndex = 0;
    {
        std::string numPart = stem.substr(prefix.size());
        startIndex = numPart.empty() ? 0 : std::stoi(numPart);
    }

    // Auto-fill spriteW/H from the raw frame size if not yet set by the user.
    // This sizes the preview cell correctly on first drop so the hitbox editor
    // is immediately accurate without requiring a manual size entry.
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
        auto ss = std::make_unique<SpriteSheet>(dir + "/", prefix, frameCount, tW, tH, padDigits, startIndex);
        auto frames = ss->GetAnimation(prefix);
        if (!frames.empty()) {
            SlotPreview p;
            p.frameW = fw;
            p.frameH = fh;
            p.frames = std::move(frames);
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

// ─────────────────────────────────────────────────────────────────────────────
// Hitbox editor helpers
// ─────────────────────────────────────────────────────────────────────────────

void PlayerCreatorScene::recomputePreviewRect() {
    const int srcW = (mProfile.spriteW > 0) ? mProfile.spriteW : PREVIEW_W;
    const int srcH = (mProfile.spriteH > 0) ? mProfile.spriteH : PREVIEW_H;

    // Preview top is always anchored relative to the centre panel, never
    // derived from the old mPreviewCellRect.y (which may be stale/zero).
    const int previewTop = mCenterPanel.y + 126;
    const int MID_W      = mCenterPanel.w;

    const int cellW = srcW + PREVIEW_PAD * 2;
    const int cellH = srcH + PREVIEW_PAD * 2;
    mPreviewCellRect = {mCenterPanel.x + (MID_W - cellW) / 2, previewTop, cellW, cellH};

    // Sprite sits at cell top-left + padding, 1:1
    mPreviewRenderRect = {mPreviewCellRect.x + PREVIEW_PAD,
                          mPreviewCellRect.y + PREVIEW_PAD,
                          srcW, srcH};

    mDropZone      = {mCenterPanel.x, mPreviewCellRect.y + cellH + 14, MID_W - 2, 56};
    mClearSlotRect = {mDropZone.x + mDropZone.w - 100, mDropZone.y, 96, mDropZone.h};
}

void PlayerCreatorScene::initHBFromProfile(int slotIdx) {
    // ── Sprite-first layout ────────────────────────────────────────────────────
    //
    // Game model (GameScene::Spawn):
    //   Transform  = collider top-left
    //   roffX = -hb.x   (sprite left is hb.x px to the LEFT of the collider left)
    //   roffY = -hb.y   (sprite top  is hb.y px ABOVE the collider top)
    //   Physics floor = collider BOTTOM = transform.y + hb.h
    //   Sprite bottom = transform.y - hb.y + spriteH  (may extend below floor)
    //
    // Editor preview mirrors this exactly:
    //   1. Sprite bottom-aligned to cell floor (sprite bottom = floorY)
    //      => spriteY = floorY - drawH
    //   2. Sprite centered horizontally
    //      => spriteX = cellX + (cellW - drawW) / 2
    //   3. Collider placed at sprite + hb offset
    //      => colScreenX = spriteX + hbX*scale
    //      => colScreenY = spriteY + hbY*scale
    //   4. Green floor line drawn at collider bottom (= physics floor)
    //      => dim line at sprite bottom shows any sprite overflow below floor
    //
    // Round-trip: hb.x = (col.x - sprite.x) / scale  (no drift, no circularity)

    const int srcW = (mProfile.spriteW > 0) ? mProfile.spriteW : PREVIEW_W;
    const int srcH = (mProfile.spriteH > 0) ? mProfile.spriteH : PREVIEW_H;
    const int cellX = (mPreviewCellRect.w > 0) ? mPreviewCellRect.x : 0;
    const int cellW = (mPreviewCellRect.w > 0) ? mPreviewCellRect.w : PREVIEW_W;
    const int cellH = (mPreviewCellRect.h > 0) ? mPreviewCellRect.h : PREVIEW_H;
    const int floorY = (mPreviewCellRect.h > 0)
                       ? mPreviewCellRect.y + cellH
                       : mPreviewRenderRect.y + mPreviewRenderRect.h;

    // Always render at 1:1 pixel scale so the editor exactly matches the game.
    // The cell (300x300) is large enough to contain any reasonable sprite size.
    const float scale = 1.0f;
    const int   drawW = srcW;
    const int   drawH = srcH;

    // Resolve effective hitbox (sprite-local px)
    const auto& hb = mProfile.slots[slotIdx].hitbox;
    int hbX, hbY, hbW, hbH;
    if (!hb.IsDefault()) {
        hbX = hb.x;  hbY = hb.y;  hbW = hb.w;  hbH = hb.h;
    } else {
        // Mirror game fallback: proportional frost-knight insets
        const float baseW = 120.0f, baseH = 160.0f;
        hbX = (int)std::round(32.0f * srcW / baseW);
        hbY = (int)std::round(33.0f * srcH / baseH);
        hbW = (int)std::round(srcW - 2.0f * 32.0f * srcW / baseW);
        hbH = (int)std::round(srcH - 33.0f * srcH / baseH - 26.0f * srcH / baseH);
    }

    // ── Step 1: sprite rect — inset by padding inside the cell at 1:1 scale ──
    const int spriteX = mPreviewCellRect.x + PREVIEW_PAD;
    const int spriteY = mPreviewCellRect.y + PREVIEW_PAD;
    mPreviewRenderRect = {spriteX, spriteY, drawW, drawH};

    // ── Step 2: collider rect — offset from sprite by hb.x/hb.y in scaled px ───
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

    // mHBRect   = COLLIDER in screen coords  (dragged by the user)
    // mPreviewRenderRect = SPRITE in screen coords  (set once by initHBFromProfile,
    //             NEVER overwritten by Render() or drag handlers — it is stable).
    //
    // The sprite position is invariant while editing: only the collider box moves.
    // So mPreviewRenderRect.x/y is always the correct sprite origin for conversion.

    const int srcW = (mProfile.spriteW > 0) ? mProfile.spriteW : PREVIEW_W;
    const int srcH = (mProfile.spriteH > 0) ? mProfile.spriteH : PREVIEW_H;
    // 1:1 scale — no conversion needed, screen px == sprite px
    const float scale    = 1.0f;
    const float invScale = 1.0f;

    // Convert collider screen coords to sprite-local px.
    // mPreviewRenderRect is the stable sprite origin (set once at init, never
    // overwritten by drags or Render). Collider offset from sprite origin = hb.x/y.
    int localX = nr.x - mPreviewRenderRect.x;
    int localY = nr.y - mPreviewRenderRect.y;

    hb.x = (int)std::round(localX * invScale);
    hb.y = (int)std::round(localY * invScale);
    hb.w = std::max(1, (int)std::round(nr.w * invScale));
    hb.h = std::max(1, (int)std::round(nr.h * invScale));

    // Clamp to sprite bounds
    hb.x = std::clamp(hb.x, 0, srcW - 1);
    hb.y = std::clamp(hb.y, 0, srcH - 1);
    hb.w = std::clamp(hb.w, 1, srcW - hb.x);
    hb.h = std::clamp(hb.h, 1, srcH - hb.y);
}

int PlayerCreatorScene::hitboxHandleAt(int mx, int my) const {
    if (!mHBInitialised) return -1;
    SDL_Rect r = normaliseRect(mHBRect);
    const int HSZ = HB_HANDLE_SZ;

    // Corner handles (priority over body)
    SDL_Rect corners[4] = {
        {r.x - HSZ/2,           r.y - HSZ/2,           HSZ, HSZ}, // TL=0
        {r.x + r.w - HSZ/2,     r.y - HSZ/2,           HSZ, HSZ}, // TR=1
        {r.x + r.w - HSZ/2,     r.y + r.h - HSZ/2,     HSZ, HSZ}, // BR=2
        {r.x - HSZ/2,           r.y + r.h - HSZ/2,     HSZ, HSZ}, // BL=3
    };
    for (int i = 0; i < 4; ++i)
        if (hit(corners[i], mx, my)) return i;

    // Body
    if (hit(r, mx, my)) return 4;
    return -1;
}

void PlayerCreatorScene::renderHitboxOverlay(SDL_Surface* surf) const {
    if (!mHBInitialised) return;
    SDL_Rect r = normaliseRect(mHBRect);  // true logical rect — never clip this

    // Semi-transparent fill — clip fill to the cell (not the sprite rect) so it
    // doesn't bleed outside the preview area. The collider can be smaller than the sprite.
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

    // Outline drawn at true rect position (may extend 1px outside preview — fine)
    outlineRect(surf, r, HB_COLOR, 2);

    // Center crosshair — shows the hitbox midpoint so you can verify body alignment
    {
        int cx = r.x + r.w / 2;
        int cy = r.y + r.h / 2;
        const int CL = 6; // crosshair arm length
        SDL_Rect hLine = {cx - CL, cy, CL * 2 + 1, 1};
        SDL_Rect vLine = {cx,      cy - CL, 1, CL * 2 + 1};
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(surf->format);
        Uint32 col = SDL_MapRGBA(fmt, nullptr, 255, 255, 80, 220);
        SDL_FillSurfaceRect(surf, &hLine, col);
        SDL_FillSurfaceRect(surf, &vLine, col);
    }

    // Corner handles — always drawn at true logical corners so click targets match visuals
    const int HSZ = HB_HANDLE_SZ;  // larger handles = easier to grab
    SDL_Rect corners[4] = {
        {r.x - HSZ/2,       r.y - HSZ/2,       HSZ, HSZ},  // TL
        {r.x + r.w - HSZ/2, r.y - HSZ/2,       HSZ, HSZ},  // TR
        {r.x + r.w - HSZ/2, r.y + r.h - HSZ/2, HSZ, HSZ},  // BR
        {r.x - HSZ/2,       r.y + r.h - HSZ/2, HSZ, HSZ},  // BL
    };
    for (auto& c : corners) {
        fillRect(surf, c, {255, 220, 80, 255});
        outlineRect(surf, c, {180, 150, 40, 255});
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Roster
// ─────────────────────────────────────────────────────────────────────────────

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
        // Only commit if we have a valid layout — avoid writing garbage on first load
        if (mHBInitialised && mPreviewCellRect.w > 0)
            commitHBToProfile(mSelectedSlot);
        // Clear old previews
        for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) mPreviews[i].reset();
        mProfile = std::move(loaded);
        mLoadedName  = mProfile.name;
        mWidthStr    = mProfile.spriteW > 0 ? std::to_string(mProfile.spriteW) : "120";
        mHeightStr   = mProfile.spriteH > 0 ? std::to_string(mProfile.spriteH) : "160";
        // Rebuild previews for slots that have a path.
        // rebuildPreview() safely skips missing/stale paths (e.g. Mac paths on WSL).
        for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i)
            rebuildPreview(i);
        mSelectedSlot = 0;
        recomputePreviewRect();
        initHBFromProfile(0);
        mAnimFrame = 0; mAnimTimer = 0;
        mDropMsg = "Loaded: " + mRoster[idx].name;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Text input
// ─────────────────────────────────────────────────────────────────────────────

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

// ─────────────────────────────────────────────────────────────────────────────
// Draw helpers
// ─────────────────────────────────────────────────────────────────────────────

void PlayerCreatorScene::fillRect(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
    SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
}

void PlayerCreatorScene::outlineRect(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t) {
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
    Uint32 col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
    SDL_Rect sides[4] = {{r.x,       r.y,       r.w, t},
                         {r.x,       r.y+r.h-t, r.w, t},
                         {r.x,       r.y,       t,   r.h},
                         {r.x+r.w-t, r.y,       t,   r.h}};
    for (auto& sr : sides) SDL_FillSurfaceRect(s, &sr, col);
}

void PlayerCreatorScene::drawText(SDL_Surface* s, const std::string& str,
                                   int x, int y, int ptSize, SDL_Color col) {
    if (str.empty()) return;
    TTF_Font* font = FontCache::Get(ptSize);
    if (!font) return;
    SDL_Surface* ts = TTF_RenderText_Blended(font, str.c_str(), 0, col);
    if (ts) { SDL_Rect dst = {x, y, ts->w, ts->h}; SDL_BlitSurface(ts, nullptr, s, &dst); SDL_DestroySurface(ts); }
}

void PlayerCreatorScene::drawTextCentered(SDL_Surface* s, const std::string& str,
                                           SDL_Rect r, int ptSize, SDL_Color col) {
    if (str.empty()) return;
    auto [tx, ty] = Text::CenterInRect(str, ptSize, r);
    drawText(s, str, tx, ty, ptSize, col);
}

void PlayerCreatorScene::blitScaled(SDL_Surface* dst, SDL_Surface* src, SDL_Rect dstRect) {
    if (!src || !dst) return;
    // Maintain aspect ratio, centre in dstRect
    float scaleX = (float)dstRect.w / src->w;
    float scaleY = (float)dstRect.h / src->h;
    float scale  = std::min(scaleX, scaleY);
    int   w      = (int)(src->w * scale);
    int   h      = (int)(src->h * scale);
    int   ox     = dstRect.x + (dstRect.w - w) / 2;
    int   oy     = dstRect.y + (dstRect.h - h) / 2;
    SDL_Rect dst2 = {ox, oy, w, h};
    SDL_BlitSurfaceScaled(src, nullptr, dst, &dst2, SDL_SCALEMODE_NEAREST);
}

SDL_Rect PlayerCreatorScene::normaliseRect(SDL_Rect r) {
    if (r.w < 0) { r.x += r.w; r.w = -r.w; }
    if (r.h < 0) { r.y += r.h; r.h = -r.h; }
    return r;
}
