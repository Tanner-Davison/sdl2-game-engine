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
    // Detect file format by extension or content
    if (coordFile.find(".xml") != std::string::npos) {
        LoadXMLFormat(coordFile);
    } else {
        LoadTextFormat(coordFile);
    }
}

void SpriteSheet::LoadTextFormat(const std::string& coordFile) {
    std::ifstream file(coordFile);
    if (!file.is_open()) {
        std::cout << "Failed to open coordinate file: " << coordFile
                  << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#')
            continue;

        // Parse format: "name = x y w h"
        std::istringstream iss(line);
        std::string        name, equals;
        int                x, y, w, h;

        if (iss >> name >> equals >> x >> y >> w >> h) {
            frames[name] = {x, y, w, h};
        }
    }

    // std::cout << "Loaded " << frames.size() << " frames from sprite sheet"
    //           << std::endl;
}

void SpriteSheet::LoadXMLFormat(const std::string& coordFile) {
    std::ifstream file(coordFile);
    if (!file.is_open()) {
        std::cout << "Failed to open coordinate file: " << coordFile
                  << std::endl;
        return;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Look for SubTexture tags
        if (line.find("<SubTexture") == std::string::npos)
            continue;

        // Extract attributes using simple string parsing
        std::string name;
        int         x = 0, y = 0, width = 0, height = 0;

        // Extract name
        size_t namePos = line.find("name=\"");
        if (namePos != std::string::npos) {
            namePos += 6; // Skip 'name="'
            size_t nameEnd = line.find("\"", namePos);
            name           = line.substr(namePos, nameEnd - namePos);

            // Remove .png extension if present
            if (name.size() > 4 && name.substr(name.size() - 4) == ".png") {
                name = name.substr(0, name.size() - 4);
            }
        }

        // Extract x
        size_t xPos = line.find("x=\"");
        if (xPos != std::string::npos) {
            xPos += 3;
            size_t xEnd = line.find("\"", xPos);
            x           = std::stoi(line.substr(xPos, xEnd - xPos));
        }

        // Extract y
        size_t yPos = line.find("y=\"");
        if (yPos != std::string::npos) {
            yPos += 3;
            size_t yEnd = line.find("\"", yPos);
            y           = std::stoi(line.substr(yPos, yEnd - yPos));
        }

        // Extract width
        size_t wPos = line.find("width=\"");
        if (wPos != std::string::npos) {
            wPos += 7;
            size_t wEnd = line.find("\"", wPos);
            width       = std::stoi(line.substr(wPos, wEnd - wPos));
        }

        // Extract height
        size_t hPos = line.find("height=\"");
        if (hPos != std::string::npos) {
            hPos += 8;
            size_t hEnd = line.find("\"", hPos);
            height      = std::stoi(line.substr(hPos, hEnd - hPos));
        }

        if (!name.empty()) {
            frames[name] = {x, y, width, height};
        }
    }

    // std::cout << "Loaded " << frames.size() << " frames from XML sprite
    // sheet"
    //           << std::endl;
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

    // Collect all frames that start with baseName
    std::vector<std::pair<std::string, SDL_Rect>> matchingFrames;

    for (const auto& [name, rect] : frames) {
        if (name.find(baseName) == 0) {
            matchingFrames.push_back({name, rect});
        }
    }

    // Sort by name to get correct order
    std::sort(matchingFrames.begin(),
              matchingFrames.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    // Extract just the rects
    for (const auto& [name, rect] : matchingFrames) {
        animation.push_back(rect);
    }

    return animation;
}
