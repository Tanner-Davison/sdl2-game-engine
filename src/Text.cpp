#include "Text.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <iostream>
#include <optional>
#include <string>

/*
 * SDL2  Text Rendering Options-----------------------
 *
 * Solid -> fastest (but jagged edges) this is best for debug text:
 * TTF_RenderText_Solid()
 *
 * Shaded -> anti-aliased but is opaque slightly Slower than Solid:
 * TTF_RednerText_Shaded()
 *
 * Blended -> Highest Quality with alpha blending and anti-aliased:
 * TTF_RenderText_Blended()
 *
 * LCD -> SubPixel rendering optimized for LCD screens (best quality but is also
 * the slowest)
 * TTF_RenderUTF8_LCD
 * --------------------------------------------------------------------
 * */
Text::Text(std::string Content, int posX, int posY, int fontSize)
    : Text(Content, {255, 255, 255, 255}, std::nullopt, posX, posY, fontSize) {}

Text::Text(
    std::string Content, SDL_Color ColorFg, int posX, int posY, int fontSize)
    : Text(Content, ColorFg, std::nullopt, posX, posY, fontSize) {}
Text::Text(std::string              Content,
           SDL_Color                ColorFg,
           std::optional<SDL_Color> ColorBg,
           int                      posX,
           int                      posY,
           int                      fontSize)
    : mFont{TTF_OpenFont("fonts/Roboto-VariableFont_wdth,wght.ttf", fontSize)}
    , mColor{ColorFg}
    , mColorBg{ColorBg}
    , mFontSize{fontSize}
    , mPosX{posX}
    , mPosY{posY} {
    if (!mFont) {
        std::cerr << "Error loading font: " << TTF_GetError() << '\n';
        return;
    }
    CreateSurface(Content);
}

Text::~Text() {
    if (mTextSurface) {
        SDL_FreeSurface(mTextSurface);
    }
    if (mFont) {
        TTF_CloseFont(mFont);
    }
}

void Text::Render(SDL_Surface* DestinationSurface) {
    if (!mTextSurface || !DestinationSurface) {
        return;
    }
    SDL_BlitSurface(
        mTextSurface, nullptr, DestinationSurface, &mDestinationRectangle);
}

void Text::CreateSurface(std::string Content) {
    if (!mFont) {
        std::cerr << "Cannot create surface: font is null\n";
        return;
    }
    SDL_Surface* newSurface = nullptr;
    if (mColorBg.has_value()) {
        newSurface =
            TTF_RenderText_Shaded(mFont, Content.c_str(), mColor, *mColorBg);
    } else {
        newSurface = TTF_RenderText_Blended(mFont, Content.c_str(), mColor);
    }

    if (newSurface) {
        // Free old surface if it exists
        if (mTextSurface) {
            SDL_FreeSurface(mTextSurface);
        }

        mTextSurface = newSurface;

        // Update destination rectangle
        mDestinationRectangle.x = mPosX;
        mDestinationRectangle.y = mPosY;
        mDestinationRectangle.w = newSurface->w;
        mDestinationRectangle.h = newSurface->h;
    } else {
        std::cerr << "Error creating text surface: " << TTF_GetError() << '\n';
    }
}
