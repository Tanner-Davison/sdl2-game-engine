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
    : mFont{FontCache::Get(fontSize)}  // shared cached font — never close this
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
    if (mTextSurface) SDL_DestroySurface(mTextSurface);
    // mFont is owned by FontCache — do NOT close it here
}

void Text::Render(SDL_Surface* DestinationSurface) {
    if (!mTextSurface || !DestinationSurface) return;
    SDL_BlitSurface(mTextSurface, nullptr, DestinationSurface,
                    &mDestinationRectangle);
}

void Text::CreateSurface(std::string Content) {
    if (!mFont) {
        std::print("Cannot create surface: font is null\n");
        return;
    }
    if (Content.empty()) return;

    SDL_Surface* newSurface = nullptr;
    if (mColorBg.has_value()) {
        newSurface = TTF_RenderText_Shaded(mFont, Content.c_str(), 0,
                                           mColor, *mColorBg);
    } else {
        newSurface = TTF_RenderText_Blended(mFont, Content.c_str(), 0, mColor);
    }

    if (newSurface) {
        if (mTextSurface) SDL_DestroySurface(mTextSurface);
        mTextSurface = newSurface;
        mDestinationRectangle = {mPosX, mPosY, newSurface->w, newSurface->h};
    } else {
        std::print("Error creating text surface: {}\n", SDL_GetError());
    }
}

void Text::SetFontSize(int fontsize) { mFontSize = fontsize; }
