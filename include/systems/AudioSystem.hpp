// systems/AudioSystem.hpp -- DEPRECATED / REMOVED.
//
// The audio system has been split into proper modular components:
//
//   include/audio/AudioDevice.hpp   -- RAII mixer device wrapper
//   include/audio/SoundBank.hpp     -- SFX resource manager
//   include/audio/MusicPlayer.hpp   -- Streaming music playback
//   include/audio/AudioEngine.hpp   -- High-level facade (use this)
//   include/audio/AudioEvents.hpp   -- SFX ID constants and mappings
//
// This file is kept temporarily to avoid breaking any includes.
// Remove it once all references are updated.
#pragma once
#include "audio/AudioEngine.hpp"
