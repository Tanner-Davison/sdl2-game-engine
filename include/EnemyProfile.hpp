#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <print>
#include <string>
#include <vector>

using json = nlohmann::json;
namespace fs = std::filesystem;

// ── Animation slot indices ────────────────────────────────────────────────────
// Maps to the named animation states the enemy creator exposes.
// Enemies have fewer states than players -- no crouch, slash, etc.
// "Move" covers walking, flying, swimming depending on the enemy type.
enum class EnemyAnimSlot : int {
    Idle   = 0,   // standing / default pose
    Move   = 1,   // walking, flying, swimming -- the main locomotion anim
    Attack = 2,   // enemy attacking the player (bite, slash, projectile)
    Hurt   = 3,   // took damage from the player (hit reaction)
    Dead   = 4,   // death animation
    COUNT  = 5
};

inline constexpr int ENEMY_ANIM_SLOT_COUNT = static_cast<int>(EnemyAnimSlot::COUNT);

inline const char* EnemyAnimSlotName(EnemyAnimSlot s) {
    switch (s) {
        case EnemyAnimSlot::Idle:   return "Idle";
        case EnemyAnimSlot::Move:   return "Move";
        case EnemyAnimSlot::Attack: return "Attack";
        case EnemyAnimSlot::Hurt:   return "Hurt";
        case EnemyAnimSlot::Dead:   return "Dead";
        default:                    return "Unknown";
    }
}

// ── Per-animation hitbox ──────────────────────────────────────────────────────
// x/y are offsets from the sprite's top-left; w/h are the hitbox dimensions.
// If w == 0 and h == 0, the system falls back to the default global hitbox.
struct EnemyAnimHitbox {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool IsDefault() const { return w == 0 && h == 0; }
};

// ── Enemy profile ─────────────────────────────────────────────────────────────
// Describes a fully configured enemy type: name, per-slot sprite folder paths,
// per-slot hitbox overrides, default speed, health, and behavior flags.
// Any slot with an empty folderPath is treated as missing -- the runtime should
// gracefully skip or fall back to Idle.
struct EnemyProfile {
    std::string name     = "Unnamed";
    int         spriteW  = 40;    // render width in px
    int         spriteH  = 40;    // render height in px
    float       speed    = 120.0f; // default movement speed (px/s)
    float       health   = 30.0f;  // max HP (default = slime HP)

    struct SfxEntry {
        std::string path;
        float       volume      = 1.0f;
        bool        timeStretch = false;
    };

    struct SlotData {
        std::string    folderPath;
        EnemyAnimHitbox hitbox;
        float          fps = 0.0f;
        std::vector<SfxEntry> sfx; // per-file SFX — multiple = round-robin on each trigger
    };

    std::array<SlotData, ENEMY_ANIM_SLOT_COUNT> slots;

    // Convenience accessors
    SlotData&       Slot(EnemyAnimSlot s)       { return slots[static_cast<int>(s)]; }
    const SlotData& Slot(EnemyAnimSlot s) const { return slots[static_cast<int>(s)]; }

    bool HasSlot(EnemyAnimSlot s) const { return !Slot(s).folderPath.empty(); }
    bool HasHitbox(EnemyAnimSlot s) const { return !Slot(s).hitbox.IsDefault(); }
    bool HasFps(EnemyAnimSlot s) const { return Slot(s).fps > 0.0f; }
    bool HasSFX(EnemyAnimSlot s) const { return !Slot(s).sfx.empty(); }
};

// ── Path portability helpers ─────────────────────────────────────────────────
// Sprite folders are stored as paths RELATIVE to the project root (CWD at
// runtime). On save, any absolute path is copied into
//   game_assets/enemy_sprites/<enemyName>/<slotName>/
// and the stored path becomes that relative path.

inline std::string EnemySpriteRelDir(const std::string& enemyName,
                                     const std::string& slotName) {
    return "game_assets/enemy_sprites/" + enemyName + "/" + slotName;
}

// Copy every PNG from srcDir into dstDir, creating dstDir if needed.
// Returns true if at least one PNG was copied successfully.
// Also handles the case where srcDir points to a single PNG file
// (for enemies that use single-image sprites rather than frame sequences).
inline bool CopyEnemySpritePNGs(const fs::path& srcDir, const fs::path& dstDir) {
    std::error_code ec;

    // Handle single-file case: if srcDir is actually a file, copy just that file
    if (fs::is_regular_file(srcDir, ec) && !ec) {
        auto ext = srcDir.extension().string();
        if (ext == ".png" || ext == ".PNG") {
            fs::create_directories(dstDir, ec);
            if (ec) return false;
            fs::path dst = dstDir / srcDir.filename();
            fs::copy_file(srcDir, dst, fs::copy_options::overwrite_existing, ec);
            return !ec;
        }
        return false;
    }

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

// Resolve a stored folderPath to a usable path on the current machine.
// Same logic as the player version.
inline std::string ResolveEnemyFolderPath(const std::string& stored) {
    if (stored.empty()) return "";
    fs::path p(stored);
    std::error_code ec;
    if (p.is_relative()) return stored;
    // Absolute path: only usable if it's actually a directory or file on THIS machine
    if ((fs::is_directory(p, ec) || fs::is_regular_file(p, ec)) && !ec)
        return stored;
    return "";
}

// ── Serialization ─────────────────────────────────────────────────────────────

inline bool SaveEnemyProfile(const EnemyProfile& p, const std::string& path) {
    json j;
    j["name"]    = p.name;
    j["spriteW"] = p.spriteW;
    j["spriteH"] = p.spriteH;
    j["speed"]   = p.speed;
    j["health"]  = p.health;
    j["slots"]   = json::array();

    for (int i = 0; i < ENEMY_ANIM_SLOT_COUNT; ++i) {
        const auto& s = p.slots[i];
        std::string savePath = s.folderPath;

        // If the stored path is absolute, copy sprites to a portable relative
        // location inside game_assets and switch to that relative path.
        if (!savePath.empty()) {
            fs::path fp(savePath);
            if (fp.is_absolute()) {
                std::string slotName = EnemyAnimSlotName(static_cast<EnemyAnimSlot>(i));
                std::string relDir   = EnemySpriteRelDir(p.name, slotName);
                fs::path    dstDir(relDir);
                if (CopyEnemySpritePNGs(fp, dstDir)) {
                    savePath = relDir;
                } else {
                    std::print("EnemyProfile: could not copy sprites for slot {} "
                               "-- keeping absolute path\n", slotName);
                }
            }
        }

        json slotJson = {
            {"slot",       i},
            {"folderPath", savePath},
            {"fps",        s.fps},
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
                arr.push_back({{"path", e.path}, {"volume", e.volume}, {"timeStretch", e.timeStretch}});
            slotJson["sfx"] = std::move(arr);
        }
        j["slots"].push_back(std::move(slotJson));
    }

    std::ofstream f(path);
    if (!f.is_open()) {
        std::print("EnemyProfile: failed to save {}\n", path);
        return false;
    }
    f << j.dump(4);
    std::print("EnemyProfile saved: {}\n", path);
    return true;
}

inline bool LoadEnemyProfile(const std::string& path, EnemyProfile& out) {
    std::ifstream f(path);
    if (!f.is_open()) {
        std::print("EnemyProfile: failed to open {}\n", path);
        return false;
    }
    json j;
    try { f >> j; }
    catch (const json::parse_error& e) {
        std::print("EnemyProfile JSON parse error in {}: {}\n", path, e.what());
        return false;
    }
    out.name    = j.value("name",    "Unnamed");
    out.spriteW = j.value("spriteW", 40);
    out.spriteH = j.value("spriteH", 40);
    out.speed   = j.value("speed",   120.0f);
    out.health  = j.value("health",  30.0f);

    for (const auto& entry : j.value("slots", json::array())) {
        int idx = entry.value("slot", -1);
        if (idx < 0 || idx >= ENEMY_ANIM_SLOT_COUNT) continue;
        auto& s = out.slots[idx];
        s.folderPath     = ResolveEnemyFolderPath(entry.value("folderPath", ""));
        s.fps            = entry.value("fps", 0.0f);
        if (entry.contains("sfx") && entry["sfx"].is_array()) {
            for (const auto& e : entry["sfx"]) {
                EnemyProfile::SfxEntry se;
                se.path        = e.value("path", std::string{});
                se.volume      = e.value("volume", 1.0f);
                se.timeStretch = e.value("timeStretch", false);
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
    std::print("EnemyProfile loaded: {}\n", out.name);
    return true;
}

// ── Roster helpers ────────────────────────────────────────────────────────────

inline std::vector<fs::path> ScanEnemyProfiles() {
    std::vector<fs::path> result;
    if (!fs::exists("enemies")) return result;
    for (const auto& e : fs::directory_iterator("enemies"))
        if (e.path().extension() == ".json")
            result.push_back(e.path());
    std::sort(result.begin(), result.end());
    return result;
}

inline std::string EnemyProfilePath(const std::string& name) {
    return "enemies/" + name + ".json";
}

// ── Preview helper ──────────────────────────────────────────────────────────
// Returns the path to the first PNG found in the Idle slot (or Move slot as
// fallback) for use as a thumbnail/preview in the editor's enemy picker.
// Returns empty string if no sprite is assigned.
inline std::string EnemyPreviewImagePath(const EnemyProfile& p) {
    // Try Idle first, then Move
    for (auto slot : {EnemyAnimSlot::Idle, EnemyAnimSlot::Move}) {
        const auto& fp = p.Slot(slot).folderPath;
        if (fp.empty()) continue;

        fs::path fpath(fp);
        std::error_code ec;

        // Single file
        if (fs::is_regular_file(fpath, ec) && !ec) {
            auto ext = fpath.extension().string();
            if (ext == ".png" || ext == ".PNG") return fp;
            continue;
        }

        // Directory -- find first PNG alphabetically
        if (fs::is_directory(fpath, ec) && !ec) {
            std::vector<fs::path> pngs;
            for (const auto& entry : fs::directory_iterator(fpath, ec)) {
                if (ec) break;
                auto ext = entry.path().extension().string();
                if (ext == ".png" || ext == ".PNG")
                    pngs.push_back(entry.path());
            }
            if (!pngs.empty()) {
                std::sort(pngs.begin(), pngs.end());
                return pngs[0].string();
            }
        }
    }
    return "";
}
