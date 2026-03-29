// audio/AudioEngine.hpp -- High-level audio facade (SDL3_mixer).
// Composes AudioDevice, SoundBank, and MusicPlayer. NOT a singleton —
// owned by SceneManager and injected into scenes via Scene::SetAudio().
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

    /// Call once after SDL_Init(SDL_INIT_AUDIO).
    bool Init();

    void Shutdown();

    [[nodiscard]] bool IsReady() const { return mDevice.IsOpen(); }

    [[nodiscard]] SoundBank&   Sfx()   { return mSfx; }
    [[nodiscard]] MusicPlayer& Music() { return mMusic; }

    [[nodiscard]] const SoundBank&   Sfx()   const { return mSfx; }
    [[nodiscard]] const MusicPlayer& Music() const { return mMusic; }

    void PlayAnimSFX(AnimationID anim) {
        auto id = PlayerAnimSfxId(anim);
        if (!id.empty())
            mSfx.Play(id);
    }

    /// Time-stretch SFX to match animation duration.
    void PlayAnimSFXTimed(AnimationID anim, float animDurationSec, bool looping,
                          float gain = 1.0f) {
        auto id = PlayerAnimSfxId(anim);
        if (!id.empty())
            mSfx.PlayTimed(id, animDurationSec, looping, gain);
    }

    /// Stop looping animation SFX on the managed track.
    void StopAnimSFX() {
        mSfx.StopManaged();
    }

    /// Fade out managed + sequential SFX tracks over @p ms milliseconds.
    void FadeOutAnimSFX(int ms) {
        mSfx.FadeOutManaged(ms);
        mSfx.FadeOutSeq(ms);
    }

    /// Hard-stop all animation SFX (managed + sequential + one-shot).
    void StopAllAnimSFX() {
        mSfx.StopManaged();
        mSfx.StopOneShot();
        mSfx.StopSeq();
    }

    void PlayEvent(std::string_view eventId) {
        mSfx.Play(eventId);
    }

    /// Load and start playing level music. If @p path is empty, stops music.
    void StartLevelMusic(const std::string& path, float volume = 1.0f,
                         int fadeInMs = 0);

    void StopLevelMusic(int fadeOutMs = 0);

  private:
    AudioDevice mDevice;
    SoundBank   mSfx;
    MusicPlayer mMusic;
};

} // namespace audio
