#pragma once
// AnimatedTile.hpp
// ────────────────────────────────────────────────────────────────────────────
// Data model, serializer, and runtime state for animated tiles.
//
// A "tile animation" is defined by:
//   - A name
//   - An ordered list of PNG frame paths
//   - A frames-per-second playback rate
//
// At runtime every placed tile that references an animated tile manifest
// (.json inside animated_tiles/) gets a runtime AnimatedTileState — a simple
// frame counter that the GameScene and LevelEditorScene tick each Update().
//
// Storage layout on disk:
//   game_assets/tiles/animated_tiles/<Name>.json
//
// TileSpawn::imagePath stores the manifest path.
// The editor and game detect it via the IsAnimatedTile() helper below.
// ────────────────────────────────────────────────────────────────────────────

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

// ── Manifest directory ────────────────────────────────────────────────────────
inline constexpr const char* ANIMATED_TILE_DIR = "game_assets/tiles/animated_tiles";

// ── Data model ────────────────────────────────────────────────────────────────
struct AnimatedTileDef {
    std::string              name;
    std::vector<std::string> framePaths; // ordered list of absolute/relative PNG paths
    float                    fps  = 8.0f;
};

// ── Runtime state (one per placed animated tile instance) ─────────────────────
struct AnimatedTileState {
    float timer      = 0.0f;
    int   frameIndex = 0;
};

// ── Helpers ───────────────────────────────────────────────────────────────────

// Returns true if a tile's imagePath points to an animated tile manifest.
inline bool IsAnimatedTile(const std::string& imagePath) {
    if (imagePath.size() < 5) return false;
    // Must end in .json and live under the animated_tiles dir
    return imagePath.substr(imagePath.size() - 5) == ".json";
}

// Canonical manifest path for a given animation name
inline std::string AnimatedTilePath(const std::string& name) {
    return std::string(ANIMATED_TILE_DIR) + "/" + name + ".json";
}

// ── Serialization ─────────────────────────────────────────────────────────────

inline bool SaveAnimatedTileDef(const AnimatedTileDef& def, const std::string& path) {
    json j;
    j["name"]  = def.name;
    j["fps"]   = def.fps;
    j["frames"] = def.framePaths;

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
    for (const auto& fp : j.value("frames", json::array()))
        out.framePaths.push_back(fp.get<std::string>());
    return true;
}

// Scan animated_tiles/ and return all manifest paths
inline std::vector<fs::path> ScanAnimatedTiles() {
    std::vector<fs::path> result;
    if (!fs::exists(ANIMATED_TILE_DIR)) return result;
    for (const auto& e : fs::directory_iterator(ANIMATED_TILE_DIR))
        if (e.path().extension() == ".json")
            result.push_back(e.path());
    std::sort(result.begin(), result.end());
    return result;
}

// ── Runtime surface cache ─────────────────────────────────────────────────────
// Given a loaded AnimatedTileDef, builds a vector of SDL_Surface* (one per frame).
// Caller owns all surfaces and must SDL_DestroySurface each one.
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

// ── Runtime tick ──────────────────────────────────────────────────────────────
inline void TickAnimatedTile(AnimatedTileState& state, float fps, int frameCount, float dt) {
    if (frameCount <= 0) return;
    state.timer += dt;
    float frameDur = (fps > 0.0f) ? 1.0f / fps : 0.125f;
    while (state.timer >= frameDur) {
        state.timer -= frameDur;
        state.frameIndex = (state.frameIndex + 1) % frameCount;
    }
}
