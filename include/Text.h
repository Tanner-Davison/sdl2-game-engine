#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <optional>
#include <string>

class Text {
  public:
    /**
     * @brief Constructor for creating a Text object with default white color
     *
     * @param Content   The text to render
     * @param posX      X position (default: 0)
     * @param posY      Y position (default: 0)
     * @param fontSize  Font size in points (default: 24)
     */
    Text(std::string Content, int posX = 0, int posY = 0, int fontSize = 24);

    /**
     * @brief Constructor with custom foreground color
     *
     * @param Content   The text to render
     * @param ColorFg   Foreground text color
     * @param posX      X position (default: 0)
     * @param posY      Y position (default: 0)
     * @param fontSize  Font size in points (default: 24)
     */
    Text(std::string Content,
         SDL_Color   ColorFg,
         int         posX     = 0,
         int         posY     = 0,
         int         fontSize = 24);

    /**
     * @brief Constructor with custom foreground and optional background color
     *
     * @param Content   The text to render
     * @param Color     Text color
     * @param posX      X position (default: 0)
     * @param posY      Y position (default: 0)
     * @param fontSize  Font size in points (default: 24)
     */
    Text(std::string              Content,
         SDL_Color                ColorFg,
         std::optional<SDL_Color> Colorbg,
         int                      posX     = 0,
         int                      posY     = 0,
         int                      fontSize = 24);

    Text(const Text&)            = delete;
    Text& operator=(const Text&) = delete;
    ~Text();

    void Render(SDL_Surface* DestinationSurface);

  private:
    void CreateSurface(std::string content);

  private:
    TTF_Font*                mFont        = nullptr;
    SDL_Surface*             mTextSurface = nullptr;
    SDL_Rect                 mDestinationRectangle{0, 0, 0, 0};
    SDL_Color                mColor{255, 255, 255, 255}; // White default
    std::optional<SDL_Color> mColorBg;
    int                      mFontSize = 24;
    int                      mPosX     = 0;
    int                      mPosY     = 0;
};
