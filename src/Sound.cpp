#include "win32_support.h"
#ifdef SOUND
#include "Sound.h"
#include "ds.h"
#include "wasapi.h"
#include <string>
#include "audio_mixer.h"
#include <mutex>

namespace {
std::mutex sound_mutex;
bool initialized = false, emulating = false;
rxpce_audio::Mixer mixer;
rxpce_audio::OutputSettings active_settings;
HWND output_window = nullptr;
unsigned output_channels = 2;

} // namespace

static bool InitializeSound(rxpce_audio::OutputSettings settings, HWND window,
                            const rxpce_audio::PsgState& state, std::uint32_t cycles,
                            const rxpce_audio::PsgPhaseState* phase,
                            std::function<void(float*, std::size_t, unsigned)> source) {
    TrashSound();
    if ((settings.channels != 1 && settings.channels != 2) || !settings.sample_rate ||
        settings.sample_rate > 192000 || !settings.base_clock_hz || !settings.latency_ms ||
        settings.latency_ms > 1000 || settings.cooperative_level < 0 ||
        settings.cooperative_level > 2 ||
        (settings.backend != rxpce_audio::OutputBackend::wasapi &&
         settings.backend != rxpce_audio::OutputBackend::directsound))
        return false;
    {
        std::lock_guard<std::mutex> lock(sound_mutex);
        active_settings = settings;
        output_window = window;
        output_channels = settings.channels;
    }
    try {
        std::lock_guard<std::mutex> lock(sound_mutex);
        mixer.configure(settings.sample_rate, settings.channels, settings.base_clock_hz, state);
        mixer.set_extra_source(std::move(source));
        mixer.reset(cycles, state);
        if (phase) {
            mixer.restore_phase(phase->voices);
            mixer.restore_clock(phase->rate, phase->base_clock, phase->remainder);
        }
        emulating = true;
        initialized = settings.enabled;
    } catch (const std::exception&) {
        return false;
    }
    if (!settings.enabled)
        return true;
    const bool started =
        settings.backend == rxpce_audio::OutputBackend::wasapi
            ? InitWasapi(settings.sample_rate, settings.channels, settings.latency_ms) != FALSE
            : InitDSound(window, settings) != FALSE;
    if (!started) {
        TrashSound();
        return false;
    }
    return true;
}

bool InitSound(rxpce_audio::OutputSettings settings, HWND window,
               const rxpce_audio::PsgState& state) {
    return InitializeSound(settings, window, state, 0, nullptr, {});
}

void TrashSound() {
    {
        std::lock_guard<std::mutex> lock(sound_mutex);
        initialized = emulating = false;
    }
    // Never join the playback thread while holding its audio lock.
    ReleaseDSound();
    ReleaseWasapi();
    std::lock_guard<std::mutex> lock(sound_mutex);
    mixer.set_extra_source({});
    mixer.reset(0, rxpce_audio::PsgState{});
}

void SetSoundExtraSource(std::function<void(float*, std::size_t, unsigned)> source) {
    std::lock_guard<std::mutex> lock(sound_mutex);
    mixer.set_extra_source(std::move(source));
}

void SoundPsgChanged(std::uint32_t cycles, const rxpce_audio::PsgState& state) {
    std::lock_guard<std::mutex> lock(sound_mutex);
    if (emulating)
        mixer.update(cycles, state);
}

rxpce_audio::PsgPhaseState CaptureSoundPhase(std::uint32_t cycles) {
    std::lock_guard<std::mutex> lock(sound_mutex);
    if (emulating)
        mixer.sync(cycles);
    return {mixer.rate(), mixer.phase_state(), mixer.base_clock(), mixer.sample_remainder()};
}

bool RestoreSoundPhase(const rxpce_audio::PsgPhaseState& phase, std::uint32_t cycles,
                       const rxpce_audio::PsgState& state) {
    std::function<void(float*, std::size_t, unsigned)> source;
    rxpce_audio::OutputSettings settings;
    HWND window;
    {
        std::lock_guard<std::mutex> lock(sound_mutex);
        if (emulating) {
            mixer.reset(cycles, state);
            mixer.restore_phase(phase.voices);
            mixer.restore_clock(phase.rate, phase.base_clock, phase.remainder);
            return true;
        }
        source = mixer.extra_source();
        settings = active_settings;
        window = output_window;
    }
    return InitializeSound(settings, window, state, cycles, &phase, std::move(source));
}

void SyncSoundCycles(std::uint32_t cycles) {
    std::lock_guard<std::mutex> lock(sound_mutex);
    if (emulating)
        mixer.sync(cycles);
}

void ResetSound(std::uint32_t cycles, const rxpce_audio::PsgState& state) {
    std::lock_guard<std::mutex> lock(sound_mutex);
    if (emulating)
        mixer.reset(cycles, state);
}

void ReadMixedAudio(float* output, std::size_t frames) {
    if (!frames)
        return;
    std::lock_guard<std::mutex> lock(sound_mutex);
    if (initialized)
        mixer.mix(output, frames);
    else
        std::fill_n(output, frames * output_channels, 0.0f);
}
#endif // SOUND
