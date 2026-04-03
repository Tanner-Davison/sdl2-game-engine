#pragma once
#include <algorithm>

// =============================================================================
//  GlobalSettings
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
