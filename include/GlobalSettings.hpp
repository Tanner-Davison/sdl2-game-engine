#pragma once
#include <algorithm>
#include <filesystem>
#include <string>

// =============================================================================
//  Path utilities
//
//  toRelPath() converts an absolute path to a relative one by stripping the
//  current working directory prefix. This ensures paths saved in JSON files
//  (player profiles, enemy profiles, level data) are portable across machines
//  and operating systems regardless of where the project lives on disk.
// =============================================================================
inline std::string toRelPath(const std::string& absPath) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path rel = fs::relative(fs::path(absPath), fs::current_path(ec), ec);
    if (!ec && !rel.empty() && rel.native().find("..") == std::string::npos)
        return rel.generic_string(); // always forward slashes
    return absPath; // fallback: return as-is if outside cwd
}

// =============================================================================
//
//  Lightweight POD singleton that carries runtime-configurable game settings.
//  Persisted to / loaded from forge2d_settings.json by TitleScene.
//
//  Access anywhere via: GlobalSettings::Get()
// =============================================================================
struct GlobalSettings {

    // ---- Blood & Gore -------------------------------------------------------
    bool  bloodEnabled   = true;   // master switch; false = no blood at all
    float bloodIntensity = 1.0f;   // multiplier in [0.1, 10.0]; 1.0 = normal

    // ---- Singleton ----------------------------------------------------------
    static GlobalSettings& Get() {
        static GlobalSettings s;
        return s;
    }

    // ---- Slider mapping -----------------------------------------------------
    //  Maps intensity [0.1, 10.0] -> slider position [0.0, 1.0].
    //  Piecewise linear so "Normal" (1.0) sits exactly at the 50% mark.
    //    [0.1 ...  1.0]  ->  [0.00 ... 0.50]   (left half  -- subtle to normal)
    //    [1.0 ... 10.0]  ->  [0.50 ... 1.00]   (right half -- normal to INSANE)
    float SliderT() const {
        if (bloodIntensity <= 1.0f)
            return (bloodIntensity - 0.1f) / 1.8f;
        return 0.5f + (bloodIntensity - 1.0f) / 18.0f;
    }

    static float SliderTToIntensity(float t) {
        t = std::clamp(t, 0.f, 1.f);
        if (t <= 0.5f) return 0.1f + t * 1.8f;
        return 1.0f + (t - 0.5f) * 18.0f;   // right half now reaches 10.0
    }

    // Human-readable label for the current intensity level.
    const char* IntensityLabel() const {
        if (bloodIntensity < 0.30f) return "Very Low";
        if (bloodIntensity < 0.75f) return "Low";
        if (bloodIntensity < 1.30f) return "Normal";
        if (bloodIntensity < 3.50f) return "High";
        if (bloodIntensity < 6.50f) return "Very High";
        if (bloodIntensity < 8.50f) return "Extreme";
        return "INSANE!";
    }

  private:
    GlobalSettings() = default;
};
