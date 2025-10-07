#include "SpriteSheet.hpp"
#include <SDL_image.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

SpriteSheet::SpriteSheet(const std::string& imageFile,
                         const std::string& coordFile)
    : surface(nullptr) {
    // Load the sprite sheet image
    surface = IMG_Load(imageFile.c_str());
    if (!surface) {
        std::cout << "Failed to load sprite sheet: " << imageFile << "\n"
                  << SDL_GetError() << std::endl;
        return;
    }

    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    // Load the coordinate data
    LoadCoordinates(coordFile);
}

SpriteSheet::~SpriteSheet() {
    if (surface) {
        SDL_FreeSurface(surface);
    }
}

void SpriteSheet::LoadCoordinates(const std::string& coordFile) {
    std::ifstream file(coordFile);
    if (!file.is_open()) {
        std::cout << "Failed to open coordinate file: " << coordFile
                  << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines
        if (line.empty())
            continue;

        // Parse format: "name = x y w h"
        std::istringstream iss(line);
        std::string        name, equals;
        int                x, y, w, h;

        if (iss >> name >> equals >> x >> y >> w >> h) {
            frames[name] = {x, y, w, h};
        }
    }

    std::cout << "Loaded " << frames.size() << " frames from sprite sheet"
              << std::endl;
}

SDL_Rect SpriteSheet::GetFrame(const std::string& name) const {
    auto it = frames.find(name);
    if (it != frames.end()) {
        return it->second;
    }
    std::cout << "Frame not found: " << name << std::endl;
    return {0, 0, 0, 0};
}

std::vector<SDL_Rect> SpriteSheet::GetAnimation(
    const std::string& baseName) const {
    std::vector<SDL_Rect> animation;

    // Collect all frames that start with baseName (e.g., "p1_walk")
    std::vector<std::pair<std::string, SDL_Rect>> matchingFrames;

    for (const auto& [name, rect] : frames) {
        if (name.find(baseName) == 0) {
            matchingFrames.push_back({name, rect});
        }
    }

    // Sort by name to get correct order (p1_walk01, p1_walk02, etc.)
    std::sort(matchingFrames.begin(),
              matchingFrames.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Extract just the rects
    for (const auto& [name, rect] : matchingFrames) {
        animation.push_back(rect);
    }

    return animation;
}
