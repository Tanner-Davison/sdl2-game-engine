#ifndef SPRITESHEET_HPP
#define SPRITESHEET_HPP

#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

/**
 * @class SpriteSheet
 * @brief Loads a sprite sheet image and its associated frame coordinate data.
 *
 * Parses either a plain text (.txt) or XML (.xml) coordinate file to extract
 * named frame rectangles from a single sprite sheet surface. Supports
 * retrieving individual frames or ordered animation sequences by name prefix.
 *
 * @par Coordinate File Formats
 * - **Text format**: Tab-separated lines of `name x y w h`
 * - **XML format**: Standard texture atlas XML (e.g. from TexturePacker)
 *
 * @par Example Usage
 * @code
 * SpriteSheet sheet("assets/player.png", "assets/player.txt");
 * std::vector<SDL_Rect> walkFrames = sheet.GetAnimation("p1_walk");
 * @endcode
 */
class SpriteSheet {
  public:
    /**
     * @brief Loads the sprite sheet image and parses its coordinate file.
     *
     * Automatically detects whether the coordinate file is text or XML format
     * based on the file extension.
     *
     * @param imageFile  Path to the sprite sheet image (PNG, BMP, etc.).
     * @param coordFile  Path to the coordinate file (.txt or .xml).
     */
    SpriteSheet(const std::string& imageFile, const std::string& coordFile);

    /**
     * @brief Builds a sprite sheet by stitching individually-numbered PNG files
     * into a single surface at load time.
     *
     * Files are expected to follow the pattern: `prefix1.png`, `prefix2.png`, ...
     * They are sorted numerically and laid out left-to-right into one surface.
     * All frames must be the same dimensions.
     *
     * @param directory  Path to the folder containing the PNG files (trailing slash optional).
     * @param prefix     Filename prefix shared by all frames (e.g. "Gold_").
     * @param frameCount Total number of frames to load.
     *
     * @par Example
     * @code
     * SpriteSheet coins("game_assets/gold_coins/", "Gold_", 30);
     * auto frames = coins.GetAnimation("Gold_");
     * @endcode
     */
    // padDigits:   zero-pad frame numbers to this width (0 = no padding, 3 = "000", "001"...)
    // startIndex:  override the first frame number (-1 = auto: 0 if padded, 1 if not)
    SpriteSheet(const std::string& directory, const std::string& prefix, int frameCount, int targetW = 0, int targetH = 0, int padDigits = 0, int startIndex = -1);

    /// Frees the loaded SDL_Surface.
    ~SpriteSheet();

    /**
     * @brief Returns the SDL_Rect for a single named frame.
     *
     * @param name The exact name of the frame as defined in the coordinate file.
     * @return SDL_Rect with the frame's position and dimensions on the sheet.
     * @throws std::out_of_range if the frame name is not found.
     */
    SDL_Rect GetFrame(const std::string& name) const;

    /**
     * @brief Returns all frames whose names begin with a given prefix, in order.
     *
     * Useful for extracting animation sequences. For example, passing "p1_walk"
     * returns all frames named "p1_walk1", "p1_walk2", etc. in sorted order.
     *
     * @param baseName Prefix to match against frame names.
     * @return Ordered vector of SDL_Rect frames matching the prefix.
     */
    std::vector<SDL_Rect> GetAnimation(const std::string& baseName) const;

    /**
     * @brief Returns a pointer to the raw sprite sheet SDL_Surface.
     *
     * The surface is owned by this SpriteSheet and must not be freed externally.
     * @return Non-owning pointer to the sheet's SDL_Surface.
     */
    /// Returns the raw stitched surface (used for building textures at load time).
    /// Do NOT hold onto this after CreateTexture() has been called.
    SDL_Surface* GetSurface() const { return surface; }

    /// Upload the stitched surface to the GPU and cache the texture.
    /// Must be called once after construction, before GetTexture() is used.
    /// The CPU surface is kept alive so scenes that blit directly from it
    /// (e.g. PlayerCreatorScene preview) can still access it via GetSurface().
    /// Call FreeSurface() explicitly once you no longer need CPU-side access.
    SDL_Texture* CreateTexture(SDL_Renderer* renderer) {
        if (!texture && surface)
            texture = SDL_CreateTextureFromSurface(renderer, surface);
        return texture;
    }

    /// Free the CPU surface after GPU upload when it is no longer needed.
    /// Safe to call multiple times. GameScene calls this after Load() to
    /// reclaim RAM; PlayerCreatorScene does NOT call it so the preview blit works.
    void FreeSurface() {
        if (surface) { SDL_DestroySurface(surface); surface = nullptr; }
    }

    /// Returns the GPU texture, or nullptr if CreateTexture() hasn't been called.
    SDL_Texture* GetTexture() const { return texture; }

  private:
    SDL_Surface*                    surface  = nullptr; ///< The loaded sprite sheet surface.
    SDL_Texture*                    texture  = nullptr; ///< GPU texture built from surface.
    std::unordered_map<std::string, SDL_Rect> frames;    ///< Named frame rectangles parsed from the coord file.

    /// Detects format and dispatches to the appropriate loader.
    void LoadCoordinates(const std::string& coordFile);

    /// Parses a tab-separated text coordinate file.
    void LoadTextFormat(const std::string& coordFile);

    /// Parses an XML texture atlas coordinate file.
    void LoadXMLFormat(const std::string& coordFile);
};

#endif // SPRITESHEET_HPP
