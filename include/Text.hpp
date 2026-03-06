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
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <optional>
#include <string>
#include <unordered_map>

// ── Global font cache ─────────────────────────────────────────────────────────
// TTF_OpenFont is expensive (disk I/O + parsing). This cache ensures each
// font size is opened exactly once for the lifetime of the process.
namespace FontCache {
    inline TTF_Font* Get(int fontSize) {
        static std::unordered_map<int, TTF_Font*> cache;
        auto it = cache.find(fontSize);
        if (it != cache.end()) return it->second;
        TTF_Font* f = TTF_OpenFont("fonts/Roboto-VariableFont_wdth,wght.ttf", fontSize);
        cache[fontSize] = f;
        return f;
    }
    // Call once at shutdown to free all cached fonts.
    inline void Clear() {
        static std::unordered_map<int, TTF_Font*>& cache = []() -> auto& {
            static std::unordered_map<int, TTF_Font*> c;
            return c;
        }();
        for (auto& [sz, f] : cache) if (f) TTF_CloseFont(f);
        cache.clear();
    }
}

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

    TTF_Font* mFont = nullptr; ///< TrueType font handle
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
    void SetFontSize(int fontsize);
    int  mFontSize = 24; ///< Font size in points

    /// Returns the pixel width and height of a string at a given font size.
    /// Useful for centering text without creating a full Text object.
    /// @param content  The string to measure
    /// @param fontSize Font size in points
    /// @return SDL_Point where x = width, y = height
    static SDL_Point Measure(const std::string& content, int fontSize) {
        TTF_Font* font = FontCache::Get(fontSize);
        if (!font) return {0, 0};
        int w = 0, h = 0;
        TTF_GetStringSize(font, content.c_str(), 0, &w, &h);
        return {w, h};
    }

    /// Returns the x position to horizontally center text within a rect.
    static int CenterX(const std::string& content, int fontSize, const SDL_Rect& rect) {
        return rect.x + (rect.w - Measure(content, fontSize).x) / 2;
    }

    /// Returns the y position to vertically center text within a rect.
    static int CenterY(int fontSize, const SDL_Rect& rect) {
        return rect.y + (rect.h - Measure("A", fontSize).y) / 2;
    }

    /// Returns both x and y to center text within a rect.
    static SDL_Point CenterInRect(const std::string& content,
                                  int                fontSize,
                                  const SDL_Rect&    rect) {
        return {CenterX(content, fontSize, rect), CenterY(fontSize, rect)};
    }
    /// Returns the internal text surface (non-owning). May be nullptr.
    SDL_Surface* GetSurface() const { return mTextSurface; }

    // utility funciton Sets position of the text
    void SetPosition(int x, int y) {
        mPosX                   = x;
        mPosY                   = y;
        mDestinationRectangle.x = x;
        mDestinationRectangle.y = y;
    }

  private:
    SDL_Surface* mTextSurface = nullptr;            ///< Rendered text surface
    SDL_Rect     mDestinationRectangle{0, 0, 0, 0}; ///< Render destination
    SDL_Color    mColor{255, 255, 255, 255};        ///< Foreground color (white default)
    std::optional<SDL_Color> mColorBg;              ///< Optional background color
    int                      mPosX = 0;             ///< X position
    int                      mPosY = 0;             ///< Y position
};
