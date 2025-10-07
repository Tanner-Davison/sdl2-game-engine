#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <iostream>
#include <string>

class Text {
  public:
    Text(std::string Content);
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
};
