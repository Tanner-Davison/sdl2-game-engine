// audio/AudioDevice.hpp -- RAII wrapper for SDL3_mixer init + mixer device.
// Owns MIX_Mixer*; exactly one instance, created early in main(). Non-copyable, movable.
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
    bool Open();

    /// Close the mixer device and quit SDL3_mixer.
    void Close();

    [[nodiscard]] bool IsOpen() const { return mMixer != nullptr; }

    /// Needed by SoundBank and MusicPlayer to load audio and create tracks.
    [[nodiscard]] MIX_Mixer* GetMixer() const { return mMixer; }

  private:
    MIX_Mixer* mMixer = nullptr;
    bool       mInited = false; // true if MIX_Init() succeeded
};

} // namespace audio
