#include "tools/PlacementTools.hpp"
#include "EnemyProfile.hpp"
#include "Text.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <filesystem>
#include <print>
#include <string>

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
// EnemyTool — implementation
// ─────────────────────────────────────────────────────────────────────────────

static bool hitRect(const SDL_Rect& r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

// ── Refresh the picker entry list from saved enemy profiles ──────────────────
void EnemyTool::RefreshPicker() {
    pickerEntries.clear();

    // Entry 0: generic slime (legacy, always available)
    pickerEntries.push_back({"(default slime)", "", {}});

    // Scan enemies/ directory for profiles
    for (const auto& path : ScanEnemyProfiles()) {
        EnemyProfile prof;
        if (!LoadEnemyProfile(path.string(), prof)) continue;
        std::string preview = EnemyPreviewImagePath(prof);
        pickerEntries.push_back({prof.name, preview, {}});
    }
}

// ── Commit the speed edit popup value ────────────────────────────────────────
void EnemyTool::CommitSpeedEdit(EditorToolContext& ctx) {
    if (!speedInputActive || speedPopupIdx < 0) return;
    if (speedPopupIdx < (int)ctx.level.enemies.size()) {
        float val = speedStr.empty() ? 120.0f : std::stof(speedStr);
        if (val < 0.0f) val = 0.0f;
        ctx.level.enemies[speedPopupIdx].speed = val;
        ctx.SetStatus("Enemy speed set to " + std::to_string((int)val));
    }
    speedInputActive = false;
    speedPopupOpen   = false;
    speedPopupIdx    = -1;
    SDL_StopTextInput(ctx.sdlWindow);
}

// ── Mouse down ──────────────────────────────────────────────────────────────
ToolResult EnemyTool::OnMouseDown(EditorToolContext& ctx, int mx, int my,
                                  Uint8 button, SDL_Keymod /*mods*/) {
    // Speed edit popup interactions
    if (speedPopupOpen) {
        // Close button
        SDL_Rect closeBtn = {speedPopupRect.x + speedPopupRect.w - 40,
                             speedPopupRect.y + 4, 36, 20};
        if (button == SDL_BUTTON_LEFT && hitRect(closeBtn, mx, my)) {
            if (speedInputActive) CommitSpeedEdit(ctx);
            speedPopupOpen = false;
            speedPopupIdx  = -1;
            return ToolResult::Consumed;
        }
        // Speed field click
        SDL_Rect fieldRect = {speedPopupRect.x + 80, speedPopupRect.y + 30,
                              speedPopupRect.w - 90, 24};
        if (button == SDL_BUTTON_LEFT && hitRect(fieldRect, mx, my)) {
            speedStr = std::to_string((int)ctx.level.enemies[speedPopupIdx].speed);
            speedInputActive = true;
            SDL_StartTextInput(ctx.sdlWindow);
            return ToolResult::Consumed;
        }
        // Click inside popup but not on a control — consume to prevent placing
        if (hitRect(speedPopupRect, mx, my))
            return ToolResult::Consumed;
        // Click outside popup — close it
        if (speedInputActive) CommitSpeedEdit(ctx);
        speedPopupOpen = false;
        speedPopupIdx  = -1;
        // Fall through to normal click handling
    }

    // Picker panel click
    if (pickerVisible && button == SDL_BUTTON_LEFT) {
        for (int i = 0; i < (int)pickerEntries.size(); ++i) {
            if (pickerEntries[i].rect.w > 0 && hitRect(pickerEntries[i].rect, mx, my)) {
                if (i == 0) {
                    selectedType.clear();
                    placementSpeed = 120.0f;
                    ctx.SetStatus("Enemy: default slime  spd=" +
                                  std::to_string((int)placementSpeed));
                } else {
                    selectedType = pickerEntries[i].name;
                    // Load the profile to get its default speed
                    EnemyProfile prof;
                    std::string profPath = EnemyProfilePath(selectedType);
                    if (LoadEnemyProfile(profPath, prof))
                        placementSpeed = prof.speed;
                    ctx.SetStatus("Enemy: " + selectedType + "  spd=" +
                                  std::to_string((int)placementSpeed));
                }
                return ToolResult::Consumed;
            }
        }
    }

    // Right-click on placed enemy: open speed edit popup
    // Shift+RClick toggles start direction
    if (button == SDL_BUTTON_RIGHT) {
        if (my >= ctx.ToolbarH() && mx < ctx.CanvasW()) {
            int ei = ctx.HitEnemy(mx, my);
            if (ei >= 0) {
                SDL_Keymod mods = SDL_GetModState();
                if (mods & SDL_KMOD_SHIFT) {
                    // Toggle start direction
                    auto& en = ctx.level.enemies[ei];
                    en.startLeft = !en.startLeft;
                    ctx.SetStatus("Enemy #" + std::to_string(ei) + " starts " +
                                  (en.startLeft ? "LEFT" : "RIGHT"));
                    return ToolResult::Consumed;
                }
                speedPopupIdx  = ei;
                speedPopupOpen = true;
                speedStr = std::to_string((int)ctx.level.enemies[ei].speed);
                speedPopupRect = {mx, my, 220, 60};
                if (speedPopupRect.x + speedPopupRect.w > ctx.CanvasW())
                    speedPopupRect.x = ctx.CanvasW() - speedPopupRect.w - 4;
                ctx.SetStatus("RClick enemy #" + std::to_string(ei) +
                              " - edit speed (" + std::to_string((int)ctx.level.enemies[ei].speed) + ")");
                return ToolResult::Consumed;
            }
        }
    }

    // Left-click: place enemy
    if (button != SDL_BUTTON_LEFT) return ToolResult::Ignored;
    if (my < ctx.ToolbarH() || mx >= ctx.CanvasW()) return ToolResult::Ignored;

    // Don't place if clicking inside the picker area
    // (picker is rendered at top of canvas, height ~56px below toolbar)
    if (pickerVisible && my < ctx.ToolbarH() + 56) return ToolResult::Consumed;

    auto [sx, sy] = ctx.SnapToGrid(mx, my);
    EnemySpawn es;
    es.x         = static_cast<float>(sx);
    es.y         = static_cast<float>(sy);
    es.speed     = placementSpeed;
    es.startLeft = placementStartLeft;
    es.enemyType = selectedType;
    ctx.level.enemies.push_back(std::move(es));

    std::string typeName = selectedType.empty() ? "slime" : selectedType;
    ctx.SetStatus("Enemy: " + typeName + " at " + std::to_string(sx) + "," +
                  std::to_string(sy) + "  spd=" + std::to_string((int)placementSpeed));
    return ToolResult::Consumed;
}

// ── Key down (speed popup text input) ────────────────────────────────────────
ToolResult EnemyTool::OnKeyDown(EditorToolContext& ctx, SDL_Keycode key,
                                SDL_Keymod /*mods*/) {
    if (!speedInputActive) return ToolResult::Ignored;

    if (key == SDLK_BACKSPACE) {
        if (!speedStr.empty()) speedStr.pop_back();
        return ToolResult::Consumed;
    }
    if (key == SDLK_RETURN || key == SDLK_ESCAPE) {
        CommitSpeedEdit(ctx);
        return ToolResult::Consumed;
    }
    return ToolResult::Ignored;
}

// Note: F key for direction toggle is handled below via a separate
// non-text-input path. The orchestrator calls OnKeyDown for all keys
// when speedInputActive is false, so we catch F there too.
// However, since this OnKeyDown only fires when speedInputActive is true,
// the F-key toggle is handled in OnMouseDown (right-click) for existing
// enemies and via the overlay button for new placements.

// ── Mouse move: track cursor for ghost outline ───────────────────────────────
ToolResult EnemyTool::OnMouseMove(EditorToolContext& ctx, int mx, int my) {
    cursorScreenX  = mx;
    cursorScreenY  = my;
    cursorOnCanvas = (my >= ctx.ToolbarH() && mx < ctx.CanvasW());
    return ToolResult::Ignored;
}

// ── Scroll: adjust placement speed ───────────────────────────────────────────
ToolResult EnemyTool::OnScroll(EditorToolContext& ctx, float wheelY,
                               int mx, int my, SDL_Keymod /*mods*/) {
    // Scroll on picker: scroll the list
    if (pickerVisible && my < ctx.ToolbarH() + 56) {
        pickerScroll = std::max(0, pickerScroll - (int)wheelY);
        return ToolResult::Consumed;
    }

    // Scroll on canvas: adjust placement speed
    int delta = (int)(wheelY * 10.0f);
    placementSpeed = std::max(0.0f, placementSpeed + delta);
    ctx.SetStatus("Placement speed: " + std::to_string((int)placementSpeed));
    return ToolResult::Consumed;
}

// ── Render overlay: picker panel + speed popup ───────────────────────────────
void EnemyTool::RenderOverlay(EditorToolContext& ctx, SDL_Surface* screen,
                              int canvasW) {
    // ── Enemy type picker bar ────────────────────────────────────────────────
    if (pickerVisible) {
        const int barY = ctx.ToolbarH() + 2;
        const int barH = 52;
        const int pad  = 4;
        const int itemW = 80;
        const int itemH = barH - 8;
        const int thumbSz = 32;

        // Background bar
        ctx.DrawRectAlpha(screen, {0, barY, canvasW, barH}, {10, 20, 40, 220});
        ctx.DrawOutline(screen, {0, barY, canvasW, barH}, {80, 60, 50, 255});

        // Label
        SDL_Surface* lbl = ctx.GetBadge("Enemy Type:", {220, 160, 120, 255});
        if (lbl) {
            SDL_Rect ld = {pad + 2, barY + 4, lbl->w, lbl->h};
            SDL_BlitSurface(lbl, nullptr, screen, &ld);
        }

        // Direction + Speed indicator
        {
            std::string dirLabel = placementStartLeft ? "<< LEFT" : "RIGHT >>";
            std::string infoLabel = dirLabel + "  spd=" + std::to_string((int)placementSpeed) + "  (F=flip  scroll=spd)";
            SDL_Surface* infoBadge = ctx.GetBadge(infoLabel, {180, 220, 255, 255});
            if (infoBadge) {
                SDL_Rect sd = {canvasW - infoBadge->w - 6, barY + 4, infoBadge->w, infoBadge->h};
                SDL_BlitSurface(infoBadge, nullptr, screen, &sd);
            }
        }

        // Picker entries
        int startX = 90;
        int visibleCount = (canvasW - startX - 80) / (itemW + pad);
        int firstIdx = pickerScroll;
        int lastIdx  = std::min(firstIdx + visibleCount, (int)pickerEntries.size());

        for (int i = firstIdx; i < lastIdx; ++i) {
            auto& entry = pickerEntries[i];
            int x = startX + (i - firstIdx) * (itemW + pad);
            int y = barY + 4;

            bool selected = (i == 0 && selectedType.empty()) ||
                            (i > 0 && entry.name == selectedType);

            SDL_Color bg = selected ? SDL_Color{160, 60, 40, 255}
                                    : SDL_Color{40, 40, 60, 230};
            SDL_Color border = selected ? SDL_Color{255, 120, 80, 255}
                                        : SDL_Color{80, 70, 90, 255};

            entry.rect = {x, y, itemW, itemH};
            ctx.DrawRect(screen, entry.rect, bg);
            ctx.DrawOutline(screen, entry.rect, border);

            // Thumbnail
            if (!entry.previewPath.empty()) {
                SDL_Surface* thumb = ctx.surfaceCache.FindTileSurface(entry.previewPath);
                if (!thumb) {
                    // Try loading it
                    thumb = ctx.surfaceCache.LoadAndCache(entry.previewPath);
                }
                if (thumb) {
                    SDL_Rect dst = {x + 2, y + 2, thumbSz, thumbSz};
                    SDL_ScaleMode sm = (dst.w < thumb->w) ? SDL_SCALEMODE_LINEAR
                                                          : SDL_SCALEMODE_PIXELART;
                    SDL_BlitSurfaceScaled(thumb, nullptr, screen, &dst, sm);
                }
            }

            // Name label (truncated)
            std::string displayName = entry.name;
            if ((int)displayName.size() > 9)
                displayName = displayName.substr(0, 7) + "..";
            SDL_Surface* nameBadge = ctx.GetBadge(displayName,
                selected ? SDL_Color{255, 255, 220, 255} : SDL_Color{180, 180, 200, 255});
            if (nameBadge) {
                SDL_Rect nd = {x + thumbSz + 4, y + itemH / 2 - nameBadge->h / 2,
                               nameBadge->w, nameBadge->h};
                SDL_BlitSurface(nameBadge, nullptr, screen, &nd);
            }
        }

        // Scroll arrows if needed
        if (pickerScroll > 0) {
            SDL_Surface* leftArr = ctx.GetBadge("<", {255, 200, 100, 255});
            if (leftArr) {
                SDL_Rect ad = {startX - 14, barY + barH / 2 - 6, leftArr->w, leftArr->h};
                SDL_BlitSurface(leftArr, nullptr, screen, &ad);
            }
        }
        if (lastIdx < (int)pickerEntries.size()) {
            SDL_Surface* rightArr = ctx.GetBadge(">", {255, 200, 100, 255});
            if (rightArr) {
                int rx = startX + visibleCount * (itemW + pad) + 2;
                SDL_Rect ad = {rx, barY + barH / 2 - 6, rightArr->w, rightArr->h};
                SDL_BlitSurface(rightArr, nullptr, screen, &ad);
            }
        }
    }

    // ── Speed edit popup ─────────────────────────────────────────────────────
    if (speedPopupOpen && speedPopupIdx >= 0 &&
        speedPopupIdx < (int)ctx.level.enemies.size()) {

        ctx.DrawRect(screen, speedPopupRect, {14, 22, 40, 245});
        ctx.DrawOutline(screen, speedPopupRect, {255, 140, 60, 255}, 2);

        // Title
        SDL_Surface* title = ctx.GetBadge("Enemy Speed", {255, 200, 140, 255});
        if (title) {
            SDL_Rect td = {speedPopupRect.x + 6, speedPopupRect.y + 6, title->w, title->h};
            SDL_BlitSurface(title, nullptr, screen, &td);
        }

        // Close button
        SDL_Rect closeBtn = {speedPopupRect.x + speedPopupRect.w - 40,
                             speedPopupRect.y + 4, 36, 20};
        ctx.DrawRect(screen, closeBtn, {60, 30, 30, 220});
        ctx.DrawOutline(screen, closeBtn, {180, 80, 80, 255});
        SDL_Surface* closeLbl = ctx.GetBadge("close", {220, 140, 140, 255});
        if (closeLbl) {
            SDL_Rect cd = {closeBtn.x + 4, closeBtn.y + 4, closeLbl->w, closeLbl->h};
            SDL_BlitSurface(closeLbl, nullptr, screen, &cd);
        }

        // Speed label + field
        SDL_Surface* spdLbl = ctx.GetBadge("Speed:", {160, 200, 220, 255});
        if (spdLbl) {
            SDL_Rect sl = {speedPopupRect.x + 6, speedPopupRect.y + 34, spdLbl->w, spdLbl->h};
            SDL_BlitSurface(spdLbl, nullptr, screen, &sl);
        }
        SDL_Rect fieldRect = {speedPopupRect.x + 80, speedPopupRect.y + 30,
                              speedPopupRect.w - 90, 24};
        SDL_Color fBg = speedInputActive ? SDL_Color{40, 80, 160, 255}
                                         : SDL_Color{25, 40, 70, 255};
        SDL_Color fBd = speedInputActive ? SDL_Color{100, 180, 255, 255}
                                         : SDL_Color{60, 100, 140, 255};
        ctx.DrawRect(screen, fieldRect, fBg);
        ctx.DrawOutline(screen, fieldRect, fBd);
        std::string disp = speedInputActive ? speedStr + "|"
                         : std::to_string((int)ctx.level.enemies[speedPopupIdx].speed);
        SDL_Surface* valBadge = ctx.GetBadge(disp, {220, 240, 255, 255});
        if (valBadge) {
            SDL_Rect vd = {fieldRect.x + 4, fieldRect.y + 4, valBadge->w, valBadge->h};
            SDL_BlitSurface(valBadge, nullptr, screen, &vd);
        }
    }

    // ── Ghost outline at cursor ──────────────────────────────────────────────
    if (cursorOnCanvas && !speedPopupOpen) {
        int sprW = ctx.Grid(), sprH = ctx.Grid();
        EnemyProfile ghostProf;
        bool hasProf = false;
        if (!selectedType.empty())
            hasProf = LoadEnemyProfile(EnemyProfilePath(selectedType), ghostProf);
        if (hasProf) {
            sprW = (ghostProf.spriteW > 0) ? ghostProf.spriteW : sprW;
            sprH = (ghostProf.spriteH > 0) ? ghostProf.spriteH : sprH;
        }

        auto [sx, sy] = ctx.SnapToGrid(cursorScreenX, cursorScreenY);
        auto sp = ctx.WorldToScreen((float)sx, (float)sy);

        SDL_Rect ghostRect = {sp.x, sp.y, (int)(sprW * ctx.Zoom()), (int)(sprH * ctx.Zoom())};

        // Semi-transparent fill
        ctx.DrawRectAlpha(screen, ghostRect, {255, 100, 60, 45});

        // Sprite thumbnail inside the ghost
        if (hasProf) {
            std::string preview = EnemyPreviewImagePath(ghostProf);
            if (!preview.empty()) {
                SDL_Surface* thumb = ctx.surfaceCache.FindTileSurface(preview);
                if (!thumb) thumb = ctx.surfaceCache.LoadAndCache(preview);
                if (thumb) {
                    SDL_SetSurfaceAlphaMod(thumb, 120);
                    SDL_BlitSurfaceScaled(thumb, nullptr, screen, &ghostRect, SDL_SCALEMODE_PIXELART);
                    SDL_SetSurfaceAlphaMod(thumb, 255);
                }
            }
        }

        // Outline
        ctx.DrawOutline(screen, ghostRect, {255, 140, 80, 200}, 2);

        // Size label above the ghost
        std::string sizeStr = std::to_string(sprW) + "x" + std::to_string(sprH);
        SDL_Surface* sizeBadge = ctx.GetBadge(sizeStr, {255, 200, 140, 220});
        if (sizeBadge) {
            SDL_Rect sd = {ghostRect.x, ghostRect.y - sizeBadge->h - 2, sizeBadge->w, sizeBadge->h};
            SDL_BlitSurface(sizeBadge, nullptr, screen, &sd);
        }
    }
}
