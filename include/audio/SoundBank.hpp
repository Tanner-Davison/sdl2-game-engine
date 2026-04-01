// audio/SoundBank.hpp -- Named sound effect resource manager (SDL3_mixer).
// Owns MIX_Audio* resources keyed by string ID. Supports fire-and-forget,
// duration-matched, and sequential playback modes.
#pragma once

#include <SDL3_mixer/SDL_mixer.h>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace audio {

class SoundBank {
  public:
    SoundBank() = default;
    ~SoundBank();

    SoundBank(SoundBank&& other) noexcept;
    SoundBank& operator=(SoundBank&& other) noexcept;
    SoundBank(const SoundBank&)            = delete;
    SoundBank& operator=(const SoundBank&) = delete;

    /// Bind to a mixer. Must be called before Load/Play.
    void SetMixer(MIX_Mixer* mixer);

    /// Load a WAV/OGG/MP3, pre-decode into RAM, register under @p id.
    /// If @p id already exists the old audio is freed.
    bool Load(const std::string& id, const std::string& path);

    /// Load a trimmed portion of the audio (trimStart/trimEnd are 0–1 ratios).
    /// Uses ffmpeg to extract the sub-clip if trim is non-default.
    bool LoadTrimmed(const std::string& id, const std::string& path,
                     float trimStart, float trimEnd);

    /// Free a single SFX by id. No-op if not found.
    void Unload(const std::string& id);

    void UnloadAll();

    /// Fire-and-forget playback at natural speed.
    bool Play(std::string_view id);

    /// Fire-and-forget with per-sound gain. Sounds overlap freely —
    /// each call creates an independent playback that finishes on its own.
    bool PlayOverlap(std::string_view id, float gain = 1.0f);

    /// Destroy finished overlap tracks. Call once per frame.
    void PruneOverlaps();

    /// Play SFX adjusted to fit @p targetSec by changing frequency ratio.
    /// If targetSec <= 0 and !loop, plays on the one-shot track instead.
    /// @param gain  Per-sound volume multiplier (0.0 .. 1.0).
    bool PlayTimed(std::string_view id, float targetSec, bool loop = false,
                   float gain = 1.0f);

    /// Play SFX sequentially — always cuts the previous sound and starts
    /// the new one. Callers advance their round-robin index when true is returned.
    bool PlayOneShotSeq(std::string_view id, float gain = 1.0f);

    /// Stop the managed SFX track (ends looping animation SFX).
    void StopManaged();

    void StopOneShot();

    /// Fade out the managed (looping/time-stretched) track over @p ms.
    void FadeOutManaged(int ms);

    void FadeOutSeq(int ms);

    /// Stop sequential track and clear its busy state.
    void StopSeq();

    void StopAll();

    /// Play once on the dedicated preview track (non-looping, editor use).
    /// @param startSec  Seek to this position (in seconds) after starting.
    bool PlayPreview(std::string_view id, float gain = 1.0f, float startSec = 0.0f);

    /// Adjust preview track gain in real-time (for trim muting).
    void SetPreviewGain(float gain);

    void StopPreview();

    [[nodiscard]] bool IsPreviewPlaying() const;

    [[nodiscard]] bool Has(std::string_view id) const;
    [[nodiscard]] std::size_t Count() const { return mAudios.size(); }

    /// Returns 0 if not found or duration unknown.
    [[nodiscard]] float GetDuration(std::string_view id) const;

    /// Set the master mixer gain (0.0 = silent, 1.0 = unity).
    void SetVolume(float v);
    [[nodiscard]] float GetVolume() const { return mVolume; }

  private:
    struct StringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view sv) const {
            return std::hash<std::string_view>{}(sv);
        }
        std::size_t operator()(const std::string& s) const {
            return std::hash<std::string_view>{}(s);
        }
    };

    MIX_Mixer* mMixer = nullptr;

    // Looping / time-stretched SFX — stopped by StopManaged() on anim transition.
    MIX_Track* mSfxTrack = nullptr;

    // One-shot SFX — NOT stopped by StopManaged(), finishes naturally.
    MIX_Track* mOneShotTrack = nullptr;

    // Sequential one-shot for multi-file slots. Busy state tracked via timing.
    MIX_Track* mSeqTrack      = nullptr;
    Uint64     mSeqStartMs    = 0;
    Uint64     mSeqDurationMs = 0;

    MIX_Track* mPreviewTrack  = nullptr;

    std::unordered_map<std::string, MIX_Audio*, StringHash, std::equal_to<>> mAudios;
    float mVolume = 1.0f;

    // Tracks created by PlayOverlap — cleaned up once finished.
    std::vector<MIX_Track*> mOverlapTracks;
};

} // namespace audio
