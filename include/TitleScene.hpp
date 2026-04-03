#pragma once
#include "DrawPrimitives.hpp"
#include "GlobalSettings.hpp"
#include "Image.hpp"
#include "PlayerProfile.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SpriteSheet.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class GameScene;
class LevelEditorScene;
class PlayerCreatorScene;
class TileAnimCreatorScene;
class EnemyCreatorScene;
namespace fs = std::filesystem;

class TitleScene : public Scene {
  public:
    void Load(Window& window) override {
        mSDLWindow = window.GetRaw();
        mRenderer  = window.GetRenderer();
        SDL_GetWindowSize(mSDLWindow, &mWindowW, &mWindowH);

        background = std::make_unique<Image>("game_assets/backgrounds/5.png",
                                             FitMode::COVER);

        SDL_Rect windowRect = {0, 0, mWindowW, mWindowH};

        const int btnW  = 180;
        const int btnH  = 50;
        const int gap   = 16;
        const int rowGap = 14;
        const int cx    = mWindowW / 2;

        auto [titleX, titleY] = Text::CenterInRect("Forge2D", 72, windowRect);
        const int row1Y = mWindowH / 2 - 60;
        titleText = std::make_unique<Text>("Forge2D", SDL_Color{255, 255, 255, 255},
                                           titleX, row1Y - 80 - 72, 72);
        const int row2Y = row1Y + btnH + rowGap;

        const float    btnRadius  = 8.0f;
        const SDL_Color hoverOutline = {255, 220, 60, 255};

        editorBtnRect = {cx - btnW - gap / 2, row1Y, btnW, btnH};
        editorButton  = std::make_unique<Rectangle>(editorBtnRect);
        editorButton->SetColor({80, 120, 200, 255});
        editorButton->SetHoverColor({100, 150, 230, 255});
        editorButton->SetCornerRadius(btnRadius);
        editorButton->SetHoverOutline(hoverOutline, 2.0f);
        auto [eBtnX, eBtnY] = Text::CenterInRect("Level Editor", 22, editorBtnRect);
        editorBtnText = std::make_unique<Text>("Level Editor", SDL_Color{255, 255, 255, 255},
                                               eBtnX, eBtnY, 22);

        createPlayerBtnRect = {cx - btnW - gap / 2, row2Y, btnW, btnH};
        createPlayerButton  = std::make_unique<Rectangle>(createPlayerBtnRect);
        createPlayerButton->SetColor({160, 80, 180, 255});
        createPlayerButton->SetHoverColor({200, 110, 220, 255});
        createPlayerButton->SetCornerRadius(btnRadius);
        createPlayerButton->SetHoverOutline(hoverOutline, 2.0f);
        auto [cpbx, cpby] = Text::CenterInRect("Create Player", 20, createPlayerBtnRect);
        createPlayerBtnText = std::make_unique<Text>("Create Player",
                                                     SDL_Color{255, 255, 255, 255},
                                                     cpbx, cpby, 20);

        tileAnimBtnRect = {cx + gap / 2, row2Y, btnW, btnH};
        tileAnimButton  = std::make_unique<Rectangle>(tileAnimBtnRect);
        tileAnimButton->SetColor({40, 140, 160, 255});
        tileAnimButton->SetHoverColor({60, 180, 200, 255});
        tileAnimButton->SetCornerRadius(btnRadius);
        tileAnimButton->SetHoverOutline(hoverOutline, 2.0f);
        auto [tabx, taby] = Text::CenterInRect("Tile Animator", 20, tileAnimBtnRect);
        tileAnimBtnText = std::make_unique<Text>("Tile Animator",
                                                  SDL_Color{255, 255, 255, 255},
                                                  tabx, taby, 20);

        const int row3Y = row2Y + btnH + rowGap;
        createEnemyBtnRect = {cx - btnW - gap / 2, row3Y, btnW, btnH};
        createEnemyButton  = std::make_unique<Rectangle>(createEnemyBtnRect);
        createEnemyButton->SetColor({180, 60, 40, 255});
        createEnemyButton->SetHoverColor({220, 90, 60, 255});
        createEnemyButton->SetCornerRadius(btnRadius);
        createEnemyButton->SetHoverOutline(hoverOutline, 2.0f);
        auto [cebx, ceby] = Text::CenterInRect("Create Enemy", 20, createEnemyBtnRect);
        createEnemyBtnText = std::make_unique<Text>("Create Enemy",
                                                     SDL_Color{255, 255, 255, 255},
                                                     cebx, ceby, 20);

        tileAssetsBtnRect = {cx + gap / 2, row3Y, btnW, btnH};
        tileAssetsButton  = std::make_unique<Rectangle>(tileAssetsBtnRect);
        tileAssetsButton->SetColor({120, 100, 40, 255});
        tileAssetsButton->SetHoverColor({160, 140, 60, 255});
        tileAssetsButton->SetCornerRadius(btnRadius);
        tileAssetsButton->SetHoverOutline(hoverOutline, 2.0f);
        auto [tabx2, taby2] = Text::CenterInRect("Tile Assets", 20, tileAssetsBtnRect);
        tileAssetsBtnText = std::make_unique<Text>("Tile Assets",
                                                    SDL_Color{255, 255, 255, 255},
                                                    tabx2, taby2, 20);

        mRow2BottomY = row3Y + btnH;

        mProfileSelectorBaseY = mRow2BottomY;
        loadSettings();
        scanProfiles();
        restoreProfileFromSettings();
        rebuildProfileSelector();
        loadChosenPreviewTex();
        preloadCharCards();
        mRow2BottomY += 108;

        viewLevelsBtnRect = {cx + gap / 2, row1Y, btnW, btnH};
        viewLevelsButton  = std::make_unique<Rectangle>(viewLevelsBtnRect);
        viewLevelsButton->SetColor({40, 140, 60, 255});
        viewLevelsButton->SetHoverColor({60, 180, 80, 255});
        viewLevelsButton->SetCornerRadius(btnRadius);
        viewLevelsButton->SetHoverOutline(hoverOutline, 2.0f);
        auto [vlx, vly] = Text::CenterInRect("Play Level", 22, viewLevelsBtnRect);
        viewLevelsBtnText = std::make_unique<Text>("Play Level",
            SDL_Color{255, 255, 255, 255}, vlx, vly, 22);

        // Settings button — top-right corner
        mSettingsBtnRect = {mWindowW - 116, 14, 102, 32};

        scanLevels();
    }

    void Unload() override {
        if (mNamingActive)
            SDL_StopTextInput(mSDLWindow);
        stashCharCardCache();
        if (mChosenPreviewTex) { SDL_DestroyTexture(mChosenPreviewTex); mChosenPreviewTex = nullptr; }
    }

    bool HandleEvent(SDL_Event& e) override {
        if (e.type == SDL_EVENT_QUIT) return false;

        if (mNamingActive) {
            if (e.type == SDL_EVENT_TEXT_INPUT) {
                for (char c : std::string(e.text.text))
                    if (std::isalnum((unsigned char)c) || c == '-' || c == '_')
                        mNewLevelName += c;
                rebuildNamePrompt();
                return true;
            }
            if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (e.key.key) {
                    case SDLK_BACKSPACE:
                        if (!mNewLevelName.empty()) { mNewLevelName.pop_back(); rebuildNamePrompt(); }
                        return true;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        if (!mNewLevelName.empty()) {
                            std::string candidate = "levels/" + mNewLevelName + ".json";
                            if (fs::exists(candidate)) {
                                mNameError = "\"" + mNewLevelName + "\" already exists — choose another name";
                                rebuildNamePrompt();
                            } else {
                                mEditorPath = ""; mEditorForce = true; mEditorName = mNewLevelName;
                                mLevelBrowserOpen = false;
                                closeNamePrompt(); openEditor = true;
                            }
                        }
                        return true;
                    case SDLK_ESCAPE: closeNamePrompt(); return true;
                    default: break;
                }
            }
            return true;
        }

        // Delete confirmation dialog intercepts all input when open
        if (mDelConfirmOpen) {
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                mDelConfirmOpen = false; return true;
            }
            if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                auto btn = e.gbutton.button;
                if (btn == SDL_GAMEPAD_BUTTON_EAST || btn == SDL_GAMEPAD_BUTTON_BACK) {
                    mDelConfirmOpen = false; return true;
                }
                if (btn == SDL_GAMEPAD_BUTTON_DPAD_LEFT || btn == SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
                    mDelPadOnYes = !mDelPadOnYes;
                if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                    if (mDelPadOnYes) {
                        std::error_code ec;
                        fs::remove(mDelConfirmPath, ec);
                        mDelConfirmOpen = false; mDelConfirmPath.clear();
                        scanLevels(); mLevelBrowserOpen = true;
                    } else {
                        mDelConfirmOpen = false;
                    }
                }
                return true;
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = (int)e.button.x, my = (int)e.button.y;
                if (hit(mDelConfirmYes, mx, my)) {
                    std::error_code ec;
                    bool removed = fs::remove(mDelConfirmPath, ec);
                    if (!removed)
                        std::print("[Delete] FAILED: '{}' ec={}\n", mDelConfirmPath, ec.message());
                    else
                        std::print("[Delete] OK: '{}'\n", mDelConfirmPath);
                    mDelConfirmOpen = false;
                    mDelConfirmPath.clear();
                    scanLevels(); // refresh list
                    mLevelBrowserOpen = true; // keep browser open
                    return true;
                }
                if (hit(mDelConfirmNo, mx, my)) {
                    mDelConfirmOpen = false; return true;
                }
            }
            return true;
        }

        if (mLevelBrowserOpen) {
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                mLevelBrowserOpen = false; mLoadingEditor = false; return true;
            }
            if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                auto btn = e.gbutton.button;
                if (btn == SDL_GAMEPAD_BUTTON_EAST || btn == SDL_GAMEPAD_BUTTON_BACK) {
                    mLevelBrowserOpen = false; mLoadingEditor = false; return true;
                }
                int count = (int)mLevelButtons.size();
                if (btn == SDL_GAMEPAD_BUTTON_DPAD_UP && count > 0) {
                    mBrowserPadRow = (mBrowserPadRow - 1 + count) % count;
                    mHoverRow = mBrowserPadRow; mHoverPlay = true; mHoverEdit = false; mHoverDel = false;
                    return true;
                }
                if (btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN && count > 0) {
                    mBrowserPadRow = (mBrowserPadRow + 1) % count;
                    mHoverRow = mBrowserPadRow; mHoverPlay = true; mHoverEdit = false; mHoverDel = false;
                    return true;
                }
                if (btn == SDL_GAMEPAD_BUTTON_SOUTH && count > 0 && mBrowserPadRow < count) {
                    mChosenLevel = mLevelButtons[mBrowserPadRow].path;
                    mLevelBrowserOpen = false;
                    { int padCount = 0; SDL_GetGamepads(&padCount);
                      if (padCount >= 2) { mPendingP2Pick = true; mP2ChosenProfile.clear(); openCharPicker(); }
                      else               { startGame = true; } }
                    return true;
                }
                if (btn == SDL_GAMEPAD_BUTTON_WEST && count > 0 && mBrowserPadRow < count) {
                    mLoadingEditor = true; mLoadingTimer = 0.0f; mLoadingIdx = mBrowserPadRow;
                    mEditorPath = mLevelButtons[mBrowserPadRow].path; mEditorForce = false; mEditorName = "";
                    return true;
                }
            }
            if (e.type == SDL_EVENT_MOUSE_WHEEL) {
                mLevelBrowserScroll -= (int)e.wheel.y; clampBrowserScroll(); return true;
            }
            // Hover tracking — use the rects stored by the last Render() pass
            if (e.type == SDL_EVENT_MOUSE_MOTION) {
                int mx = (int)e.motion.x, my = (int)e.motion.y;
                mHoverRow = -1; mHoverEdit = false; mHoverDel = false; mHoverPlay = false;
                mHoverNewBtn = hit(mBrowserNewRect, mx, my);
                mHoverCloseBtn = hit(mBrowserCloseRect, mx, my);
                for (int i = 0; i < (int)mLevelButtons.size(); i++) {
                    const auto& lb = mLevelButtons[i];
                    if (hit(lb.delRect, mx, my))      { mHoverRow = i; mHoverDel  = true; break; }
                    else if (hit(lb.editRect, mx, my)) { mHoverRow = i; mHoverEdit = true; break; }
                    else if (hit(lb.rect, mx, my))     { mHoverRow = i; mHoverPlay = true; break; }
                }
                return true;
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = (int)e.button.x, my = (int)e.button.y;
                if (hit(mBrowserCloseRect, mx, my)) { mLevelBrowserOpen = false; mLoadingEditor = false; return true; }
                if (hit(mBrowserNewRect, mx, my))   { openNamePrompt(); return true; }
                for (int i = 0; i < (int)mLevelButtons.size(); i++) {
                    const auto& lb = mLevelButtons[i];
                    // Check smaller buttons first so they take priority
                    // over the wider play-name rect
                    if (hit(lb.delRect, mx, my)) {
                        mDelConfirmPath = lb.path;
                        mDelConfirmOpen = true;
                        return true;
                    }
                    if (hit(lb.editRect, mx, my)) {
                        mLoadingEditor = true; mLoadingTimer = 0.0f; mLoadingIdx = i;
                        mEditorPath = lb.path; mEditorForce = false; mEditorName = "";
                        return true;
                    }
                    if (hit(lb.rect, mx, my)) {
                        mChosenLevel = lb.path;
                        mLevelBrowserOpen = false;
                        { int padCount = 0; SDL_GetGamepads(&padCount);
                          if (padCount >= 2) { mPendingP2Pick = true; mP2ChosenProfile.clear(); openCharPicker(); }
                          else               { startGame = true; } }
                        return true;
                    }
                }
            }
            return true;
        }



        // Character picker intercepts all input when open
        if (mCharPickerOpen) {
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                mCharPickerOpen = false; mPendingP2Pick = false; return true;
            }
            if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                auto btn = e.gbutton.button;
                int count = (int)mCharCards.size();
                if (btn == SDL_GAMEPAD_BUTTON_EAST || btn == SDL_GAMEPAD_BUTTON_BACK) {
                    mCharPickerOpen = false; mPendingP2Pick = false; return true;
                }
                if (btn == SDL_GAMEPAD_BUTTON_DPAD_LEFT && count > 0) {
                    mCharPickerHighlight = (mCharPickerHighlight - 1 + count) % count;
                    mCharCards[mCharPickerHighlight].walkAnimFrame = 0;
                    mCharCards[mCharPickerHighlight].walkAnimTimer = 0.f;
                    return true;
                }
                if (btn == SDL_GAMEPAD_BUTTON_DPAD_RIGHT && count > 0) {
                    mCharPickerHighlight = (mCharPickerHighlight + 1) % count;
                    mCharCards[mCharPickerHighlight].walkAnimFrame = 0;
                    mCharCards[mCharPickerHighlight].walkAnimTimer = 0.f;
                    return true;
                }
                if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                    if (mPendingP2Pick) {
                        mP2ChosenProfile = (mCharPickerHighlight == 0) ? "" : mCharCards[mCharPickerHighlight].profilePath;
                        mPendingP2Pick   = false;
                        mCharPickerOpen  = false;
                        startGame        = true;
                    } else {
                        mProfileIdx    = mCharPickerHighlight;
                        mChosenProfile = (mCharPickerHighlight == 0) ? "" : mCharCards[mCharPickerHighlight].profilePath;
                        rebuildProfileSelector();
                        loadChosenPreviewTex();
                        saveSettings();
                        mCharPickerOpen = false;
                    }
                    return true;
                }
            }
            if (e.type == SDL_EVENT_MOUSE_MOTION) {
                int mx = (int)e.motion.x, my = (int)e.motion.y;
                mCharPickerHoverClose  = hit(mCharPickerCloseRect, mx, my);
                mCharPickerHoverSelect = hit(mCharPickerSelectRect, mx, my);
                mCharPickerHoverCard   = -1;
                for (int i = 0; i < (int)mCharCards.size(); ++i) {
                    SDL_Rect cr = mCharCards[i].rect;
                    cr.y -= mCharPickerScroll;
                    if (hit(cr, mx, my)) { mCharPickerHoverCard = i; break; }
                }
                return true;
            }
            if (e.type == SDL_EVENT_MOUSE_WHEEL) {
                float fmx, fmy; SDL_GetMouseState(&fmx, &fmy);
                if (hit(mCharPickerPanel, (int)fmx, (int)fmy)) {
                    mCharPickerScroll = std::clamp(mCharPickerScroll - (int)e.wheel.y * 40,
                                                   0, std::max(0, mCharPickerMaxScroll));
                    return true;
                }
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = (int)e.button.x, my = (int)e.button.y;
                if (hit(mCharPickerCloseRect, mx, my)) { mCharPickerOpen = false; mPendingP2Pick = false; return true; }
                if (hit(mCharPickerSelectRect, mx, my)) {
                    if (mPendingP2Pick) {
                        mP2ChosenProfile = (mCharPickerHighlight == 0) ? "" : mCharCards[mCharPickerHighlight].profilePath;
                        mPendingP2Pick   = false;
                        mCharPickerOpen  = false;
                        startGame        = true;
                    } else {
                        mProfileIdx    = mCharPickerHighlight;
                        mChosenProfile = (mCharPickerHighlight == 0) ? "" : mCharCards[mCharPickerHighlight].profilePath;
                        rebuildProfileSelector();
                        loadChosenPreviewTex();
                        saveSettings();
                        mCharPickerOpen = false;
                    }
                    return true;
                }
                for (int i = 0; i < (int)mCharCards.size(); ++i) {
                    SDL_Rect cardRect = mCharCards[i].rect;
                    cardRect.y -= mCharPickerScroll;
                    if (hit(cardRect, mx, my)) {
                        if (mCharPickerHighlight == i) {
                            if (mPendingP2Pick) {
                                mP2ChosenProfile = (i == 0) ? "" : mCharCards[i].profilePath;
                                mPendingP2Pick   = false;
                                mCharPickerOpen  = false;
                                startGame        = true;
                            } else {
                                mProfileIdx    = i;
                                mChosenProfile = (i == 0) ? "" : mCharCards[i].profilePath;
                                rebuildProfileSelector();
                                loadChosenPreviewTex();
                                saveSettings();
                                mCharPickerOpen = false;
                            }
                        } else {
                            mCharPickerHighlight = i;
                            mCharCards[i].walkAnimFrame = 0;
                            mCharCards[i].walkAnimTimer = 0.f;
                        }
                        return true;
                    }
                }
            }
            return true;
        }

        // ---- Settings panel intercepts input when open ----
        if (mSettingsOpen) {
            // Mouse button up: stop slider drag
            if (e.type == SDL_EVENT_MOUSE_BUTTON_UP && e.button.button == SDL_BUTTON_LEFT) {
                if (mSliderDragging) { mSliderDragging = false; saveSettings(); }
                return true;
            }
            // Mouse motion: hover + slider drag
            if (e.type == SDL_EVENT_MOUSE_MOTION) {
                int mx = (int)e.motion.x, my = (int)e.motion.y;
                mSettingsHoverClose  = hit(mSettingsCloseRect, mx, my);
                mSettingsHoverToggle = hit(mBloodToggleRect, mx, my);
                mSettingsHoverSlider = hit(mBloodSliderTrack, mx, my) || mSliderDragging;
                if (mSliderDragging) {
                    float t = (float)(mx - mBloodSliderTrack.x) / (float)mBloodSliderTrack.w;
                    GlobalSettings::Get().bloodIntensity = GlobalSettings::SliderTToIntensity(t);
                }
                return true;
            }
            // Mouse button down: close / toggle / slider
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = (int)e.button.x, my = (int)e.button.y;
                if (hit(mSettingsCloseRect, mx, my)) {
                    mSettingsOpen = false; saveSettings(); return true;
                }
                if (hit(mBloodToggleRect, mx, my)) {
                    GlobalSettings::Get().bloodEnabled = !GlobalSettings::Get().bloodEnabled;
                    saveSettings(); return true;
                }
                if (hit(mBloodSliderTrack, mx, my)) {
                    mSliderDragging = true;
                    float t = (float)(mx - mBloodSliderTrack.x) / (float)mBloodSliderTrack.w;
                    GlobalSettings::Get().bloodIntensity = GlobalSettings::SliderTToIntensity(t);
                    return true;
                }
                // Click outside panel closes it
                SDL_Rect panel = settingsPanelRect();
                if (!hit(panel, mx, my)) { mSettingsOpen = false; saveSettings(); }
                return true;
            }
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                mSettingsOpen = false; saveSettings(); return true;
            }
            return true;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (hit(mSettingsBtnRect, mx, my))     { mSettingsOpen = true; return true; }
            if (hit(editorBtnRect, mx, my))        { mEditorPath = ""; mEditorForce = false; mEditorName = ""; openEditor = true; }
            if (hit(createPlayerBtnRect, mx, my))  { openPlayerCreator  = true; return true; }
            if (hit(tileAnimBtnRect, mx, my))       { openTileAnimCreator = true; return true; }
            if (hit(createEnemyBtnRect, mx, my))    { openEnemyCreator   = true; return true; }
            if (hit(tileAssetsBtnRect, mx, my))      { openTileAssets      = true; return true; }
            if (hit(viewLevelsBtnRect, mx, my))     { mLevelBrowserOpen = true; mLevelBrowserScroll = 0; return true; }
            if (hit(mChooseCharBtnRect, mx, my))    { openCharPicker(); return true; }
        }

        if (handleGamepadNav(e)) return true;

        editorButton->HandleEvent(e);
        if (createPlayerButton)  createPlayerButton->HandleEvent(e);
        if (tileAnimButton)      tileAnimButton->HandleEvent(e);
        if (createEnemyButton)   createEnemyButton->HandleEvent(e);
        if (tileAssetsButton)    tileAssetsButton->HandleEvent(e);
        if (viewLevelsButton)    viewLevelsButton->HandleEvent(e);
        if (mChooseCharButton)   mChooseCharButton->HandleEvent(e);
        return true;
    }

    void Update(float dt) override {
        if (mLoadingEditor) {
            mLoadingTimer += dt;
            if (mLoadingTimer >= 0.35f) {
                mLoadingEditor = false;
                mLevelBrowserOpen = false;
                openEditor = true;
            }
        }

        if (!mCharPickerOpen || mCharCards.empty()) return;

        // Ensure highlighted card is ready immediately
        if (mCharPickerHighlight >= 0 && mCharPickerHighlight < (int)mCharCards.size())
            buildWalkSheet(mCharCards[mCharPickerHighlight]);

        // Build one other card's sheet per frame in the background
        for (auto& c : mCharCards) {
            if (!c.walkLoaded && !c.walkPathStrs.empty()) {
                buildWalkSheet(c);
                break;
            }
        }

        if (mCharPickerHighlight >= 0 && mCharPickerHighlight < (int)mCharCards.size()) {
            auto& active = mCharCards[mCharPickerHighlight];
            if (!active.walkRects.empty()) {
                active.walkAnimTimer += dt;
                float interval = 1.f / active.walkFps;
                while (active.walkAnimTimer >= interval) {
                    active.walkAnimTimer -= interval;
                    active.walkAnimFrame = (active.walkAnimFrame + 1) % (int)active.walkRects.size();
                }
            }
        }
    }

    void Render(Window& window, float /*alpha*/ = 1.0f) override {
        window.Render();
        SDL_Renderer* ren = window.GetRenderer();
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        background->Render(ren);
        titleText->Render(ren);

        editorButton->Render(ren);  editorBtnText->Render(ren);
        if (createPlayerButton) { createPlayerButton->Render(ren); createPlayerBtnText->Render(ren); }
        if (tileAnimButton)     { tileAnimButton->Render(ren);     tileAnimBtnText->Render(ren); }
        if (createEnemyButton)  { createEnemyButton->Render(ren);  createEnemyBtnText->Render(ren); }
        if (tileAssetsButton)   { tileAssetsButton->Render(ren);   tileAssetsBtnText->Render(ren); }

        if (mProfileSelectorBg.w > 0) {
            fillRounded(ren, mProfileSelectorBg, {22, 26, 44, 240}, 8.f);
            outlineRounded(ren, mProfileSelectorBg, {60, 80, 140, 255}, 1.f, 8.f);

            fillRounded(ren, mProfileSpriteArea, {16, 18, 32, 255}, 6.f);

            if (mChosenPreviewTex) {
                float tw = 0, th = 0;
                SDL_GetTextureSize(mChosenPreviewTex, &tw, &th);
                if (tw > 0 && th > 0) {
                    const auto& sa = mProfileSpriteArea;
                    float sc = std::min((float)sa.w / tw, (float)sa.h / th);
                    int dw = (int)(tw * sc), dh = (int)(th * sc);
                    SDL_FRect dst = {(float)(sa.x + (sa.w - dw) / 2),
                                     (float)(sa.y + sa.h - dh),
                                     (float)dw, (float)dh};
                    SDL_RenderTexture(ren, mChosenPreviewTex, nullptr, &dst);
                }
            }
        }
        if (mProfileLabel)    mProfileLabel->Render(ren);
        if (mProfileNameText) mProfileNameText->Render(ren);
        if (mChooseCharButton) {
            mChooseCharButton->Render(ren);
            mChooseCharBtnText->Render(ren);
        }

        if (viewLevelsButton) { viewLevelsButton->Render(ren); viewLevelsBtnText->Render(ren); }

        if (mPadFocus >= 0) {
            SDL_Rect fr = padFocusRect();
            if (fr.w > 0) {
                SDL_Rect expanded = {fr.x - 3, fr.y - 3, fr.w + 6, fr.h + 6};
                outlineRect(ren, expanded, {255, 220, 60, 255}, 3);
            }
        }

        // --- Settings button (always visible) ---
        {
            float _smx = 0, _smy = 0; SDL_GetMouseState(&_smx, &_smy);
            bool hover = hit(mSettingsBtnRect, (int)_smx, (int)_smy);
            SDL_Color sbBg  = hover ? SDL_Color{70,60,100,255} : SDL_Color{45,38,70,255};
            fillRounded(ren, mSettingsBtnRect, sbBg, 6.f);
            if (hover)
                outlineRounded(ren, mSettingsBtnRect, {255,220,60,255}, 2.f, 6.f, 1.f);
            else
                outlineRounded(ren, mSettingsBtnRect, {80,70,120,255}, 1.f, 6.f);
            auto [stx, sty] = Text::CenterInRect("Settings", 14, mSettingsBtnRect);
            Text settingsLbl("Settings", {210, 200, 255, 255}, stx, sty, 14);
            settingsLbl.Render(ren);
        }

        // --- Settings overlay ---
        if (mSettingsOpen) {
            renderSettingsPanel(ren);
        }

        // --- Character picker popup ---
        if (mCharPickerOpen) {
            renderCharPicker(ren);
        }

        // --- Delete confirmation modal ---
        if (mDelConfirmOpen) {
            int W = window.GetWidth(), H = window.GetHeight();
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
            SDL_FRect full = {0,0,(float)W,(float)H};
            SDL_RenderFillRect(ren, &full);

            int mw = 400, mh = 170;
            int mx = (W - mw) / 2, my = (H - mh) / 2;
            SDL_Rect box = {mx, my, mw, mh};
            fillRounded(ren, box, {28, 18, 18, 255}, 8.f);
            outlineRounded(ren, box, {200, 60, 60, 255}, 2.f, 8.f);

            std::string name = fs::path(mDelConfirmPath).stem().string();
            Text t1("Delete level?", {255, 100, 100, 255}, mx + 20, my + 16, 22);
            t1.Render(ren);
            Text t2("\"" + name + "\"", {220, 200, 200, 255}, mx + 20, my + 48, 18);
            t2.Render(ren);
            Text t3("This cannot be undone.", {180, 140, 140, 255}, mx + 20, my + 76, 13);
            t3.Render(ren);

            mDelConfirmYes = {mx + 24,        my + mh - 50, 140, 36};
            mDelConfirmNo  = {mx + mw - 164,  my + mh - 50, 140, 36};

            bool yesFocused = mDelPadOnYes && mPadFocus >= 0;
            bool noFocused  = !mDelPadOnYes && mPadFocus >= 0;

            fillRounded(ren, mDelConfirmYes, yesFocused ? SDL_Color{200,40,40,255} : SDL_Color{160,30,30,255}, 6.f);
            if (yesFocused)
                outlineRounded(ren, mDelConfirmYes, {255,220,60,255}, 2.f, 6.f, 1.f);
            auto [yx, yy] = Text::CenterInRect("Delete", 16, mDelConfirmYes);
            Text yLbl("Delete", {255, 200, 200, 255}, yx, yy, 16);
            yLbl.Render(ren);

            fillRounded(ren, mDelConfirmNo, noFocused ? SDL_Color{60,60,90,255} : SDL_Color{40,40,60,255}, 6.f);
            if (noFocused)
                outlineRounded(ren, mDelConfirmNo, {255,220,60,255}, 2.f, 6.f, 1.f);
            auto [nx, ny] = Text::CenterInRect("Cancel", 16, mDelConfirmNo);
            Text nLbl("Cancel", {180, 180, 220, 255}, nx, ny, 16);
            nLbl.Render(ren);

            std::string delHint = "Esc / B to cancel";
            auto dhs = Text::Measure(delHint, 11);
            Text hint(delHint, {80, 80, 100, 255}, mx + mw/2 - dhs.x/2, my + mh - 14, 11);
            hint.Render(ren);
        }

        // --- Name prompt modal ---
        if (mNamingActive) {
            int W = window.GetWidth(), H = window.GetHeight();
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
            SDL_FRect full = {0,0,(float)W,(float)H};
            SDL_RenderFillRect(ren, &full);

            int mw = 480, mh = 200;
            int mx = (W - mw) / 2, my = (H - mh) / 2;
            SDL_Rect box = {mx, my, mw, mh};
            fillRounded(ren, box, {28, 28, 42, 255}, 8.f);
            outlineRounded(ren, box, {80, 120, 220, 255}, 2.f, 8.f);

            if (promptTitle) promptTitle->Render(ren);
            SDL_Rect field = {mx + 20, my + 70, mw - 40, 40};
            fillRounded(ren, field, {18, 18, 32, 255}, 4.f);
            outlineRounded(ren, field,
                mNameError.empty() ? SDL_Color{80, 120, 220, 255} : SDL_Color{220, 80, 80, 255}, 2.f, 4.f);
            if (promptInput) promptInput->Render(ren);
            if (promptError) promptError->Render(ren);
            if (promptHint)  promptHint->Render(ren);
        }

        // --- Level browser modal ---
        if (mLevelBrowserOpen && !mDelConfirmOpen) {
            int W = window.GetWidth(), H = window.GetHeight();
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
            SDL_FRect full = {0,0,(float)W,(float)H};
            SDL_RenderFillRect(ren, &full);

            int pw = 520, ph = std::min(H - 80, 560);
            int px = (W - pw) / 2, py = (H - ph) / 2;
            SDL_Rect panel = {px, py, pw, ph};
            fillRect(ren, panel, {18, 20, 32, 245});
            outlineRect(ren, panel, {60, 90, 180, 255}, 2);

            fillRect(ren, {px + 2, py + 2, pw - 4, 44}, {28, 32, 52, 255});
            Text hdr("Levels", {255, 215, 0, 255}, px + 16, py + 10, 22);
            hdr.Render(ren);
            {
                std::string countStr = std::to_string(mLevelButtons.size()) + " levels";
                Text cnt(countStr, {100, 110, 150, 255}, px + 100, py + 16, 13);
                cnt.Render(ren);
            }

            mBrowserCloseRect = {px + pw - 38, py + 6, 32, 32};
            {
                SDL_Color closeBg = mHoverCloseBtn ? SDL_Color{160, 50, 50, 255} : SDL_Color{120, 35, 35, 255};
                fillRounded(ren, mBrowserCloseRect, closeBg, 4.f);
                if (mHoverCloseBtn)
                    outlineRounded(ren, mBrowserCloseRect, {255, 100, 100, 255}, 2.f, 4.f, 1.f);
            }
            auto [cxx, cxy] = Text::CenterInRect("X", 14, mBrowserCloseRect);
            Text closeX("X", {255, 220, 220, 255}, cxx, cxy, 14);
            closeX.Render(ren);

            int newBtnY = py + 54;
            mBrowserNewRect = {px + 16, newBtnY, pw - 32, 34};
            {
                SDL_Color newBg = mHoverNewBtn ? SDL_Color{60, 65, 130, 255} : SDL_Color{45, 50, 100, 255};
                fillRounded(ren, mBrowserNewRect, newBg, 6.f);
                if (mHoverNewBtn)
                    outlineRounded(ren, mBrowserNewRect, {255, 220, 60, 255}, 2.f, 6.f, 1.f);
            }
            auto [nlx, nly] = Text::CenterInRect("+ New Level", 16, mBrowserNewRect);
            Text newLbl("+ New Level", {160, 180, 255, 255}, nlx, nly, 16);
            newLbl.Render(ren);

            int listTop = py + 96, listBottom = py + ph - 8;
            mBrowserListY = listTop;
            int rowH = 44, rowGap = 4, pad = 16;
            int rowX = px + pad;
            int rowW = pw - pad * 2;
            int editW = 56, delW = 46, btnGap = 4;
            int playW = rowW - editW - delW - btnGap * 2;

            SDL_Rect clipRect = {px + 2, listTop, pw - 4, listBottom - listTop};
            SDL_SetRenderClipRect(ren, (SDL_Rect*)&clipRect);

            int listY = listTop - mLevelBrowserScroll * (rowH + rowGap);
            for (int i = 0; i < (int)mLevelButtons.size(); i++) {
                int ry = listY + i * (rowH + rowGap);
                if (ry + rowH < listTop - rowH || ry > listBottom + rowH) continue;

                bool isHover = (i == mHoverRow) || (i == mBrowserPadRow && mPadFocus >= 0);
                bool isLoading = (mLoadingEditor && i == mLoadingIdx);

                constexpr float kRowR = 6.f, kBtnR = 4.f;
                const SDL_Color kGold = {255, 220, 60, 255};

                SDL_Rect pr = {rowX, ry, playW, rowH};
                SDL_Color playBg  = isLoading ? SDL_Color{60,60,60,255}
                                   : (isHover && mHoverPlay) ? SDL_Color{40,140,70,255}
                                   : isHover ? SDL_Color{35,120,60,255}
                                   : SDL_Color{30,100,50,255};
                fillRounded(ren, pr, playBg, kRowR);
                if (isHover && mHoverPlay)
                    outlineRounded(ren, pr, kGold, 2.f, kRowR, 1.f);
                std::string nm = fs::path(mLevelButtons[i].path).stem().string();
                auto [lx, ly] = Text::CenterInRect(nm, 16, pr);
                Text lbl(nm, {255,255,255,255}, lx, ly, 16);
                lbl.Render(ren);

                SDL_Rect er = {rowX + playW + btnGap, ry, editW, rowH};
                SDL_Color editBg  = isLoading ? SDL_Color{80,80,80,255}
                                    : (isHover && mHoverEdit) ? SDL_Color{70,110,200,255}
                                    : SDL_Color{45,70,150,255};
                fillRounded(ren, er, editBg, kBtnR);
                if (isHover && mHoverEdit)
                    outlineRounded(ren, er, kGold, 2.f, kBtnR, 1.f);
                if (isLoading) {
                    const char* dots[] = {".  ", ".. ", "...", " ..", "  ."};
                    int dotIdx = (int)(mLoadingTimer * 8.0f) % 5;
                    auto [ldx, ldy] = Text::CenterInRect(dots[dotIdx], 14, er);
                    Text ldots(dots[dotIdx], {200,220,255,255}, ldx, ldy, 14);
                    ldots.Render(ren);
                } else {
                    auto [ex, ey] = Text::CenterInRect("Edit", 14, er);
                    Text elbl("Edit", {200,220,255,255}, ex, ey, 14);
                    elbl.Render(ren);
                }

                SDL_Rect dr = {rowX + playW + btnGap + editW + btnGap, ry, delW, rowH};
                SDL_Color delBg  = (isHover && mHoverDel) ? SDL_Color{180,45,45,255}
                                   : SDL_Color{120,30,30,255};
                fillRounded(ren, dr, delBg, kBtnR);
                if (isHover && mHoverDel)
                    outlineRounded(ren, dr, kGold, 2.f, kBtnR, 1.f);
                auto [dx, dy] = Text::CenterInRect("Del", 12, dr);
                Text dlbl("Del", {255,200,200,255}, dx, dy, 12);
                dlbl.Render(ren);

                mLevelButtons[i].rect     = pr;
                mLevelButtons[i].editRect = er;
                mLevelButtons[i].delRect  = dr;
            }

            // Remove clip
            SDL_SetRenderClipRect(ren, nullptr);

            // Scroll indicator
            int totalH = (int)mLevelButtons.size() * (rowH + rowGap);
            int listH  = listBottom - listTop;
            if (totalH > listH) {
                float viewFrac = (float)listH / (float)totalH;
                int barH  = std::max(20, (int)(listH * viewFrac));
                float scrollFrac = (float)(mLevelBrowserScroll * (rowH + rowGap)) / (float)(totalH - listH);
                scrollFrac = std::clamp(scrollFrac, 0.0f, 1.0f);
                int barY  = listTop + (int)((listH - barH) * scrollFrac);
                fillRect(ren, {px + pw - 8, barY, 4, barH}, {80, 120, 200, 160});
            }

            // Gamepad hints
            {
                std::string bHint = "A = Play    X = Edit    B = Close";
                auto bhs = Text::Measure(bHint, 11);
                Text bh(bHint, {80, 90, 120, 255}, px + pw/2 - bhs.x/2, py + ph - 16, 11);
                bh.Render(ren);
            }

            // Empty state
            if (mLevelButtons.empty()) {
                auto [etx, ety] = Text::CenterInRect("No levels yet", 16, {px, listTop, pw, listH});
                Text empty("No levels yet", {100, 110, 140, 255}, etx, ety, 16);
                empty.Render(ren);
                auto [esx, esy] = Text::CenterInRect("Click + New Level to create one", 12,
                    {px, ety + 24, pw, 20});
                Text esub("Click + New Level to create one", {80, 90, 120, 255}, esx, esy, 12);
                esub.Render(ren);
            }
        }

        window.Update();
    }

    std::unique_ptr<Scene> NextScene() override;

  private:
    static bool hit(const SDL_Rect& r, int x, int y) {
        if (r.w <= 0 || r.h <= 0) return false;
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    }

    // --- Draw helpers ---
    static SDL_FRect toF(SDL_Rect r) {
        return {(float)r.x, (float)r.y, (float)r.w, (float)r.h};
    }
    static void fillRect(SDL_Renderer* ren, SDL_Rect r, SDL_Color c) {
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
        SDL_FRect fr = toF(r);
        SDL_RenderFillRect(ren, &fr);
    }
    static void outlineRect(SDL_Renderer* ren, SDL_Rect r, SDL_Color c, int t = 1) {
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
        SDL_FRect sides[4] = {
            {(float)r.x,           (float)r.y,           (float)r.w, (float)t},
            {(float)r.x,           (float)(r.y+r.h-t),   (float)r.w, (float)t},
            {(float)r.x,           (float)r.y,           (float)t,   (float)r.h},
            {(float)(r.x+r.w-t),   (float)r.y,           (float)t,   (float)r.h}
        };
        for (auto& s : sides) SDL_RenderFillRect(ren, &s);
    }
    static void fillRounded(SDL_Renderer* ren, SDL_Rect r, SDL_Color c, float rad) {
        DrawFilledRoundedRect(ren, toF(r), c, rad);
    }
    static void outlineRounded(SDL_Renderer* ren, SDL_Rect r, SDL_Color c, float thick, float rad, float gap = 0.f) {
        DrawRoundedRectOutline(ren, toF(r), c, thick, rad, gap);
    }

    // --- Name prompt helpers ---
    void openNamePrompt() {
        mNamingActive = true; mNewLevelName.clear(); mNameError.clear();
        mLevelBrowserOpen = false;
        SDL_StartTextInput(mSDLWindow);
        rebuildNamePrompt();
    }
    void closeNamePrompt() {
        mNamingActive = false; mNewLevelName.clear(); mNameError.clear();
        SDL_StopTextInput(mSDLWindow);
        promptTitle.reset(); promptInput.reset(); promptError.reset(); promptHint.reset();
    }
    void rebuildNamePrompt() {
        int W = mWindowW, H = mWindowH, mw = 480, mh = 200;
        int mx = (W - mw) / 2, my = (H - mh) / 2;
        promptTitle = std::make_unique<Text>("Name your level:", SDL_Color{200, 210, 255, 255}, mx + 20, my + 20, 22);
        std::string display = mNewLevelName.empty() ? "|" : mNewLevelName + "|";
        promptInput = std::make_unique<Text>(display, SDL_Color{255, 255, 255, 255}, mx + 28, my + 78, 20);
        promptHint  = std::make_unique<Text>("Letters, numbers, - and _ only.   Enter=confirm   Esc=cancel",
                                             SDL_Color{100, 110, 140, 255}, mx + 20, my + 126, 13);
        if (!mNameError.empty())
            promptError = std::make_unique<Text>(mNameError, SDL_Color{255, 100, 100, 255}, mx + 20, my + 152, 13);
        else
            promptError.reset();
    }

    // --- Level button list ---
    struct LevelButton {
        std::string path;
        SDL_Rect    rect     = {-1,-1,0,0};
        SDL_Rect    editRect = {-1,-1,0,0};
        SDL_Rect    delRect  = {-1,-1,0,0};
    };
    int  mHoverRow       = -1;
    bool mHoverEdit      = false;
    bool mHoverDel       = false;
    bool mHoverPlay      = false;
    bool mHoverNewBtn    = false;
    bool mHoverCloseBtn  = false;
    bool mLoadingEditor  = false;
    float mLoadingTimer  = 0.0f;
    int   mLoadingIdx    = -1;
    void scanLevels() {
        mLevelButtons.clear();
        if (!fs::exists("levels")) return;
        std::vector<fs::path> found;
        for (const auto& entry : fs::directory_iterator("levels"))
            if (entry.path().extension() == ".json") found.push_back(entry.path());
        std::sort(found.begin(), found.end());
        for (const auto& p : found)
            mLevelButtons.push_back({p.string(), {}, {}});
    }
    void clampBrowserScroll() {
        int rowH = 44, rowGap = 4, ph = std::min(mWindowH - 80, 560);
        int listH = ph - 104;
        int maxScroll = std::max(0, ((int)mLevelButtons.size() * (rowH + rowGap) - listH) / (rowH + rowGap));
        if (mLevelBrowserScroll < 0)         mLevelBrowserScroll = 0;
        if (mLevelBrowserScroll > maxScroll) mLevelBrowserScroll = maxScroll;
    }

    // --- Profile selector helpers ---
    void scanProfiles() {
        mProfiles.clear();
        mProfiles.push_back("");
        for (const auto& p : ScanPlayerProfiles()) mProfiles.push_back(p.string());
        mProfileIdx = 0;
    }
    void rebuildProfileSelector() {
        int cx = mWindowW / 2, selY = mProfileSelectorBaseY + 10;
        constexpr int selW = 400, selH = 88;
        constexpr int spriteW = 72, spriteH = 76;
        constexpr int btnW2 = 90, btnH2 = 32;
        constexpr int pad = 6;

        mProfileSelectorBg  = {cx - selW/2, selY, selW, selH};
        mProfileSpriteArea  = {cx - selW/2 + pad, selY + pad, spriteW, spriteH};

        int textX = mProfileSpriteArea.x + spriteW + 10;
        mProfileLabel = std::make_unique<Text>("Character", SDL_Color{120, 130, 170, 255},
                                               textX, selY + 16, 11);
        std::string name = "Frost Knight (default)";
        if (mProfileIdx > 0 && mProfileIdx < (int)mProfiles.size())
            name = fs::path(mProfiles[mProfileIdx]).stem().string();
        mProfileNameText = std::make_unique<Text>(name, SDL_Color{255, 255, 255, 255},
                                                  textX, selY + 34, 18);

        mChooseCharBtnRect = {cx + selW/2 - btnW2 - pad, selY + (selH - btnH2) / 2, btnW2, btnH2};
        mChooseCharButton = std::make_unique<Rectangle>(mChooseCharBtnRect);
        mChooseCharButton->SetColor({55, 40, 110, 255});
        mChooseCharButton->SetHoverColor({80, 60, 140, 255});
        mChooseCharButton->SetCornerRadius(6.0f);
        mChooseCharButton->SetHoverOutline({255, 220, 60, 255}, 2.0f);
        auto [cbx, cby] = Text::CenterInRect("Choose...", 13, mChooseCharBtnRect);
        mChooseCharBtnText = std::make_unique<Text>("Choose...", SDL_Color{200, 180, 255, 255}, cbx, cby, 13);
    }

    void loadChosenPreviewTex() {
        if (mChosenPreviewTex) { SDL_DestroyTexture(mChosenPreviewTex); mChosenPreviewTex = nullptr; }
        std::string idleDir;
        if (mChosenProfile.empty()) {
            idleDir = "game_assets/frost_knight_png_sequences/Idle";
        } else {
            PlayerProfile prof;
            if (LoadPlayerProfile(mChosenProfile, prof))
                idleDir = prof.Slot(PlayerAnimSlot::Idle).folderPath;
        }
        if (idleDir.empty()) return;
        std::error_code ec;
        if (!fs::is_directory(idleDir, ec) || ec) return;
        std::vector<fs::path> pngs;
        for (const auto& e : fs::directory_iterator(idleDir, ec))
            if (!ec && (e.path().extension() == ".png" || e.path().extension() == ".PNG"))
                pngs.push_back(e.path());
        if (pngs.empty()) return;
        std::sort(pngs.begin(), pngs.end());
        SDL_Surface* raw = IMG_Load(pngs[0].string().c_str());
        if (!raw) return;
        SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        if (!conv) return;
        mChosenPreviewTex = SDL_CreateTextureFromSurface(mRenderer, conv);
        SDL_DestroySurface(conv);
        if (mChosenPreviewTex)
            SDL_SetTextureScaleMode(mChosenPreviewTex, SDL_SCALEMODE_PIXELART);
    }

    static constexpr const char* kSettingsPath = "forge2d_settings.json";

    void saveSettings() const {
        json j;
        j["chosenProfile"]   = mChosenProfile;
        j["bloodEnabled"]    = GlobalSettings::Get().bloodEnabled;
        j["bloodIntensity"]  = GlobalSettings::Get().bloodIntensity;
        std::ofstream f(kSettingsPath);
        if (f.is_open()) f << j.dump(4);
    }
    void loadSettings() {
        std::ifstream f(kSettingsPath);
        if (!f.is_open()) return;
        json j;
        try { f >> j; } catch (...) { return; }
        mChosenProfile = j.value("chosenProfile", std::string{});
        GlobalSettings::Get().bloodEnabled   = j.value("bloodEnabled",   true);
        GlobalSettings::Get().bloodIntensity = j.value("bloodIntensity", 1.0f);
    }

    void restoreProfileFromSettings() {
        if (mChosenProfile.empty()) return;
        for (int i = 0; i < (int)mProfiles.size(); ++i) {
            if (mProfiles[i] == mChosenProfile) { mProfileIdx = i; return; }
        }
        mChosenProfile.clear();
    }

    // --- Character picker popup ---
    struct CharCard {
        std::string  name;
        std::string  profilePath;
        SDL_Texture* previewTex = nullptr;
        SDL_Rect     rect{};

        std::vector<std::string>      walkPathStrs;
        std::unique_ptr<SpriteSheet>  walkSheet;
        std::vector<SDL_Rect>         walkRects;
        bool  walkLoaded    = false;
        int   walkAnimFrame = 0;
        float walkAnimTimer = 0.f;
        float walkFps       = 8.f;
    };
    std::vector<CharCard>        mCharCards;
    static std::vector<CharCard> sCardCache;
    static int                   sCachedProfileCount;
    bool                  mCharPickerOpen        = false;
    int                   mCharPickerScroll      = 0;
    int                   mCharPickerMaxScroll   = 0;
    int                   mCharPickerHighlight   = 0;
    int                   mCharPickerHoverCard   = -1;
    bool                  mCharPickerHoverClose  = false;
    bool                  mCharPickerHoverSelect = false;
    SDL_Rect              mCharPickerPanel{};
    SDL_Rect              mCharPickerCloseRect{};
    SDL_Rect              mCharPickerSelectRect{};
    SDL_Rect              mChooseCharBtnRect{};
    SDL_Renderer*         mRenderer = nullptr;

    void preloadCharCards();
    void stashCharCardCache();
    void openCharPicker();
    void buildWalkSheet(CharCard& c);
    void renderCharPicker(SDL_Renderer* ren);

    // --- Settings panel ------------------------------------------------------
    SDL_Rect settingsPanelRect() const {
        constexpr int PW = 460, PH = 310;
        return {(mWindowW - PW) / 2, (mWindowH - PH) / 2, PW, PH};
    }

    void renderSettingsPanel(SDL_Renderer* ren) {
        // Dim background
        SDL_SetRenderDrawColor(ren, 0, 0, 0, 190);
        SDL_FRect full = {0, 0, (float)mWindowW, (float)mWindowH};
        SDL_RenderFillRect(ren, &full);

        SDL_Rect p = settingsPanelRect();

        // Panel body
        fillRounded(ren, p, {14, 12, 22, 255}, 10.f);
        outlineRounded(ren, p, {90, 70, 140, 255}, 2.f, 10.f);

        // Header bar
        SDL_Rect hdr = {p.x, p.y, p.w, 46};
        fillRounded(ren, hdr, {26, 20, 44, 255}, 10.f);
        // Bottom of rounded header → straight edge
        fillRounded(ren, {p.x, p.y + 24, p.w, 22}, {26, 20, 44, 255}, 0.f);

        Text title("Settings", {220, 210, 255, 255}, p.x + 18, p.y + 10, 22);
        title.Render(ren);

        // Close button
        mSettingsCloseRect = {p.x + p.w - 38, p.y + 7, 30, 30};
        SDL_Color closeBg = mSettingsHoverClose ? SDL_Color{190, 50, 50, 255}
                                                : SDL_Color{140, 35, 35, 255};
        fillRounded(ren, mSettingsCloseRect, closeBg, 4.f);
        if (mSettingsHoverClose)
            outlineRounded(ren, mSettingsCloseRect, {255, 100, 100, 255}, 1.5f, 4.f);
        auto [cxx, cxy] = Text::CenterInRect("X", 14, mSettingsCloseRect);
        Text closeX("X", {255, 220, 220, 255}, cxx, cxy, 14);
        closeX.Render(ren);

        // ---- Section: Blood & Gore ------------------------------------------
        int sy = p.y + 56;
        Text sectionLbl("Blood & Gore", {160, 120, 200, 255}, p.x + 18, sy, 12);
        sectionLbl.Render(ren);

        // Thin divider under section label
        SDL_SetRenderDrawColor(ren, 60, 45, 90, 255);
        SDL_FRect div = {(float)(p.x + 18), (float)(sy + 18), (float)(p.w - 36), 1.f};
        SDL_RenderFillRect(ren, &div);

        // ---- Blood toggle ---------------------------------------------------
        int toggleY = sy + 30;
        Text toggleLbl("Blood Effects", {210, 205, 230, 255}, p.x + 18, toggleY + 5, 16);
        toggleLbl.Render(ren);

        // Pill switch: 60 × 28, right-aligned inside panel
        constexpr int PILL_W = 60, PILL_H = 28;
        mBloodToggleRect = {p.x + p.w - PILL_W - 20, toggleY, PILL_W, PILL_H};
        bool on = GlobalSettings::Get().bloodEnabled;

        // Pill background
        SDL_Color pillBg = on ? SDL_Color{180, 30, 30, 255} : SDL_Color{45, 40, 60, 255};
        fillRounded(ren, mBloodToggleRect, pillBg, 14.f);
        if (mSettingsHoverToggle)
            outlineRounded(ren, mBloodToggleRect, {255, 180, 180, 255}, 1.5f, 14.f);

        // Toggle knob (circle) — moves left/right
        constexpr int KNOB = 22;
        int knobX = on ? (mBloodToggleRect.x + PILL_W - KNOB - 3)
                       : (mBloodToggleRect.x + 3);
        int knobY = mBloodToggleRect.y + (PILL_H - KNOB) / 2;
        SDL_Rect knob = {knobX, knobY, KNOB, KNOB};
        fillRounded(ren, knob, {245, 240, 255, 255}, 11.f);

        // ON / OFF label inside pill
        const char* pillTxt = on ? "ON" : "OFF";
        int textSide = on ? (mBloodToggleRect.x + 7) : (mBloodToggleRect.x + 28);
        Text pillLbl(pillTxt, on ? SDL_Color{255,200,200,255} : SDL_Color{130,120,160,255},
                     textSide, mBloodToggleRect.y + 7, 11);
        pillLbl.Render(ren);

        // ---- Blood Intensity slider -----------------------------------------
        int sliderLabelY = toggleY + 52;
        Text sliderLbl("Blood Intensity", {210, 205, 230, 255}, p.x + 18, sliderLabelY, 16);
        sliderLbl.Render(ren);

        // Grey-out label + slider when blood is disabled
        if (!on) {
            SDL_SetRenderDrawColor(ren, 14, 12, 22, 140);
            SDL_FRect veil = {(float)(p.x + 16), (float)(sliderLabelY - 2),
                              (float)(p.w - 32), 100.f};
            SDL_RenderFillRect(ren, &veil);
        }

        // Track rect
        constexpr int TRACK_H = 8;
        int trackY = sliderLabelY + 32;
        mBloodSliderTrack = {p.x + 20, trackY, p.w - 40, TRACK_H};

        // Track background
        fillRounded(ren, mBloodSliderTrack, {40, 32, 55, 255}, 4.f);

        // Red fill up to handle position
        float sliderT = GlobalSettings::Get().SliderT();
        int fillW = (int)(sliderT * mBloodSliderTrack.w);
        if (fillW > 0) {
            SDL_Rect fill = {mBloodSliderTrack.x, mBloodSliderTrack.y,
                             std::max(fillW, 8), TRACK_H};
            fillRounded(ren, fill, {190, 30, 30, 255}, 4.f);
        }

        // Handle (circle knob centered on track)
        constexpr int HANDLE = 22;
        int handleX = mBloodSliderTrack.x + fillW - HANDLE / 2;
        handleX = std::clamp(handleX, mBloodSliderTrack.x,
                             mBloodSliderTrack.x + mBloodSliderTrack.w - HANDLE);
        int handleY = mBloodSliderTrack.y + (TRACK_H - HANDLE) / 2;
        SDL_Rect handleRect = {handleX, handleY, HANDLE, HANDLE};

        // Subtle drop-shadow
        SDL_Rect shadow = {handleX + 2, handleY + 2, HANDLE, HANDLE};
        fillRounded(ren, shadow, {0, 0, 0, 80}, 11.f);

        // Handle fill — brighter when dragging/hovered
        bool hoverH = mSettingsHoverSlider || mSliderDragging;
        SDL_Color handleFill = hoverH ? SDL_Color{255, 60, 60, 255}
                                      : SDL_Color{220, 40, 40, 255};
        fillRounded(ren, handleRect, handleFill, 11.f);
        outlineRounded(ren, handleRect, {255, 200, 200, 255}, 2.f, 11.f);

        // Current intensity label below the handle
        const char* iLabel = GlobalSettings::Get().IntensityLabel();
        auto iLabelSz = Text::Measure(iLabel, 13);
        int lblX = handleX + HANDLE / 2 - iLabelSz.x / 2;
        lblX = std::clamp(lblX, mBloodSliderTrack.x,
                          mBloodSliderTrack.x + mBloodSliderTrack.w - iLabelSz.x);
        SDL_Color lblCol = (std::string(iLabel) == "INSANE!")
                           ? SDL_Color{255, 80, 80, 255}
                           : SDL_Color{220, 180, 180, 255};
        Text iLbl(iLabel, lblCol, lblX, handleY + HANDLE + 5, 13);
        iLbl.Render(ren);

        // Track end-cap labels: "Low" left, "INSANE!" right
        Text capL("Low", {110, 90, 130, 255}, mBloodSliderTrack.x, trackY + TRACK_H + 24, 11);
        capL.Render(ren);
        auto insaneSz = Text::Measure("Insane!", 11);
        Text capR("Insane!", {160, 60, 60, 255},
                  mBloodSliderTrack.x + mBloodSliderTrack.w - insaneSz.x,
                  trackY + TRACK_H + 24, 11);
        capR.Render(ren);

        // Normal tick mark at 50% position
        int tickX = mBloodSliderTrack.x + mBloodSliderTrack.w / 2;
        SDL_SetRenderDrawColor(ren, 100, 80, 130, 255);
        SDL_FRect tickLine = {(float)tickX, (float)(trackY - 4), 1.f, (float)(TRACK_H + 8)};
        SDL_RenderFillRect(ren, &tickLine);
        auto normSz = Text::Measure("Normal", 10);
        Text normTick("Normal", {100, 80, 130, 255},
                      tickX - normSz.x / 2, trackY + TRACK_H + 24, 10);
        normTick.Render(ren);

        // Foot hint
        auto hintStr = std::string("Esc to close");
        auto hintSz  = Text::Measure(hintStr, 11);
        Text hint(hintStr, {70, 60, 100, 255},
                  p.x + p.w / 2 - hintSz.x / 2, p.y + p.h - 18, 11);
        hint.Render(ren);
    }

    // --- Gamepad navigation ---
    int   mPadFocus       = -1;
    int   mBrowserPadRow  = 0;
    bool  mDelPadOnYes    = false;

    void padNavigate(int dx, int dy) {
        if (mPadFocus < 0) mPadFocus = 0;
        int row = (mPadFocus >= 6) ? 3 : mPadFocus / 2;
        int col = (mPadFocus >= 6) ? 0 : mPadFocus % 2;
        row += dy;
        col += dx;
        if (row < 0) row = 3;
        if (row > 3) row = 0;
        if (row == 3) { mPadFocus = 6; return; }
        col = std::clamp(col, 0, 1);
        mPadFocus = row * 2 + col;
    }

    void padActivateFocus() {
        switch (mPadFocus) {
            case 0: mEditorPath = ""; mEditorForce = false; mEditorName = ""; openEditor = true; break;
            case 1: mLevelBrowserOpen = true; mLevelBrowserScroll = 0; mBrowserPadRow = 0; break;
            case 2: openPlayerCreator = true; break;
            case 3: openTileAnimCreator = true; break;
            case 4: openEnemyCreator = true; break;
            case 5: openTileAssets = true; break;
            case 6: openCharPicker(); break;
            default: break;
        }
    }

    SDL_Rect padFocusRect() const {
        switch (mPadFocus) {
            case 0: return editorBtnRect;
            case 1: return viewLevelsBtnRect;
            case 2: return createPlayerBtnRect;
            case 3: return tileAnimBtnRect;
            case 4: return createEnemyBtnRect;
            case 5: return tileAssetsBtnRect;
            case 6: return mChooseCharBtnRect;
            default: return {};
        }
    }

    bool handleGamepadNav(SDL_Event& e) {
        if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            auto btn = e.gbutton.button;
            if (btn == SDL_GAMEPAD_BUTTON_DPAD_UP)    { padNavigate(0, -1); return true; }
            if (btn == SDL_GAMEPAD_BUTTON_DPAD_DOWN)  { padNavigate(0,  1); return true; }
            if (btn == SDL_GAMEPAD_BUTTON_DPAD_LEFT)  { padNavigate(-1, 0); return true; }
            if (btn == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) { padNavigate( 1, 0); return true; }
            if (btn == SDL_GAMEPAD_BUTTON_SOUTH) {
                if (mPadFocus >= 0) padActivateFocus();
                return true;
            }
        }
        if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
            constexpr Sint16 THRESH = 20000;
            if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) {
                if (e.gaxis.value < -THRESH)      padNavigate(0, -1);
                else if (e.gaxis.value > THRESH)   padNavigate(0,  1);
                return true;
            }
            if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) {
                if (e.gaxis.value < -THRESH)      padNavigate(-1, 0);
                else if (e.gaxis.value > THRESH)   padNavigate( 1, 0);
                return true;
            }
        }
        if (e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
            mPadFocus = -1;
        return false;
    }

    // --- Settings panel state ---
    SDL_Rect mSettingsBtnRect   = {};
    SDL_Rect mSettingsCloseRect = {};
    SDL_Rect mBloodToggleRect   = {};
    SDL_Rect mBloodSliderTrack  = {};
    bool     mSettingsOpen      = false;
    bool     mSliderDragging    = false;
    bool     mSettingsHoverClose  = false;
    bool     mSettingsHoverToggle = false;
    bool     mSettingsHoverSlider = false;

    // --- State ---
    SDL_Window*   mSDLWindow           = nullptr;
    bool          startGame            = false;
    bool          openEditor           = false;
    bool          openPlayerCreator    = false;
    bool          openTileAnimCreator  = false;
    bool          openEnemyCreator     = false;
    bool          openTileAssets       = false;
    int           mRow2BottomY         = 0;
    int           mProfileSelectorBaseY = 0;
    std::string   mChosenLevel;
    std::string   mChosenProfile;
    std::string   mP2ChosenProfile;              // P2's selected profile (empty = default frost knight)
    bool          mPendingP2Pick      = false;   // true while waiting for P2 to pick a character
    std::string   mEditorPath;
    std::string   mEditorName;
    bool          mEditorForce         = false;
    int           mWindowW             = 0;
    int           mWindowH             = 0;

    bool                   mNamingActive    = false;
    std::string            mNewLevelName;
    std::string            mNameError;
    std::unique_ptr<Text>  promptTitle;
    std::unique_ptr<Text>  promptInput;
    std::unique_ptr<Text>  promptError;
    std::unique_ptr<Text>  promptHint;

    bool     mLevelBrowserOpen   = false;
    int      mLevelBrowserScroll = 0;
    int      mBrowserListY       = 0;
    SDL_Rect mBrowserCloseRect   = {};
    SDL_Rect mBrowserNewRect     = {};

    // Delete confirmation
    bool        mDelConfirmOpen  = false;
    std::string mDelConfirmPath;
    SDL_Rect    mDelConfirmYes   = {};
    SDL_Rect    mDelConfirmNo    = {};

    std::unique_ptr<Image>     background;
    std::unique_ptr<Text>      titleText;
    std::unique_ptr<Text>      editorBtnText;
    std::unique_ptr<Text>      createPlayerBtnText;
    std::unique_ptr<Rectangle> createPlayerButton;
    SDL_Rect                   createPlayerBtnRect{};
    std::unique_ptr<Text>      tileAnimBtnText;
    std::unique_ptr<Rectangle> tileAnimButton;
    SDL_Rect                   tileAnimBtnRect{};
    std::unique_ptr<Text>      createEnemyBtnText;
    std::unique_ptr<Rectangle> createEnemyButton;
    SDL_Rect                   createEnemyBtnRect{};
    std::unique_ptr<Text>      tileAssetsBtnText;
    std::unique_ptr<Rectangle> tileAssetsButton;
    SDL_Rect                   tileAssetsBtnRect{};
    std::unique_ptr<Rectangle> viewLevelsButton;
    std::unique_ptr<Text>      viewLevelsBtnText;
    SDL_Rect                   viewLevelsBtnRect{};
    std::unique_ptr<Rectangle> editorButton;
    SDL_Rect                   editorBtnRect{};
    std::vector<LevelButton>   mLevelButtons;

    std::vector<std::string>   mProfiles;
    int                        mProfileIdx = 0;
    SDL_Rect                   mProfileSelectorBg{};
    SDL_Rect                   mProfileSpriteArea{};
    std::unique_ptr<Text>      mProfileLabel;
    std::unique_ptr<Text>      mProfileNameText;
    std::unique_ptr<Rectangle> mChooseCharButton;
    std::unique_ptr<Text>      mChooseCharBtnText;
    SDL_Texture*               mChosenPreviewTex = nullptr;
};
