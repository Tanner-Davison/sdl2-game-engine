#pragma once
#include <SDL3/SDL.h>
#include <string>
#include <array>
#include <fstream>
#include <filesystem>
#include <print>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

// --- Animation slot indices ---
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
        case PlayerAnimSlot::Crouch: return "Crouch";
        case PlayerAnimSlot::Jump:   return "Jump";
        case PlayerAnimSlot::Fall:   return "Fall";
        case PlayerAnimSlot::Slash:  return "Slash";
        case PlayerAnimSlot::Hurt:   return "Hurt";
        case PlayerAnimSlot::Death:  return "Death";
        default:                     return "Unknown";
    }
}

// --- Per-animation hitbox ---
// x/y offset from sprite top-left; w/h are hitbox dimensions.
// w == 0 && h == 0 means "use the default global hitbox".
struct AnimHitbox {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool IsDefault() const { return w == 0 && h == 0; }
};

// --- Player profile ---
// Empty folderPath = missing slot; runtime falls back to Idle.
struct PlayerProfile {
    std::string name     = "Unnamed";
    int         spriteW  = 0;   // render width  in px (0 = use engine default)
    int         spriteH  = 0;   // render height in px (0 = use engine default)

    struct SfxEntry {
        std::string path;
        float       volume      = 1.0f;
        bool        timeStretch = false;
        float       trimStart   = 0.0f;  // 0.0–1.0 fraction of audio to skip at start
        float       trimEnd     = 1.0f;  // 0.0–1.0 fraction of audio to play up to
    };

    struct SlotData {
        std::string folderPath;
        AnimHitbox  hitbox;
        float       fps = 0.0f;
        std::vector<SfxEntry> sfx; // multiple = round-robin on each trigger
    };

    std::array<SlotData, PLAYER_ANIM_SLOT_COUNT> slots;

    SlotData&       Slot(PlayerAnimSlot s)       { return slots[static_cast<int>(s)]; }
    const SlotData& Slot(PlayerAnimSlot s) const { return slots[static_cast<int>(s)]; }

    // HasSlot = has custom sprites. Distinct from HasHitbox/HasFps — a profile
    // can override hitbox/fps without providing sprites (falls back to frost knight).
    bool HasSlot(PlayerAnimSlot s) const {
        return !Slot(s).folderPath.empty();
    }
    bool HasHitbox(PlayerAnimSlot s) const {
        return !Slot(s).hitbox.IsDefault();
    }
    bool HasFps(PlayerAnimSlot s) const {
        return Slot(s).fps > 0.0f;
    }
    bool HasSFX(PlayerAnimSlot s) const {
        return !Slot(s).sfx.empty();
    }
};

// --- Path portability ---
// Paths stored relative to CWD. On save, absolute paths are copied into
// game_assets/char_sprites/<charName>/<slotName>/ and made relative.

inline std::string CharSpriteRelDir(const std::string& charName,
                                    const std::string& slotName) {
    return "game_assets/char_sprites/" + charName + "/" + slotName;
}

inline bool CopySpritePNGs(const fs::path& srcDir, const fs::path& dstDir) {
    std::error_code ec;
    if (!fs::is_directory(srcDir, ec) || ec) return false;
    fs::create_directories(dstDir, ec);
    if (ec) return false;

    bool copied = false;
    for (const auto& entry : fs::directory_iterator(srcDir, ec)) {
        if (ec) break;
        const auto& ext = entry.path().extension().string();
        if (ext == ".png" || ext == ".PNG") {
            fs::path dst = dstDir / entry.path().filename();
            fs::copy_file(entry.path(), dst,
                          fs::copy_options::overwrite_existing, ec);
            if (!ec) copied = true;
        }
    }
    return copied;
}

// Resolve stored path: relative passes through, stale absolute returns "".
inline std::string ResolveFolderPath(const std::string& stored) {
    if (stored.empty()) return "";
    fs::path p(stored);
    std::error_code ec;
    if (p.is_relative()) {
        return stored;
    }
    if (fs::is_directory(p, ec) && !ec)
        return stored;
    return "";
}

// --- Serialization ---

inline bool SavePlayerProfile(const PlayerProfile& p, const std::string& path) {
    json j;
    j["name"]    = p.name;
    j["spriteW"] = p.spriteW;
    j["spriteH"] = p.spriteH;
    j["slots"] = json::array();

    for (int i = 0; i < PLAYER_ANIM_SLOT_COUNT; ++i) {
        const auto& s    = p.slots[i];
        std::string savePath = s.folderPath;

        // Copy absolute-path sprites into a portable relative location
        if (!savePath.empty()) {
            fs::path fp(savePath);
            if (fp.is_absolute()) {
                std::string slotName = PlayerAnimSlotName(static_cast<PlayerAnimSlot>(i));
                std::string relDir   = CharSpriteRelDir(p.name, slotName);
                fs::path    dstDir(relDir);
                std::error_code ec;
                if (fs::is_directory(fp, ec) && !ec && CopySpritePNGs(fp, dstDir)) {
                    savePath = relDir;
                } else {
                    // Keep absolute so Mac still works; WSL/Windows will skip on load
                    std::print("PlayerProfile: could not copy sprites for slot {} — keeping absolute path\n", slotName);
                }
            }
        }

        json slotJson = {
            {"slot",        i},
            {"folderPath",  savePath},
            {"fps",         s.fps},
            {"hitbox", {
                {"x", s.hitbox.x},
                {"y", s.hitbox.y},
                {"w", s.hitbox.w},
                {"h", s.hitbox.h}
            }}
        };
        if (!s.sfx.empty()) {
            json arr = json::array();
            for (const auto& e : s.sfx)
                arr.push_back({{"path", e.path}, {"volume", e.volume}, {"timeStretch", e.timeStretch},
                               {"trimStart", e.trimStart}, {"trimEnd", e.trimEnd}});
            slotJson["sfx"] = std::move(arr);
        }
        j["slots"].push_back(std::move(slotJson));
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
        s.folderPath = ResolveFolderPath(entry.value("folderPath", ""));
        s.fps        = entry.value("fps", 0.0f);
        if (entry.contains("sfx") && entry["sfx"].is_array()) {
            for (const auto& e : entry["sfx"]) {
                PlayerProfile::SfxEntry se;
                se.path        = e.value("path", std::string{});
                se.volume      = e.value("volume", 1.0f);
                se.timeStretch = e.value("timeStretch", false);
                se.trimStart   = e.value("trimStart", 0.0f);
                se.trimEnd     = e.value("trimEnd", 1.0f);
                if (!se.path.empty()) s.sfx.push_back(std::move(se));
            }
        } else if (entry.contains("sfxPaths") && entry["sfxPaths"].is_array()) {
            float vol = entry.value("sfxVolume", 1.0f);
            bool  ts  = entry.value("sfxTimeStretch", false);
            for (const auto& p : entry["sfxPaths"])
                if (p.is_string() && !p.get<std::string>().empty())
                    s.sfx.push_back({p.get<std::string>(), vol, ts});
        } else if (entry.contains("sfxPath") && entry["sfxPath"].is_string()) {
            auto legacy = entry["sfxPath"].get<std::string>();
            if (!legacy.empty())
                s.sfx.push_back({std::move(legacy),
                                 entry.value("sfxVolume", 1.0f),
                                 entry.value("sfxTimeStretch", false)});
        }
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

// --- Roster helpers ---

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
