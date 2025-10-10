/**
 * @file Text.h
 * @brief SDL2-based text rendering component for displaying styled text
 *
 * This file provides a Text class that wraps SDL2_ttf functionality for
 * rendering text with customizable colors, positions, and font sizes.
 *
 * @see SDL2 documentation at https://wiki.libsdl.org/
 * @see SDL_ttf documentation for font rendering details
 *
 * Example usage:
 * @code
 * Text myText("Hello World", 100, 50, 32);
 * myText.Render(windowSurface);
 * @endcode
 */
#pragma once
#include <SDL.h>
#include <SDL_ttf.h>
#include <optional>
#include <string>

/**
 * @class Text
 * @brief Manages text rendering with SDL2_ttf
 *
 * This class handles the creation, styling, and rendering of text using
 * TrueType fonts via SDL2_ttf. It supports custom colors, positioning,
 * and font sizes. The class manages its own font and surface resources
 * and cleans them up automatically.
 *
 * @note This class is non-copyable to prevent resource management issues
 * @warning Ensure SDL2 and SDL_ttf are properly initialized before use
 */
class Text {
  public:
    /**
     * @brief Constructor for creating a Text object with default white color
     *
     * @param Content   The text string to render
     * @param posX      X position on the destination surface (default: 0)
     * @param posY      Y position on the destination surface (default: 0)
     * @param fontSize  Font size in points (default: 24)
     *
     * @throws std::runtime_error if font loading fails
     */
    Text(std::string Content, int posX = 0, int posY = 0, int fontSize = 24);

    /**
     * @brief Constructor with custom foreground color
     *
     * @param Content   The text string to render
     * @param ColorFg   Foreground (text) color as SDL_Color
     * @param posX      X position on the destination surface (default: 0)
     * @param posY      Y position on the destination surface (default: 0)
     * @param fontSize  Font size in points (default: 24)
     *
     * @throws std::runtime_error if font loading fails
     */
    Text(std::string Content,
         SDL_Color   ColorFg,
         int         posX     = 0,
         int         posY     = 0,
         int         fontSize = 24);

    /**
     * @brief Constructor with custom foreground and optional background color
     *
     * @param Content   The text string to render
     * @param ColorFg   Foreground (text) color as SDL_Color
     * @param Colorbg   Optional background color (nullopt for transparent)
     * @param posX      X position on the destination surface (default: 0)
     * @param posY      Y position on the destination surface (default: 0)
     * @param fontSize  Font size in points (default: 24)
     *
     * @note When Colorbg is provided, text will be rendered with a solid
     * background
     * @throws std::runtime_error if font loading fails
     */
    Text(std::string              Content,
         SDL_Color                ColorFg,
         std::optional<SDL_Color> Colorbg,
         int                      posX     = 0,
         int                      posY     = 0,
         int                      fontSize = 24);

    /**
     * @brief Deleted copy constructor
     *
     * Text objects cannot be copied due to owned SDL resources
     */
    Text(const Text&) = delete;

    /**
     * @brief Deleted copy assignment operator
     *
     * Text objects cannot be copy-assigned due to owned SDL resources
     */
    Text& operator=(const Text&) = delete;

    /**
     * @brief Destructor - cleans up SDL resources
     *
     * Frees the font and text surface resources
     */
    ~Text();

    /**
     * @brief Renders the text to the specified destination surface
     *
     * @param DestinationSurface Pointer to the SDL_Surface to render onto
     *
     * @pre DestinationSurface must be a valid, non-null SDL_Surface pointer
     * @post Text is blitted to the destination surface at the specified
     * position
     *
     * @warning Ensure the destination surface is not freed before rendering
     */
    void Render(SDL_Surface* DestinationSurface);

  private:
    /**
     * @brief Creates the text surface from the given content string
     *
     * @param content The text string to render into a surface
     *
     * Uses TTF_RenderText_Solid or TTF_RenderText_Shaded depending on
     * whether a background color is specified
     *
     * @throws std::runtime_error if surface creation fails
     */
    void CreateSurface(std::string content);

  private:
    TTF_Font*    mFont        = nullptr;            ///< TrueType font handle
    SDL_Surface* mTextSurface = nullptr;            ///< Rendered text surface
    SDL_Rect     mDestinationRectangle{0, 0, 0, 0}; ///< Render destination
    SDL_Color mColor{255, 255, 255, 255}; ///< Foreground color (white default)
    std::optional<SDL_Color> mColorBg;    ///< Optional background color
    int                      mFontSize = 24; ///< Font size in points
    int                      mPosX     = 0;  ///< X position
    int                      mPosY     = 0;  ///< Y position
};
