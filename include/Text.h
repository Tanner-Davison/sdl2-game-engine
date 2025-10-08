#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <iostream>
#include <string>

class Text {
  public:
    /**
     * @brief Constructor for creating a Text object
     *
     * @param Content  std::string the text you wish to render to the screen
     * @param posX  the (x) position of mDestinationRectangle (relative to the
     * surface you are rendering on)
     * @param posY  the (y) position of mDestinationRectangle (relative to the
     * surface you are rendering on)
     */
    Text(std::string Content, int posX = 0, int posY = 0);
    Text(const Text&)            = delete;
    Text& operator=(const Text&) = delete;

    ~Text();

    void Render(SDL_Surface* DestinationSurface);

    bool notread = true;

  private:
    void CreateSurface(std::string content);

  private:
    TTF_Font*    mFont;
    SDL_Surface* mTextSurface{nullptr};
    SDL_Rect     mSourceRectangle{0, 0, 0, 0};
    SDL_Rect     mDestinationRectangle{50, 50, 50, 50};
    int          mPosX;
    int          mPosY;
};
