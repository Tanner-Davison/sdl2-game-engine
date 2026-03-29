// audio/MusicPlayer.hpp -- Streaming music playback (SDL3_mixer).
// Owns a single MIX_Track* for music and the MIX_Audio* for the current file.
#pragma once

#include <SDL3_mixer/SDL_mixer.h>
#include <string>

namespace audio {

class MusicPlayer {
  public:
    MusicPlayer() = default;
    ~MusicPlayer();

    MusicPlayer(MusicPlayer&& other) noexcept;
    MusicPlayer& operator=(MusicPlayer&& other) noexcept;
    MusicPlayer(const MusicPlayer&)            = delete;
    MusicPlayer& operator=(const MusicPlayer&) = delete;

    /// Bind to a mixer and create the dedicated music track. Must be called first.
    bool Init(MIX_Mixer* mixer);

    /// Load a music file for streaming playback (predecode=false).
    /// Frees any previously loaded track first.
    bool Load(const std::string& path);

    /// Free the current music audio. Stops playback if active.
    void Unload();

    [[nodiscard]] bool IsLoaded() const { return mAudio != nullptr; }
    [[nodiscard]] const std::string& CurrentPath() const { return mPath; }

    /// @param loops  -1 = loop forever (default), 0 = play once.
    void Play(int loops = -1);

    void Stop();
    void Pause();
    void Resume();

    [[nodiscard]] bool IsPlaying() const;
    [[nodiscard]] bool IsPaused() const;

    void FadeIn(int ms, int loops = -1);

    /// Fade out over @p ms milliseconds, then stop.
    void FadeOut(int ms);

    void SetVolume(float v);
    [[nodiscard]] float GetVolume() const { return mVolume; }

  private:
    MIX_Mixer* mMixer = nullptr;
    MIX_Track* mTrack = nullptr;
    MIX_Audio* mAudio = nullptr;
    std::string mPath;
    float       mVolume = 1.0f;
};

} // namespace audio
