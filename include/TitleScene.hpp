#pragma once
#include "Image.hpp"
#include "PlayerProfile.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "SurfaceUtils.hpp"
#include "Text.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

class GameScene;
class LevelEditorScene;
class PlayerCreatorScene;
class TileAnimCreatorScene;
namespace fs = std::filesystem;

class TitleScene : public Scene {
  public:
    void Load(Window& window) override {
        mSDLWindow = window.GetRaw();
        // Use SDL directly to get the logical window size in points.
        SDL_GetWindowSize(mSDLWindow, &mWindowW, &mWindowH);

        background = std::make_unique<Image>("game_assets/backgrounds/bg_castle.png",
                                             FitMode::PRESCALED);

        SDL_Rect windowRect = {0, 0, mWindowW, mWindowH};

        const int btnW  = 180;
        const int btnH  = 50;
        const int gap   = 16;
        const int rowGap = 14;
        const int cx    = mWindowW / 2;

        auto [titleX, titleY] = Text::CenterInRect("SDL Sandbox", 72, windowRect);
        const int row1Y = mWindowH / 2 - 60;
        titleText = std::make_unique<Text>("SDL Sandbox", SDL_Color{255, 255, 255, 255},
                                           titleX, row1Y - 80 - 72, 72);
        const int row2Y = row1Y + btnH + rowGap;

        playBtnRect = {cx - btnW - gap / 2, row1Y, btnW, btnH};
        playButton  = std::make_unique<Rectangle>(playBtnRect);
        playButton->SetColor({255, 255, 255, 255});
        playButton->SetHoverColor({180, 180, 180, 255});
        auto [pbx, pby] = Text::CenterInRect("Play", 28, playBtnRect);
        playBtnText = std::make_unique<Text>("Play", SDL_Color{0, 0, 0, 255}, pbx, pby, 28);

        editorBtnRect = {cx + gap / 2, row1Y, btnW, btnH};
        editorButton  = std::make_unique<Rectangle>(editorBtnRect);
        editorButton->SetColor({80, 120, 200, 255});
        editorButton->SetHoverColor({100, 150, 230, 255});
        auto [eBtnX, eBtnY] = Text::CenterInRect("Level Editor", 22, editorBtnRect);
        editorBtnText = std::make_unique<Text>("Level Editor", SDL_Color{255, 255, 255, 255},
                                               eBtnX, eBtnY, 22);

        createPlayerBtnRect = {cx - btnW - gap / 2, row2Y, btnW, btnH};
        createPlayerButton  = std::make_unique<Rectangle>(createPlayerBtnRect);
        createPlayerButton->SetColor({160, 80, 180, 255});
        createPlayerButton->SetHoverColor({200, 110, 220, 255});
        auto [cpbx, cpby] = Text::CenterInRect("Create Player", 20, createPlayerBtnRect);
        createPlayerBtnText = std::make_unique<Text>("Create Player",
                                                     SDL_Color{255, 255, 255, 255},
                                                     cpbx, cpby, 20);

        tileAnimBtnRect = {cx + gap / 2, row2Y, btnW, btnH};
        tileAnimButton  = std::make_unique<Rectangle>(tileAnimBtnRect);
        tileAnimButton->SetColor({40, 140, 160, 255});
        tileAnimButton->SetHoverColor({60, 180, 200, 255});
        auto [tabx, taby] = Text::CenterInRect("Tile Animator", 20, tileAnimBtnRect);
        tileAnimBtnText = std::make_unique<Text>("Tile Animator",
                                                  SDL_Color{255, 255, 255, 255},
                                                  tabx, taby, 20);

        hintText = std::make_unique<Text>("Press ENTER to play hardcoded level",
                                          SDL_Color{140, 140, 140, 255},
                                          cx - 170, row2Y + btnH + 8, 13);

        mRow2BottomY = row2Y + btnH;

        mProfileSelectorBaseY = mRow2BottomY;
        scanProfiles();
        rebuildProfileSelector();
        mRow2BottomY += 52;

        int viewBtnY = mRow2BottomY + 14;
        viewLevelsBtnRect = {cx - btnW - gap / 2, viewBtnY, btnW * 2 + gap, btnH};
        viewLevelsButton  = std::make_unique<Rectangle>(viewLevelsBtnRect);
        viewLevelsButton->SetColor({60, 60, 160, 255});
        viewLevelsButton->SetHoverColor({90, 90, 200, 255});
        auto [vlx, vly] = Text::CenterInRect("View Levels", 22, viewLevelsBtnRect);
        viewLevelsBtnText = std::make_unique<Text>("View Levels",
            SDL_Color{200, 200, 255, 255}, vlx, vly, 22);

        scanLevels();
    }

    void Unload() override {
        if (mNamingActive)
            SDL_StopTextInput(mSDLWindow);
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

        if (mLevelBrowserOpen) {
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                mLevelBrowserOpen = false; return true;
            }
            if (e.type == SDL_EVENT_MOUSE_WHEEL) {
                mLevelBrowserScroll -= (int)e.wheel.y; clampBrowserScroll(); return true;
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = (int)e.button.x, my = (int)e.button.y;
                if (hit(mBrowserCloseRect, mx, my)) { mLevelBrowserOpen = false; return true; }
                if (hit(mBrowserNewRect, mx, my))   { openNamePrompt(); return true; }
                int rowH = 52, g = 8;
                int listY = mBrowserListY - mLevelBrowserScroll * (rowH + g);
                for (auto& lb : mLevelButtons) {
                    SDL_Rect pr = lb.rect; pr.y = listY;
                    SDL_Rect er = lb.editRect; er.y = listY;
                    if (hit(pr, mx, my)) { mChosenLevel = lb.path; startGame = true; mLevelBrowserOpen = false; return true; }
                    if (hit(er, mx, my)) { mEditorPath = lb.path; mEditorForce = false; mEditorName = ""; openEditor = true; mLevelBrowserOpen = false; return true; }
                    listY += rowH + g;
                }
            }
            return true;
        }

        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_RETURN) {
            mChosenLevel = ""; startGame = true;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x, my = (int)e.button.y;
            if (hit(playBtnRect, mx, my))         { mChosenLevel = ""; startGame = true; }
            if (hit(editorBtnRect, mx, my))        { mEditorPath = ""; mEditorForce = false; mEditorName = ""; openEditor = true; }
            if (hit(createPlayerBtnRect, mx, my))  { openPlayerCreator  = true; return true; }
            if (hit(tileAnimBtnRect, mx, my))       { openTileAnimCreator = true; return true; }
            if (hit(viewLevelsBtnRect, mx, my))     { mLevelBrowserOpen = true; mLevelBrowserScroll = 0; return true; }
            if (!mProfiles.empty()) {
                if (hit(mProfilePrevRect, mx, my)) {
                    mProfileIdx = (mProfileIdx - 1 + (int)mProfiles.size()) % (int)mProfiles.size();
                    mChosenProfile = mProfiles[mProfileIdx]; rebuildProfileSelector(); return true;
                }
                if (hit(mProfileNextRect, mx, my)) {
                    mProfileIdx = (mProfileIdx + 1) % (int)mProfiles.size();
                    mChosenProfile = mProfiles[mProfileIdx]; rebuildProfileSelector(); return true;
                }
            }
        }

        playButton->HandleEvent(e);
        editorButton->HandleEvent(e);
        if (createPlayerButton) createPlayerButton->HandleEvent(e);
        if (tileAnimButton)     tileAnimButton->HandleEvent(e);
        if (viewLevelsButton)   viewLevelsButton->HandleEvent(e);
        return true;
    }

    void Update(float dt) override {}

    void Render(Window& window) override {
        window.Render();
        SDL_Renderer* ren = window.GetRenderer();
        SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);

        background->Render(ren);
        titleText->Render(ren);

        playButton->Render(ren);    playBtnText->Render(ren);
        editorButton->Render(ren);  editorBtnText->Render(ren);
        if (hintText) hintText->Render(ren);
        if (createPlayerButton) { createPlayerButton->Render(ren); createPlayerBtnText->Render(ren); }
        if (tileAnimButton)     { tileAnimButton->Render(ren);     tileAnimBtnText->Render(ren); }

        // Character selector
        if (mProfileSelectorBg.w > 0) {
            fillRect(ren, mProfileSelectorBg, {28, 32, 52, 255});
            outlineRect(ren, mProfileSelectorBg, {80, 100, 180, 255});
        }
        if (mProfileLabel)    mProfileLabel->Render(ren);
        if (mProfileNameText) mProfileNameText->Render(ren);
        if (!mProfiles.empty()) {
            fillRect(ren, mProfilePrevRect, {45, 55, 100, 255});
            outlineRect(ren, mProfilePrevRect, {80, 100, 200, 255});
            Text arr1("<", {220,220,255,255}, mProfilePrevRect.x+8, mProfilePrevRect.y+4, 16);
            arr1.Render(ren);
            fillRect(ren, mProfileNextRect, {45, 55, 100, 255});
            outlineRect(ren, mProfileNextRect, {80, 100, 200, 255});
            Text arr2(">", {220,220,255,255}, mProfileNextRect.x+8, mProfileNextRect.y+4, 16);
            arr2.Render(ren);
        }

        if (viewLevelsButton) { viewLevelsButton->Render(ren); viewLevelsBtnText->Render(ren); }

        // ── Name prompt modal overlay ─────────────────────────────────────────
        if (mNamingActive) {
            int W = window.GetWidth(), H = window.GetHeight();
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
            SDL_FRect full = {0,0,(float)W,(float)H};
            SDL_RenderFillRect(ren, &full);

            int mw = 480, mh = 200;
            int mx = (W - mw) / 2, my = (H - mh) / 2;
            SDL_Rect box = {mx, my, mw, mh};
            fillRect(ren, box, {28, 28, 42, 255});
            outlineRect(ren, box, {80, 120, 220, 255}, 2);

            if (promptTitle) promptTitle->Render(ren);
            SDL_Rect field = {mx + 20, my + 70, mw - 40, 40};
            fillRect(ren, field, {18, 18, 32, 255});
            outlineRect(ren, field,
                mNameError.empty() ? SDL_Color{80, 120, 220, 255} : SDL_Color{220, 80, 80, 255}, 2);
            if (promptInput) promptInput->Render(ren);
            if (promptError) promptError->Render(ren);
            if (promptHint)  promptHint->Render(ren);
        }

        // ── Level browser modal overlay ───────────────────────────────────────
        if (mLevelBrowserOpen) {
            int W = window.GetWidth(), H = window.GetHeight();
            SDL_SetRenderDrawColor(ren, 0, 0, 0, 180);
            SDL_FRect full = {0,0,(float)W,(float)H};
            SDL_RenderFillRect(ren, &full);

            int pw = 520, ph = std::min(H - 80, 560);
            int px = (W - pw) / 2, py = (H - ph) / 2;
            SDL_Rect panel = {px, py, pw, ph};
            fillRect(ren, panel, {18, 20, 32, 245});
            outlineRect(ren, panel, {80, 120, 220, 255}, 2);

            fillRect(ren, {px, py, pw, 44}, {28, 32, 52, 255});
            Text hdr("Levels", {255, 215, 0, 255}, px + 16, py + 10, 22);
            hdr.Render(ren);

            mBrowserCloseRect = {px + pw - 38, py + 6, 32, 32};
            fillRect(ren, mBrowserCloseRect, {140, 40, 40, 255});
            outlineRect(ren, mBrowserCloseRect, {220, 80, 80, 255});
            Text closeX("X", {255, 255, 255, 255}, mBrowserCloseRect.x + 9, mBrowserCloseRect.y + 6, 14);
            closeX.Render(ren);

            int newBtnY = py + 52;
            mBrowserNewRect = {px + pw/2 - 130, newBtnY, 260, 38};
            fillRect(ren, mBrowserNewRect, {60, 60, 160, 255});
            outlineRect(ren, mBrowserNewRect, {100, 100, 200, 255});
            auto [nlx, nly] = Text::CenterInRect("+ New Level", 18, mBrowserNewRect);
            Text newLbl("+ New Level", {200, 200, 255, 255}, nlx, nly, 18);
            newLbl.Render(ren);

            int listTop = py + 100, listBottom = py + ph - 10;
            mBrowserListY = listTop;
            int rowH = 52, rowGap = 8, playW = 290, editW = 90, btnGap = 8, rowX = px + 16;
            int listY = listTop - mLevelBrowserScroll * (rowH + rowGap);
            for (int i = 0; i < (int)mLevelButtons.size(); i++) {
                int ry = listY + i * (rowH + rowGap);
                if (ry + rowH < listTop || ry > listBottom) continue;

                SDL_Rect pr = {rowX, ry, playW, rowH};
                fillRect(ren, pr, {30, 110, 55, 255});
                outlineRect(ren, pr, {50, 170, 80, 255});
                std::string nm = fs::path(mLevelButtons[i].path).stem().string();
                auto [lx, ly] = Text::CenterInRect(nm, 20, pr);
                Text lbl(nm, {255,255,255,255}, lx, ly, 20);
                lbl.Render(ren);

                SDL_Rect er = {rowX + playW + btnGap, ry, editW, rowH};
                fillRect(ren, er, {50, 80, 160, 255});
                outlineRect(ren, er, {80, 120, 220, 255});
                auto [ex, ey] = Text::CenterInRect("Edit", 18, er);
                Text elbl("Edit", {200,220,255,255}, ex, ey, 18);
                elbl.Render(ren);

                mLevelButtons[i].rect     = pr;
                mLevelButtons[i].editRect = er;
            }
            if ((int)mLevelButtons.size() * (rowH + rowGap) > listBottom - listTop) {
                Text sh("scroll to see more", {80,90,120,255}, px + pw/2 - 60, py + ph - 18, 11);
                sh.Render(ren);
            }
        }

        window.Update();
    }

    std::unique_ptr<Scene> NextScene() override;

  private:
    static bool hit(const SDL_Rect& r, int x, int y) {
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    }

    // ── Renderer-based draw helpers ───────────────────────────────────────────
    static void fillRect(SDL_Renderer* ren, SDL_Rect r, SDL_Color c) {
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
        SDL_FRect fr = {(float)r.x, (float)r.y, (float)r.w, (float)r.h};
        SDL_RenderFillRect(ren, &fr);
    }
    static void outlineRect(SDL_Renderer* ren, SDL_Rect r, SDL_Color c, int t = 1) {
        SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
        // Draw t-pixel border as filled rects on each side
        SDL_FRect sides[4] = {
            {(float)r.x,           (float)r.y,           (float)r.w, (float)t},
            {(float)r.x,           (float)(r.y+r.h-t),   (float)r.w, (float)t},
            {(float)r.x,           (float)r.y,           (float)t,   (float)r.h},
            {(float)(r.x+r.w-t),   (float)r.y,           (float)t,   (float)r.h}
        };
        for (auto& s : sides) SDL_RenderFillRect(ren, &s);
    }

    // ── Name prompt helpers ───────────────────────────────────────────────────
    void openNamePrompt() {
        mNamingActive = true; mNewLevelName.clear(); mNameError.clear();
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

    // ── Level button list ─────────────────────────────────────────────────────
    struct LevelButton {
        std::string path;
        SDL_Rect    rect     = {};
        SDL_Rect    editRect = {};
    };
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
        int rowH = 52, rowGap = 8, ph = std::min(mWindowH - 80, 560);
        int listH = ph - 110;
        int maxScroll = std::max(0, ((int)mLevelButtons.size() * (rowH + rowGap) - listH) / (rowH + rowGap));
        if (mLevelBrowserScroll < 0)         mLevelBrowserScroll = 0;
        if (mLevelBrowserScroll > maxScroll) mLevelBrowserScroll = maxScroll;
    }

    // ── Profile selector helpers ──────────────────────────────────────────────
    void scanProfiles() {
        mProfiles.clear();
        mProfiles.push_back("");
        for (const auto& p : ScanPlayerProfiles()) mProfiles.push_back(p.string());
        mProfileIdx = 0; mChosenProfile = "";
    }
    void rebuildProfileSelector() {
        int cx = mWindowW / 2, selY = mProfileSelectorBaseY + 10;
        int selW = 340, selH = 32, arrW = 28;
        mProfileSelectorBg = {cx - selW/2, selY, selW, selH};
        mProfilePrevRect   = {cx - selW/2,        selY+2, arrW, selH-4};
        mProfileNextRect   = {cx + selW/2 - arrW, selY+2, arrW, selH-4};
        mProfileLabel = std::make_unique<Text>("Character:", SDL_Color{160, 170, 220, 255},
                                               cx - selW/2 + arrW + 6, selY + 7, 13);
        std::string name = "Frost Knight (default)";
        if (mProfileIdx > 0 && mProfileIdx < (int)mProfiles.size())
            name = fs::path(mProfiles[mProfileIdx]).stem().string();
        mProfileNameText = std::make_unique<Text>(name, SDL_Color{255, 255, 255, 255},
                                                  cx - selW/2 + arrW + 80, selY + 7, 14);
    }

    // ── State ─────────────────────────────────────────────────────────────────
    SDL_Window* mSDLWindow           = nullptr;
    bool        startGame            = false;
    bool        openEditor           = false;
    bool        openPlayerCreator    = false;
    bool        openTileAnimCreator  = false;
    int         mRow2BottomY         = 0;
    int         mProfileSelectorBaseY = 0;
    std::string mChosenLevel;
    std::string mChosenProfile;
    std::string mEditorPath;
    std::string mEditorName;
    bool        mEditorForce         = false;
    int         mWindowW             = 0;
    int         mWindowH             = 0;

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

    std::unique_ptr<Image>     background;
    std::unique_ptr<Text>      titleText;
    std::unique_ptr<Text>      playBtnText;
    std::unique_ptr<Text>      editorBtnText;
    std::unique_ptr<Text>      hintText;
    std::unique_ptr<Text>      createPlayerBtnText;
    std::unique_ptr<Rectangle> createPlayerButton;
    SDL_Rect                   createPlayerBtnRect{};
    std::unique_ptr<Text>      tileAnimBtnText;
    std::unique_ptr<Rectangle> tileAnimButton;
    SDL_Rect                   tileAnimBtnRect{};
    std::unique_ptr<Rectangle> viewLevelsButton;
    std::unique_ptr<Text>      viewLevelsBtnText;
    SDL_Rect                   viewLevelsBtnRect{};
    std::unique_ptr<Rectangle> playButton;
    std::unique_ptr<Rectangle> editorButton;
    SDL_Rect                   playBtnRect{};
    SDL_Rect                   editorBtnRect{};
    std::vector<LevelButton>   mLevelButtons;

    std::vector<std::string>   mProfiles;
    int                        mProfileIdx = 0;
    SDL_Rect                   mProfileSelectorBg{};
    SDL_Rect                   mProfilePrevRect{};
    SDL_Rect                   mProfileNextRect{};
    std::unique_ptr<Text>      mProfileLabel;
    std::unique_ptr<Text>      mProfileNameText;
};
