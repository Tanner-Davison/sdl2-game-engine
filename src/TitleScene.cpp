#include "TitleScene.hpp"
#include "EnemyCreatorScene.hpp"
#include "GameScene.hpp"
#include "LevelEditorScene.hpp"
#include "PlayerCreatorScene.hpp"
#include "TileAnimCreatorScene.hpp"
#include "TileAssetScene.hpp"
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
    if (openEnemyCreator) {
        openEnemyCreator = false;
        return std::make_unique<EnemyCreatorScene>();
    }
    if (openTileAssets) {
        openTileAssets = false;
        return std::make_unique<TileAssetScene>();
    }
    return nullptr;
}

// --- openCharPicker ---
static std::vector<std::string> collectPngStrs(const std::string& dir) {
    std::vector<std::string> out;
    std::error_code ec;
    if (!fs::is_directory(dir, ec) || ec) return out;
    for (const auto& e : fs::directory_iterator(dir, ec))
        if (!ec && (e.path().extension() == ".png" || e.path().extension() == ".PNG"))
            out.push_back(e.path().string());
    std::sort(out.begin(), out.end());
    return out;
}

void TitleScene::buildWalkSheet(CharCard& c) {
    if (c.walkLoaded || c.walkPathStrs.empty()) return;
    c.walkLoaded = true;
    c.walkSheet = std::make_unique<SpriteSheet>(c.walkPathStrs);
    c.walkSheet->CreateTexture(mRenderer);
    c.walkRects = c.walkSheet->GetAnimation("");
    c.walkSheet->FreeSurface();
}

std::vector<TitleScene::CharCard> TitleScene::sCardCache;
int TitleScene::sCachedProfileCount = -1;

void TitleScene::preloadCharCards() {
    int profileCount = (int)ScanPlayerProfiles().size();

    // Reuse the static cache if the profile roster hasn't changed
    if (sCachedProfileCount == profileCount && !sCardCache.empty()) {
        mCharCards = std::move(sCardCache);
        sCardCache.clear();
        return;
    }

    // Destroy any stale cache
    for (auto& c : sCardCache) {
        if (c.previewTex) { SDL_DestroyTexture(c.previewTex); c.previewTex = nullptr; }
        c.walkSheet.reset();
    }
    sCardCache.clear();

    mCharCards.clear();

    auto loadIdlePreview = [&](CharCard& c, const std::string& dir,
                               int overrideW = 0, int overrideH = 0) {
        auto pngs = collectPngStrs(dir);
        if (pngs.empty()) return;
        SDL_Surface* raw = IMG_Load(pngs[0].c_str());
        if (!raw) return;
        SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        if (!conv) return;
        SDL_Surface* fin = conv;
        if ((overrideW > 0 && overrideW != conv->w) ||
            (overrideH > 0 && overrideH != conv->h)) {
            int dw = overrideW > 0 ? overrideW : conv->w;
            int dh = overrideH > 0 ? overrideH : conv->h;
            SDL_Surface* scaled = SDL_CreateSurface(dw, dh, SDL_PIXELFORMAT_ARGB8888);
            if (scaled) {
                SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_NONE);
                SDL_BlitSurfaceScaled(conv, nullptr, scaled, nullptr, SDL_SCALEMODE_PIXELART);
                SDL_DestroySurface(conv);
                fin = scaled;
            }
        }
        c.previewTex = SDL_CreateTextureFromSurface(mRenderer, fin);
        if (c.previewTex)
            SDL_SetTextureScaleMode(c.previewTex, SDL_SCALEMODE_PIXELART);
        SDL_DestroySurface(fin);
    };

    {
        CharCard c;
        c.name = "Frost Knight"; c.profilePath = "";
        loadIdlePreview(c, "game_assets/frost_knight_png_sequences/Idle");
        c.walkPathStrs = collectPngStrs("game_assets/frost_knight_png_sequences/Walking");
        c.walkFps = 10.f;
        mCharCards.push_back(std::move(c));
    }

    for (const auto& profilePath : ScanPlayerProfiles()) {
        PlayerProfile prof;
        if (!LoadPlayerProfile(profilePath.string(), prof)) continue;
        CharCard c;
        c.name = prof.name; c.profilePath = profilePath.string();
        const std::string& idleDir = prof.Slot(PlayerAnimSlot::Idle).folderPath;
        if (!idleDir.empty())
            loadIdlePreview(c, idleDir, prof.spriteW, prof.spriteH);
        const std::string& walkDir = prof.Slot(PlayerAnimSlot::Walk).folderPath;
        if (!walkDir.empty())
            c.walkPathStrs = collectPngStrs(walkDir);
        float wfps = prof.Slot(PlayerAnimSlot::Walk).fps;
        c.walkFps = (wfps > 0.f) ? wfps : 8.f;
        mCharCards.push_back(std::move(c));
    }

    sCachedProfileCount = profileCount;
}

void TitleScene::stashCharCardCache() {
    sCardCache = std::move(mCharCards);
    mCharCards.clear();
}

void TitleScene::openCharPicker() {
    mCharPickerOpen      = true;
    mCharPickerScroll    = 0;
    mCharPickerHighlight = mProfileIdx;
    mCharPickerHoverCard   = -1;
    mCharPickerHoverClose  = false;
    mCharPickerHoverSelect = false;

    // Build only the selected character's walk sheet immediately
    if (mProfileIdx >= 0 && mProfileIdx < (int)mCharCards.size())
        buildWalkSheet(mCharCards[mProfileIdx]);

    for (auto& c : mCharCards) {
        c.walkAnimFrame = 0;
        c.walkAnimTimer = 0.f;
    }

    const int PW = std::min(mWindowW - 80, 780);
    const int PH = std::min(mWindowH - 80, 560);
    const int PX = (mWindowW - PW) / 2;
    const int PY = (mWindowH - PH) / 2;
    mCharPickerPanel     = {PX, PY, PW, PH};
    mCharPickerCloseRect = {PX + PW - 36, PY + 6, 30, 30};

    const int FOOTER_H = 52;
    const int CARD_W = 160, CARD_H = 200, COLS = (PW - 32) / (CARD_W + 12), GAP = 12;
    const int startX = PX + (PW - (COLS * (CARD_W + GAP) - GAP)) / 2;
    int cardY = PY + 54;
    for (int i = 0; i < (int)mCharCards.size(); ++i) {
        int col = i % COLS, row = i / COLS;
        mCharCards[i].rect = {startX + col*(CARD_W+GAP), cardY + row*(CARD_H+GAP), CARD_W, CARD_H};
    }
    int totalRows = ((int)mCharCards.size() + COLS - 1) / COLS;
    mCharPickerMaxScroll = std::max(0, totalRows*(CARD_H+GAP) + 54 + 12 - (PH - FOOTER_H));

    const int SEL_W = 160, SEL_H = 36;
    mCharPickerSelectRect = {PX + PW - SEL_W - 12, PY + PH - SEL_H - 8, SEL_W, SEL_H};
}

// --- renderCharPicker ---
void TitleScene::renderCharPicker(SDL_Renderer* ren) {
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
    SDL_FRect full = {0, 0, (float)mWindowW, (float)mWindowH};
    SDL_RenderFillRect(ren, &full);

    const auto& p = mCharPickerPanel;
    fillRounded(ren, p, {18, 20, 34, 248}, 10.f);
    outlineRounded(ren, p, {80, 100, 200, 255}, 2.f, 10.f);
    fillRounded(ren, {p.x, p.y, p.w, 44}, {28, 32, 55, 255}, 10.f);
    Text hdr("Choose Character", {220, 210, 255, 255}, p.x + 16, p.y + 10, 22);
    hdr.Render(ren);

    {
        SDL_Color closeBg = mCharPickerHoverClose ? SDL_Color{180, 55, 55, 255} : SDL_Color{140, 40, 40, 255};
        fillRounded(ren, mCharPickerCloseRect, closeBg, 4.f);
        if (mCharPickerHoverClose)
            outlineRounded(ren, mCharPickerCloseRect, {255, 100, 100, 255}, 2.f, 4.f, 1.f);
    }
    Text closeX("X", {255,255,255,255}, mCharPickerCloseRect.x+8, mCharPickerCloseRect.y+5, 14);
    closeX.Render(ren);

    SDL_Rect clipRect = {p.x+4, p.y+46, p.w-8, p.h-50};
    SDL_SetRenderClipRect(ren, &clipRect);

    for (int i = 0; i < (int)mCharCards.size(); ++i) {
        const auto& card = mCharCards[i];
        SDL_Rect r = card.rect;
        r.y -= mCharPickerScroll;
        if (r.y + r.h < clipRect.y || r.y > clipRect.y + clipRect.h) continue;

        bool highlighted = (i == mCharPickerHighlight);
        bool hovered     = (i == mCharPickerHoverCard);
        bool committed   = (i == mProfileIdx);
        SDL_Color bgCol  = highlighted ? SDL_Color{50,70,150,255}
                         : hovered     ? SDL_Color{38,46,90,255}
                         : committed   ? SDL_Color{30,60,80,255}
                                       : SDL_Color{28,32,54,255};
        SDL_Color outCol = highlighted ? SDL_Color{120,160,255,255}
                         : hovered     ? SDL_Color{100,130,220,255}
                         : committed   ? SDL_Color{60,140,160,255}
                                       : SDL_Color{60,70,110,255};
        fillRounded(ren, r, bgCol, 6.f);
        if (highlighted || hovered)
            outlineRounded(ren, r, outCol, 2.f, 6.f, 1.f);

        const int previewH = r.h - 36;
        SDL_Rect previewArea = {r.x+4, r.y+4, r.w-8, previewH};

        bool walkReady = highlighted && card.walkSheet && card.walkSheet->GetTexture()
                         && !card.walkRects.empty();
        if (walkReady) {
            int fi = card.walkAnimFrame % (int)card.walkRects.size();
            SDL_FRect srcF = {(float)card.walkRects[fi].x, (float)card.walkRects[fi].y,
                              (float)card.walkRects[fi].w, (float)card.walkRects[fi].h};
            float tw = srcF.w, th = srcF.h;
            float sc = std::min((float)previewArea.w/tw, (float)previewArea.h/th);
            int dw = (int)(tw*sc), dh = (int)(th*sc);
            SDL_FRect dstF = {
                (float)(previewArea.x + (previewArea.w-dw)/2),
                (float)(previewArea.y + (previewArea.h-dh)),
                (float)dw, (float)dh
            };
            SDL_RenderTexture(ren, card.walkSheet->GetTexture(), &srcF, &dstF);
        } else if (card.previewTex) {
            float tw = 0, th = 0;
            SDL_GetTextureSize(card.previewTex, &tw, &th);
            if (tw > 0 && th > 0) {
                float sc = std::min((float)previewArea.w/tw, (float)previewArea.h/th);
                int dw = (int)(tw*sc), dh = (int)(th*sc);
                SDL_FRect dstF = {
                    (float)(previewArea.x + (previewArea.w-dw)/2),
                    (float)(previewArea.y + (previewArea.h-dh)),
                    (float)dw, (float)dh
                };
                SDL_RenderTexture(ren, card.previewTex, nullptr, &dstF);
            }
        } else {
            fillRounded(ren, previewArea, {35,38,60,255}, 4.f);
            auto [px2,py2] = Text::CenterInRect("?", 32, previewArea);
            Text q("?", {60,70,100,255}, px2, py2, 32);
            q.Render(ren);
        }

        SDL_Rect nameArea = {r.x+2, r.y+r.h-30, r.w-4, 28};
        SDL_Color nameBg  = highlighted ? SDL_Color{40,60,130,255}
                          : committed   ? SDL_Color{20,55,60,255}
                                        : SDL_Color{20,22,40,255};
        fillRounded(ren, nameArea, nameBg, 4.f);
        std::string displayName = card.name + (committed && !highlighted ? " ✓" : "");
        auto [nx,ny] = Text::CenterInRect(displayName, 13, nameArea);
        SDL_Color nameFg = highlighted ? SDL_Color{255,255,180,255}
                        : committed    ? SDL_Color{120,230,210,255}
                                       : SDL_Color{200,210,240,255};
        Text nameTxt(displayName, nameFg, nx, ny, 13);
        nameTxt.Render(ren);
    }

    SDL_SetRenderClipRect(ren, nullptr);

    SDL_Rect footer = {p.x, p.y + p.h - 52, p.w, 52};
    fillRounded(ren, footer, {20, 22, 38, 255}, 10.f);

    Text esc("Esc to close    Click to highlight    Click again to select", {60,70,100,255}, p.x + 10, p.y + p.h - 36, 11);
    esc.Render(ren);
    if (mCharPickerMaxScroll > 0) {
        Text sh("scroll for more", {80,90,120,255}, p.x + 10, p.y + p.h - 20, 11);
        sh.Render(ren);
    }

    bool selIsHighlighted = (mCharPickerHighlight == mProfileIdx);
    SDL_Color selBg  = selIsHighlighted ? SDL_Color{40, 90, 60, 255}
                     : mCharPickerHoverSelect ? SDL_Color{60, 150, 95, 255}
                                              : SDL_Color{50, 130, 80, 255};
    fillRounded(ren, mCharPickerSelectRect, selBg, 6.f);
    if (!selIsHighlighted)
        outlineRounded(ren, mCharPickerSelectRect, {80, 200, 120, 255}, 2.f, 6.f, 1.f);
    std::string selLabel = selIsHighlighted ? "Already Selected" : "Select Player";
    auto [slx, sly] = Text::CenterInRect(selLabel, 14, mCharPickerSelectRect);
    SDL_Color selTxt = selIsHighlighted ? SDL_Color{120,160,120,255} : SDL_Color{180,255,180,255};
    Text selTxtObj(selLabel, selTxt, slx, sly, 14);
    selTxtObj.Render(ren);
}
