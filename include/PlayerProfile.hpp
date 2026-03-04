#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <array>
#include <fstream>
#include <filesystem>
#include <print>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ── Animation slot indices ────────────────────────────────────────────────────
// Maps to the 8 named animation states the player creator exposes.
enum class PlayerAnimSlot : int {
    Idle   = 0,
    Walk   = 1,
    Run    = 2,
    Jump   = 3,
    Fall   = 4,
    Slash  = 5,
    Hurt   = 6,
    Death  = 7,
    COUNT  = 8
};

inline constexpr int PLAYER_ANIM_SLOT_COUNT = static_cast<int>(PlayerAnimSlot::COUNT);

inline const char* PlayerAnimSlotName(PlayerAnimSlot s) {
    switch (s) {
        case PlayerAnimSlot::Idle:  return "Idle";
        case PlayerAnimSlot::Walk:  return "Walk";
        case PlayerAnimSlot::Run:   return "Run";
        case PlayerAnimSlot::Jump:  return "Jump";
        case PlayerAnimSlot::Fall:  return "Fall";
        case PlayerAnimSlot::Slash: return "Slash";
        case PlayerAnimSlot::Hurt:  return "Hurt";
        case PlayerAnimSlot::Death: return "Death";
        default:                    return "Unknown";
    }
}

// ── Per-animation hitbox ──────────────────────────────────────────────────────
// x/y are offsets from the sprite's top-left; w/h are the hitbox dimensions.
// If w == 0 and h == 0, the system falls back to the default global hitbox.
struct AnimHitbox {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool IsDefault() const { return w == 0 && h == 0; }
};

// ── Player profile ────────────────────────────────────────────────────────────
// Describes a fully configured custom character: name, per-slot sprite folder
// paths, and per-slot hitbox overrides.  Any slot with an empty folderPath is
// treated as missing — the runtime should gracefully skip or fall back to Idle.
struct PlayerProfile {
    std::string name     = "Unnamed";
    int         spriteW  = 0;   // render width  in px (0 = use engine default)
    int         spriteH  = 0;   // render height in px (0 = use engine default)

    struct SlotData {
        std::string folderPath; // absolute or relative path to a directory of PNGs
        AnimHitbox  hitbox;     // custom hitbox for this animation (zeros = use default)
        float       fps = 0.0f; // playback speed in frames/sec (0 = use engine default)
    };

    std::array<SlotData, PLAYER_ANIM_SLOT_COUNT> slots;

    // Convenience accessors
    SlotData&       Slot(PlayerAnimSlot s)       { return slots[static_cast<int>(s)]; }
    const SlotData& Slot(PlayerAnimSlot s) const { return slots[static_cast<int>(s)]; }

    bool HasSlot(PlayerAnimSlot s) const {
        return !Slot(s).folderPath.empty();
    }
};

// ── Serialization ─────────────────────────────────────────────────────────────

inline bool SavePlayerProfile(const PlayerProfile& p, const std::string& path) {
    json j;
    j["name"]    = p.name;
    j["spriteW"] = p.spriteW;
    j["spriteH"] = p.spriteH;
    j["slots"] = json::array();
    for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
        const auto& s = p.slots[i];
        j["slots"].push_back({
            {"slot",        i},
            {"folderPath",  s.folderPath},
            {"fps",         s.fps},
            {"hitbox", {
                {"x", s.hitbox.x},
                {"y", s.hitbox.y},
                {"w", s.hitbox.w},
                {"h", s.hitbox.h}
            }}
        });
    }
    std::ofstream f(path);
    if (!f.is_open()) {
        std::print("PlayerProfile: failed to save {}\n", path);
        return false;
    }
    f << j.dump(4);
    std::print("PlayerProfile saved: {}\n", path);
    return true;
}

inline bool LoadPlayerProfile(const std::string& path, PlayerProfile& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::print("PlayerProfile: failed to open {}\n", path);
        return false;
    }
    json j;
    try { f >> j; }
    catch (const json::parse_error& e) {
        std::print("PlayerProfile JSON parse error in {}: {}\n", path, e.what());
        return false;
    }
    out.name    = j.value("name",    "Unnamed");
    out.spriteW = j.value("spriteW", 0);
    out.spriteH = j.value("spriteH", 0);
    for (const auto& entry : j.value("slots", json::array())) {
        int idx = entry.value("slot", -1);
        if (idx < 0 || idx >= PLAYER_ANIM_SLOT_COUNT) continue;
        auto& s       = out.slots[idx];
        s.folderPath  = entry.value("folderPath", "");
        s.fps         = entry.value("fps", 0.0f);
        if (entry.contains("hitbox")) {
            s.hitbox.x = entry["hitbox"].value("x", 0);
            s.hitbox.y = entry["hitbox"].value("y", 0);
            s.hitbox.w = entry["hitbox"].value("w", 0);
            s.hitbox.h = entry["hitbox"].value("h", 0);
        }
    }
    std::print("PlayerProfile loaded: {}\n", out.name);
    return true;
}

// ── Roster helpers ────────────────────────────────────────────────────────────

inline std::vector<fs::path> ScanPlayerProfiles() {
    std::vector<fs::path> result;
    if (!fs::exists("players")) return result;
    for (const auto& e : fs::directory_iterator("players"))
        if (e.path().extension() == ".json")
            result.push_back(e.path());
    std::sort(result.begin(), result.end());
    return result;
}

inline std::string PlayerProfilePath(const std::string& name) {
    return "players/" + name + ".json";
}
