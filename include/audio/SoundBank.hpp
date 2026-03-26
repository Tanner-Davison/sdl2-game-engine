// audio/SoundBank.hpp -- Named sound effect resource manager (SDL3_mixer).
//
// Owns MIX_Audio* resources keyed by string ID. Supports both
// fire-and-forget playback (Play) and duration-matched playback
// (PlayTimed) that adjusts frequency ratio to match animation speed.
#pragma once

#include <SDL3_mixer/SDL_mixer.h>
#include <string>
#include <string_view>
#include <unordered_map>

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

    // ── Loading ──────────────────────────────────────────────────────────

    /// Load a WAV/OGG/MP3 file, pre-decode it into RAM, and register
    /// it under @p id. If @p id already exists, the old audio is freed.
    /// @return true on success.
    bool Load(const std::string& id, const std::string& path);

    /// Free a single SFX by id. No-op if not found.
    void Unload(const std::string& id);

    /// Free every loaded SFX.
    void UnloadAll();

    // ── Playback ─────────────────────────────────────────────────────────

    /// Play a loaded SFX by id (fire-and-forget, natural speed).
    /// @return true if playback started.
    bool Play(std::string_view id);

    /// Play a loaded SFX adjusted to fit a target duration in seconds.
    /// The audio is sped up or slowed down so it finishes in @p targetSec.
    /// If targetSec <= 0, plays at natural speed (same as Play).
    /// @param loop  If true, the SFX loops continuously (for walk, idle, etc).
    /// @return true if playback started.
    /// @param gain  Per-sound volume multiplier (0.0 .. 1.0). Applied to
    ///               the managed track before playback.
    bool PlayTimed(std::string_view id, float targetSec, bool loop = false,
                   float gain = 1.0f);

    /// Play a loaded SFX sequentially — if the previous sequential sound is
    /// still playing, the call is skipped and returns false. This lets
    /// multi-file slots cycle through sounds one at a time rather than
    /// overlapping. Callers should only advance their round-robin index
    /// when this returns true.
    bool PlayOneShotSeq(std::string_view id, float gain = 1.0f);

    /// Stop the managed SFX track (used to end looping animation SFX).
    void StopManaged();

    /// Stop the one-shot track immediately.
    void StopOneShot();

    /// Fade out the managed (looping/time-stretched) track over @p ms.
    void FadeOutManaged(int ms);

    /// Fade out the sequential one-shot track over @p ms.
    void FadeOutSeq(int ms);

    /// Stop the sequential one-shot track immediately and clear its busy state.
    void StopSeq();

    /// Stop all tracks on the mixer.
    void StopAll();

    // ── Preview (editor) ─────────────────────────────────────────────────

    /// Play a sound once on the dedicated preview track (non-looping).
    /// Use SetPreviewGain() in Update to mute/unmute for trim window.
    bool PlayPreview(std::string_view id, float gain = 1.0f);

    /// Adjust preview track gain in real-time (for trim muting).
    void SetPreviewGain(float gain);

    /// Stop preview playback immediately.
    void StopPreview();

    /// Returns true if the preview track is currently playing.
    [[nodiscard]] bool IsPreviewPlaying() const;

    // ── Query ────────────────────────────────────────────────────────────

    [[nodiscard]] bool Has(std::string_view id) const;
    [[nodiscard]] std::size_t Count() const { return mAudios.size(); }

    /// Get the natural duration of a loaded SFX in seconds.
    /// Returns 0 if not found or duration unknown.
    [[nodiscard]] float GetDuration(std::string_view id) const;

    // ── Volume ───────────────────────────────────────────────────────────

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

    // Looping / time-stretched SFX track — stopped by StopManaged() when
    // the animation transitions away (walk→idle, etc.).
    MIX_Track* mSfxTrack = nullptr;

    // One-shot SFX track — plays a single non-looping sound with gain
    // control. NOT stopped by StopManaged(), so the sound finishes
    // naturally even after the animation transitions to something else.
    MIX_Track* mOneShotTrack = nullptr;

    // Sequential one-shot track for multi-file slots. Plays one sound
    // at a time; PlayOneShotSeq() skips if the previous sound hasn't
    // finished yet. Busy state is tracked via start time + duration.
    MIX_Track* mSeqTrack      = nullptr;
    Uint64     mSeqStartMs    = 0;
    Uint64     mSeqDurationMs = 0;

    // Editor preview track (looping trim playback)
    MIX_Track* mPreviewTrack  = nullptr;

    std::unordered_map<std::string, MIX_Audio*, StringHash, std::equal_to<>> mAudios;
    float mVolume = 1.0f;
};

} // namespace audio
