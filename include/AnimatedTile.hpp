#pragma once
// AnimatedTile.hpp — Data model, serializer, and runtime state for animated tiles.
// Manifests live at game_assets/tiles/animated_tiles/<Name>.json.

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <print>
#include <string>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

inline constexpr const char* ANIMATED_TILE_DIR = "game_assets/tiles/animated_tiles";

struct AnimatedTileDef {
    std::string              name;
    std::vector<std::string> framePaths; // ordered list of absolute/relative PNG paths
    float                    fps  = 8.0f;
};

struct AnimatedTileState {
    float timer      = 0.0f;
    int   frameIndex = 0;
};

inline bool IsAnimatedTile(const std::string& imagePath) {
    if (imagePath.size() < 5) return false;
    return imagePath.substr(imagePath.size() - 5) == ".json";
}

inline std::string AnimatedTilePath(const std::string& name) {
    return std::string(ANIMATED_TILE_DIR) + "/" + name + ".json";
}

inline bool SaveAnimatedTileDef(const AnimatedTileDef& def, const std::string& path) {
    json j;
    j["name"]  = def.name;
    j["fps"]   = def.fps;

    // Save paths relative to cwd for cross-machine portability
    json frames = json::array();
    fs::path cwd = fs::current_path();
    for (const auto& fp : def.framePaths) {
        fs::path p(fp);
        std::string stored = fp;
        if (p.is_absolute()) {
            std::error_code ec;
            auto rel = fs::relative(p, cwd, ec);
            if (!ec && !rel.empty()) stored = rel.string();
        }
        frames.push_back(stored);
    }
    j["frames"] = frames;

    if (!fs::exists(ANIMATED_TILE_DIR))
        fs::create_directories(ANIMATED_TILE_DIR);

    std::ofstream f(path);
    if (!f.is_open()) {
        std::print("AnimatedTile: failed to save {}\n", path);
        return false;
    }
    f << j.dump(4);
    std::print("AnimatedTile saved: {}\n", path);
    return true;
}

inline bool LoadAnimatedTileDef(const std::string& path, AnimatedTileDef& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::print("AnimatedTile: failed to open {}\n", path);
        return false;
    }
    json j;
    try { f >> j; }
    catch (const json::parse_error& e) {
        std::print("AnimatedTile JSON parse error in {}: {}\n", path, e.what());
        return false;
    }
    out.name  = j.value("name", "Unnamed");
    out.fps   = j.value("fps", 8.0f);
    out.framePaths.clear();
    for (const auto& fp : j.value("frames", json::array())) {
        std::string p = fp.get<std::string>();
        std::error_code ec;
        if (!fs::exists(p, ec) || ec) {
            std::print("AnimatedTile: skipping missing frame: {}\n", p);
            continue;
        }
        out.framePaths.push_back(p);
    }
    // Sort numerically by trailing number ("2.png" before "10.png")
    std::sort(out.framePaths.begin(), out.framePaths.end(),
              [](const std::string& a, const std::string& b) {
                  auto trailingNum = [](const std::string& s) -> int {
                      fs::path p(s);
                      std::string stem = p.stem().string();
                      int i = (int)stem.size() - 1;
                      while (i >= 0 && std::isdigit((unsigned char)stem[i])) --i;
                      std::string digits = stem.substr(i + 1);
                      return digits.empty() ? 0 : std::stoi(digits);
                  };
                  return trailingNum(a) < trailingNum(b);
              });
    if (out.framePaths.empty()) {
        std::print("AnimatedTile: no valid frames found in {}\n", path);
        return false;
    }
    return true;
}

inline std::vector<fs::path> ScanAnimatedTiles() {
    std::vector<fs::path> result;
    if (!fs::exists(ANIMATED_TILE_DIR)) return result;
    for (const auto& e : fs::directory_iterator(ANIMATED_TILE_DIR))
        if (e.path().extension() == ".json")
            result.push_back(e.path());
    std::sort(result.begin(), result.end());
    return result;
}

/// Builds a vector of SDL_Surface* (one per frame). Caller owns all surfaces.
inline std::vector<SDL_Surface*> LoadAnimatedTileFrames(const AnimatedTileDef& def) {
    std::vector<SDL_Surface*> result;
    result.reserve(def.framePaths.size());
    for (const auto& p : def.framePaths) {
        SDL_Surface* raw = IMG_Load(p.c_str());
        if (!raw) { result.push_back(nullptr); continue; }
        SDL_Surface* conv = SDL_ConvertSurface(raw, SDL_PIXELFORMAT_ARGB8888);
        SDL_DestroySurface(raw);
        SDL_SetSurfaceBlendMode(conv, SDL_BLENDMODE_BLEND);
        result.push_back(conv);
    }
    return result;
}

inline void TickAnimatedTile(AnimatedTileState& state, float fps, int frameCount, float dt) {
    if (frameCount <= 0) return;
    state.timer += dt;
    float frameDur = (fps > 0.0f) ? 1.0f / fps : 0.125f;
    while (state.timer >= frameDur) {
        state.timer -= frameDur;
        state.frameIndex = (state.frameIndex + 1) % frameCount;
    }
}
