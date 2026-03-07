#include "TitleScene.hpp"
#include "GameScene.hpp"
#include "LevelEditorScene.hpp"
#include "PlayerCreatorScene.hpp"
#include "TileAnimCreatorScene.hpp"
#include <SDL3_image/SDL_image.h>

std::unique_ptr<Scene> TitleScene::NextScene() {
    if (startGame) {
        startGame = false;
        return std::make_unique<GameScene>(mChosenLevel, false, mChosenProfile);
    }
    if (openEditor) {
        openEditor   = false;
        bool force   = mEditorForce;
        mEditorForce = false;
        std::string name = mEditorName;
        mEditorName.clear();
        return std::make_unique<LevelEditorScene>(mEditorPath, force, name, mChosenProfile);
    }
    if (openPlayerCreator) {
        openPlayerCreator = false;
        return std::make_unique<PlayerCreatorScene>();
    }
    if (openTileAnimCreator) {
        openTileAnimCreator = false;
        return std::make_unique<TileAnimCreatorScene>();
    }
    return nullptr;
}

// ── openCharPicker ───────────────────────────────────────────────────────────
void TitleScene::openCharPicker() {
    // Destroy previous textures before rebuilding
    for (auto& c : mCharCards)
        if (c.previewTex) { SDL_DestroyTexture(c.previewTex); c.previewTex = nullptr; }
    mCharCards.clear();
    mCharPickerOpen   = true;
    mCharPickerScroll = 0;

    // Helper: collect all sorted PNGs in a folder
    auto collectPngs = [&](const std::string& dir) -> std::vector<fs::path> {
        std::vector<fs::path> out;
        std::error_code ec;
        if (!fs::is_directory(dir, ec) || ec) return out;
        for (const auto& e : fs::directory_iterator(dir, ec))
            if (!ec && (e.path().extension() == ".png" || e.path().extension() == ".PNG"))
                out.push_back(e.path());
        std::sort(out.begin(), out.end());
        return out;
    };

    // Helper: load ONLY the first PNG from a folder as a card preview texture.
    // One disk read + one GPU upload per character — no SpriteSheet, no atlas.
    auto loadFirstFrame = [&](CharCard& c, const std::string& dir,
                              int overrideW = 0, int overrideH = 0) {
        std::error_code ec;
        if (!fs::is_directory(dir, ec) || ec) return;
        std::vector<fs::path> pngs;
        for (const auto& e : fs::directory_iterator(dir, ec))
            if (!ec && (e.path().extension() == ".png" || e.path().extension() == ".PNG"))
                pngs.push_back(e.path());
        if (pngs.empty()) return;
        std::sort(pngs.begin(), pngs.end());
        SDL_Surface* raw = IMG_Load(pngs[0].string().c_str());
        if (!raw) return;
        SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        if (!conv) return;
        // Scale to override dims if provided (respects profile's spriteW/H)
        SDL_Surface* final = conv;
        if ((overrideW > 0 && overrideW != conv->w) ||
            (overrideH > 0 && overrideH != conv->h)) {
            int dstW = overrideW > 0 ? overrideW : conv->w;
            int dstH = overrideH > 0 ? overrideH : conv->h;
            SDL_Surface* scaled = SDL_CreateSurface(dstW, dstH, SDL_PIXELFORMAT_ARGB8888);
            if (scaled) {
                SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_NONE);
                SDL_BlitSurfaceScaled(conv, nullptr, scaled, nullptr, SDL_SCALEMODE_LINEAR);
                SDL_DestroySurface(conv);
                final = scaled;
            }
        }
        c.previewTex = SDL_CreateTextureFromSurface(mRenderer, final);
        SDL_DestroySurface(final);
    };

    // Card 0: Frost Knight (built-in default)
    {
        CharCard c;
        c.name = "Frost Knight"; c.profilePath = "";
        loadFirstFrame(c, "game_assets/frost_knight_png_sequences/Idle");
        c.walkPaths = collectPngs("game_assets/frost_knight_png_sequences/Walking");
        c.walkFps   = 10.f;
        mCharCards.push_back(std::move(c));
    }

    // Remaining cards: saved player profiles
    for (const auto& profilePath : ScanPlayerProfiles()) {
        PlayerProfile prof;
        if (!LoadPlayerProfile(profilePath.string(), prof)) continue;
        CharCard c;
        c.name = prof.name; c.profilePath = profilePath.string();
        const std::string& idleDir = prof.Slot(PlayerAnimSlot::Idle).folderPath;
        if (!idleDir.empty())
            loadFirstFrame(c, idleDir, prof.spriteW, prof.spriteH);
        const std::string& walkDir = prof.Slot(PlayerAnimSlot::Walk).folderPath;
        if (!walkDir.empty())
            c.walkPaths = collectPngs(walkDir);
        float wfps = prof.Slot(PlayerAnimSlot::Walk).fps;
        c.walkFps = (wfps > 0.f) ? wfps : 8.f;
        mCharCards.push_back(std::move(c));
    }

    // Lay out card rects
    const int PW = std::min(mWindowW - 80, 780);
    const int PH = std::min(mWindowH - 80, 560);
    const int PX = (mWindowW - PW) / 2;
    const int PY = (mWindowH - PH) / 2;
    mCharPickerPanel     = {PX, PY, PW, PH};
    mCharPickerCloseRect = {PX + PW - 36, PY + 6, 30, 30};

    const int CARD_W = 160, CARD_H = 200, COLS = (PW - 32) / (CARD_W + 12), GAP = 12;
    const int startX = PX + (PW - (COLS * (CARD_W + GAP) - GAP)) / 2;
    int cardY = PY + 54;
    for (int i = 0; i < (int)mCharCards.size(); ++i) {
        int col = i % COLS, row = i / COLS;
        mCharCards[i].rect = {startX + col*(CARD_W+GAP), cardY + row*(CARD_H+GAP), CARD_W, CARD_H};
    }
    int totalRows = ((int)mCharCards.size() + COLS - 1) / COLS;
    mCharPickerMaxScroll = std::max(0, totalRows*(CARD_H+GAP) + 54 + 12 - PH);
}

// ── renderCharPicker ─────────────────────────────────────────────────────────
void TitleScene::renderCharPicker(SDL_Renderer* ren) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    SDL_FRect full = {0, 0, (float)mWindowW, (float)mWindowH};
    SDL_RenderFillRect(ren, &full);

    const auto& p = mCharPickerPanel;
    fillRect(ren, p, {18, 20, 34, 248});
    outlineRect(ren, p, {80, 100, 200, 255}, 2);
    fillRect(ren, {p.x, p.y, p.w, 44}, {28, 32, 55, 255});
    Text hdr("Choose Character", {220, 210, 255, 255}, p.x + 16, p.y + 10, 22);
    hdr.Render(ren);

    fillRect(ren, mCharPickerCloseRect, {140, 40, 40, 255});
    outlineRect(ren, mCharPickerCloseRect, {220, 80, 80, 255});
    Text closeX("X", {255,255,255,255}, mCharPickerCloseRect.x+8, mCharPickerCloseRect.y+5, 14);
    closeX.Render(ren);

    SDL_Rect clipRect = {p.x+4, p.y+46, p.w-8, p.h-50};
    SDL_SetRenderClipRect(ren, &clipRect);

    for (int i = 0; i < (int)mCharCards.size(); ++i) {
        const auto& card = mCharCards[i];
        SDL_Rect r = card.rect;
        r.y -= mCharPickerScroll;
        if (r.y + r.h < clipRect.y || r.y > clipRect.y + clipRect.h) continue;

        bool sel = (i == mProfileIdx);
        fillRect(ren, r, sel ? SDL_Color{50,70,150,255} : SDL_Color{28,32,54,255});
        outlineRect(ren, r, sel ? SDL_Color{120,160,255,255} : SDL_Color{60,70,110,255}, sel?2:1);

        const int previewH = r.h - 36;
        SDL_Rect previewArea = {r.x+4, r.y+4, r.w-8, previewH};

        // Active card: walk animation (if frames loaded), else idle frame
        // Inactive card: idle still frame
        SDL_Texture* displayTex = nullptr;
        if (sel && !card.walkFrames.empty()) {
            int fi = card.walkAnimFrame % (int)card.walkFrames.size();
            displayTex = card.walkFrames[fi];
        } else {
            displayTex = card.previewTex;
        }

        if (displayTex) {
            float tw = 0, th = 0;
            SDL_GetTextureSize(displayTex, &tw, &th);
            if (tw > 0 && th > 0) {
                float sc = std::min((float)previewArea.w/tw, (float)previewArea.h/th);
                int dw = (int)(tw*sc), dh = (int)(th*sc);
                SDL_FRect dstF = {
                    (float)(previewArea.x + (previewArea.w-dw)/2),
                    (float)(previewArea.y + (previewArea.h-dh)),
                    (float)dw, (float)dh
                };
                SDL_RenderTexture(ren, displayTex, nullptr, &dstF);
            }
        } else {
            fillRect(ren, previewArea, {35,38,60,255});
            auto [px2,py2] = Text::CenterInRect("?", 32, previewArea);
            Text q("?", {60,70,100,255}, px2, py2, 32);
            q.Render(ren);
        }

        SDL_Rect nameArea = {r.x+2, r.y+r.h-30, r.w-4, 28};
        fillRect(ren, nameArea, sel ? SDL_Color{40,60,130,255} : SDL_Color{20,22,40,255});
        auto [nx,ny] = Text::CenterInRect(card.name, 13, nameArea);
        Text nameTxt(card.name, sel ? SDL_Color{255,255,180,255} : SDL_Color{200,210,240,255}, nx, ny, 13);
        nameTxt.Render(ren);
    }

    SDL_SetRenderClipRect(ren, nullptr);
    if (mCharPickerMaxScroll > 0) {
        Text sh("scroll for more", {80,90,120,255}, p.x+p.w/2-44, p.y+p.h-16, 11);
        sh.Render(ren);
    }
    Text esc("Esc to close", {60,70,100,255}, p.x+8, p.y+p.h-16, 11);
    esc.Render(ren);
}
