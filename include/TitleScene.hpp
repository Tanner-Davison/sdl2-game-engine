#pragma once
#include "Image.hpp"
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

        auto [titleX, titleY] = Text::CenterInRect("SDL Sandbox", 72, windowRect);
        titleText = std::make_unique<Text>("SDL Sandbox", SDL_Color{255, 255, 255, 255},
                                           titleX, titleY - 120, 72);

        int btnW = 180, btnH = 55;
        int gap  = 20;
        int cy   = mWindowH / 2 - 80;
        int cx   = mWindowW / 2;

        playBtnRect = {cx - btnW - gap / 2, cy, btnW, btnH};
        playButton  = std::make_unique<Rectangle>(playBtnRect);
        playButton->SetColor({255, 255, 255, 255});
        playButton->SetHoverColor({180, 180, 180, 255});
        auto [pbx, pby] = Text::CenterInRect("Play", 32, playBtnRect);
        playBtnText = std::make_unique<Text>("Play", SDL_Color{0, 0, 0, 255}, pbx, pby, 32);

        editorBtnRect = {cx + gap / 2, cy, btnW, btnH};
        editorButton  = std::make_unique<Rectangle>(editorBtnRect);
        editorButton->SetColor({80, 120, 200, 255});
        editorButton->SetHoverColor({100, 150, 230, 255});
        auto [eBtnX, eBtnY] = Text::CenterInRect("Level Editor", 24, editorBtnRect);
        editorBtnText = std::make_unique<Text>("Level Editor", SDL_Color{255, 255, 255, 255},
                                               eBtnX, eBtnY, 24);

        hintText = std::make_unique<Text>("Press ENTER to play hardcoded level",
                                          SDL_Color{160, 160, 160, 255},
                                          cx - 190, cy + btnH + 10, 16);

        scanLevels(mWindowW, mWindowH);

        if (mLevelButtons.empty()) {
            noLevelsText = std::make_unique<Text>(
                "No saved levels yet — make one in the Level Editor!",
                SDL_Color{140, 140, 140, 255},
                cx - 230, editorBtnRect.y + editorBtnRect.h + 60, 16);
        }
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

        // ── Normal title screen events ───────────────────────────────────────
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_RETURN) {
            mChosenLevel = "";
            startGame    = true;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x;
            int my = (int)e.button.y;

            // Hardcoded Play button
            if (hit(playBtnRect, mx, my)) {
                mChosenLevel = "";
                startGame    = true;
            }
            // Level Editor button — open last auto-save / blank
            if (hit(editorBtnRect, mx, my)) {
                mEditorPath  = "";
                mEditorForce = false;
                mEditorName  = "";
                openEditor   = true;
            }
            // "+ New Level" — open naming modal
            if (hit(newLevelRect, mx, my)) {
                openNamePrompt();
                return true;
            }
            // Saved level rows
            for (auto& lb : mLevelButtons) {
                if (hit(lb.rect, mx, my)) {
                    mChosenLevel = lb.path;
                    startGame    = true;
                }
                if (hit(lb.editRect, mx, my)) {
                    mEditorPath  = lb.path;
                    mEditorForce = false;
                    mEditorName  = "";
                    openEditor   = true;
                }
            }
        }

        playButton->HandleEvent(e);
        editorButton->HandleEvent(e);
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
        if (hintText) hintText->Render(s);

        if (levelsHeader) levelsHeader->Render(s);
        if (newLevelBtn)  { newLevelBtn->Render(s); newLevelLabel->Render(s); }
        if (mLevelButtons.empty() && noLevelsText) noLevelsText->Render(s);
        for (auto& lb : mLevelButtons) {
            lb.btn->Render(s);   lb.label->Render(s);
            if (lb.editBtn) { lb.editBtn->Render(s); lb.editLabel->Render(s); }
        }

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
        SDL_Rect                   rect;
        std::string                path;
        std::unique_ptr<Rectangle> btn;
        std::unique_ptr<Text>      label;
        SDL_Rect                   editRect{};
        std::unique_ptr<Rectangle> editBtn;
        std::unique_ptr<Text>      editLabel;
    };

    void scanLevels(int winW, int winH) {
        mLevelButtons.clear();
        newLevelBtn.reset(); newLevelLabel.reset(); newLevelRect = {};

        const int playW  = 200;
        const int editW  = 80;
        const int btnH   = 48;
        const int gap    = 8;
        const int rowGap = 12;
        const int totalW = playW + gap + editW;
        int startY       = editorBtnRect.y + editorBtnRect.h + 30;
        int centerX      = winW / 2;

        // "+ New Level" button
        {
            newLevelRect = {centerX - totalW / 2, startY, totalW, btnH};
            auto btn = std::make_unique<Rectangle>(newLevelRect);
            btn->SetColor({60, 60, 160, 255});
            btn->SetHoverColor({90, 90, 200, 255});
            auto [lx, ly] = Text::CenterInRect("+ New Level", 22, newLevelRect);
            newLevelLabel = std::make_unique<Text>(
                "+ New Level", SDL_Color{200, 200, 255, 255}, lx, ly, 22);
            newLevelBtn = std::move(btn);
            startY += btnH + rowGap;
        }

        if (!fs::exists("levels")) return;

        std::vector<fs::path> found;
        for (const auto& entry : fs::directory_iterator("levels"))
            if (entry.path().extension() == ".json")
                found.push_back(entry.path());
        std::sort(found.begin(), found.end());

        if (!found.empty()) {
            levelsHeader = std::make_unique<Text>(
                "-- Saved Levels --",
                SDL_Color{255, 215, 0, 255},
                centerX - 90, startY, 20);
            startY += 34;
        }

        for (const auto& p : found) {
            SDL_Rect playR = {centerX - totalW / 2, startY, playW, btnH};
            auto playBtnL  = std::make_unique<Rectangle>(playR);
            playBtnL->SetColor({40, 160, 80, 255});
            playBtnL->SetHoverColor({60, 200, 100, 255});
            std::string name = p.stem().string();
            auto [lx, ly] = Text::CenterInRect(name, 22, playR);
            auto lbl = std::make_unique<Text>(
                name, SDL_Color{255, 255, 255, 255}, lx, ly, 22);

            SDL_Rect editR = {playR.x + playW + gap, startY, editW, btnH};
            auto editBtnL  = std::make_unique<Rectangle>(editR);
            editBtnL->SetColor({70, 110, 190, 255});
            editBtnL->SetHoverColor({100, 150, 230, 255});
            auto [ex, ey] = Text::CenterInRect("Edit", 20, editR);
            auto editLblT = std::make_unique<Text>(
                "Edit", SDL_Color{200, 220, 255, 255}, ex, ey, 20);

            LevelButton lb;
            lb.rect      = playR;
            lb.path      = p.string();
            lb.btn       = std::move(playBtnL);
            lb.label     = std::move(lbl);
            lb.editRect  = editR;
            lb.editBtn   = std::move(editBtnL);
            lb.editLabel = std::move(editLblT);
            mLevelButtons.push_back(std::move(lb));
            startY += btnH + rowGap;
        }
    }

    // ── State ─────────────────────────────────────────────────────────────────
    SDL_Window* mSDLWindow    = nullptr;  // needed for SDL_StartTextInput
    bool        startGame     = false;
    bool        openEditor    = false;
    std::string mChosenLevel;
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

    // Widgets
    std::unique_ptr<Image>     background;
    std::unique_ptr<Text>      titleText;
    std::unique_ptr<Text>      playBtnText;
    std::unique_ptr<Text>      editorBtnText;
    std::unique_ptr<Text>      hintText;
    std::unique_ptr<Text>      noLevelsText;
    std::unique_ptr<Text>      levelsHeader;
    std::unique_ptr<Rectangle> playButton;
    std::unique_ptr<Rectangle> editorButton;
    SDL_Rect                   playBtnRect{};
    SDL_Rect                   editorBtnRect{};
    std::vector<LevelButton>   mLevelButtons;

    SDL_Rect                   newLevelRect{};
    std::unique_ptr<Rectangle> newLevelBtn;
    std::unique_ptr<Text>      newLevelLabel;
};
