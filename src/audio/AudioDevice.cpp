// audio/AudioDevice.cpp
#include "audio/AudioDevice.hpp"
#include <print>
#include <utility>

namespace audio {

AudioDevice::AudioDevice(AudioDevice&& other) noexcept
    : mMixer(std::exchange(other.mMixer, nullptr))
    , mInited(std::exchange(other.mInited, false)) {}

AudioDevice& AudioDevice::operator=(AudioDevice&& other) noexcept {
    if (this != &other) {
        Close();
        mMixer  = std::exchange(other.mMixer, nullptr);
        mInited = std::exchange(other.mInited, false);
    }
    return *this;
}

AudioDevice::~AudioDevice() {
    Close();
}

bool AudioDevice::Open() {
    if (mMixer) return true;

    if (!MIX_Init()) {
        std::print("[AudioDevice] MIX_Init failed: {}\n", SDL_GetError());
        return false;
    }
    mInited = true;

    mMixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
    if (!mMixer) {
        std::print("[AudioDevice] MIX_CreateMixerDevice failed: {}\n", SDL_GetError());
        MIX_Quit();
        mInited = false;
        return false;
    }

    std::print("[AudioDevice] Opened\n");
    return true;
}

void AudioDevice::Close() {
    if (mMixer) {
        MIX_DestroyMixer(mMixer);
        mMixer = nullptr;
        std::print("[AudioDevice] Closed\n");
    }
    if (mInited) {
        MIX_Quit();
        mInited = false;
    }
}

} // namespace audio
