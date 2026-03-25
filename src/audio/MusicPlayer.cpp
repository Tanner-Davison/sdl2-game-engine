// audio/MusicPlayer.cpp -- Streaming music playback (SDL3_mixer).
#include "audio/MusicPlayer.hpp"
#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <print>
#include <utility>

namespace fs = std::filesystem;

// Same auto-convert helper as SoundBank (duplicated here to keep the two
// compilation units independent -- a shared header would be cleaner but
// this is a small function and avoids a new header just for it).
static std::string TryAutoConvert(const std::string& srcPath) {
    fs::path src(srcPath);
    if (!fs::is_regular_file(src)) return "";
    fs::path dst = src.parent_path() / (src.stem().string() + "_pcm16.wav");
    if (fs::exists(dst)) return dst.string();
    std::print("[MusicPlayer] Auto-converting {} to PCM16 WAV...\n", srcPath);
    std::string cmd = "afconvert -f WAVE -d LEI16@44100 '"
                    + srcPath + "' '" + dst.string() + "' 2>/dev/null";
    if (std::system(cmd.c_str()) == 0 && fs::exists(dst)) return dst.string();
    cmd = "ffmpeg -y -loglevel error -i '" + srcPath
        + "' -acodec pcm_s16le -ar 44100 '" + dst.string() + "' 2>/dev/null";
    if (std::system(cmd.c_str()) == 0 && fs::exists(dst)) return dst.string();
    return "";
}

namespace audio {

MusicPlayer::~MusicPlayer() {
    Unload();
    if (mTrack) {
        MIX_DestroyTrack(mTrack);
        mTrack = nullptr;
    }
}

MusicPlayer::MusicPlayer(MusicPlayer&& other) noexcept
    : mMixer(std::exchange(other.mMixer, nullptr))
    , mTrack(std::exchange(other.mTrack, nullptr))
    , mAudio(std::exchange(other.mAudio, nullptr))
    , mPath(std::move(other.mPath))
    , mVolume(other.mVolume) {
    other.mVolume = 1.0f;
}

MusicPlayer& MusicPlayer::operator=(MusicPlayer&& other) noexcept {
    if (this != &other) {
        Unload();
        if (mTrack) MIX_DestroyTrack(mTrack);

        mMixer  = std::exchange(other.mMixer, nullptr);
        mTrack  = std::exchange(other.mTrack, nullptr);
        mAudio  = std::exchange(other.mAudio, nullptr);
        mPath   = std::move(other.mPath);
        mVolume = other.mVolume;
        other.mVolume = 1.0f;
    }
    return *this;
}

bool MusicPlayer::Init(MIX_Mixer* mixer) {
    if (!mixer) return false;
    mMixer = mixer;

    mTrack = MIX_CreateTrack(mMixer);
    if (!mTrack) {
        std::print("[MusicPlayer] Failed to create music track: {}\n", SDL_GetError());
        return false;
    }

    // Tag the track so we can control it by tag later if needed
    MIX_TagTrack(mTrack, "music");

    std::print("[MusicPlayer] Initialized\n");
    return true;
}

// ── Loading ──────────────────────────────────────────────────────────────────

bool MusicPlayer::Load(const std::string& path) {
    if (path.empty() || !mMixer) return false;
    Unload();

    // predecode=false: stream from compressed data (efficient for music)
    mAudio = MIX_LoadAudio(mMixer, path.c_str(), false);
    if (!mAudio) {
        std::string converted = TryAutoConvert(path);
        if (!converted.empty())
            mAudio = MIX_LoadAudio(mMixer, converted.c_str(), false);
        if (!mAudio) {
            std::print("[MusicPlayer] Failed to load {}: {}\n", path, SDL_GetError());
            return false;
        }
    }
    mPath = path;

    // Assign the audio to our dedicated music track
    if (!MIX_SetTrackAudio(mTrack, mAudio)) {
        std::print("[MusicPlayer] Failed to set track audio: {}\n", SDL_GetError());
        MIX_DestroyAudio(mAudio);
        mAudio = nullptr;
        mPath.clear();
        return false;
    }

    std::print("[MusicPlayer] Loaded {}\n", path);
    return true;
}

void MusicPlayer::Unload() {
    if (mTrack) {
        MIX_StopTrack(mTrack, 0);
        MIX_SetTrackAudio(mTrack, nullptr); // detach audio from track
    }
    if (mAudio) {
        MIX_DestroyAudio(mAudio);
        mAudio = nullptr;
        mPath.clear();
    }
}

// ── Playback ─────────────────────────────────────────────────────────────────

void MusicPlayer::Play(int loops) {
    if (!mTrack || !mAudio) return;

    // Build playback properties
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
    bool ok = MIX_PlayTrack(mTrack, props);
    SDL_DestroyProperties(props);

    if (!ok) {
        std::print("[MusicPlayer] Play failed: {}\n", SDL_GetError());
    }
}

void MusicPlayer::Stop() {
    if (mTrack)
        MIX_StopTrack(mTrack, 0);
}

void MusicPlayer::Pause() {
    if (mTrack && IsPlaying())
        MIX_PauseTrack(mTrack);
}

void MusicPlayer::Resume() {
    if (mTrack && IsPaused())
        MIX_ResumeTrack(mTrack);
}

bool MusicPlayer::IsPlaying() const {
    if (!mTrack) return false;
    return MIX_TrackPlaying(mTrack) && !MIX_TrackPaused(mTrack);
}

bool MusicPlayer::IsPaused() const {
    if (!mTrack) return false;
    return MIX_TrackPaused(mTrack);
}

// ── Fade ─────────────────────────────────────────────────────────────────────

void MusicPlayer::FadeIn(int ms, int loops) {
    if (!mTrack || !mAudio) return;

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, ms);
    bool ok = MIX_PlayTrack(mTrack, props);
    SDL_DestroyProperties(props);

    if (!ok) {
        std::print("[MusicPlayer] FadeIn failed: {}\n", SDL_GetError());
    }
}

void MusicPlayer::FadeOut(int ms) {
    if (!mTrack) return;
    // StopTrack fade_out is in sample frames, so convert from ms
    Sint64 frames = MIX_TrackMSToFrames(mTrack, ms);
    MIX_StopTrack(mTrack, frames);
}

// ── Volume ───────────────────────────────────────────────────────────────────

void MusicPlayer::SetVolume(float v) {
    mVolume = std::clamp(v, 0.0f, 1.0f);
    if (mTrack)
        MIX_SetTrackGain(mTrack, mVolume);
}

} // namespace audio
