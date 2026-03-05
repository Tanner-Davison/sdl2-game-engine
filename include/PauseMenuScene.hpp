#pragma once
#include "GameScene.hpp"
#include "LevelEditorScene.hpp"
#include "Rectangle.hpp"
#include "Scene.hpp"
#include "Text.hpp"
#include "TitleScene.hpp"
#include "Window.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// PauseMenuScene
//
// Shown when the player presses ESC during gameplay. Renders the frozen game
// frame underneath a dark overlay, then presents two buttons:
//
//   Resume        — returns to the game (re-creates GameScene with same path)
//   Back to X     — goes to LevelEditorScene (if launched from editor)
//                   or TitleScene (if launched from title screen)
//
// Construction:
//   levelPath   — the JSON path that was loaded (may be "" for sandbox mode)
//   fromEditor  — true when the game was launched via the editor Play button
// ─────────────────────────────────────────────────────────────────────────────
class PauseMenuScene : public Scene {
  public:
    PauseMenuScene(const std::string& levelPath, bool fromEditor,
                   const std::string& profilePath = "")
        : mLevelPath(levelPath), mFromEditor(fromEditor), mProfilePath(profilePath) {}

    void Load(Window& window) override {
        mW = window.GetWidth();
        mH = window.GetHeight();

        // Grab a snapshot of the current framebuffer to use as backdrop.
        // SceneManager already called Unload on GameScene, so the window surface
        // holds whatever the last rendered frame was — capture it now.
        SDL_Surface* win = window.GetSurface();
        if (win) {
            mBackdrop.reset(SDL_CreateSurface(win->w, win->h, win->format));
            if (mBackdrop)
                SDL_BlitSurface(win, nullptr, mBackdrop.get(), nullptr);
        }

        buildUI(mW, mH);
    }

    void Unload() override {
        mBackdrop.reset();
    }

    bool HandleEvent(SDL_Event& e) override {
        if (e.type == SDL_EVENT_QUIT) return false;

        // ESC again = resume
        if (e.type == SDL_EVENT_KEY_DOWN && e.key.key == SDLK_ESCAPE) {
            mResume = true;
            return true;
        }

        if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && e.button.button == SDL_BUTTON_LEFT) {
            int mx = (int)e.button.x;
            int my = (int)e.button.y;

            if (hit(mResumeRect, mx, my))  { mResume = true;  return true; }
            if (hit(mBackRect,   mx, my))  { mGoBack = true;  return true; }
        }

        if (mResumeBtn) mResumeBtn->HandleEvent(e);
        if (mBackBtn)   mBackBtn->HandleEvent(e);
        return true;
    }

    void Update(float /*dt*/) override {}

    void Render(Window& window) override {
        SDL_Surface* screen = window.GetSurface();

        // 1. Draw the frozen game frame
        if (mBackdrop)
            SDL_BlitSurface(mBackdrop.get(), nullptr, screen, nullptr);

        // 2. Dark semi-transparent overlay
        const SDL_PixelFormatDetails* fmt = SDL_GetPixelFormatDetails(screen->format);
        Uint32 dim = SDL_MapRGBA(fmt, nullptr, 0, 0, 0, 160);
        SDL_FillSurfaceRect(screen, nullptr, dim);

        // 3. Panel background
        SDL_Rect panel = {mW/2 - 180, mH/2 - 160, 360, 320};
        Uint32 panelCol = SDL_MapRGBA(fmt, nullptr, 18, 20, 32, 230);
        SDL_FillSurfaceRect(screen, &panel, panelCol);
        // Panel border
        drawOutline(screen, panel, {80, 120, 220, 255}, fmt);

        // 4. UI elements
        if (mTitle)     mTitle->Render(screen);
        if (mResumeBtn) mResumeBtn->Render(screen);
        if (mResumeLbl) mResumeLbl->Render(screen);
        if (mBackBtn)   mBackBtn->Render(screen);
        if (mBackLbl)   mBackLbl->Render(screen);
        if (mHint)      mHint->Render(screen);

        window.Update();
    }

    std::unique_ptr<Scene> NextScene() override {
        if (mResume)
            return std::make_unique<GameScene>(mLevelPath, mFromEditor, mProfilePath);
        if (mGoBack) {
            if (mFromEditor)
                return std::make_unique<LevelEditorScene>(mLevelPath, false, "", mProfilePath);
            else
                return std::make_unique<TitleScene>();
        }
        return nullptr;
    }

  private:
    std::string  mLevelPath;
    std::string  mProfilePath;
    bool         mFromEditor = false;
    bool         mResume     = false;
    bool         mGoBack     = false;
    int          mW = 0, mH = 0;

    // Captured backdrop surface (owned)
    struct SurfaceDeleter { void operator()(SDL_Surface* s) { if (s) SDL_DestroySurface(s); } };
    std::unique_ptr<SDL_Surface, SurfaceDeleter> mBackdrop;

    SDL_Rect mResumeRect{};
    SDL_Rect mBackRect{};

    std::unique_ptr<Rectangle> mResumeBtn;
    std::unique_ptr<Rectangle> mBackBtn;
    std::unique_ptr<Text>      mTitle;
    std::unique_ptr<Text>      mResumeLbl;
    std::unique_ptr<Text>      mBackLbl;
    std::unique_ptr<Text>      mHint;

    // ── Helpers ───────────────────────────────────────────────────────────────
    static bool hit(const SDL_Rect& r, int x, int y) {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    }

    static void drawOutline(SDL_Surface* s, SDL_Rect r,
                            SDL_Color c, const SDL_PixelFormatDetails* fmt) {
        Uint32 col = SDL_MapRGBA(fmt, nullptr, c.r, c.g, c.b, c.a);
        constexpr int T = 2;
        SDL_Rect rects[4] = {
            {r.x,       r.y,       r.w, T},
            {r.x,       r.y+r.h-T, r.w, T},
            {r.x,       r.y,       T,   r.h},
            {r.x+r.w-T, r.y,       T,   r.h},
        };
        for (auto& rr : rects) SDL_FillSurfaceRect(s, &rr, col);
    }

    void buildUI(int W, int H) {
        int cx = W / 2;
        int cy = H / 2;

        // Title
        SDL_Rect titleRect = {cx - 160, cy - 145, 320, 50};
        auto [tx, ty] = Text::CenterInRect("PAUSED", 36, titleRect);
        mTitle = std::make_unique<Text>("PAUSED", SDL_Color{255, 215, 0, 255}, tx, ty, 36);

        // Resume button
        mResumeRect = {cx - 130, cy - 60, 260, 55};
        mResumeBtn  = std::make_unique<Rectangle>(mResumeRect);
        mResumeBtn->SetColor({40, 160, 80, 255});
        mResumeBtn->SetHoverColor({60, 200, 100, 255});
        auto [rx, ry] = Text::CenterInRect("Resume", 28, mResumeRect);
        mResumeLbl = std::make_unique<Text>("Resume", SDL_Color{255, 255, 255, 255}, rx, ry, 28);

        // Back button — label depends on origin
        std::string backLabel = mFromEditor ? "Back to Editor" : "Back to Title";
        mBackRect = {cx - 130, cy + 20, 260, 55};
        mBackBtn  = std::make_unique<Rectangle>(mBackRect);
        mBackBtn->SetColor({120, 50, 50, 255});
        mBackBtn->SetHoverColor({180, 70, 70, 255});
        auto [bx, by] = Text::CenterInRect(backLabel, 22, mBackRect);
        mBackLbl = std::make_unique<Text>(backLabel, SDL_Color{255, 220, 220, 255}, bx, by, 22);

        // Hint
        mHint = std::make_unique<Text>("ESC to resume", SDL_Color{100, 100, 120, 255},
                                       cx - 70, cy + 100, 14);
    }
};
