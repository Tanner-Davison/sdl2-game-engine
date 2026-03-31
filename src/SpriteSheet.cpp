#include "SpriteSheet.hpp"
#include "AnimCache.hpp"
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <fstream>
#include <print>
#include <sstream>

static constexpr int MAX_TEX = 4096;

// ---------------------------------------------------------------------------
// Atlas-from-coordinate-file (enemies spritesheet, etc.)
// ---------------------------------------------------------------------------
SpriteSheet::SpriteSheet(const std::string& imageFile, const std::string& coordFile)
    : surface(nullptr) {
    surface = IMG_Load(imageFile.c_str());
    if (!surface) {
        std::print("SpriteSheet: failed to load {}\n{}\n", imageFile, SDL_GetError());
        return;
    }
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
    LoadCoordinates(coordFile);
}

// ---------------------------------------------------------------------------
// Numbered-PNG-sequence (player animation slots)
// ---------------------------------------------------------------------------
SpriteSheet::SpriteSheet(const std::string& directory, const std::string& prefix,
                         int frameCount, int targetW, int targetH,
                         int padDigits, int startIndex)
    : surface(nullptr) {
    std::string dir = directory;
    if (!dir.empty() && dir.back() != '/') dir += '/';

    const std::string cacheKey = "seq|" + dir + "|" + prefix
                               + "|n" + std::to_string(frameCount)
                               + "|" + std::to_string(targetW) + "x" + std::to_string(targetH)
                               + "|p" + std::to_string(padDigits);

    if (const auto* cached = AnimCache::Get().Fetch(cacheKey)) {
        surface  = SDL_DuplicateSurface(cached->surface);
        frames   = cached->frames;
        mRenderW = cached->renderW;
        mRenderH = cached->renderH;
        if (surface) SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
        return;
    }

    std::vector<SDL_Surface*> frameSurfaces;
    int frameW = 0, frameH = 0;

    int startIdx = (startIndex >= 0) ? startIndex : (padDigits > 0 ? 0 : 1);
    int endIdx   = startIdx + frameCount - 1;

    for (int i = startIdx; i <= endIdx; ++i) {
        std::string num = std::to_string(i);
        if (padDigits > 0)
            num = std::string(std::max(0, padDigits - (int)num.size()), '0') + num;
        std::string path = dir + prefix + num + ".png";
        SDL_Surface* s   = IMG_Load(path.c_str());
        if (!s) {
            std::print("SpriteSheet: failed to load frame {}\n{}\n", path, SDL_GetError());
            for (auto* f : frameSurfaces) SDL_DestroySurface(f);
            return;
        }
        if (i == startIdx) { frameW = s->w; frameH = s->h; }
        frameSurfaces.push_back(s);
    }
    if (frameSurfaces.empty()) return;

    int cols = std::max(1, MAX_TEX / frameW);
    if (cols > frameCount) cols = frameCount;
    int rows = (frameCount + cols - 1) / cols;

    surface = SDL_CreateSurface(frameW * cols, frameH * rows, frameSurfaces[0]->format);
    if (!surface) {
        for (auto* f : frameSurfaces) SDL_DestroySurface(f);
        return;
    }
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < (int)frameSurfaces.size(); ++i) {
        int col = i % cols, row = i / cols;
        SDL_Rect dst = {col * frameW, row * frameH, frameW, frameH};
        SDL_SetSurfaceBlendMode(frameSurfaces[i], SDL_BLENDMODE_NONE);
        SDL_BlitSurface(frameSurfaces[i], nullptr, surface, &dst);

        std::string num = std::to_string(startIdx + i);
        if (padDigits > 0)
            num = std::string(std::max(0, padDigits - (int)num.size()), '0') + num;
        frames[prefix + num] = {col * frameW, row * frameH, frameW, frameH};
        SDL_DestroySurface(frameSurfaces[i]);
    }

    mRenderW = targetW;
    mRenderH = targetH;

    AnimCache::Get().Store(cacheKey, {SDL_DuplicateSurface(surface), frames, mRenderW, mRenderH});
}

// ---------------------------------------------------------------------------
// Explicit path list (custom player profiles)
// ---------------------------------------------------------------------------
SpriteSheet::SpriteSheet(const std::vector<std::string>& paths, int targetW, int targetH)
    : surface(nullptr) {
    if (paths.empty()) return;

    const std::string cacheKey = "paths|" + std::to_string(paths.size())
                               + "|" + paths.front()
                               + "|" + std::to_string(targetW) + "x" + std::to_string(targetH);

    if (const auto* cached = AnimCache::Get().Fetch(cacheKey)) {
        surface  = SDL_DuplicateSurface(cached->surface);
        frames   = cached->frames;
        mRenderW = cached->renderW;
        mRenderH = cached->renderH;
        if (surface) SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);
        return;
    }

    std::vector<SDL_Surface*> frameSurfaces;
    int frameW = 0, frameH = 0;
    for (const auto& path : paths) {
        SDL_Surface* s = IMG_Load(path.c_str());
        if (!s) {
            std::print("SpriteSheet: failed to load frame {}\n{}\n", path, SDL_GetError());
            for (auto* f : frameSurfaces) SDL_DestroySurface(f);
            return;
        }
        if (frameSurfaces.empty()) { frameW = s->w; frameH = s->h; }
        frameSurfaces.push_back(s);
    }
    if (frameSurfaces.empty()) return;

    int frameCount = (int)frameSurfaces.size();
    int cols = std::max(1, MAX_TEX / frameW);
    if (cols > frameCount) cols = frameCount;
    int rows = (frameCount + cols - 1) / cols;

    surface = SDL_CreateSurface(frameW * cols, frameH * rows, frameSurfaces[0]->format);
    if (!surface) {
        for (auto* f : frameSurfaces) SDL_DestroySurface(f);
        return;
    }
    SDL_SetSurfaceBlendMode(surface, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < frameCount; ++i) {
        int col = i % cols, row = i / cols;
        SDL_Rect dst = {col * frameW, row * frameH, frameW, frameH};
        SDL_SetSurfaceBlendMode(frameSurfaces[i], SDL_BLENDMODE_NONE);
        SDL_BlitSurface(frameSurfaces[i], nullptr, surface, &dst);
        frames[std::to_string(i)] = {col * frameW, row * frameH, frameW, frameH};
        SDL_DestroySurface(frameSurfaces[i]);
    }
    mRenderW = targetW;
    mRenderH = targetH;

    AnimCache::Get().Store(cacheKey, {SDL_DuplicateSurface(surface), frames, mRenderW, mRenderH});
}

// ---------------------------------------------------------------------------

SpriteSheet::~SpriteSheet() {
    if (texture) SDL_DestroyTexture(texture);
    if (surface) SDL_DestroySurface(surface);
}

void SpriteSheet::LoadCoordinates(const std::string& coordFile) {
    if (coordFile.find(".xml") != std::string::npos)
        LoadXMLFormat(coordFile);
    else
        LoadTextFormat(coordFile);
}

void SpriteSheet::LoadTextFormat(const std::string& coordFile) {
    std::ifstream file(coordFile);
    if (!file.is_open()) {
        std::print("SpriteSheet: cannot open {}\n", coordFile);
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);
        std::string name, eq;
        int x, y, w, h;
        if (iss >> name >> eq >> x >> y >> w >> h)
            frames[name] = {x, y, w, h};
    }
}

void SpriteSheet::LoadXMLFormat(const std::string& coordFile) {
    std::ifstream file(coordFile);
    if (!file.is_open()) {
        std::print("SpriteSheet: cannot open {}\n", coordFile);
        return;
    }
    std::string line;
    while (std::getline(file, line)) {
        if (line.find("<SubTexture") == std::string::npos) continue;

        auto extract = [&](const std::string& attr) -> std::string {
            auto pos = line.find(attr + "=\"");
            if (pos == std::string::npos) return {};
            pos += attr.size() + 2;
            auto end = line.find('"', pos);
            return line.substr(pos, end - pos);
        };

        std::string name = extract("name");
        if (name.size() > 4 && name.substr(name.size() - 4) == ".png")
            name = name.substr(0, name.size() - 4);

        auto xs = extract("x"), ys = extract("y"),
             ws = extract("width"), hs = extract("height");
        if (!name.empty() && !xs.empty())
            frames[name] = {std::stoi(xs), std::stoi(ys), std::stoi(ws), std::stoi(hs)};
    }
}

SDL_Rect SpriteSheet::GetFrame(const std::string& name) const {
    auto it = frames.find(name);
    if (it != frames.end()) return it->second;
    std::print("SpriteSheet: frame not found: {}\n", name);
    return {0, 0, 0, 0};
}

std::vector<SDL_Rect> SpriteSheet::GetAnimation(const std::string& baseName) const {
    std::vector<std::pair<std::string, SDL_Rect>> matches;
    for (const auto& [name, rect] : frames)
        if (name.find(baseName) == 0)
            matches.emplace_back(name, rect);

    std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
        auto trailingNum = [](const std::string& s) {
            int i = (int)s.size() - 1;
            while (i >= 0 && std::isdigit((unsigned char)s[i])) --i;
            auto digits = s.substr(i + 1);
            return digits.empty() ? 0 : std::stoi(digits);
        };
        return trailingNum(a.first) < trailingNum(b.first);
    });

    std::vector<SDL_Rect> out;
    out.reserve(matches.size());
    for (const auto& [name, rect] : matches)
        out.push_back(rect);
    return out;
}
