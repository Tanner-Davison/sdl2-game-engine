#include "Text.h"
#include <SDL.h>
#include <SDL_ttf.h>
#include <iostream>
#include <string>

Text::Text(std::string Content, int posX, int posY, int fontSize)
    : Text(Content, {255, 255, 255, 255}, posX, posY, fontSize) {}

Text::Text(
    std::string Content, SDL_Color Color, int posX, int posY, int fontSize)
    : mFont{TTF_OpenFont("fonts/Roboto-VariableFont_wdth,wght.ttf", fontSize)}
    , mColor{Color}
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

    SDL_Surface* newSurface =
        TTF_RenderText_Blended(mFont, Content.c_str(), mColor);

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
