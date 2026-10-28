#pragma once
#include "audio_output.h"
#include <atomic>
#include <filesystem>
namespace rxpce_host {
struct RuntimeSettings {
    // UI writes only before starting a worker or after joining it.
    rxpce_audio::OutputSettings sound;
    int base_clock_hz = 7170000;
    std::filesystem::path rom_directory, state_directory;
    // These values can also change while the worker is running.
    std::atomic_bool show_fps{false}, auto_frame_skip{true};
    std::atomic_int frame_skip{0};
};
extern RuntimeSettings runtime;
} // namespace rxpce_host
