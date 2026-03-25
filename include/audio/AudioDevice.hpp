// audio/AudioDevice.hpp -- RAII wrapper for SDL3_mixer init + mixer device.
//
// Owns the MIX_Mixer* (the top-level SDL3_mixer object that drives an
// audio device). Exactly one instance should exist, created early in
// main() and destroyed at shutdown. Non-copyable, movable.
#pragma once

#include <SDL3_mixer/SDL_mixer.h>

namespace audio {

class AudioDevice {
  public:
    AudioDevice() = default;

    AudioDevice(AudioDevice&& other) noexcept;
    AudioDevice& operator=(AudioDevice&& other) noexcept;
    AudioDevice(const AudioDevice&)            = delete;
    AudioDevice& operator=(const AudioDevice&) = delete;

    ~AudioDevice();

    /// Initialize SDL3_mixer and open the default playback device.
    /// @return true on success.
    bool Open();

    /// Close the mixer device and quit SDL3_mixer.
    void Close();

    [[nodiscard]] bool IsOpen() const { return mMixer != nullptr; }

    /// Raw access to the MIX_Mixer — needed by SoundBank and MusicPlayer
    /// to load audio and create tracks.
    [[nodiscard]] MIX_Mixer* GetMixer() const { return mMixer; }

  private:
    MIX_Mixer* mMixer = nullptr;
    bool       mInited = false; // true if MIX_Init() succeeded
};

} // namespace audio
