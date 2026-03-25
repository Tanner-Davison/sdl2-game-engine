// audio/MusicPlayer.hpp -- Streaming music playback (SDL3_mixer).
//
// Owns a single MIX_Track* dedicated to music and the MIX_Audio* for
// the currently loaded music file. Provides load, play, pause, resume,
// stop, fade, and volume control.
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

    /// Bind to a mixer and create the dedicated music track.
    /// Must be called before any other method.
    /// @return true on success.
    bool Init(MIX_Mixer* mixer);

    // ── Loading ──────────────────────────────────────────────────────────

    /// Load a music file (OGG, MP3, FLAC, etc.) for streaming playback.
    /// predecode=false so it streams from the compressed data.
    /// Frees any previously loaded track first.
    bool Load(const std::string& path);

    /// Free the current music audio. Stops playback if active.
    void Unload();

    [[nodiscard]] bool IsLoaded() const { return mAudio != nullptr; }
    [[nodiscard]] const std::string& CurrentPath() const { return mPath; }

    // ── Playback ─────────────────────────────────────────────────────────

    /// Start playing the loaded music.
    /// @param loops  -1 = loop forever (default), 0 = play once.
    void Play(int loops = -1);

    /// Stop playback immediately.
    void Stop();

    /// Pause playback.
    void Pause();

    /// Resume paused playback.
    void Resume();

    [[nodiscard]] bool IsPlaying() const;
    [[nodiscard]] bool IsPaused() const;

    // ── Fade ─────────────────────────────────────────────────────────────

    /// Start playing with a fade-in over @p ms milliseconds.
    void FadeIn(int ms, int loops = -1);

    /// Fade out over @p ms milliseconds, then stop.
    void FadeOut(int ms);

    // ── Volume ───────────────────────────────────────────────────────────

    void SetVolume(float v);
    [[nodiscard]] float GetVolume() const { return mVolume; }

  private:
    MIX_Mixer* mMixer = nullptr;
    MIX_Track* mTrack = nullptr;  // dedicated music track
    MIX_Audio* mAudio = nullptr;  // currently loaded music data
    std::string mPath;
    float       mVolume = 1.0f;
};

} // namespace audio
