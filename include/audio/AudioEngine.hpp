// audio/AudioEngine.hpp -- High-level audio facade (SDL3_mixer).
//
// Composes AudioDevice, SoundBank, and MusicPlayer into a single object
// that scenes receive by reference. NOT a singleton -- owned by
// SceneManager and injected into scenes via Scene::SetAudio().
#pragma once

#include "audio/AudioDevice.hpp"
#include "audio/AudioEvents.hpp"
#include "audio/MusicPlayer.hpp"
#include "audio/SoundBank.hpp"

namespace audio {

class AudioEngine {
  public:
    AudioEngine() = default;
    ~AudioEngine() = default;

    AudioEngine(AudioEngine&&) noexcept            = default;
    AudioEngine& operator=(AudioEngine&&) noexcept = default;
    AudioEngine(const AudioEngine&)                = delete;
    AudioEngine& operator=(const AudioEngine&)     = delete;

    // ── Lifecycle ────────────────────────────────────────────────────────

    /// Open the audio device and initialize sub-components.
    /// Call once after SDL_Init(SDL_INIT_AUDIO).
    /// @return true on success.
    bool Init();

    /// Shut everything down: stop playback, free resources, close device.
    void Shutdown();

    [[nodiscard]] bool IsReady() const { return mDevice.IsOpen(); }

    // ── Sub-component access ─────────────────────────────────────────────

    [[nodiscard]] SoundBank&   Sfx()   { return mSfx; }
    [[nodiscard]] MusicPlayer& Music() { return mMusic; }

    [[nodiscard]] const SoundBank&   Sfx()   const { return mSfx; }
    [[nodiscard]] const MusicPlayer& Music() const { return mMusic; }

    // ── Convenience: player animation SFX ────────────────────────────────

    /// Trigger the SFX for a player animation (natural speed).
    void PlayAnimSFX(AnimationID anim) {
        auto id = PlayerAnimSfxId(anim);
        if (!id.empty())
            mSfx.Play(id);
    }

    /// Trigger the SFX for a player animation, time-stretched to match
    /// the animation duration. The audio is sped up or slowed down so
    /// one cycle matches @p animDurationSec seconds.
    /// @param looping  If true, the SFX loops (for walk, idle, etc).
    void PlayAnimSFXTimed(AnimationID anim, float animDurationSec, bool looping,
                          float gain = 1.0f) {
        auto id = PlayerAnimSfxId(anim);
        if (!id.empty())
            mSfx.PlayTimed(id, animDurationSec, looping, gain);
    }

    /// Stop any looping animation SFX on the managed track.
    void StopAnimSFX() {
        mSfx.StopManaged();
    }

    // ── Convenience: game event SFX ──────────────────────────────────────

    void PlayEvent(std::string_view eventId) {
        mSfx.Play(eventId);
    }

    // ── Convenience: level music ─────────────────────────────────────────

    /// Load and start playing level music. If @p path is empty, stops music.
    void StartLevelMusic(const std::string& path, float volume = 1.0f,
                         int fadeInMs = 0);

    /// Stop level music with optional fade-out.
    void StopLevelMusic(int fadeOutMs = 0);

  private:
    AudioDevice mDevice;
    SoundBank   mSfx;
    MusicPlayer mMusic;
};

} // namespace audio
