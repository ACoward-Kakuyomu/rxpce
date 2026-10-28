#pragma once
namespace rxpce_audio {
enum class OutputBackend { wasapi, directsound };
struct OutputSettings {
    bool enabled = true;
    OutputBackend backend = OutputBackend::wasapi;
    unsigned sample_rate = 44100, channels = 2, latency_ms = 40, base_clock_hz = 7170000;
    int cooperative_level = 2;
};
} // namespace rxpce_audio
