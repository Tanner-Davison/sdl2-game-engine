#include "Text.h"
#include <SDL.h>
#include <SDL_error.h>
#include <SDL_surface.h>
#include <SDL_ttf.h>
#include <iostream>
#include <string>

/**
 * @brief create a text object at position x and y relative to the screen
 * surface;
 *
 * @param Content the std::string you wish to render
 * @param posX the x position relative to the surface you are rendering on
 * @param posY  the y position relative to the surface you are rendering on
 */
Text::Text(std::string Content, int posX, int posY)
    : mFont{TTF_OpenFont("fonts/Roboto-VariableFont_wdth,wght.ttf", 50)}
    , mPosX{posX}
    , mPosY{posY} {
    if (!mFont) {
        std::cout << "Error loading font: " << SDL_GetError();
    }
    CreateSurface(Content);
    if (!mTextSurface) {
        std::cout << "Error Setting Texture Surface: " << SDL_GetError();
    }
};

Text::~Text() {
    if (TTF_WasInit()) {
        SDL_FreeSurface(this->mTextSurface);
        TTF_CloseFont(this->mFont);
    }
}

void Text::Render(SDL_Surface* DestinationSurface) {
    if (notread) {
        std::cout << "This is running" << std::endl;
        notread = false;
    }
    SDL_BlitSurface(
        mTextSurface, nullptr, DestinationSurface, &mDestinationRectangle);
};

void Text::CreateSurface(std::string Content) {
    SDL_Surface* newSurface = TTF_RenderText_Blended(
        this->mFont, Content.c_str(), {255, 255, 255, 0});
    if (newSurface) {
        SDL_FreeSurface(this->mTextSurface);
        this->mTextSurface = newSurface;
    } else {
        std::cout << "Error Creating Text Surface: " << SDL_GetError() << '\n';
    }
    if (mPosX > 0 && mPosY > 0) {
        mDestinationRectangle.x = mPosX;
        mDestinationRectangle.y = mPosY;
    }
}
