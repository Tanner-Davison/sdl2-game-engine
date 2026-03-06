#pragma once
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <optional>
#include <string>
#include <unordered_map>

// ── Global font cache ─────────────────────────────────────────────────────────
namespace FontCache {
    inline TTF_Font* Get(int fontSize) {
        static std::unordered_map<int, TTF_Font*> cache;
        auto it = cache.find(fontSize);
        if (it != cache.end()) return it->second;
        TTF_Font* f = TTF_OpenFont("fonts/Roboto-VariableFont_wdth,wght.ttf", fontSize);
        cache[fontSize] = f;
        return f;
    }
    inline void Clear() {
        static std::unordered_map<int, TTF_Font*>& cache = []() -> auto& {
            static std::unordered_map<int, TTF_Font*> c;
            return c;
        }();
        for (auto& [sz, f] : cache) if (f) TTF_CloseFont(f);
        cache.clear();
    }
}

class Text {
  public:
    Text(std::string Content, int posX = 0, int posY = 0, int fontSize = 24);
    Text(std::string Content, SDL_Color ColorFg, int posX = 0, int posY = 0, int fontSize = 24);
    Text(std::string Content, SDL_Color ColorFg, std::optional<SDL_Color> Colorbg,
         int posX = 0, int posY = 0, int fontSize = 24);

    Text(const Text&)            = delete;
    Text& operator=(const Text&) = delete;

    ~Text();

    // Render to the GPU renderer (main path for all normal scenes)
    void Render(SDL_Renderer* renderer);

    void CreateSurface(std::string content);
    void SetFontSize(int fontsize);

    TTF_Font* mFont    = nullptr;
    int       mFontSize = 24;

    // ── Measurement utilities ─────────────────────────────────────────────────
    static SDL_Point Measure(const std::string& content, int fontSize) {
        TTF_Font* font = FontCache::Get(fontSize);
        if (!font) return {0, 0};
        int w = 0, h = 0;
        TTF_GetStringSize(font, content.c_str(), 0, &w, &h);
        return {w, h};
    }
    static int CenterX(const std::string& content, int fontSize, const SDL_Rect& rect) {
        return rect.x + (rect.w - Measure(content, fontSize).x) / 2;
    }
    static int CenterY(int fontSize, const SDL_Rect& rect) {
        return rect.y + (rect.h - Measure("A", fontSize).y) / 2;
    }
    static SDL_Point CenterInRect(const std::string& content, int fontSize, const SDL_Rect& rect) {
        return {CenterX(content, fontSize, rect), CenterY(fontSize, rect)};
    }

    void SetPosition(int x, int y) { mPosX = x; mPosY = y; }

    // Surface-based render path for editor/creator scenes that use a staging
    // surface pipeline. Blits text directly without touching the GPU texture.
    void RenderToSurface(SDL_Surface* dst) const {
        if (!dst) return;
        // Fast path: use the already-rendered surface from construction.
        if (mTextSurface) {
            SDL_Rect d = {mPosX, mPosY, mTextSurface->w, mTextSurface->h};
            SDL_BlitSurface(mTextSurface, nullptr, dst, &d);
            return;
        }
        // Fallback: re-render from mContent (after GPU upload consumed mTextSurface).
        TTF_Font* font = FontCache::Get(mFontSize);
        if (!font || mContent.empty()) return;
        SDL_Surface* tmp = mColorBg.has_value()
            ? TTF_RenderText_Shaded(font, mContent.c_str(), 0, mColor, *mColorBg)
            : TTF_RenderText_Blended(font, mContent.c_str(), 0, mColor);
        if (tmp) {
            SDL_Rect d = {mPosX, mPosY, tmp->w, tmp->h};
            SDL_BlitSurface(tmp, nullptr, dst, &d);
            SDL_DestroySurface(tmp);
        }
    }

    int GetWidth()  const { return mTexW; }
    int GetHeight() const { return mTexH; }

    // Returns the internal staging surface (valid until first GPU upload).
    // Used by LevelEditorScene::GetBadge to snapshot text into a cached SDL_Surface.
    SDL_Surface* GetSurface() const { return mTextSurface; }

  private:
    SDL_Surface* mTextSurface = nullptr; // staging surface until first GPU upload
    SDL_Texture* mTexture     = nullptr; // GPU texture
    bool         mDirty       = false;   // true when mTextSurface needs uploading
    int          mTexW        = 0;
    int          mTexH        = 0;

    SDL_Color                mColor{255, 255, 255, 255};
    std::optional<SDL_Color> mColorBg;
    std::string              mContent;   // kept for RenderToSurface fallback
    int                      mPosX = 0;
    int                      mPosY = 0;
};
