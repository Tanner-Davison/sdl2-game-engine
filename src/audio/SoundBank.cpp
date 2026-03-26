// audio/SoundBank.cpp -- Sound effect resource management (SDL3_mixer).
#include "audio/SoundBank.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <utility>

namespace fs = std::filesystem;

// ── Auto-convert helper ──────────────────────────────────────────────────────
// When MIX_LoadAudio fails on a WAV/audio file, attempt to convert it to
// standard 16-bit PCM WAV using platform-native tools (afconvert on macOS,
// ffmpeg on Linux/Windows). The converted file is saved next to the original
// with a "_pcm16" suffix and reused on subsequent loads.
//
// Returns the path to the converted file, or empty string on failure.
static std::string TryAutoConvert(const std::string& srcPath) {
    fs::path src(srcPath);
    if (!fs::is_regular_file(src)) return "";

    // Build output path: same directory, same stem + _pcm16.wav
    fs::path dstDir = src.parent_path();
    std::string stem = src.stem().string();
    fs::path dst = dstDir / (stem + "_pcm16.wav");

    // If already converted, just return the cached path
    if (fs::exists(dst)) return dst.string();

    std::print("[SoundBank] Auto-converting {} to PCM16 WAV...\n", srcPath);

    // Try afconvert first (macOS built-in)
    std::string cmd = "afconvert -f WAVE -d LEI16@44100 "
                    + std::string("'") + srcPath + "' '"
                    + dst.string() + "' 2>/dev/null";
    int ret = std::system(cmd.c_str());
    if (ret == 0 && fs::exists(dst)) {
        std::print("[SoundBank] Converted via afconvert: {}\n", dst.string());
        return dst.string();
    }

    // Try ffmpeg (Linux, Windows, or macOS with brew)
    cmd = "ffmpeg -y -loglevel error -i "
        + std::string("'") + srcPath + "' -acodec pcm_s16le -ar 44100 '"
        + dst.string() + "' 2>/dev/null";
    ret = std::system(cmd.c_str());
    if (ret == 0 && fs::exists(dst)) {
        std::print("[SoundBank] Converted via ffmpeg: {}\n", dst.string());
        return dst.string();
    }

    std::print("[SoundBank] Auto-conversion failed for {}\n", srcPath);
    return "";
}

namespace audio {

SoundBank::~SoundBank() {
    UnloadAll();
    if (mPreviewTrack) {
        MIX_DestroyTrack(mPreviewTrack);
        mPreviewTrack = nullptr;
    }
    if (mSeqTrack) {
        MIX_DestroyTrack(mSeqTrack);
        mSeqTrack = nullptr;
    }
    if (mOneShotTrack) {
        MIX_DestroyTrack(mOneShotTrack);
        mOneShotTrack = nullptr;
    }
    if (mSfxTrack) {
        MIX_DestroyTrack(mSfxTrack);
        mSfxTrack = nullptr;
    }
}

SoundBank::SoundBank(SoundBank&& other) noexcept
    : mMixer(std::exchange(other.mMixer, nullptr))
    , mSfxTrack(std::exchange(other.mSfxTrack, nullptr))
    , mOneShotTrack(std::exchange(other.mOneShotTrack, nullptr))
    , mSeqTrack(std::exchange(other.mSeqTrack, nullptr))
    , mSeqStartMs(other.mSeqStartMs)
    , mSeqDurationMs(other.mSeqDurationMs)
    , mPreviewTrack(std::exchange(other.mPreviewTrack, nullptr))
    , mAudios(std::move(other.mAudios))
    , mVolume(other.mVolume) {
    other.mSeqStartMs = 0;
    other.mSeqDurationMs = 0;
    other.mVolume = 1.0f;
}

SoundBank& SoundBank::operator=(SoundBank&& other) noexcept {
    if (this != &other) {
        UnloadAll();
        if (mSeqTrack) MIX_DestroyTrack(mSeqTrack);
        if (mOneShotTrack) MIX_DestroyTrack(mOneShotTrack);
        if (mSfxTrack) MIX_DestroyTrack(mSfxTrack);
        mMixer        = std::exchange(other.mMixer, nullptr);
        mSfxTrack     = std::exchange(other.mSfxTrack, nullptr);
        mOneShotTrack = std::exchange(other.mOneShotTrack, nullptr);
        mSeqTrack     = std::exchange(other.mSeqTrack, nullptr);
        mSeqStartMs   = other.mSeqStartMs;
        mSeqDurationMs = other.mSeqDurationMs;
        other.mSeqStartMs = 0;
        other.mSeqDurationMs = 0;
        if (mPreviewTrack) MIX_DestroyTrack(mPreviewTrack);
        mPreviewTrack = std::exchange(other.mPreviewTrack, nullptr);
        mAudios       = std::move(other.mAudios);
        mVolume       = other.mVolume;
        other.mVolume = 1.0f;
    }
    return *this;
}

void SoundBank::SetMixer(MIX_Mixer* mixer) {
    mMixer = mixer;
    if (mMixer && !mSfxTrack) {
        mSfxTrack = MIX_CreateTrack(mMixer);
        if (mSfxTrack)
            MIX_TagTrack(mSfxTrack, "sfx");
    }
    if (mMixer && !mOneShotTrack) {
        mOneShotTrack = MIX_CreateTrack(mMixer);
        if (mOneShotTrack)
            MIX_TagTrack(mOneShotTrack, "sfx_oneshot");
    }
    if (mMixer && !mSeqTrack) {
        mSeqTrack = MIX_CreateTrack(mMixer);
        if (mSeqTrack)
            MIX_TagTrack(mSeqTrack, "sfx_seq");
    }
    if (mMixer && !mPreviewTrack) {
        mPreviewTrack = MIX_CreateTrack(mMixer);
        if (mPreviewTrack)
            MIX_TagTrack(mPreviewTrack, "sfx_preview");
    }
}

// ── Loading ──────────────────────────────────────────────────────────────────

bool SoundBank::Load(const std::string& id, const std::string& path) {
    if (path.empty() || !mMixer) return false;

    Unload(id);

    // predecode=true: decompress fully into RAM for instant playback
    MIX_Audio* audio = MIX_LoadAudio(mMixer, path.c_str(), true);
    if (!audio) {
        // First attempt failed -- try auto-converting to standard PCM WAV
        std::string converted = TryAutoConvert(path);
        if (!converted.empty())
            audio = MIX_LoadAudio(mMixer, converted.c_str(), true);

        if (!audio) {
            std::print("[SoundBank] Failed to load '{}' from {}: {}\n",
                       id, path, SDL_GetError());
            return false;
        }
    }

    mAudios[id] = audio;

    // Log duration for debugging
    Sint64 frames = MIX_GetAudioDuration(audio);
    if (frames > 0) {
        Sint64 ms = MIX_AudioFramesToMS(audio, frames);
        std::print("[SoundBank] Loaded '{}' from {} ({}ms)\n", id, path, ms);
    } else {
        std::print("[SoundBank] Loaded '{}' from {}\n", id, path);
    }
    return true;
}

void SoundBank::Unload(const std::string& id) {
    auto it = mAudios.find(id);
    if (it != mAudios.end()) {
        MIX_DestroyAudio(it->second);
        mAudios.erase(it);
    }
}

void SoundBank::UnloadAll() {
    if (mPreviewTrack) {
        MIX_StopTrack(mPreviewTrack, 0);
        MIX_SetTrackAudio(mPreviewTrack, nullptr);
    }
    if (mSeqTrack) {
        MIX_StopTrack(mSeqTrack, 0);
        MIX_SetTrackAudio(mSeqTrack, nullptr);
    }
    mSeqStartMs = 0;
    mSeqDurationMs = 0;
    if (mOneShotTrack) {
        MIX_StopTrack(mOneShotTrack, 0);
        MIX_SetTrackAudio(mOneShotTrack, nullptr);
    }
    if (mSfxTrack) {
        MIX_StopTrack(mSfxTrack, 0);
        MIX_SetTrackAudio(mSfxTrack, nullptr);
    }
    for (auto& [id, audio] : mAudios)
        MIX_DestroyAudio(audio);
    mAudios.clear();
}

// ── Playback ─────────────────────────────────────────────────────────────────

bool SoundBank::Play(std::string_view id) {
    if (!mMixer) return false;

    auto it = mAudios.find(id);
    if (it == mAudios.end()) return false;

    // Fire-and-forget: SDL3_mixer manages the track internally
    if (!MIX_PlayAudio(mMixer, it->second)) {
        std::print("[SoundBank] Play '{}' failed: {}\n", id, SDL_GetError());
        return false;
    }
    return true;
}

bool SoundBank::PlayOneShotSeq(std::string_view id, float gain) {
    if (!mMixer) return false;

    auto it = mAudios.find(id);
    if (it == mAudios.end()) return false;

    if (!mSeqTrack) return Play(id);

    // Always cut the previous sound and start the new one immediately.
    MIX_StopTrack(mSeqTrack, 0);
    if (!MIX_SetTrackAudio(mSeqTrack, it->second)) return Play(id);
    MIX_SetTrackGain(mSeqTrack, std::clamp(gain, 0.0f, 1.0f));
    MIX_SetTrackFrequencyRatio(mSeqTrack, 1.0f);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, 0);
    bool ok = MIX_PlayTrack(mSeqTrack, props);
    SDL_DestroyProperties(props);

    if (ok) {
        mSeqStartMs = SDL_GetTicks();
        Sint64 frames = MIX_GetAudioDuration(it->second);
        mSeqDurationMs = (frames > 0)
            ? static_cast<Uint64>(MIX_AudioFramesToMS(it->second, frames))
            : 0;
    }
    return ok;
}

bool SoundBank::PlayTimed(std::string_view id, float targetSec, bool loop, float gain) {
    if (!mMixer) return false;

    // Non-looping, non-time-stretched: play on the one-shot track so the
    // sound finishes naturally (StopManaged won't touch it) with gain.
    // Falls back to fire-and-forget when the one-shot track is unavailable.
    if (targetSec <= 0.0f && !loop) {
        if (gain >= 0.99f && !mOneShotTrack) return Play(id);

        auto it = mAudios.find(id);
        if (it == mAudios.end()) return false;

        if (!mOneShotTrack) return Play(id);

        MIX_StopTrack(mOneShotTrack, 0);
        if (!MIX_SetTrackAudio(mOneShotTrack, it->second)) return Play(id);
        MIX_SetTrackGain(mOneShotTrack, std::clamp(gain, 0.0f, 1.0f));
        MIX_SetTrackFrequencyRatio(mOneShotTrack, 1.0f);

        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, 0);
        bool ok = MIX_PlayTrack(mOneShotTrack, props);
        SDL_DestroyProperties(props);
        return ok;
    }

    auto it = mAudios.find(id);
    if (it == mAudios.end()) {
        std::print("[SoundBank] PlayTimed '{}' — not loaded\n", id);
        return false;
    }

    MIX_Audio* audio = it->second;

    Sint64 frames = MIX_GetAudioDuration(audio);
    if (frames <= 0) return Play(id);
    Sint64 naturalMs = MIX_AudioFramesToMS(audio, frames);
    if (naturalMs <= 0) return Play(id);

    float naturalSec = static_cast<float>(naturalMs) / 1000.0f;

    float ratio = 1.0f;
    if (targetSec > 0.0f) {
        ratio = naturalSec / targetSec;
        ratio = std::clamp(ratio, 0.25f, 4.0f);
    }

    if (!mSfxTrack) return Play(id);

    MIX_StopTrack(mSfxTrack, 0);

    if (!MIX_SetTrackAudio(mSfxTrack, audio)) return Play(id);

    MIX_SetTrackFrequencyRatio(mSfxTrack, ratio);
    MIX_SetTrackGain(mSfxTrack, std::clamp(gain, 0.0f, 1.0f));

    std::print("[SoundBank] PlayTimed '{}': natural={:.2f}s target={:.2f}s ratio={:.2f} loop={}\n",
              id, naturalSec, targetSec, ratio, loop);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loop ? -1 : 0);
    bool ok = MIX_PlayTrack(mSfxTrack, props);
    SDL_DestroyProperties(props);

    if (!ok) {
        std::print("[SoundBank] PlayTimed '{}' MIX_PlayTrack FAILED: {}\n", id, SDL_GetError());
        MIX_SetTrackFrequencyRatio(mSfxTrack, 1.0f);
        return false;
    }

    std::print("[SoundBank] PlayTimed '{}' started OK\n", id);
    return true;
}

void SoundBank::StopManaged() {
    if (mSfxTrack) {
        MIX_StopTrack(mSfxTrack, 0);
        MIX_SetTrackFrequencyRatio(mSfxTrack, 1.0f);
    }
}

void SoundBank::StopOneShot() {
    if (mOneShotTrack)
        MIX_StopTrack(mOneShotTrack, 0);
}

void SoundBank::FadeOutManaged(int ms) {
    if (mSfxTrack)
        MIX_StopTrack(mSfxTrack, ms);
}

void SoundBank::FadeOutSeq(int ms) {
    if (mSeqTrack)
        MIX_StopTrack(mSeqTrack, ms);
    mSeqDurationMs = 0;
}

void SoundBank::StopSeq() {
    if (mSeqTrack)
        MIX_StopTrack(mSeqTrack, 0);
    mSeqStartMs = 0;
    mSeqDurationMs = 0;
}

void SoundBank::StopAll() {
    if (mMixer)
        MIX_StopAllTracks(mMixer, 0);
}

// ── Preview (editor) ────────────────────────────────────────────────────────

bool SoundBank::PlayPreview(std::string_view id, float gain) {
    if (!mMixer || !mPreviewTrack) return false;

    auto it = mAudios.find(id);
    if (it == mAudios.end()) return false;

    MIX_StopTrack(mPreviewTrack, 0);
    if (!MIX_SetTrackAudio(mPreviewTrack, it->second)) return false;
    MIX_SetTrackGain(mPreviewTrack, std::clamp(gain, 0.0f, 1.0f));
    MIX_SetTrackFrequencyRatio(mPreviewTrack, 1.0f);

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, 0);
    bool ok = MIX_PlayTrack(mPreviewTrack, props);
    SDL_DestroyProperties(props);
    return ok;
}

void SoundBank::SetPreviewGain(float gain) {
    if (mPreviewTrack)
        MIX_SetTrackGain(mPreviewTrack, std::clamp(gain, 0.0f, 1.0f));
}

void SoundBank::StopPreview() {
    if (mPreviewTrack)
        MIX_StopTrack(mPreviewTrack, 0);
}

bool SoundBank::IsPreviewPlaying() const {
    // Check if the track is actively playing by checking if we have audio set
    // SDL3_mixer doesn't have a direct IsPlaying, so we rely on our timer logic
    return mPreviewTrack != nullptr;
}

// ── Query ────────────────────────────────────────────────────────────────────

bool SoundBank::Has(std::string_view id) const {
    return mAudios.find(id) != mAudios.end();
}


float SoundBank::GetDuration(std::string_view id) const {
    auto it = mAudios.find(id);
    if (it == mAudios.end()) return 0.0f;

    Sint64 frames = MIX_GetAudioDuration(it->second);
    if (frames <= 0) return 0.0f;
    Sint64 ms = MIX_AudioFramesToMS(it->second, frames);
    return static_cast<float>(ms) / 1000.0f;
}

// ── Volume ───────────────────────────────────────────────────────────────────

void SoundBank::SetVolume(float v) {
    mVolume = std::clamp(v, 0.0f, 1.0f);
    if (mMixer)
        MIX_SetMixerGain(mMixer, mVolume);
}

} // namespace audio
