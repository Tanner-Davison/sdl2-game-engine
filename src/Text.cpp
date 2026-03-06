#include "Text.hpp"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <optional>
#include <print>
#include <string>

Text::Text(std::string Content, int posX, int posY, int fontSize)
    : Text(Content, {255, 255, 255, 255}, std::nullopt, posX, posY, fontSize) {}

Text::Text(std::string Content, SDL_Color ColorFg, int posX, int posY, int fontSize)
    : Text(Content, ColorFg, std::nullopt, posX, posY, fontSize) {}

Text::Text(std::string Content, SDL_Color ColorFg,
           std::optional<SDL_Color> ColorBg,
           int posX, int posY, int fontSize)
    : mFont{FontCache::Get(fontSize)}
    , mColor{ColorFg}
    , mColorBg{ColorBg}
    , mFontSize{fontSize}
    , mPosX{posX}
    , mPosY{posY} {
    if (!mFont) {
        std::print("Error loading font: {}\n", SDL_GetError());
        return;
    }
    CreateSurface(Content);
}

Text::~Text() {
    if (mTexture)     SDL_DestroyTexture(mTexture);
    if (mTextSurface) SDL_DestroySurface(mTextSurface);
}

void Text::Render(SDL_Renderer* renderer) {
    if (!renderer) return;

    // Upload pending surface to texture on first render (or after text change)
    if (mDirty && mTextSurface) {
        if (mTexture) SDL_DestroyTexture(mTexture);
        mTexture = SDL_CreateTextureFromSurface(renderer, mTextSurface);
        SDL_DestroySurface(mTextSurface);
        mTextSurface = nullptr;
        mDirty       = false;
    }

    if (!mTexture) return;

    SDL_FRect dst = {(float)mPosX, (float)mPosY, (float)mTexW, (float)mTexH};
    SDL_RenderTexture(renderer, mTexture, nullptr, &dst);
}

void Text::CreateSurface(std::string Content) {
    if (!mFont || Content.empty()) return;
    mContent = Content;

    SDL_Surface* s = nullptr;
    if (mColorBg.has_value())
        s = TTF_RenderText_Shaded(mFont, Content.c_str(), 0, mColor, *mColorBg);
    else
        s = TTF_RenderText_Blended(mFont, Content.c_str(), 0, mColor);

    if (s) {
        if (mTextSurface) SDL_DestroySurface(mTextSurface);
        if (mTexture)     { SDL_DestroyTexture(mTexture); mTexture = nullptr; }
        mTextSurface = s;
        mTexW        = s->w;
        mTexH        = s->h;
        mDirty       = true;
    } else {
        std::print("Error creating text surface: {}\n", SDL_GetError());
    }
}

void Text::SetFontSize(int fontsize) { mFontSize = fontsize; }
