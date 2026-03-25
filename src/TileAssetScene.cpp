#include "TileAssetScene.hpp"
#include "TitleScene.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <algorithm>
#include <filesystem>
#include <print>

namespace fs = std::filesystem;

// ---- Scene interface --------------------------------------------------------

void TileAssetScene::Load(Window& window) {
    mW      = window.GetWidth();
    mH      = window.GetHeight();
    mSDLWin = window.GetRaw();

    if (!fs::exists(TILE_ROOT))
        fs::create_directories(TILE_ROOT);

    mTileRoot = fs::path(TILE_ROOT).lexically_normal().string();
    mCurrentDir = mTileRoot;
    scanDir(mCurrentDir);
}

void TileAssetScene::Unload() {
    if (mNamingFolder) SDL_StopTextInput(mSDLWin);
    freeThumbs();
}

// ---- Scan directory ---------------------------------------------------------

void TileAssetScene::scanDir(std::string dir) {
    freeThumbs();
    mEntries.clear();
    mSelected.clear();
    mScroll = 0;
    mDragging = false;
    mMarquee  = false;
    // Normalize path to avoid mixed representations (e.g. "./foo" vs "foo")
    mCurrentDir = fs::path(dir).lexically_normal().string();

    // Guard: if directory doesn't exist, just show empty
    std::error_code ec;
    if (!fs::is_directory(mCurrentDir, ec) || ec) {
        std::print("[TileAssets] scanDir: NOT a directory: '{}' ec={}\n", dir, ec.message());
        return;
    }

    std::vector<fs::path> dirs, files;
    try {
        for (const auto& e : fs::directory_iterator(mCurrentDir)) {
            if (e.is_directory())
                dirs.push_back(e.path());
            else {
                auto ext = e.path().extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".png") files.push_back(e.path());
            }
        }
    } catch (const std::exception& ex) {
        std::print("[TileAssets] scanDir exception: {}\n", ex.what());
    } catch (...) {
        std::print("[TileAssets] scanDir unknown exception\n");
    }

    std::sort(dirs.begin(), dirs.end());
    std::sort(files.begin(), files.end());

    for (const auto& d : dirs) {
        TileEntry e;
        e.path  = d.lexically_normal().string();
        e.name  = d.filename().string();
        e.isDir = true;
        mEntries.push_back(std::move(e));
    }
    for (const auto& f : files) {
        TileEntry e;
        e.path  = f.lexically_normal().string();
        e.name  = f.filename().string();
        e.isDir = false;
        e.thumb = makeThumbnail(e.path, THUMB_SZ);
        mEntries.push_back(std::move(e));
    }
    std::print("[TileAssets] scanDir: '{}' -> {} dirs, {} files\n", dir, dirs.size(), files.size());
}

void TileAssetScene::freeThumbs() {
    for (auto& e : mEntries)
        if (e.thumb) { SDL_DestroySurface(e.thumb); e.thumb = nullptr; }
}

SDL_Surface* TileAssetScene::makeThumbnail(const std::string& path, int sz) {
    SDL_Surface* raw = IMG_Load(path.c_str());
    if (!raw) return nullptr;
    SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
    SDL_DestroySurface(raw);
    if (!conv) return nullptr;
    if (conv->w == sz && conv->h == sz) return conv;
    float scale = std::min((float)sz / conv->w, (float)sz / conv->h);
    int tw = std::max(1, (int)(conv->w * scale));
    int th = std::max(1, (int)(conv->h * scale));
    SDL_Surface* thumb = SDL_CreateSurface(tw, th, SDL_PIXELFORMAT_ARGB8888);
    if (!thumb) { SDL_DestroySurface(conv); return nullptr; }
    SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_NONE);
    SDL_BlitSurfaceScaled(conv, nullptr, thumb, nullptr, SDL_SCALEMODE_NEAREST);
    SDL_DestroySurface(conv);
    return thumb;
}

int TileAssetScene::entryAt(int mx, int my) const {
    for (int i = 0; i < (int)mEntries.size(); ++i)
        if (hit(mEntries[i].rect, mx, my)) return i;
    return -1;
}

int TileAssetScene::folderAt(int mx, int my) const {
    for (int i = 0; i < (int)mEntries.size(); ++i)
        if (mEntries[i].isDir && hit(mEntries[i].rect, mx, my)) return i;
    return -1;
}

void TileAssetScene::moveSelectionToFolder(int folderIdx) {
    if (folderIdx < 0 || folderIdx >= (int)mEntries.size()) return;
    if (!mEntries[folderIdx].isDir) return;
    std::string destDir = fs::path(mEntries[folderIdx].path).lexically_normal().string();
    int moved = 0;
    for (int idx : mSelected) {
        if (idx == folderIdx) continue; // don't move folder into itself
        if (idx < 0 || idx >= (int)mEntries.size()) continue;
        const auto& entry = mEntries[idx];
        fs::path dst = fs::path(destDir) / fs::path(entry.path).filename();
        std::error_code ec;
        fs::rename(entry.path, dst, ec);
        if (!ec) ++moved;
    }
    mStatusMsg = "Moved " + std::to_string(moved) + " item(s) to " + mEntries[folderIdx].name;
    mSelected.clear();
    scanDir(mCurrentDir);
}

// ---- Events -----------------------------------------------------------------

bool TileAssetScene::HandleEvent(SDL_Event& e) {
    if (e.type == SDL_EVENT_QUIT) return false;

    // New folder naming
    if (mNamingFolder) {
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            for (char c : std::string(e.text.text))
                if (std::isalnum((unsigned char)c) || c == '-' || c == '_' || c == ' ')
                    mNewFolderName += c;
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_BACKSPACE && !mNewFolderName.empty()) {
                mNewFolderName.pop_back(); return true;
            }
            if (e.key.key == SDLK_RETURN && !mNewFolderName.empty()) {
                std::string newPath = mCurrentDir + "/" + mNewFolderName;
                std::error_code ec;
                fs::create_directory(newPath, ec);
                mStatusMsg = ec ? "Failed to create folder" : ("Created: " + mNewFolderName);
                mNamingFolder = false;
                mNewFolderName.clear();
                SDL_StopTextInput(mSDLWin);
                scanDir(mCurrentDir);
                return true;
            }
            if (e.key.key == SDLK_ESCAPE) {
                mNamingFolder = false;
                mNewFolderName.clear();
                SDL_StopTextInput(mSDLWin);
                return true;
            }
        }
        return true;
    }

    // Delete confirmation
    if (mDelConfirm) {
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
            mDelConfirm = false; return true;
        }
        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (hit(mDelYesRect, mx, my)) {
                if (mDelMulti) {
                    int count = 0;
                    // Delete in reverse order so indices stay valid
                    std::vector<int> sorted(mSelected.begin(), mSelected.end());
                    std::sort(sorted.rbegin(), sorted.rend());
                    for (int idx : sorted) {
                        if (idx < 0 || idx >= (int)mEntries.size()) continue;
                        std::error_code ec;
                        if (mEntries[idx].isDir) fs::remove_all(mEntries[idx].path, ec);
                        else                     fs::remove(mEntries[idx].path, ec);
                        if (!ec) ++count;
                    }
                    mStatusMsg = "Deleted " + std::to_string(count) + " item(s)";
                } else {
                    std::error_code ec;
                    if (mDelIsDir) fs::remove_all(mDelTarget, ec);
                    else           fs::remove(mDelTarget, ec);
                    mStatusMsg = ec ? "Delete failed" : ("Deleted: " + fs::path(mDelTarget).filename().string());
                }
                mDelConfirm = false;
                mDelMulti = false;
                mSelected.clear();
                scanDir(mCurrentDir);
                return true;
            }
            if (hit(mDelNoRect, mx, my)) {
                mDelConfirm = false; mDelMulti = false; return true;
            }
        }
        return true;
    }

    // Keyboard
    if (e.type == SDL_EVENT_KEY_DOWN) {
        if (e.key.key == SDLK_ESCAPE) {
            if (!mSelected.empty()) {
                mSelected.clear(); // first press clears selection
            } else if (mCurrentDir != mTileRoot) {
                std::string parent = fs::path(mCurrentDir).parent_path().string();
                scanDir(parent);
            } else {
                mGoBack = true;
            }
            return true;
        }
        if (e.key.key == SDLK_DELETE || e.key.key == SDLK_BACKSPACE) {
            if (!mSelected.empty()) {
                mDelConfirm = true;
                mDelMulti = true;
                return true;
            }
        }
        // Ctrl+A select all
        if (e.key.key == SDLK_A && (SDL_GetModState() & SDL_KMOD_GUI)) {
            mSelected.clear();
            for (int i = 0; i < (int)mEntries.size(); ++i) mSelected.insert(i);
            return true;
        }
    }

    // External drag and drop (from Finder/Explorer)
    if (e.type == SDL_EVENT_DROP_BEGIN) { mDropHover = true; return true; }
    if (e.type == SDL_EVENT_DROP_COMPLETE) { mDropHover = false; return true; }
    if (e.type == SDL_EVENT_DROP_FILE || e.type == SDL_EVENT_DROP_TEXT) {
        mDropHover = false;
        std::string dropped = e.drop.data ? e.drop.data : "";
        if (!dropped.empty()) {
            fs::path src(dropped);
            std::error_code ec;
            if (fs::is_regular_file(src, ec) && !ec) {
                auto ext = src.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                if (ext == ".png") {
                    fs::path dst = fs::path(mCurrentDir) / src.filename();
                    fs::copy_file(src, dst, fs::copy_options::skip_existing, ec);
                    mStatusMsg = ec ? "Copy failed" : ("Added: " + src.filename().string());
                    scanDir(mCurrentDir);
                } else {
                    mStatusMsg = "Only PNG files supported";
                }
            } else if (fs::is_directory(src, ec) && !ec) {
                fs::path dst = fs::path(mCurrentDir) / src.filename();
                fs::copy(src, dst, fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
                mStatusMsg = ec ? "Copy failed" : ("Added folder: " + src.filename().string());
                scanDir(mCurrentDir);
            }
        }
        return true;
    }

    // Mouse wheel
    if (e.type == SDL_EVENT_MOUSE_WHEEL) {
        mScroll = std::clamp(mScroll - (int)e.wheel.y * 40, 0, std::max(0, mMaxScroll));
        return true;
    }

    // Mouse down
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = (int)e.button.x, my = (int)e.button.y;

        // Buttons first
        if (hit(mBackBtnRect, mx, my)) {
            if (mCurrentDir != mTileRoot) {
                std::string parent = fs::path(mCurrentDir).parent_path().string();
                scanDir(parent);
            }
            else
                mGoBack = true;
            return true;
        }
        if (hit(mUpBtnRect, mx, my) && mCurrentDir != mTileRoot) {
            scanDir(TILE_ROOT);
            return true;
        }
        if (hit(mNewFolderBtnRect, mx, my)) {
            mNamingFolder = true;
            mNewFolderName.clear();
            SDL_StartTextInput(mSDLWin);
            return true;
        }

        // Delete buttons on individual entries
        for (int i = 0; i < (int)mEntries.size(); ++i) {
            if (hit(mEntries[i].delRect, mx, my)) {
                mDelConfirm = true;
                mDelMulti = false;
                mDelTarget = mEntries[i].path;
                mDelIsDir  = mEntries[i].isDir;
                return true;
            }
        }

        // Check if clicking on an entry
        int clicked = entryAt(mx, my);
        bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
        bool cmd   = (SDL_GetModState() & SDL_KMOD_GUI) != 0;

        if (clicked >= 0) {
            if (shift || cmd) {
                // Toggle selection
                if (mSelected.count(clicked)) mSelected.erase(clicked);
                else                          mSelected.insert(clicked);
            } else if (mSelected.count(clicked)) {
                // Clicked on already-selected item: begin drag
                mDragging   = true;
                mDragStartX = mx;
                mDragStartY = my;
                mDragCurX   = mx;
                mDragCurY   = my;
            } else {
                // Click on unselected item: select it (double-click opens folders)
                mSelected.clear();
                mSelected.insert(clicked);
                // If it's a folder and user single-clicks, we start potential drag
                // Double-click to enter is handled in mouse up with distance check
                mDragging   = true;
                mDragStartX = mx;
                mDragStartY = my;
                mDragCurX   = mx;
                mDragCurY   = my;
            }
        } else {
            // Clicked on empty space: start marquee selection
            if (!shift && !cmd) mSelected.clear();
            mMarquee    = true;
            mDragStartX = mx;
            mDragStartY = my;
            mDragCurX   = mx;
            mDragCurY   = my;
        }
        return true;
    }

    // Mouse motion
    if (e.type == SDL_EVENT_MOUSE_MOTION) {
        int mx = (int)e.motion.x, my = (int)e.motion.y;
        if (mDragging) {
            mDragCurX = mx;
            mDragCurY = my;
        }
        if (mMarquee) {
            mDragCurX = mx;
            mDragCurY = my;
            // Update marquee rect
            int x1 = std::min(mDragStartX, mDragCurX);
            int y1 = std::min(mDragStartY, mDragCurY);
            int x2 = std::max(mDragStartX, mDragCurX);
            int y2 = std::max(mDragStartY, mDragCurY);
            mMarqueeRect = {x1, y1, x2 - x1, y2 - y1};
            // Select entries that intersect marquee
            bool shift = (SDL_GetModState() & SDL_KMOD_SHIFT) != 0;
            if (!shift) mSelected.clear();
            for (int i = 0; i < (int)mEntries.size(); ++i) {
                const auto& r = mEntries[i].rect;
                if (r.w <= 0 || r.h <= 0) continue;
                // AABB intersection
                if (r.x < x2 && r.x + r.w > x1 && r.y < y2 && r.y + r.h > y1)
                    mSelected.insert(i);
            }
        }
    }

    // Mouse up
    if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
        int mx = (int)e.button.x, my = (int)e.button.y;

        if (mDragging) {
            int dx = mx - mDragStartX;
            int dy = my - mDragStartY;
            int dist = dx * dx + dy * dy;
            if (dist > 100) { // moved enough to be a drag
                // Check if dropped on a folder
                int target = folderAt(mx, my);
                if (target >= 0 && !mSelected.count(target)) {
                    moveSelectionToFolder(target);
                }
            } else {
                // It was a click, not a drag
                int clicked = entryAt(mx, my);
                if (clicked >= 0 && mEntries[clicked].isDir && mSelected.size() == 1 && mSelected.count(clicked)) {
                    // Single click on folder with no drag: enter it
                    // MUST copy the path — scanDir clears mEntries, which
                    // would destroy the string while the reference is live.
                    std::string folderPath = mEntries[clicked].path;
                    std::print("[TileAssets] Entering folder: '{}'\n", folderPath);
                    scanDir(folderPath);
                    mDragging = false;
                    return true;
                }
            }
            mDragging = false;
        }
        if (mMarquee) {
            mMarquee = false;
            mMarqueeRect = {};
        }
    }

    return true;
}

// ---- Update -----------------------------------------------------------------

void TileAssetScene::Update(float /*dt*/) {}

// ---- Render -----------------------------------------------------------------

void TileAssetScene::Render(Window& window, float /*alpha*/) {
    window.Render();
    SDL_Renderer* ren = window.GetRenderer();
    SDL_Surface* s = SDL_CreateSurface(mW, mH, SDL_PIXELFORMAT_ARGB8888);
    if (!s) { window.Update(); return; }

    fillRect(s, {0, 0, mW, mH}, BG);

    // Title
    drawTextCentered(s, "Tile Assets", {0, 4, mW, 32}, 24, {220, 200, 120, 255});

    // Breadcrumb
    drawText(s, mCurrentDir, PANEL_PAD, 38, 12, {140, 150, 180, 255});

    // Selection count
    if (!mSelected.empty()) {
        std::string selInfo = std::to_string(mSelected.size()) + " selected";
        drawText(s, selInfo, mW - PANEL_PAD - 120, 38, 12, {100, 200, 255, 255});
    }

    // Content panel
    const int topY = 56;
    const int botH = 80;
    mContentPanel = {PANEL_PAD, topY, mW - PANEL_PAD * 2, mH - topY - botH};
    fillRect(s, mContentPanel, PANEL_BG);
    outlineRect(s, mContentPanel, PANEL_OUT);

    // Grid layout
    const int cellW  = THUMB_SZ + CELL_PAD * 2;
    const int cellH  = THUMB_SZ + 28;
    const int cols   = std::max(1, (mContentPanel.w - 8) / cellW);
    const int startX = mContentPanel.x + 4;
    const int startY = mContentPanel.y + 4;

    int totalRows = mEntries.empty() ? 0 : ((int)mEntries.size() + cols - 1) / cols;
    int contentH  = totalRows * cellH + 8;
    mMaxScroll    = std::max(0, contentH - mContentPanel.h);

    for (int i = 0; i < (int)mEntries.size(); ++i) {
        int row = i / cols;
        int col = i % cols;
        int cx  = startX + col * cellW;
        int cy  = startY + row * cellH - mScroll;

        if (cy + cellH < mContentPanel.y || cy > mContentPanel.y + mContentPanel.h) {
            mEntries[i].rect    = {};
            mEntries[i].delRect = {};
            continue;
        }

        SDL_Rect cellRect = {cx, cy, cellW, cellH};
        mEntries[i].rect = cellRect;

        bool selected = mSelected.count(i) > 0;

        if (mEntries[i].isDir) {
            // Highlight folder if dragging selection over it
            bool dropTarget = mDragging && !mSelected.empty() && !mSelected.count(i);
            SDL_Color fbg = dropTarget ? SDL_Color{60, 90, 140, 255} : FOLDER_BG;
            // Check if cursor is actually over this folder during drag
            if (dropTarget && !hit(cellRect, mDragCurX, mDragCurY))
                fbg = FOLDER_BG;
            fillRect(s, {cx + 2, cy + 2, THUMB_SZ + CELL_PAD, THUMB_SZ}, fbg);
            SDL_Color fout = selected ? SEL_OUT : SDL_Color{80, 100, 140, 255};
            outlineRect(s, {cx + 2, cy + 2, THUMB_SZ + CELL_PAD, THUMB_SZ}, fout, selected ? 2 : 1);
            drawTextCentered(s, "[DIR]", {cx + 2, cy + 2, THUMB_SZ + CELL_PAD, THUMB_SZ}, 14, {140, 180, 220, 255});
        } else {
            SDL_Rect thumbDst = {cx + CELL_PAD, cy + 2, THUMB_SZ, THUMB_SZ};
            fillRect(s, thumbDst, {10, 12, 22, 255});
            if (mEntries[i].thumb) {
                int tw = mEntries[i].thumb->w;
                int th = mEntries[i].thumb->h;
                SDL_Rect dst = {thumbDst.x + (THUMB_SZ - tw) / 2,
                                thumbDst.y + (THUMB_SZ - th) / 2, tw, th};
                SDL_BlitSurface(mEntries[i].thumb, nullptr, s, &dst);
            }
            SDL_Color tout = selected ? SEL_OUT : SDL_Color{50, 55, 75, 255};
            outlineRect(s, thumbDst, tout, selected ? 2 : 1);
        }

        // Name label
        std::string label = mEntries[i].name;
        if ((int)label.size() > 10) label = label.substr(0, 8) + "..";
        SDL_Color labelCol = selected ? SDL_Color{100, 200, 255, 255} : SDL_Color{180, 180, 200, 255};
        drawText(s, label, cx + 2, cy + THUMB_SZ + 4, 9, labelCol);

        // Delete button
        SDL_Rect delBtn = {cx + cellW - 16, cy + 2, 14, 14};
        mEntries[i].delRect = delBtn;
        fillRect(s, delBtn, BTN_DEL);
        outlineRect(s, delBtn, {200, 80, 80, 255});
        drawTextCentered(s, "x", delBtn, 9, {255, 200, 200, 255});
    }

    if (mEntries.empty()) {
        drawTextCentered(s, "Empty folder - drop files here or press Esc to go back",
                         mContentPanel, 14, {80, 90, 110, 255});
    }

    // Marquee selection rectangle
    if (mMarquee && mMarqueeRect.w > 2 && mMarqueeRect.h > 2) {
        outlineRect(s, mMarqueeRect, {100, 200, 255, 180}, 1);
    }

    // Drag indicator
    if (mDragging && !mSelected.empty()) {
        int dx = mDragCurX - mDragStartX;
        int dy = mDragCurY - mDragStartY;
        if (dx * dx + dy * dy > 100) {
            std::string dragLabel = std::to_string(mSelected.size()) + " item(s)";
            drawText(s, dragLabel, mDragCurX + 12, mDragCurY - 6, 11, {100, 200, 255, 255});
        }
    }

    // Drop zone
    mDropZone = {PANEL_PAD, mH - botH + 4, mW - PANEL_PAD * 2, 32};
    fillRect(s, mDropZone, mDropHover ? DROP_HOV : DROP_IDLE);
    outlineRect(s, mDropZone, mDropHover ? SDL_Color{100, 180, 255, 255} : PANEL_OUT, 2);
    std::string dzLabel = mDropHover ? "Release to add..."
                        : (mStatusMsg.empty() ? "Drop PNG files or folders here to add" : mStatusMsg);
    drawTextCentered(s, dzLabel, mDropZone, 13,
                     mDropHover ? SDL_Color{200, 230, 255, 255} : SDL_Color{160, 160, 180, 255});

    // Bottom buttons
    const int btnW = 130, btnH2 = 34, btnY2 = mH - 42;
    const int btnGap = 10;

    mBackBtnRect = {PANEL_PAD, btnY2, btnW, btnH2};
    fillRect(s, mBackBtnRect, BTN_BACK);
    outlineRect(s, mBackBtnRect, {100, 100, 200, 255});
    std::string backLabel = (mCurrentDir != mTileRoot) ? "< Up" : "< Back";
    drawTextCentered(s, backLabel, mBackBtnRect, 15);

    mUpBtnRect = {PANEL_PAD + btnW + btnGap, btnY2, btnW, btnH2};
    if (mCurrentDir != mTileRoot) {
        fillRect(s, mUpBtnRect, {60, 80, 120, 255});
        outlineRect(s, mUpBtnRect, {100, 120, 180, 255});
        drawTextCentered(s, "Root", mUpBtnRect, 14);
    } else {
        mUpBtnRect = {};
    }

    mNewFolderBtnRect = {mW - PANEL_PAD - btnW, btnY2, btnW, btnH2};
    fillRect(s, mNewFolderBtnRect, BTN_ADD);
    outlineRect(s, mNewFolderBtnRect, {80, 200, 120, 255});
    drawTextCentered(s, "+ New Folder", mNewFolderBtnRect, 14);

    // New folder naming overlay
    if (mNamingFolder) {
        fillRect(s, {0, 0, mW, mH}, {0, 0, 0, 160});
        int mw = 400, mh = 140;
        int mx2 = (mW - mw) / 2, my2 = (mH - mh) / 2;
        fillRect(s, {mx2, my2, mw, mh}, {30, 35, 55, 255});
        outlineRect(s, {mx2, my2, mw, mh}, {80, 100, 180, 255}, 2);
        drawText(s, "Folder name:", mx2 + 16, my2 + 16, 16, {200, 210, 255, 255});
        mNameFieldRect = {mx2 + 16, my2 + 50, mw - 32, 32};
        fillRect(s, mNameFieldRect, {18, 18, 32, 255});
        outlineRect(s, mNameFieldRect, {100, 150, 255, 255}, 2);
        drawText(s, mNewFolderName + "|", mNameFieldRect.x + 8, mNameFieldRect.y + 8, 16);
        drawText(s, "Enter=confirm  Esc=cancel", mx2 + 16, my2 + 100, 12, {100, 110, 140, 255});
    }

    // Delete confirmation overlay
    if (mDelConfirm) {
        fillRect(s, {0, 0, mW, mH}, {0, 0, 0, 160});
        int mw = 420, mh = 130;
        int mx2 = (mW - mw) / 2, my2 = (mH - mh) / 2;
        fillRect(s, {mx2, my2, mw, mh}, {40, 30, 30, 255});
        outlineRect(s, {mx2, my2, mw, mh}, {200, 80, 80, 255}, 2);
        std::string msg = mDelMulti
            ? ("Delete " + std::to_string(mSelected.size()) + " selected item(s)?")
            : ("Delete \"" + fs::path(mDelTarget).filename().string() + "\"?" + (mDelIsDir ? " (entire folder)" : ""));
        drawTextCentered(s, msg, {mx2, my2 + 10, mw, 40}, 16, {255, 200, 200, 255});
        mDelYesRect = {mx2 + mw / 2 - 120, my2 + 70, 100, 36};
        mDelNoRect  = {mx2 + mw / 2 + 20,  my2 + 70, 100, 36};
        fillRect(s, mDelYesRect, BTN_DEL);
        outlineRect(s, mDelYesRect, {200, 80, 80, 255});
        drawTextCentered(s, "Delete", mDelYesRect, 15);
        fillRect(s, mDelNoRect, BTN_BACK);
        outlineRect(s, mDelNoRect, {100, 100, 200, 255});
        drawTextCentered(s, "Cancel", mDelNoRect, 15);
    }

    // Upload surface to texture and render.
    // LINEAR keeps anti-aliased text smooth on Retina/HiDPI displays.
    SDL_Texture* tex = SDL_CreateTextureFromSurface(ren, s);
    SDL_DestroySurface(s);
    if (tex) {
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_LINEAR);
        SDL_RenderTexture(ren, tex, nullptr, nullptr);
        SDL_DestroyTexture(tex);
    }
    window.Update();
}

// ---- NextScene --------------------------------------------------------------

std::unique_ptr<Scene> TileAssetScene::NextScene() {
    if (mGoBack) {
        mGoBack = false;
        return std::make_unique<TitleScene>();
    }
    return nullptr;
}

// ---- Draw helpers -----------------------------------------------------------

void TileAssetScene::fillRect(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
    SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
}

void TileAssetScene::outlineRect(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t) {
    const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
    Uint32 col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
    SDL_Rect sides[4] = {{r.x, r.y, r.w, t}, {r.x, r.y+r.h-t, r.w, t},
                         {r.x, r.y, t, r.h}, {r.x+r.w-t, r.y, t, r.h}};
    for (auto& sr : sides) SDL_FillSurfaceRect(s, &sr, col);
}

void TileAssetScene::drawText(SDL_Surface* s, const std::string& str,
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

void TileAssetScene::drawTextCentered(SDL_Surface* s, const std::string& str,
                                       SDL_Rect r, int ptSize, SDL_Color col) {
    if (str.empty()) return;
    auto [tx, ty] = Text::CenterInRect(str, ptSize, r);
    drawText(s, str, tx, ty, ptSize, col);
}
