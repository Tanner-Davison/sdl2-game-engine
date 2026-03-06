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
    Crouch = 2,  // crouch / duck — triggered by Ctrl in-game (was "Run", value unchanged)
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
        case PlayerAnimSlot::Idle:   return "Idle";
        case PlayerAnimSlot::Walk:   return "Walk";
        case PlayerAnimSlot::Crouch: return "Crouch";  // Ctrl in-game
        case PlayerAnimSlot::Jump:   return "Jump";
        case PlayerAnimSlot::Fall:   return "Fall";
        case PlayerAnimSlot::Slash:  return "Slash";
        case PlayerAnimSlot::Hurt:   return "Hurt";
        case PlayerAnimSlot::Death:  return "Death";
        default:                     return "Unknown";
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

    // HasSlot: true if this slot has custom sprite frames to load.
    // Distinct from HasHitbox/HasFps — a profile can override hitbox and fps
    // without providing custom sprites (falls back to frost knight visuals).
    bool HasSlot(PlayerAnimSlot s) const {
        return !Slot(s).folderPath.empty();
    }
    bool HasHitbox(PlayerAnimSlot s) const {
        return !Slot(s).hitbox.IsDefault();
    }
    bool HasFps(PlayerAnimSlot s) const {
        return Slot(s).fps > 0.0f;
    }
};

// ── Serialization ─────────────────────────────────────────────────────────────

// ── Path portability helpers ─────────────────────────────────────────────────
//
// Sprite folders are stored as paths RELATIVE to the project root (CWD at
// runtime). On save, any absolute path is copied into
//   game_assets/char_sprites/<charName>/<slotName>/
// and the stored path becomes that relative path.  This makes profiles
// portable across Mac, WSL, and Windows with no manual path editing.

inline std::string CharSpriteRelDir(const std::string& charName,
                                    const std::string& slotName) {
    // Forward slashes — works on all platforms as a relative path.
    return "game_assets/char_sprites/" + charName + "/" + slotName;
}

// Copy every PNG from srcDir into dstDir, creating dstDir if needed.
// Returns true if at least one PNG was copied successfully.
inline bool CopySpritePNGs(const fs::path& srcDir, const fs::path& dstDir) {
    std::error_code ec;
    if (!fs::is_directory(srcDir, ec) || ec) return false;
    fs::create_directories(dstDir, ec);
    if (ec) return false;

    bool copied = false;
    for (const auto& entry : fs::directory_iterator(srcDir, ec)) {
        if (ec) break;
        const auto& ext = entry.path().extension().string();
        // Copy .png and .PNG
        if (ext == ".png" || ext == ".PNG") {
            fs::path dst = dstDir / entry.path().filename();
            fs::copy_file(entry.path(), dst,
                          fs::copy_options::overwrite_existing, ec);
            if (!ec) copied = true;
        }
    }
    return copied;
}

// Resolve a stored folderPath to a usable absolute path on the current machine:
//   - Empty  → return ""
//   - Relative → resolve from CWD (the project root at runtime)
//   - Absolute + exists + is_directory → use as-is
//   - Absolute + missing/not-dir → return "" (stale cross-machine path)
inline std::string ResolveFolderPath(const std::string& stored) {
    if (stored.empty()) return "";
    fs::path p(stored);
    std::error_code ec;
    if (p.is_relative()) {
        // Relative paths are always valid — rebuildPreview will catch missing dirs
        return stored;
    }
    // Absolute path: only usable if it's actually a directory on THIS machine
    if (fs::is_directory(p, ec) && !ec)
        return stored;
    return ""; // stale absolute path from another machine
}

inline bool SavePlayerProfile(const PlayerProfile& p, const std::string& path) {
    json j;
    j["name"]    = p.name;
    j["spriteW"] = p.spriteW;
    j["spriteH"] = p.spriteH;
    j["slots"] = json::array();

    for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
        const auto& s    = p.slots[i];
        std::string savePath = s.folderPath;

        // If the stored path is absolute, copy sprites to a portable relative
        // location inside game_assets and switch to that relative path.
        if (!savePath.empty()) {
            fs::path fp(savePath);
            if (fp.is_absolute()) {
                std::string slotName = PlayerAnimSlotName(static_cast<PlayerAnimSlot>(i));
                std::string relDir   = CharSpriteRelDir(p.name, slotName);
                fs::path    dstDir(relDir);
                std::error_code ec;
                if (fs::is_directory(fp, ec) && !ec && CopySpritePNGs(fp, dstDir)) {
                    savePath = relDir; // store relative from now on
                } else {
                    // Source didn't copy — keep absolute so Mac still works,
                    // but WSL/Windows will gracefully skip it on load.
                    std::print("PlayerProfile: could not copy sprites for slot {} — keeping absolute path\n", slotName);
                }
            }
        }

        j["slots"].push_back({
            {"slot",        i},
            {"folderPath",  savePath},
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
        auto& s      = out.slots[idx];
        // ResolveFolderPath silently drops stale absolute paths from other machines
        s.folderPath = ResolveFolderPath(entry.value("folderPath", ""));
        s.fps        = entry.value("fps", 0.0f);
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
