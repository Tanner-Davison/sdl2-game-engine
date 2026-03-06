#include "TileAnimCreatorScene.hpp"
#include "TitleScene.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <charconv>
#include <filesystem>
#include <print>

namespace fs = std::filesystem;

static bool hit(const SDL_Rect& r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

// Numeric sort helper: extract leading integer from a filename stem.
// "1" -> 1,  "04" -> 4,  "frame_03" -> 3,  "abc" -> INT_MAX
static int leadingInt(const std::string& stem) {
    auto it = std::find_if(stem.begin(), stem.end(),
                           [](char c){ return std::isdigit((unsigned char)c); });
    if (it == stem.end()) return INT_MAX;
    int val = 0;
    for (; it != stem.end() && std::isdigit((unsigned char)*it); ++it)
        val = val * 10 + (*it - '0');
    return val;
}

// Compact float-to-string: "8.000000" -> "8", "12.5" -> "12.5"
static std::string fmtFloat(float f) {
    std::string s = std::to_string(f);
    s.erase(s.find_last_not_of('0') + 1, std::string::npos);
    if (!s.empty() && s.back() == '.') s.pop_back();
    return s;
}

// ---- Load / Unload ----------------------------------------------------------

void TileAnimCreatorScene::Load(Window& window) {
    mW      = window.GetWidth();
    mH      = window.GetHeight();
    mSDLWin = window.GetRaw();

    if (!fs::exists(ANIMATED_TILE_DIR))
        fs::create_directories(ANIMATED_TILE_DIR);

    mDef         = AnimatedTileDef{};
    mDef.name    = "MyAnimation";
    mDef.fps     = DEFAULT_FPS;
    mFpsEditStr  = fmtFloat(mDef.fps);
    mPaused      = false;
    mAnimFrame   = 0;
    mAnimTimer   = 0.0f;
    mSelectedFrame = -1;
    mFrameScroll   = 0;

    computeLayout();
    refreshRoster();
}

void TileAnimCreatorScene::Unload() {
    stopNameInput();
    stopFpsInput();
    clearFrames();
}

// ---- Layout -----------------------------------------------------------------

void TileAnimCreatorScene::computeLayout() {
    const int LEFT_W  = 230;
    const int RIGHT_W = 250;
    const int MID_W   = mW - LEFT_W - RIGHT_W - PAD * 4;

    mFramePanel  = {PAD,                          40, LEFT_W,  mH - 80};
    mCenterPanel = {PAD * 2 + LEFT_W,             40, MID_W,   mH - 80};
    mRosterPanel = {PAD * 3 + LEFT_W + MID_W,     40, RIGHT_W, mH - 80};

    int cy = mCenterPanel.y;

    mNameFieldRect = {mCenterPanel.x, cy + 36, MID_W - 2, 34};
    mFpsFieldRect  = {mCenterPanel.x, cy + 84, 110, 30};

    int previewTop = cy + 130;
    mPreviewRect   = {mCenterPanel.x + (MID_W - PREVIEW_SZ) / 2,
                      previewTop, PREVIEW_SZ, PREVIEW_SZ};

    int ctrlY = previewTop + PREVIEW_SZ + 10;
    mPauseBtn = {mCenterPanel.x, ctrlY, 100, 32};

    int dz1Y = ctrlY + 46;
    mDropZoneSingle = {mCenterPanel.x, dz1Y,      MID_W - 2, 48};
    mDropZoneFolder = {mCenterPanel.x, dz1Y + 58, MID_W - 2, 48};

    int botY = mCenterPanel.y + mCenterPanel.h - 46;
    mSaveBtnRect  = {mCenterPanel.x,               botY, 140, 40};
    mClearBtnRect = {mCenterPanel.x + 150,          botY, 100, 40};
    mBackBtnRect  = {mCenterPanel.x + MID_W - 112,  botY, 108, 40};

    int fbY = mFramePanel.y + mFramePanel.h - 42;
    mMoveUpBtn      = {mFramePanel.x + 4,              fbY, 54, 34};
    mMoveDownBtn    = {mFramePanel.x + 62,              fbY, 54, 34};
    mDeleteFrameBtn = {mFramePanel.x + LEFT_W - 64,     fbY, 58, 34};
}

// ---- Events -----------------------------------------------------------------

bool TileAnimCreatorScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) return false;

    // Drop hover
    if (e.type == SDL_EVENT_DROP_BEGIN)    { mDropHover = mFolderHover = true;  return true; }
    if (e.type == SDL_EVENT_DROP_COMPLETE) { mDropHover = mFolderHover = false; return true; }

    // File / folder drop
    if (e.type == SDL_EVENT_DROP_FILE || e.type == SDL_EVENT_DROP_TEXT) {
        mDropHover = mFolderHover = false;
        std::string dropped = e.drop.data ? e.drop.data : "";
        if (dropped.empty()) return true;
        fs::path p(dropped);
        if (fs::is_directory(p)) {
            importFolder(dropped);
        } else if (fs::is_regular_file(p)) {
            std::string ext = p.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".png") importSingleFrame(dropped);
            else               mDropMsg = "Only PNG files accepted.";
        }
        return true;
    }

    // Text input while a field is focused (typing / backspace)
    if (mNameActive || mFpsActive) {
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            std::string& target = mNameActive ? mDef.name : mFpsEditStr;
            for (char c : std::string(e.text.text)) {
                if (mNameActive) {
                    if (std::isalnum((unsigned char)c) || c == '-' || c == '_')
                        target += c;
                } else {
                    bool hasDot = (target.find('.') != std::string::npos);
                    if (std::isdigit((unsigned char)c) || (c == '.' && !hasDot))
                        target += c;
                }
            }
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            std::string& target = mNameActive ? mDef.name : mFpsEditStr;
            switch (e.key.key) {
                case SDLK_BACKSPACE:
                    if (!target.empty()) target.pop_back();
                    return true;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    if (mFpsActive)  stopFpsInput();
                    else             stopNameInput();
                    return true;
                case SDLK_ESCAPE:
                    if (mFpsActive)  stopFpsInput();
                    else             stopNameInput();
                    break; // fall through so Escape can be handled globally
                default: break;
            }
        }
        // NOTE: do NOT return here for mouse events -- let them fall through.
    }

    // Global keyboard shortcuts (work whether or not a field is focused)
    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
            case SDLK_ESCAPE:
                // Only go back if no field was just closed this event
                if (!mNameActive && !mFpsActive) mGoBack = true;
                break;
            case SDLK_SPACE:
                mPaused = !mPaused;
                break;
            case SDLK_UP:
                if (mSelectedFrame > 0) {
                    --mSelectedFrame;
                    if (mSelectedFrame < mFrameScroll) mFrameScroll = mSelectedFrame;
                }
                break;
            case SDLK_DOWN:
                if (mSelectedFrame < (int)mDef.framePaths.size() - 1)
                    ++mSelectedFrame;
                break;
            case SDLK_DELETE:
                if (!mNameActive && !mFpsActive &&
                    mSelectedFrame >= 0 &&
                    mSelectedFrame < (int)mDef.framePaths.size()) {
                    if (mFrameSurfs[mSelectedFrame]) SDL_DestroySurface(mFrameSurfs[mSelectedFrame]);
                    mDef.framePaths.erase(mDef.framePaths.begin() + mSelectedFrame);
                    mFrameSurfs.erase(mFrameSurfs.begin() + mSelectedFrame);
                    mSelectedFrame = std::min(mSelectedFrame, (int)mDef.framePaths.size() - 1);
                    mAnimFrame = 0; mAnimTimer = 0;
                }
                break;
            default: break;
        }
    }

    // Mouse clicks
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = (int)e.button.x;
        int my = (int)e.button.y;

        // Always commit active fields before processing any button click.
        // This ensures mDef.fps is up-to-date before Save/Pause/etc run.
        if (mFpsActive  && !hit(mFpsFieldRect,  mx, my)) stopFpsInput();
        if (mNameActive && !hit(mNameFieldRect, mx, my)) stopNameInput();

        // Name field
        if (hit(mNameFieldRect, mx, my)) { startNameInput(); return true; }

        // FPS field
        if (hit(mFpsFieldRect, mx, my)) { startFpsInput(); return true; }

        // Pause / play toggle
        if (hit(mPauseBtn, mx, my)) {
            mPaused = !mPaused;
            return true;
        }

        // Save
        if (hit(mSaveBtnRect, mx, my)) {
            if (mDef.name.empty()) {
                mSaveMsg = "Set a name first!";
            } else if (mDef.framePaths.empty()) {
                mSaveMsg = "Add at least one frame!";
            } else {
                std::string path = AnimatedTilePath(mDef.name);
                mSaveMsg = SaveAnimatedTileDef(mDef, path)
                         ? "Saved: " + mDef.name
                         : "Save failed!";
                if (mSaveMsg.rfind("Saved", 0) == 0) refreshRoster();
            }
            return true;
        }

        // Clear all frames
        if (hit(mClearBtnRect, mx, my)) {
            clearFrames();
            mDef.framePaths.clear();
            mFrameSurfs.clear();
            mSelectedFrame = -1;
            mAnimFrame = 0; mAnimTimer = 0;
            mDropMsg = "Frames cleared.";
            mSaveMsg.clear();
            return true;
        }

        // Back
        if (hit(mBackBtnRect, mx, my)) { mGoBack = true; return true; }

        // Move frame up
        if (hit(mMoveUpBtn, mx, my)) {
            if (mSelectedFrame > 0) {
                std::swap(mDef.framePaths[mSelectedFrame], mDef.framePaths[mSelectedFrame-1]);
                std::swap(mFrameSurfs[mSelectedFrame],     mFrameSurfs[mSelectedFrame-1]);
                --mSelectedFrame;
            }
            return true;
        }

        // Move frame down
        if (hit(mMoveDownBtn, mx, my)) {
            if (mSelectedFrame >= 0 && mSelectedFrame < (int)mDef.framePaths.size() - 1) {
                std::swap(mDef.framePaths[mSelectedFrame], mDef.framePaths[mSelectedFrame+1]);
                std::swap(mFrameSurfs[mSelectedFrame],     mFrameSurfs[mSelectedFrame+1]);
                ++mSelectedFrame;
            }
            return true;
        }

        // Delete selected frame button
        if (hit(mDeleteFrameBtn, mx, my)) {
            if (mSelectedFrame >= 0 && mSelectedFrame < (int)mDef.framePaths.size()) {
                if (mFrameSurfs[mSelectedFrame]) SDL_DestroySurface(mFrameSurfs[mSelectedFrame]);
                mDef.framePaths.erase(mDef.framePaths.begin() + mSelectedFrame);
                mFrameSurfs.erase(mFrameSurfs.begin() + mSelectedFrame);
                mSelectedFrame = std::min(mSelectedFrame, (int)mDef.framePaths.size() - 1);
                mAnimFrame = 0; mAnimTimer = 0;
            }
            return true;
        }

        // Frame list click
        {
            int visCount = (mFramePanel.h - 80) / FRAME_ROW_H;
            for (int i = 0; i < visCount; ++i) {
                int fi = i + mFrameScroll;
                if (fi >= (int)mDef.framePaths.size()) break;
                SDL_Rect r = {mFramePanel.x + 4,
                              mFramePanel.y + 36 + i * FRAME_ROW_H,
                              mFramePanel.w - 8, FRAME_ROW_H - 4};
                if (hit(r, mx, my)) { mSelectedFrame = fi; return true; }
            }
        }

        // Roster buttons
        int ry = mRosterPanel.y + 36;
        for (int i = mRosterScroll; i < (int)mRoster.size(); ++i) {
            auto& entry = mRoster[i];
            if (hit(entry.loadRect, mx, my)) { loadRosterEntry(i); return true; }
            if (hit(entry.delRect,  mx, my)) {
                try { fs::remove(entry.path); } catch (...) {}
                refreshRoster();
                return true;
            }
            ry += 52;
            if (ry > mRosterPanel.y + mRosterPanel.h - 10) break;
        }
    }

    // Scroll
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        float fmx, fmy;
        SDL_GetMouseState(&fmx, &fmy);
        int mx = (int)fmx, my = (int)fmy;
        int maxVis = (mFramePanel.h - 80) / FRAME_ROW_H;
        if (hit(mFramePanel, mx, my))
            mFrameScroll = std::clamp(mFrameScroll - (int)e.wheel.y,
                                      0, std::max(0, (int)mDef.framePaths.size() - maxVis));
        if (hit(mRosterPanel, mx, my))
            mRosterScroll = std::clamp(mRosterScroll - (int)e.wheel.y,
                                       0, std::max(0, (int)mRoster.size() - 1));
    }

    return true;
}

// ---- Update -----------------------------------------------------------------

void TileAnimCreatorScene::Update(float dt) {
    if (mPaused || mDef.framePaths.empty()) return;
    mAnimTimer += dt;
    float frameDur = (mDef.fps > 0.0f) ? 1.0f / mDef.fps : 0.125f;
    while (mAnimTimer >= frameDur) {
        mAnimTimer -= frameDur;
        mAnimFrame = (mAnimFrame + 1) % (int)mDef.framePaths.size();
    }
}

// ---- Render -----------------------------------------------------------------

void TileAnimCreatorScene::Render(Window& window) {
    window.Render();
    SDL_Renderer* ren = window.GetRenderer();
    SDL_Surface* s = SDL_CreateSurface(mW, mH, SDL_PIXELFORMAT_ARGB8888);
    if (!s) { window.Update(); return; }

    fillRect(s, {0, 0, mW, mH}, BG);
    drawTextCentered(s, "Tile Animation Creator", {0, 4, mW, 32}, 22, {255, 180, 60, 255});

    // LEFT: frame list
    fillRect(s, mFramePanel, PANEL_BG);
    outlineRect(s, mFramePanel, PANEL_OUT);
    drawText(s, "Frames  (" + std::to_string(mDef.framePaths.size()) + ")",
             mFramePanel.x + 6, mFramePanel.y + 8, 14, ACCENT);

    int maxVis = (mFramePanel.h - 80) / FRAME_ROW_H;
    for (int i = 0; i < maxVis; ++i) {
        int fi = i + mFrameScroll;
        if (fi >= (int)mDef.framePaths.size()) break;
        bool isSel      = (fi == mSelectedFrame);
        bool isPlaying  = (!mPaused && fi == mAnimFrame);
        SDL_Rect rowR = {mFramePanel.x + 4,
                         mFramePanel.y + 36 + i * FRAME_ROW_H,
                         mFramePanel.w - 8, FRAME_ROW_H - 4};
        fillRect(s, rowR, isSel ? SEL_BG : SDL_Color{36, 40, 60, 255});
        if (isPlaying) outlineRect(s, rowR, ACCENT, 2);
        else           outlineRect(s, rowR, PANEL_OUT);

        SDL_Surface* surf = (fi < (int)mFrameSurfs.size()) ? mFrameSurfs[fi] : nullptr;
        if (surf) {
            SDL_Rect thumb = {rowR.x + 2, rowR.y + 2, FRAME_ROW_H - 8, FRAME_ROW_H - 8};
            blitScaled(s, surf, thumb);
        }
        std::string fname = fs::path(mDef.framePaths[fi]).filename().string();
        if ((int)fname.size() > 18) fname = fname.substr(0, 16) + "..";
        drawText(s, std::to_string(fi + 1) + ". " + fname,
                 rowR.x + FRAME_ROW_H - 2,
                 rowR.y + (FRAME_ROW_H - 20) / 2,
                 12, isSel ? SDL_Color{255, 255, 255, 255} : SDL_Color{180, 190, 210, 255});
    }
    if ((int)mDef.framePaths.size() > maxVis) {
        int total = (int)mDef.framePaths.size();
        drawText(s, std::to_string(mFrameScroll+1) + "-"
                    + std::to_string(std::min(mFrameScroll+maxVis, total))
                    + " / " + std::to_string(total),
                 mFramePanel.x + 4, mFramePanel.y + mFramePanel.h - 54, 10, {100,110,140,255});
    }

    fillRect(s, mMoveUpBtn,      BTN_MOVE); outlineRect(s, mMoveUpBtn,      PANEL_OUT);
    fillRect(s, mMoveDownBtn,    BTN_MOVE); outlineRect(s, mMoveDownBtn,    PANEL_OUT);
    fillRect(s, mDeleteFrameBtn, BTN_DEL);  outlineRect(s, mDeleteFrameBtn, PANEL_OUT);
    drawTextCentered(s, "Up",    mMoveUpBtn,      12);
    drawTextCentered(s, "Down",  mMoveDownBtn,    12);
    drawTextCentered(s, "Del",   mDeleteFrameBtn, 12);

    // CENTRE panel
    fillRect(s, mCenterPanel, PANEL_BG);
    outlineRect(s, mCenterPanel, PANEL_OUT);

    // Name field
    drawText(s, "Animation Name:", mCenterPanel.x + 4, mCenterPanel.y + 10, 13, {160,170,220,255});
    fillRect(s, mNameFieldRect, {18, 18, 32, 255});
    outlineRect(s, mNameFieldRect,
                mNameActive ? SDL_Color{100,150,255,255} : PANEL_OUT, 2);
    drawText(s, mDef.name + (mNameActive ? "|" : ""),
             mNameFieldRect.x + 8, mNameFieldRect.y + 7, 17);

    // FPS field
    drawText(s, "FPS:", mCenterPanel.x + 4, mCenterPanel.y + 84, 13, {160,170,220,255});
    fillRect(s, mFpsFieldRect, {18, 18, 32, 255});
    outlineRect(s, mFpsFieldRect,
                mFpsActive ? SDL_Color{100,150,255,255} : PANEL_OUT, 2);
    drawText(s, mFpsEditStr + (mFpsActive ? "|" : ""),
             mFpsFieldRect.x + 6, mFpsFieldRect.y + 6, 16);
    // Show live FPS value next to field when not editing
    if (!mFpsActive) {
        drawText(s, "= " + fmtFloat(mDef.fps) + " fps",
                 mFpsFieldRect.x + mFpsFieldRect.w + 8,
                 mFpsFieldRect.y + 6, 13, {120, 180, 120, 255});
    }

    // Preview
    fillRect(s, mPreviewRect, {8, 10, 18, 255});
    outlineRect(s, mPreviewRect,
                mPaused ? SDL_Color{160, 100, 60, 255} : SDL_Color{60, 80, 120, 255});

    if (!mDef.framePaths.empty()) {
        int fi = std::min(mAnimFrame, (int)mDef.framePaths.size() - 1);
        SDL_Surface* surf = (fi < (int)mFrameSurfs.size()) ? mFrameSurfs[fi] : nullptr;
        if (surf) blitScaled(s, surf, mPreviewRect);
        drawText(s,
                 std::to_string(fi+1) + " / " + std::to_string(mDef.framePaths.size()),
                 mPreviewRect.x + 4, mPreviewRect.y + PREVIEW_SZ - 16, 11,
                 {140,150,180,255});
    } else {
        drawTextCentered(s, "No frames yet", mPreviewRect, 14, {70,80,100,255});
    }

    // Pause button — colour reflects state clearly
    SDL_Color pauseCol = mPaused ? SDL_Color{200, 130, 40, 255}
                                 : SDL_Color{40,  150,  60, 255};
    fillRect(s, mPauseBtn, pauseCol);
    outlineRect(s, mPauseBtn, {255, 255, 255, 60});
    drawTextCentered(s, mPaused ? "  Play" : "  Pause", mPauseBtn, 13,
                     {255, 255, 255, 255});

    // Drop zones
    fillRect(s, mDropZoneSingle, mDropHover ? DROP_HOV : DROP_IDLE);
    outlineRect(s, mDropZoneSingle,
                mDropHover ? SDL_Color{100,180,255,255} : PANEL_OUT, 2);
    drawTextCentered(s, "Drop a PNG  ->  add one frame", mDropZoneSingle, 13,
                     mDropHover ? SDL_Color{200,230,255,255} : SDL_Color{140,150,180,255});

    fillRect(s, mDropZoneFolder, mFolderHover ? DROP_HOV : SDL_Color{30,50,90,255});
    outlineRect(s, mDropZoneFolder,
                mFolderHover ? SDL_Color{100,180,255,255} : SDL_Color{50,80,140,255}, 2);
    drawTextCentered(s, "Drop a FOLDER  ->  import all PNGs", mDropZoneFolder, 13,
                     mFolderHover ? SDL_Color{200,230,255,255} : SDL_Color{120,140,180,255});

    int msgY = mDropZoneFolder.y + mDropZoneFolder.h + 8;
    if (!mDropMsg.empty())
        drawText(s, mDropMsg, mCenterPanel.x + 4, msgY,      12, {160,210,160,255});
    if (!mSaveMsg.empty())
        drawText(s, mSaveMsg, mCenterPanel.x + 4, msgY + 18, 12,
                 mSaveMsg.rfind("Saved", 0) == 0 ? SDL_Color{100,255,120,255}
                                                  : SDL_Color{255,120,120,255});

    // Bottom action buttons
    fillRect(s, mSaveBtnRect,  BTN_SAVE);  outlineRect(s, mSaveBtnRect,  {60,200,100,255});
    fillRect(s, mClearBtnRect, BTN_CLEAR); outlineRect(s, mClearBtnRect, {200,80,80,255});
    fillRect(s, mBackBtnRect,  BTN_BACK);  outlineRect(s, mBackBtnRect,  {100,100,200,255});
    drawTextCentered(s, "Save Animation", mSaveBtnRect,  15);
    drawTextCentered(s, "Clear All",      mClearBtnRect, 13);
    drawTextCentered(s, "< Back",         mBackBtnRect,  15);

    // RIGHT: roster
    fillRect(s, mRosterPanel, PANEL_BG);
    outlineRect(s, mRosterPanel, PANEL_OUT);
    drawText(s, "Saved Animations",
             mRosterPanel.x + 6, mRosterPanel.y + 8, 14, ACCENT);

    int ry = mRosterPanel.y + 34;
    if (mRoster.empty()) {
        drawText(s, "None yet.", mRosterPanel.x + 10, ry + 10, 12, {70,80,100,255});
    } else {
        for (int i = mRosterScroll; i < (int)mRoster.size(); ++i) {
            auto& entry = mRoster[i];
            if (ry + 48 > mRosterPanel.y + mRosterPanel.h) break;
            fillRect(s, {mRosterPanel.x+4, ry, mRosterPanel.w-8, 46}, {34,38,58,255});
            drawText(s, entry.name, mRosterPanel.x + 10, ry + 4, 14, {220,200,150,255});
            entry.loadRect = {mRosterPanel.x+4,                   ry+24, 80, 18};
            entry.delRect  = {mRosterPanel.x + mRosterPanel.w-54, ry+24, 50, 18};
            fillRect(s, entry.loadRect, BTN_LOAD);
            fillRect(s, entry.delRect,  BTN_DEL);
            drawTextCentered(s, "Load",   entry.loadRect, 11);
            drawTextCentered(s, "Delete", entry.delRect,  11, {255,200,200,255});
            ry += 52;
        }
    }

    // Upload the completed surface to GPU and present
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, s);
    SDL_DestroySurface(s);
    if (tex) {
        SDL_RenderTexture(ren, tex, nullptr, nullptr);
        SDL_DestroyTexture(tex);
    }
    window.Update();
}

// ---- NextScene --------------------------------------------------------------

std::unique_ptr<Scene> TileAnimCreatorScene::NextScene() {
    if (mGoBack) { mGoBack = false; return std::make_unique<TitleScene>(); }
    return nullptr;
}

// ---- Import helpers ---------------------------------------------------------

void TileAnimCreatorScene::importFolder(const std::string& dir) {
    std::vector<fs::path> pngs;
    try {
        for (const auto& entry : fs::directory_iterator(dir)) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".png") pngs.push_back(entry.path());
        }
    } catch (...) { mDropMsg = "Cannot read that folder."; return; }

    if (pngs.empty()) { mDropMsg = "No PNGs found in folder."; return; }

    std::sort(pngs.begin(), pngs.end(), [](const fs::path& a, const fs::path& b) {
        int ia = leadingInt(a.stem().string());
        int ib = leadingInt(b.stem().string());
        return (ia != ib) ? ia < ib : a.stem().string() < b.stem().string();
    });

    if (mDef.name == "MyAnimation" && mDef.framePaths.empty())
        mDef.name = fs::path(dir).filename().string();

    clearFrames();
    mDef.framePaths.clear();
    mFrameSurfs.clear();

    for (const auto& p : pngs) {
        SDL_Surface* raw = IMG_Load(p.string().c_str());
        if (!raw) { mDef.framePaths.push_back(p.string()); mFrameSurfs.push_back(nullptr); continue; }
        SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_BLEND);
        mDef.framePaths.push_back(p.string());
        mFrameSurfs.push_back(conv);
    }

    mSelectedFrame = 0;
    mAnimFrame = 0; mAnimTimer = 0;
    mPaused    = false;
    mDropMsg   = "Imported " + std::to_string(pngs.size()) + " frames from "
               + fs::path(dir).filename().string();
    mSaveMsg.clear();
}

void TileAnimCreatorScene::importSingleFrame(const std::string& path) {
    SDL_Surface* raw = IMG_Load(path.c_str());
    if (!raw) { mDropMsg = "Failed to load: " + fs::path(path).filename().string(); return; }
    SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(raw);
    SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_BLEND);
    mDef.framePaths.push_back(path);
    mFrameSurfs.push_back(conv);
    mSelectedFrame = (int)mDef.framePaths.size() - 1;
    mDropMsg = "Added: " + fs::path(path).filename().string()
             + "  (" + std::to_string(mDef.framePaths.size()) + " total)";
    mSaveMsg.clear();
}

void TileAnimCreatorScene::clearFrames() {
    for (auto* surf : mFrameSurfs)
        if (surf) SDL_DestroySurface(surf);
    mFrameSurfs.clear();
}

// ---- Roster -----------------------------------------------------------------

void TileAnimCreatorScene::refreshRoster() {
    mRoster.clear();
    for (const auto& p : ScanAnimatedTiles()) {
        RosterEntry entry;
        entry.name = p.stem().string();
        entry.path = p.string();
        mRoster.push_back(std::move(entry));
    }
    mRosterScroll = 0;
}

void TileAnimCreatorScene::loadRosterEntry(int idx) {
    if (idx < 0 || idx >= (int)mRoster.size()) return;
    AnimatedTileDef loaded;
    if (!LoadAnimatedTileDef(mRoster[idx].path, loaded)) return;

    clearFrames();
    mDef = std::move(loaded);
    mFrameSurfs.clear();
    mFrameSurfs.reserve(mDef.framePaths.size());
    for (const auto& p : mDef.framePaths) {
        SDL_Surface* raw = IMG_Load(p.c_str());
        if (!raw) { mFrameSurfs.push_back(nullptr); continue; }
        SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_BLEND);
        mFrameSurfs.push_back(conv);
    }
    mFpsEditStr    = fmtFloat(mDef.fps);
    mSelectedFrame = 0;
    mAnimFrame     = 0;
    mAnimTimer     = 0;
    mPaused        = false;
    mDropMsg       = "Loaded: " + mDef.name;
    mSaveMsg.clear();
}

// ---- Text input helpers -----------------------------------------------------

void TileAnimCreatorScene::startNameInput() {
    if (mFpsActive) stopFpsInput();
    if (!mNameActive) { mNameActive = true; SDL_StartTextInput(mSDLWin); }
}
void TileAnimCreatorScene::stopNameInput() {
    if (!mNameActive) return;
    mNameActive = false;
    SDL_StopTextInput(mSDLWin);
    if (mDef.name.empty()) mDef.name = "MyAnimation";
}
void TileAnimCreatorScene::startFpsInput() {
    if (mNameActive) stopNameInput();
    if (!mFpsActive) {
        mFpsActive  = true;
        mFpsEditStr = fmtFloat(mDef.fps); // pre-fill with current value
        SDL_StartTextInput(mSDLWin);
    }
}
void TileAnimCreatorScene::stopFpsInput() {
    if (!mFpsActive) return;
    mFpsActive = false;
    SDL_StopTextInput(mSDLWin);
    // Parse the edited string into mDef.fps
    if (!mFpsEditStr.empty()) {
        try {
            float v = std::stof(mFpsEditStr);
            if (v > 0.0f && v <= 120.0f) mDef.fps = v;
        } catch (...) {}
    }
    mFpsEditStr = fmtFloat(mDef.fps); // normalise display string
    // Reset timer so the new fps takes effect immediately
    mAnimTimer = 0.0f;
}

// ---- Draw helpers -----------------------------------------------------------

void TileAnimCreatorScene::fillRect(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
    SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
}
void TileAnimCreatorScene::outlineRect(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t) {
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
    Uint32 col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
    SDL_Rect sides[4] = {{r.x,       r.y,       r.w, t},
                         {r.x,       r.y+r.h-t, r.w, t},
                         {r.x,       r.y,       t,   r.h},
                         {r.x+r.w-t, r.y,       t,   r.h}};
    for (auto& sr : sides) SDL_FillSurfaceRect(s, &sr, col);
}
void TileAnimCreatorScene::drawText(SDL_Surface* s, const std::string& str,
                                     int x, int y, int sz, SDL_Color col) {
    if (str.empty()) return;
    TTF_Font* font = FontCache::Get(sz);
    if (!font) return;
    SDL_Surface* ts = TTF_RenderText_Blended(font, str.c_str(), 0, col);
    if (ts) { SDL_Rect dst = {x, y, ts->w, ts->h}; SDL_BlitSurface(ts, nullptr, s, &dst); SDL_DestroySurface(ts); }
}
void TileAnimCreatorScene::drawTextCentered(SDL_Surface* s, const std::string& str,
                                             SDL_Rect r, int sz, SDL_Color col) {
    if (str.empty()) return;
    auto [tx, ty] = Text::CenterInRect(str, sz, r);
    drawText(s, str, tx, ty, sz, col);
}
void TileAnimCreatorScene::blitScaled(SDL_Surface* dst, SDL_Surface* src, SDL_Rect dr) {
    if (!src || !dst) return;
    float sc = std::min((float)dr.w / src->w, (float)dr.h / src->h);
    int   w  = (int)(src->w * sc);
    int   h  = (int)(src->h * sc);
    SDL_Rect dest = {dr.x + (dr.w - w)/2, dr.y + (dr.h - h)/2, w, h};
    SDL_BlitSurfaceScaled(src, nullptr, dst, &dest, SDL_SCALEMODE_NEAREST);
}
