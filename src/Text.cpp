#include "Text.h"
#include <SDL.h>
#include <SDL_surface.h>
#include <SDL_ttf.h>
#include <iostream>
#include <string>

Text::Text(std::string Content)
    : mFont{TTF_OpenFont("fonts/Roboto-VariableFont_wdth,wght.ttf", 50)} {
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
    SDL_BlitSurface(mTextSurface, nullptr, DestinationSurface, nullptr);
};

void Text::CreateSurface(std::string Content) {
    SDL_Surface* newSurface = TTF_RenderText_Blended(
        this->mFont, Content.c_str(), {255, 255, 255, 0});
    if (newSurface) {
        SDL_FreeSurface(this->mTextSurface);
        this->mTextSurface = newSurface;
    }
}
