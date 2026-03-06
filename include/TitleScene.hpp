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
        mSDLWindow = window.GetRaw();   // store for SDL_StartTextInput
        mWindowW   = window.GetWidth();
        mWindowH   = window.GetHeight();

        background = std::make_unique<Image>("game_assets/backgrounds/bg_castle.png",
                                             nullptr, FitMode::PRESCALED);

        SDL_Rect windowRect = {0, 0, mWindowW, mWindowH};

        // ── Layout constants ──────────────────────────────────────────────────
        const int btnW  = 180;  // button width
        const int btnH  = 50;   // button height
        const int gap   = 16;   // horizontal gap between sibling buttons
        const int rowGap = 14;  // vertical gap between button rows
        const int cx    = mWindowW / 2;

        // Title sits well above the button cluster
        auto [titleX, titleY] = Text::CenterInRect("SDL Sandbox", 72, windowRect);
        // Anchor buttons so they're centred in the lower portion of the window;
        // leave ~100px of clear space below the title baseline.
        const int row1Y = mWindowH / 2 - 60;  // shifted down so title has room
        // Place title so its bottom edge is ~80px above row1Y
        titleText = std::make_unique<Text>("SDL Sandbox", SDL_Color{255, 255, 255, 255},
                                           titleX, row1Y - 80 - 72, 72);  // 72 ≈ font height
        const int row2Y = row1Y + btnH + rowGap;
        // Row 1: Play  |  Level Editor  (centred as a pair)
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

        // Row 2: Create Player  |  Tile Animator  (centred as a pair)
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

        // Hint text sits below row 2, well clear of everything
        hintText = std::make_unique<Text>("Press ENTER to play hardcoded level",
                                          SDL_Color{140, 140, 140, 255},
                                          cx - 170, row2Y + btnH + 8, 13);

        mRow2BottomY = row2Y + btnH;

        // Character selector
        mProfileSelectorBaseY = mRow2BottomY;
        scanProfiles();
        rebuildProfileSelector();
        mRow2BottomY += 52;

        // "View Levels" button — replaces the inline level list
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

        // ── Name prompt modal (shown after clicking "+ New Level") ──────────
        if (mNamingActive) {
            if (e.type == SDL_EVENT_TEXT_INPUT) {
                // Only accept alphanumeric, dash, underscore — safe as a filename
                for (char c : std::string(e.text.text)) {
                    if (std::isalnum((unsigned char)c) || c == '-' || c == '_')
                        mNewLevelName += c;
                }
                rebuildNamePrompt();
                return true;
            }
            if (e.type == SDL_EVENT_KEY_DOWN) {
                switch (e.key.key) {
                    case SDLK_BACKSPACE:
                        if (!mNewLevelName.empty()) {
                            mNewLevelName.pop_back();
                            rebuildNamePrompt();
                        }
                        return true;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        if (!mNewLevelName.empty()) {
                            // Check the name isn't already taken
                            std::string candidate = "levels/" + mNewLevelName + ".json";
                            if (fs::exists(candidate)) {
                                mNameError = "\"" + mNewLevelName + "\" already exists — choose another name";
                                rebuildNamePrompt();
                            } else {
                                // Commit: open editor with that name, force blank
                                mEditorPath  = "";
                                mEditorForce = true;
                                mEditorName  = mNewLevelName;
                                closeNamePrompt();
                                openEditor = true;
                            }
                        }
                        return true;
                    case SDLK_ESCAPE:
                        closeNamePrompt();
                        return true;
                    default: break;
                }
            }
            return true; // swallow all other events while modal is open
        }

        // ── Level browser modal ───────────────────────────────────────────────
        if (mLevelBrowserOpen) {
            if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
                mLevelBrowserOpen = false;
                return true;
            }
            if (e.type == SDL_EVENT_MOUSE_WHEEL) {
                mLevelBrowserScroll -= (int)e.wheel.y;
                clampBrowserScroll();
                return true;
            }
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = (int)e.button.x, my = (int)e.button.y;
                // Close button
                if (hit(mBrowserCloseRect, mx, my)) { mLevelBrowserOpen = false; return true; }
                // New level button inside modal
                if (hit(mBrowserNewRect, mx, my)) { openNamePrompt(); return true; }
                // Level rows (offset by scroll)
                int rowH = 52, gap = 8;
                int listY = mBrowserListY - mLevelBrowserScroll * (rowH + gap);
                for (auto& lb : mLevelButtons) {
                    SDL_Rect pr = lb.rect; pr.y = listY;
                    SDL_Rect er = lb.editRect; er.y = listY;
                    if (hit(pr, mx, my)) { mChosenLevel = lb.path; startGame = true; mLevelBrowserOpen = false; return true; }
                    if (hit(er, mx, my)) { mEditorPath = lb.path; mEditorForce = false; mEditorName = ""; openEditor = true; mLevelBrowserOpen = false; return true; }
                    listY += rowH + gap;
                }
            }
            return true; // swallow all input while modal open
        }

        // ── Normal title screen events ───────────────────────────────────────
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_RETURN) {
            mChosenLevel = "";
            startGame    = true;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x;
            int my = (int)e.button.y;

            if (hit(playBtnRect, mx, my)) { mChosenLevel = ""; startGame = true; }
            if (hit(editorBtnRect, mx, my)) { mEditorPath = ""; mEditorForce = false; mEditorName = ""; openEditor = true; }
            if (hit(createPlayerBtnRect, mx, my)) { openPlayerCreator = true; return true; }
            if (hit(tileAnimBtnRect, mx, my))     { openTileAnimCreator = true; return true; }
            if (hit(viewLevelsBtnRect, mx, my))   { mLevelBrowserOpen = true; mLevelBrowserScroll = 0; return true; }
            // Character selector arrows
            if (!mProfiles.empty()) {
                if (hit(mProfilePrevRect, mx, my)) {
                    mProfileIdx = (mProfileIdx - 1 + (int)mProfiles.size()) % (int)mProfiles.size();
                    mChosenProfile = mProfiles[mProfileIdx];
                    rebuildProfileSelector();
                    return true;
                }
                if (hit(mProfileNextRect, mx, my)) {
                    mProfileIdx = (mProfileIdx + 1) % (int)mProfiles.size();
                    mChosenProfile = mProfiles[mProfileIdx];
                    rebuildProfileSelector();
                    return true;
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
        SDL_Surface* s = window.GetSurface();
        background->Render(s);
        titleText->Render(s);

        playButton->Render(s);    playBtnText->Render(s);
        editorButton->Render(s);  editorBtnText->Render(s);
        if (hintText)  hintText->Render(s);
        if (createPlayerButton) { createPlayerButton->Render(s); createPlayerBtnText->Render(s); }
        if (tileAnimButton)     { tileAnimButton->Render(s);     tileAnimBtnText->Render(s); }

        // Character selector
        if (mProfileSelectorBg.w > 0) {
            fillRect(s, mProfileSelectorBg, {28, 32, 52, 255});
            outlineRect(s, mProfileSelectorBg, {80, 100, 180, 255});
        }
        if (mProfileLabel)    mProfileLabel->Render(s);
        if (mProfileNameText) mProfileNameText->Render(s);
        if (!mProfiles.empty()) {
            // Prev arrow
            fillRect(s, mProfilePrevRect, {45, 55, 100, 255});
            outlineRect(s, mProfilePrevRect, {80, 100, 200, 255});
            Text arr1("<", {220,220,255,255}, mProfilePrevRect.x+8, mProfilePrevRect.y+4, 16);
            arr1.Render(s);
            // Next arrow
            fillRect(s, mProfileNextRect, {45, 55, 100, 255});
            outlineRect(s, mProfileNextRect, {80, 100, 200, 255});
            Text arr2(">", {220,220,255,255}, mProfileNextRect.x+8, mProfileNextRect.y+4, 16);
            arr2.Render(s);
        }

        if (viewLevelsButton) { viewLevelsButton->Render(s); viewLevelsBtnText->Render(s); }

        // ── Name prompt modal overlay ────────────────────────────────────────
        if (mNamingActive) {
            int W = window.GetWidth(), H = window.GetHeight();

            // Dim the background
            SDL_Surface* dim = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_ARGB8888);
            if (dim) {
                SDL_SetSurfaceBlendMode(dim, SDL_BLENDMODE_BLEND);
                const SDL_PixelFormatDetails* fmt =
                    SDL_GetPixelFormatDetails(dim->format);
                SDL_FillSurfaceRect(dim, nullptr,
                    SDL_MapRGBA(fmt, nullptr, 0, 0, 0, 180));
                SDL_BlitSurface(dim, nullptr, s, nullptr);
                SDL_DestroySurface(dim);
            }

            // Modal box
            int mw = 480, mh = 200;
            int mx = (W - mw) / 2, my = (H - mh) / 2;
            SDL_Rect box = {mx, my, mw, mh};
            fillRect(s, box, {28, 28, 42, 255});
            outlineRect(s, box, {80, 120, 220, 255}, 2);

            // Title
            if (promptTitle)  promptTitle->Render(s);
            // Input field
            SDL_Rect field = {mx + 20, my + 70, mw - 40, 40};
            fillRect(s, field, {18, 18, 32, 255});
            outlineRect(s, field,
                mNameError.empty() ? SDL_Color{80, 120, 220, 255}
                                   : SDL_Color{220, 80, 80, 255}, 2);
            if (promptInput)  promptInput->Render(s);
            if (promptError)  promptError->Render(s);
            if (promptHint)   promptHint->Render(s);
        }

        // ── Level browser modal overlay ───────────────────────────────────────
        if (mLevelBrowserOpen) {
            int W = window.GetWidth(), H = window.GetHeight();
            // Dim
            SDL_Surface* dim = SDL_CreateSurface(W, H, SDL_PIXELFORMAT_ARGB8888);
            if (dim) {
                SDL_SetSurfaceBlendMode(dim, SDL_BLENDMODE_BLEND);
                const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(dim->format);
                SDL_FillSurfaceRect(dim, nullptr, SDL_MapRGBA(fmt, nullptr, 0, 0, 0, 180));
                SDL_BlitSurface(dim, nullptr, s, nullptr);
                SDL_DestroySurface(dim);
            }
            // Panel
            int pw = 520, ph = std::min(H - 80, 560);
            int px = (W - pw) / 2, py = (H - ph) / 2;
            SDL_Rect panel = {px, py, pw, ph};
            fillRect(s, panel, {18, 20, 32, 245});
            outlineRect(s, panel, {80, 120, 220, 255}, 2);

            // Title bar
            fillRect(s, {px, py, pw, 44}, {28, 32, 52, 255});
            Text hdr("Levels", {255, 215, 0, 255}, px + 16, py + 10, 22);
            hdr.Render(s);

            // Close button  [X]
            mBrowserCloseRect = {px + pw - 38, py + 6, 32, 32};
            fillRect(s, mBrowserCloseRect, {140, 40, 40, 255});
            outlineRect(s, mBrowserCloseRect, {220, 80, 80, 255});
            Text closeX("X", {255, 255, 255, 255}, mBrowserCloseRect.x + 9, mBrowserCloseRect.y + 6, 14);
            closeX.Render(s);

            // "+ New Level" button inside modal
            int newBtnY = py + 52;
            mBrowserNewRect = {px + pw/2 - 130, newBtnY, 260, 38};
            fillRect(s, mBrowserNewRect, {60, 60, 160, 255});
            outlineRect(s, mBrowserNewRect, {100, 100, 200, 255});
            auto [nlx, nly] = Text::CenterInRect("+ New Level", 18, mBrowserNewRect);
            Text newLbl("+ New Level", {200, 200, 255, 255}, nlx, nly, 18);
            newLbl.Render(s);

            // Scrollable list
            int listTop    = py + 100;
            int listBottom = py + ph - 10;
            mBrowserListY  = listTop;
            int rowH = 52, rowGap = 8;
            int playW = 290, editW = 90, btnGap = 8;
            int rowX = px + 16;

            // Scissor-clip to list area using a temporary surface blit
            SDL_Rect clipArea = {px + 8, listTop, pw - 16, listBottom - listTop};

            int listY = listTop - mLevelBrowserScroll * (rowH + rowGap);
            for (int i = 0; i < (int)mLevelButtons.size(); i++) {
                int ry = listY + i * (rowH + rowGap);
                if (ry + rowH < listTop || ry > listBottom) continue;

                // Play row
                SDL_Rect pr = {rowX, ry, playW, rowH};
                fillRect(s, pr, {30, 110, 55, 255});
                outlineRect(s, pr, {50, 170, 80, 255});
                std::string nm = fs::path(mLevelButtons[i].path).stem().string();
                auto [lx, ly] = Text::CenterInRect(nm, 20, pr);
                Text lbl(nm, {255,255,255,255}, lx, ly, 20);
                lbl.Render(s);

                // Edit button
                SDL_Rect er = {rowX + playW + btnGap, ry, editW, rowH};
                fillRect(s, er, {50, 80, 160, 255});
                outlineRect(s, er, {80, 120, 220, 255});
                auto [ex, ey] = Text::CenterInRect("Edit", 18, er);
                Text elbl("Edit", {200,220,255,255}, ex, ey, 18);
                elbl.Render(s);

                // Cache hit rects for click detection (adjusted for scroll)
                mLevelButtons[i].rect     = pr;
                mLevelButtons[i].editRect = er;
            }

            // Scroll hint
            if ((int)mLevelButtons.size() * (rowH + rowGap) > listBottom - listTop) {
                Text sh("scroll to see more", {80,90,120,255}, px + pw/2 - 60, py + ph - 18, 11);
                sh.Render(s);
            }
        }

        window.Update();
    }

    std::unique_ptr<Scene> NextScene() override;

  private:
    // ── Hit test helper ───────────────────────────────────────────────────────
    static bool hit(const SDL_Rect& r, int x, int y) {
        return x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h;
    }

    // ── Simple draw helpers (avoid pulling in all of LevelEditorScene) ────────
    static void fillRect(SDL_Surface* s, SDL_Rect r, SDL_Color c) {
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
        SDL_FillSurfaceRect(s, &r, SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a));
    }
    static void outlineRect(SDL_Surface* s, SDL_Rect r, SDL_Color c, int t = 1) {
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(s->format);
        Uint32 col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
        SDL_Rect sides[4] = {{r.x, r.y, r.w, t}, {r.x, r.y+r.h-t, r.w, t},
                             {r.x, r.y, t, r.h}, {r.x+r.w-t, r.y, t, r.h}};
        for (auto& sr : sides) SDL_FillSurfaceRect(s, &sr, col);
    }

    // ── Name prompt helpers ───────────────────────────────────────────────────
    void openNamePrompt() {
        mNamingActive = true;
        mNewLevelName.clear();
        mNameError.clear();
        SDL_StartTextInput(mSDLWindow);
        rebuildNamePrompt();
    }

    void closeNamePrompt() {
        mNamingActive = false;
        mNewLevelName.clear();
        mNameError.clear();
        SDL_StopTextInput(mSDLWindow);
        promptTitle.reset();
        promptInput.reset();
        promptError.reset();
        promptHint.reset();
    }

    void rebuildNamePrompt() {
        int W  = mWindowW, H = mWindowH;
        int mw = 480, mh = 200;
        int mx = (W - mw) / 2, my = (H - mh) / 2;

        promptTitle = std::make_unique<Text>(
            "Name your level:",
            SDL_Color{200, 210, 255, 255},
            mx + 20, my + 20, 22);

        // Input text with blinking-cursor feel (just a trailing pipe)
        std::string display = mNewLevelName.empty() ? "|" : mNewLevelName + "|";
        promptInput = std::make_unique<Text>(
            display,
            SDL_Color{255, 255, 255, 255},
            mx + 28, my + 78, 20);

        promptHint = std::make_unique<Text>(
            "Letters, numbers, - and _ only.   Enter=confirm   Esc=cancel",
            SDL_Color{100, 110, 140, 255},
            mx + 20, my + 126, 13);

        if (!mNameError.empty()) {
            promptError = std::make_unique<Text>(
                mNameError,
                SDL_Color{255, 100, 100, 255},
                mx + 20, my + 152, 13);
        } else {
            promptError.reset();
        }
    }

    // ── Level button list ─────────────────────────────────────────────────────
    struct LevelButton {
        std::string path;
        SDL_Rect    rect     = {}; // updated each render frame
        SDL_Rect    editRect = {}; // updated each render frame
    };

    void scanLevels() {
        mLevelButtons.clear();
        if (!fs::exists("levels")) return;
        std::vector<fs::path> found;
        for (const auto& entry : fs::directory_iterator("levels"))
            if (entry.path().extension() == ".json")
                found.push_back(entry.path());
        std::sort(found.begin(), found.end());
        for (const auto& p : found) {
            LevelButton lb;
            lb.path     = p.string();
            lb.rect     = {}; // positions set at render time
            lb.editRect = {};
            mLevelButtons.push_back(std::move(lb));
        }
    }

    void clampBrowserScroll() {
        int rowH = 52, rowGap = 8;
        int ph   = std::min(mWindowH - 80, 560);
        int listH = ph - 110; // listTop offset + bottom padding
        int totalH = (int)mLevelButtons.size() * (rowH + rowGap);
        int maxScroll = std::max(0, (totalH - listH) / (rowH + rowGap));
        if (mLevelBrowserScroll < 0)          mLevelBrowserScroll = 0;
        if (mLevelBrowserScroll > maxScroll)  mLevelBrowserScroll = maxScroll;
    }

    // ── Profile selector helpers ──────────────────────────────────────────────
    void scanProfiles() {
        mProfiles.clear();
        mProfiles.push_back(""); // index 0 = default frost knight
        for (const auto& p : ScanPlayerProfiles())
            mProfiles.push_back(p.string());
        mProfileIdx = 0;
        mChosenProfile = "";
    }

    void rebuildProfileSelector() {
        int cx   = mWindowW / 2;
        int selY = mProfileSelectorBaseY + 10;
        int selW = 340;
        int selH = 32;
        int arrW = 28;

        mProfileSelectorBg = {cx - selW/2, selY, selW, selH};
        mProfilePrevRect   = {cx - selW/2,           selY+2, arrW, selH-4};
        mProfileNextRect   = {cx + selW/2 - arrW,    selY+2, arrW, selH-4};

        mProfileLabel = std::make_unique<Text>(
            "Character:",
            SDL_Color{160, 170, 220, 255},
            cx - selW/2 + arrW + 6, selY + 7, 13);

        std::string name = "Frost Knight (default)";
        if (mProfileIdx > 0 && mProfileIdx < (int)mProfiles.size()) {
            name = fs::path(mProfiles[mProfileIdx]).stem().string();
        }
        mProfileNameText = std::make_unique<Text>(
            name, SDL_Color{255, 255, 255, 255},
            cx - selW/2 + arrW + 80, selY + 7, 14);
    }

    // ── State ─────────────────────────────────────────────────────────────────
    SDL_Window* mSDLWindow    = nullptr;  // needed for SDL_StartTextInput
    bool        startGame         = false;
    bool        openEditor        = false;
    bool        openPlayerCreator   = false;
    bool        openTileAnimCreator  = false;
    int         mRow2BottomY         = 0;   // bottom edge of the second button row (bumped after selector)
    int         mProfileSelectorBaseY = 0;   // locked Y anchor for the character selector bar
    std::string mChosenLevel;
    std::string mChosenProfile; // path to selected PlayerProfile JSON (empty = frost knight)
    std::string mEditorPath;   // file path for Edit buttons (empty = new/resume)
    std::string mEditorName;   // level name chosen in the naming modal
    bool        mEditorForce  = false;
    int         mWindowW      = 0;
    int         mWindowH      = 0;

    // Name prompt modal state
    bool                   mNamingActive = false;
    std::string            mNewLevelName;
    std::string            mNameError;
    std::unique_ptr<Text>  promptTitle;
    std::unique_ptr<Text>  promptInput;
    std::unique_ptr<Text>  promptError;
    std::unique_ptr<Text>  promptHint;

    // Level browser modal state
    bool     mLevelBrowserOpen   = false;
    int      mLevelBrowserScroll = 0;
    int      mBrowserListY       = 0;  // set each Render frame
    SDL_Rect mBrowserCloseRect   = {}; // set each Render frame
    SDL_Rect mBrowserNewRect     = {}; // set each Render frame

    // Widgets
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

    // Character selector widgets & state
    std::vector<std::string>   mProfiles;           // index 0 = empty (frost knight default)
    int                        mProfileIdx = 0;
    SDL_Rect                   mProfileSelectorBg{};
    SDL_Rect                   mProfilePrevRect{};
    SDL_Rect                   mProfileNextRect{};
    std::unique_ptr<Text>      mProfileLabel;
    std::unique_ptr<Text>      mProfileNameText;
};
