// audio/AudioEngine.cpp
#include "audio/AudioEngine.hpp"
#include <print>

namespace audio {

bool AudioEngine::Init() {
    if (!mDevice.Open()) return false;

    mSfx.SetMixer(mDevice.GetMixer());

    if (!mMusic.Init(mDevice.GetMixer())) {
        // Non-fatal: SFX still work even if music track creation fails
        std::print("[AudioEngine] MusicPlayer init failed (non-fatal)\n");
    }

    std::print("[AudioEngine] Initialized\n");
    return true;
}

void AudioEngine::Shutdown() {
    mMusic.Unload();
    mSfx.UnloadAll();
    mDevice.Close(); // destroys the mixer, which destroys all tracks
    std::print("[AudioEngine] Shutdown complete\n");
}

void AudioEngine::StartLevelMusic(const std::string& path, float volume,
                                  int fadeInMs) {
    if (path.empty()) {
        StopLevelMusic();
        return;
    }

    // Skip reload if the same track is already loaded and playing
    if (mMusic.CurrentPath() == path && mMusic.IsPlaying()) {
        mMusic.SetVolume(volume);
        return;
    }

    if (!mMusic.Load(path)) {
        std::print("[AudioEngine] Could not load level music: {}\n", path);
        return;
    }

    mMusic.SetVolume(volume);
    if (fadeInMs > 0) {
        mMusic.FadeIn(fadeInMs);
    } else {
        mMusic.Play();
    }
}

void AudioEngine::StopLevelMusic(int fadeOutMs) {
    if (fadeOutMs > 0) {
        mMusic.FadeOut(fadeOutMs);
    } else {
        mMusic.Stop();
    }
}

} // namespace audio
