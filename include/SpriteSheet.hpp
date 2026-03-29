#ifndef SPRITESHEET_HPP
#define SPRITESHEET_HPP

#include <SDL3/SDL.h>
#include <string>
#include <unordered_map>
#include <vector>

/// Loads a sprite sheet image and parses its frame coordinate data (.txt or .xml).
class SpriteSheet {
  public:
    SpriteSheet(const std::string& imageFile, const std::string& coordFile);

    /// Stitches individually-numbered PNGs (prefix1.png, prefix2.png, ...) into one surface.
    /// padDigits: zero-pad width (0=none, 3="000"). startIndex: first frame# (-1=auto).
    SpriteSheet(const std::string& directory, const std::string& prefix, int frameCount, int targetW = 0, int targetH = 0, int padDigits = 0, int startIndex = -1);

    /// Builds a sheet from an explicit sorted list of PNG paths (no prefix matching).
    SpriteSheet(const std::vector<std::string>& paths, int targetW = 0, int targetH = 0);

    /// Frees the loaded SDL_Surface.
    ~SpriteSheet();

    SDL_Rect GetFrame(const std::string& name) const;
    std::vector<SDL_Rect> GetAnimation(const std::string& baseName) const;

    /// Do NOT hold onto this after CreateTexture() has been called.
    SDL_Surface* GetSurface() const { return surface; }

    /// Upload surface to GPU. CPU surface stays alive for direct-blit scenes;
    /// call FreeSurface() when CPU-side access is no longer needed.
    SDL_Texture* CreateTexture(SDL_Renderer* renderer) {
        if (!texture && surface) {
            texture = SDL_CreateTextureFromSurface(renderer, surface);
            if (texture)
                SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART);
        }
        return texture;
    }

    void FreeSurface() {
        if (surface) { SDL_DestroySurface(surface); surface = nullptr; }
    }

    SDL_Texture* GetTexture() const { return texture; }

    int RenderW() const { return mRenderW; }
    int RenderH() const { return mRenderH; }

  private:
    SDL_Surface*                    surface  = nullptr;
    SDL_Texture*                    texture  = nullptr;
    int                             mRenderW = 0;
    int                             mRenderH = 0;
    std::unordered_map<std::string, SDL_Rect> frames;

    void LoadCoordinates(const std::string& coordFile);
    void LoadTextFormat(const std::string& coordFile);
    void LoadXMLFormat(const std::string& coordFile);
};

#endif // SPRITESHEET_HPP
