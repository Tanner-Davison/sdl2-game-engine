#include "EditorPopups.hpp"
#include "AnimatedTile.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <print>

namespace fs = std::filesystem;

// ─── Helpers ──────────────────────────────────────────────────────────────────
static bool HitTest(const SDL_Rect& r, int x, int y) {
    return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
}

// ─── OpenDeleteConfirm ────────────────────────────────────────────────────────
void EditorPopups::OpenDeleteConfirm(const std::string& path, bool isDir,
                                     const std::string& name) {
    delActive = true;
    delPath   = path;
    delIsDir  = isDir;
    delName   = name;
    // Button rects are computed by EditorUIRenderer each frame — just leave them
    // as zero here; HandleDeleteConfirmEvent reads them after Render has set them.
}

// ─── OpenImportInput ─────────────────────────────────────────────────────────
void EditorPopups::OpenImportInput(bool isBgTab, Ctx& ctx) {
    importActive = true;
    importText.clear();
    SDL_StartTextInput(ctx.sdlWindow);
    ctx.setStatus(isBgTab
                      ? "Import bg path or folder (Enter=go, Esc=cancel):"
                      : "Import tile path or folder (Enter=go, Esc=cancel):");
}

// ─── OpenAnimPicker ───────────────────────────────────────────────────────────
void EditorPopups::OpenAnimPicker(int tileIdx, Ctx& ctx) {
    animPickerTile = tileIdx;
    animPickerEntries.clear();

    // "None" entry first — always available to clear any existing assignment
    animPickerEntries.push_back({"", "None (no death anim)", nullptr});

    auto manifests = ScanAnimatedTiles();
    for (const auto& p : manifests) {
        AnimatedTileDef def;
        if (!LoadAnimatedTileDef(p.string(), def))
            continue;
        SDL_Surface* thumb = ctx.getAnimThumb(p.string());
        animPickerEntries.push_back({p.string(), def.name, thumb});
    }
    // animPickerRect is computed by EditorUIRenderer each frame — left as zero here.
}

// ─── CloseAnimPicker ─────────────────────────────────────────────────────────
void EditorPopups::CloseAnimPicker() {
    animPickerTile = -1;
    animPickerEntries.clear();
}

// ─── OpenPowerUpPicker ───────────────────────────────────────────────────────
void EditorPopups::OpenPowerUpPicker(int tileIdx, int screenX, int screenY,
                                     int windowW, int windowH, int toolbarH) {
    powerUpTileIdx = tileIdx;
    powerUpOpen    = true;

    if (powerUpRegistry) {
        int ph = 32 + (int)(powerUpRegistry->size() + 1) * 30 + 8;
        int pw = 200;
        int px = std::clamp(screenX, 0, windowW - pw);
        int py = std::clamp(screenY, toolbarH, windowH - ph);
        powerUpRect = {px, py, pw, ph};
    }
}

// ─── ClosePowerUpPicker ──────────────────────────────────────────────────────
void EditorPopups::ClosePowerUpPicker() {
    powerUpOpen    = false;
    powerUpTileIdx = -1;
}

// ─── HandleEvent (main dispatcher) ───────────────────────────────────────────
bool EditorPopups::HandleEvent(const SDL_Event& e, Ctx& ctx,
                                std::vector<int>& movPlatIndices) {
    // Priority order: most-modal first so a speed-input field swallows keys before
    // the delete confirm can see them, etc.

    // 1. MovingPlat speed text field (highest priority — keyboard modal)
    if (movPlatOpen && movPlatSpeedInput)
        return HandleMovPlatPopupEvent(e, ctx, movPlatIndices);

    // 2. Import text input
    if (importActive)
        return HandleImportInputEvent(e, ctx);

    // 3. Delete confirmation
    if (delActive)
        return HandleDeleteConfirmEvent(e, ctx);

    // 4. Anim picker (mouse only, non-keyboard-modal)
    if (animPickerTile >= 0)
        if (HandleAnimPickerEvent(e, ctx))
            return true;

    // 5. Power-up picker (mouse only)
    if (powerUpOpen && powerUpTileIdx >= 0)
        if (HandlePowerUpPickerEvent(e, ctx))
            return true;

    // 6. MovingPlat config popup (non-speed-input clicks)
    if (movPlatOpen)
        if (HandleMovPlatPopupEvent(e, ctx, movPlatIndices))
            return true;

    return false;
}

// ─── HandleDeleteConfirmEvent ────────────────────────────────────────────────
bool EditorPopups::HandleDeleteConfirmEvent(const SDL_Event& e, Ctx& ctx) {
    if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
        delActive = false;
        ctx.setStatus("Delete cancelled");
        return true;
    }
    if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
        int mx = (int)e.button.x, my = (int)e.button.y;
        if (HitTest(delYes, mx, my)) {
            std::error_code ec;
            if (delIsDir)
                fs::remove_all(delPath, ec);
            else
                fs::remove(delPath, ec);
            delActive = false;

            bool wasBg = (delPath.rfind(ctx.bgRoot, 0) == 0);
            if (wasBg)
                ctx.refreshBgPalette();
            else
                ctx.refreshTileView();

            ctx.setStatus((delIsDir ? "Deleted folder: " : "Deleted: ") + delName);
            return true;
        }
        if (HitTest(delNo, mx, my)) {
            delActive = false;
            ctx.setStatus("Delete cancelled");
            return true;
        }
    }
    return true; // swallow all input while popup is open
}

// ─── HandleImportInputEvent ───────────────────────────────────────────────────
bool EditorPopups::HandleImportInputEvent(const SDL_Event& e, Ctx& ctx) {
    if (e.type == SDL_EVENT_TEXT_INPUT) {
        importText += e.text.text;
        return true;
    }
    if (e.type == SDL_EVENT_KEY_DOWN) {
        switch (e.key.key) {
            case SDLK_ESCAPE:
                importActive = false;
                importText.clear();
                ctx.setStatus("Import cancelled");
                SDL_StopTextInput(ctx.sdlWindow);
                return true;
            case SDLK_BACKSPACE:
                if (!importText.empty())
                    importText.pop_back();
                return true;
            case SDLK_RETURN:
            case SDLK_KP_ENTER: {
                std::string path = importText;
                importActive     = false;
                importText.clear();
                SDL_StopTextInput(ctx.sdlWindow);
                if (!path.empty())
                    ctx.importPath(path);
                return true;
            }
            default:
                break;
        }
    }
    return true; // swallow all other input while text field is open
}

// ─── HandleAnimPickerEvent ───────────────────────────────────────────────────
bool EditorPopups::HandleAnimPickerEvent(const SDL_Event& e, Ctx& ctx) {
    if (e.type != SDL_EVENT_MOUSE_BUTTON_DOWN || e.button.button != SDL_BUTTON_LEFT)
        return false;

    int mx = (int)e.button.x, my = (int)e.button.y;

    if (!HitTest(animPickerRect, mx, my)) {
        // Click outside — close picker without changing assignment
        CloseAnimPicker();
        return false; // let click propagate to canvas
    }

    if (HitTest(camShakeToggleRect, mx, my)) {
        if (animPickerTile < (int)ctx.level.tiles.size() &&
            ctx.level.tiles[animPickerTile].HasAction()) {
            auto& shake = ctx.level.tiles[animPickerTile].action->cameraShake;
            shake = !shake;
            ctx.setStatus("Tile " + std::to_string(animPickerTile) +
                          ": camera shake " + (shake ? "ON" : "OFF"));
        }
        return true;
    }

    // Re-derive cell geometry (must match EditorUIRenderer exactly)
    const int THUMB   = 48;
    const int ROW_H   = THUMB + 10;
    const int PAD     = 8;
    const int COL_W   = THUMB + PAD * 2;
    const int COLS    = 4;
    const int TITLE_H = 28;
    int       px      = animPickerRect.x;
    int       ey      = animPickerRect.y + TITLE_H;
    int       n       = (int)animPickerEntries.size();

    for (int i = 0; i < n; i++) {
        int      col  = i % COLS;
        int      row  = i / COLS;
        int      ex   = px + PAD + col * COL_W;
        int      ey2  = ey + PAD + row * (ROW_H + PAD);
        SDL_Rect cell = {ex, ey2, COL_W - PAD, ROW_H};
        if (HitTest(cell, mx, my)) {
            const auto& entry = animPickerEntries[i];
            // Write back through the level reference
            if (animPickerTile < (int)ctx.level.tiles.size()) {
                ctx.level.tiles[animPickerTile].action->destroyAnimPath = entry.path;
                if (!entry.path.empty())
                    ctx.getAnimThumb(entry.path); // warm the cache
                ctx.setStatus("Tile " + std::to_string(animPickerTile) +
                              ": death anim -> " +
                              (entry.path.empty() ? "None" : entry.name));
            }
            CloseAnimPicker();
            return true;
        }
    }
    return true; // click inside popup but no cell hit — absorb
}

// ─── HandlePowerUpPickerEvent ────────────────────────────────────────────────
bool EditorPopups::HandlePowerUpPickerEvent(const SDL_Event& e, Ctx& ctx) {
    if (e.type != SDL_EVENT_MOUSE_BUTTON_DOWN || e.button.button != SDL_BUTTON_LEFT)
        return false;

    int mx = (int)e.button.x, my = (int)e.button.y;

    if (!HitTest(powerUpRect, mx, my)) {
        ClosePowerUpPicker();
        return false; // let click propagate
    }

    if (!powerUpRegistry || powerUpTileIdx < 0 ||
        powerUpTileIdx >= (int)ctx.level.tiles.size())
        return true;

    const auto& reg     = *powerUpRegistry;
    const int   PAD     = 8;
    const int   ROW_H   = 28;
    const int   TITLE_H = 32;
    int         py      = powerUpRect.y + TITLE_H;

    for (int i = 0; i < (int)reg.size(); i++) {
        SDL_Rect row = {powerUpRect.x + PAD, py + i * (ROW_H + 2),
                        powerUpRect.w - PAD * 2, ROW_H};
        if (HitTest(row, mx, my)) {
            auto& t           = ctx.level.tiles[powerUpTileIdx];
            t.powerUp         = PowerUpData{reg[i].id, reg[i].defaultDuration};
            ctx.setStatus("Tile " + std::to_string(powerUpTileIdx) +
                          " -> PowerUp: " + reg[i].label);
            ClosePowerUpPicker();
            return true;
        }
    }

    // "None" row below all entries
    SDL_Rect noneRow = {powerUpRect.x + PAD,
                        py + (int)reg.size() * (ROW_H + 2),
                        powerUpRect.w - PAD * 2, ROW_H};
    if (HitTest(noneRow, mx, my)) {
        ctx.level.tiles[powerUpTileIdx].powerUp.reset();
        ctx.setStatus("Tile " + std::to_string(powerUpTileIdx) + " -> PowerUp removed");
        ClosePowerUpPicker();
        return true;
    }

    return true; // click inside popup but missed every row — absorb
}

// ─── HandleMovPlatPopupEvent ─────────────────────────────────────────────────
bool EditorPopups::HandleMovPlatPopupEvent(const SDL_Event& e, Ctx& ctx,
                                           std::vector<int>& movPlatIndices) {
    // ── Speed text field is focused — swallow all keyboard input ─────────────
    if (movPlatSpeedInput) {
        if (e.type == SDL_EVENT_TEXT_INPUT) {
            for (char ch : std::string(e.text.text))
                if (ch >= '0' && ch <= '9')
                    movPlatSpeedStr += ch;
            return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_BACKSPACE && !movPlatSpeedStr.empty()) {
                movPlatSpeedStr.pop_back();
                return true;
            }
            if (e.key.key == SDLK_RETURN || e.key.key == SDLK_KP_ENTER ||
                e.key.key == SDLK_ESCAPE || e.key.key == SDLK_TAB) {
                CommitSpeedField(ctx, movPlatIndices);
                movPlatSpeedInput = false;
                SDL_StopTextInput(ctx.sdlWindow);
                return true;
            }
        }
        return true; // swallow everything else
    }

    // ── Mouse clicks on the popup ─────────────────────────────────────────────
    if (e.type != SDL_EVENT_MOUSE_BUTTON_DOWN || e.button.button != SDL_BUTTON_LEFT)
        return false;

    int mx = (int)e.button.x, my = (int)e.button.y;

    if (!HitTest(movPlatRect, mx, my))
        return false; // don't consume — let canvas see it

    const int PW      = 280;
    const int PAD     = 8;
    const int ROW_H   = 26;
    const int TITLE_H = 30;
    const int px      = movPlatRect.x;
    const int py      = movPlatRect.y;
    int       ry      = py + TITLE_H;

    // Row 0: speed field + close button
    SDL_Rect speedField = {px + PAD + 90, ry + (ROW_H - 20) / 2,
                           PW - PAD * 2 - 90 - 44, 20};
    SDL_Rect closeBtn   = {px + PW - PAD - 36, ry + (ROW_H - 20) / 2, 36, 20};

    if (HitTest(speedField, mx, my)) {
        movPlatSpeedInput = true;
        SDL_StartTextInput(ctx.sdlWindow);
        return true;
    }
    if (HitTest(closeBtn, mx, my)) {
        CommitSpeedField(ctx, movPlatIndices);
        movPlatOpen       = false;
        movPlatSpeedInput = false;
        SDL_StopTextInput(ctx.sdlWindow);
        return true;
    }
    ry += ROW_H + PAD;

    // Row 1: H/V toggle
    SDL_Rect btnH = {px + PAD + 90, ry, 48, ROW_H - 4};
    SDL_Rect btnV = {px + PAD + 90 + 54, ry, 48, ROW_H - 4};
    if (HitTest(btnH, mx, my)) {
        movPlatHoriz = true;
        for (int idx : movPlatIndices)
            ctx.level.tiles[idx].moving->horiz = true;
        return true;
    }
    if (HitTest(btnV, mx, my)) {
        movPlatHoriz = false;
        for (int idx : movPlatIndices)
            ctx.level.tiles[idx].moving->horiz = false;
        return true;
    }
    ry += ROW_H + PAD;

    // Row 2: Loop (ping-pong) checkbox
    SDL_Rect loopRow = {px + PAD, ry, PW - PAD * 2, ROW_H};
    if (HitTest(loopRow, mx, my)) {
        movPlatLoop = !movPlatLoop;
        if (!movPlatLoop)
            movPlatTrigger = false;
        for (int idx : movPlatIndices) {
            ctx.level.tiles[idx].moving->loop    = movPlatLoop;
            ctx.level.tiles[idx].moving->trigger = movPlatTrigger;
        }
        return true;
    }
    ry += ROW_H + PAD;

    // Row 3: Move on Touch checkbox
    SDL_Rect trigRow = {px + PAD, ry, PW - PAD * 2, ROW_H};
    if (HitTest(trigRow, mx, my)) {
        movPlatTrigger = !movPlatTrigger;
        for (int idx : movPlatIndices)
            ctx.level.tiles[idx].moving->trigger = movPlatTrigger;
        return true;
    }

    return true; // absorb all other clicks inside popup
}

// ─── CommitSpeedField (private helper) ───────────────────────────────────────
void EditorPopups::CommitSpeedField(Ctx& ctx, std::vector<int>& movPlatIndices) {
    if (movPlatSpeedStr.empty())
        return;

    int v         = std::clamp(std::stoi(movPlatSpeedStr), 10, 2000);
    movPlatSpeed  = (float)v;
    movPlatSpeedStr = std::to_string(v);

    // Apply to current session tiles
    for (int idx : movPlatIndices)
        ctx.level.tiles[idx].moving->speed = movPlatSpeed;

    // Also apply to all tiles in the current group (catches already-placed platforms)
    for (auto& t : ctx.level.tiles) {
        if (!t.HasMoving())
            continue;
        bool inGroup =
            (movPlatGroupId != 0 && t.moving->groupId == movPlatGroupId) ||
            std::any_of(movPlatIndices.begin(), movPlatIndices.end(),
                        [&](int i) { return &t == &ctx.level.tiles[i]; });
        if (inGroup)
            t.moving->speed = movPlatSpeed;
    }
}
