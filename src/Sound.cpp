#include "win32_support.h"
#ifdef SOUND
#include "Sound.h"
#include "ds.h"
#include "wasapi.h"
#include "audio_mixer.h"
#include <chrono>
#include <atomic>
#include <mutex>

namespace {
std::mutex sound_mutex;
bool initialized = false, emulating = false;
rxpce_audio::Mixer mixer;
rxpce_audio::OutputSettings active_settings;
HWND output_window = nullptr;
unsigned output_channels = 2;
std::uint64_t requested_frames = 0, missing_frames = 0, missing_events = 0;
std::atomic<std::uint64_t> output_empty{0}, output_deferred{0};
using WallClock = std::chrono::steady_clock;
WallClock::time_point last_sync_end;
SoundQueueStatistics timing;
std::uint64_t reported_clock_resets = 0;
std::uint64_t elapsed_us(WallClock::time_point begin, WallClock::time_point end) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count());
}

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
            if (!mixer.restore_render(phase->render, phase->channels, phase->base_clock))
                return false;
        }
        requested_frames = missing_frames = missing_events = 0;
        timing = {};
        last_sync_end = {};
        reported_clock_resets = 0;
        output_empty = output_deferred = 0;
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
        {
            std::lock_guard<std::mutex> lock(sound_mutex);
            initialized = false;
        }
        ReleaseDSound();
        ReleaseWasapi();
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

std::optional<rxpce_audio::PsgState> SoundPsgWrite(std::uint32_t cycles, unsigned reg,
                                                   std::uint8_t value) {
    const auto begin = WallClock::now();
    std::lock_guard<std::mutex> lock(sound_mutex);
    if (emulating) {
        auto result = mixer.write(cycles, reg, value);
        timing.write_us = (std::max)(timing.write_us, elapsed_us(begin, WallClock::now()));
        return result;
    }
    return std::nullopt;
}

rxpce_audio::PsgPhaseState CaptureSoundPhase(std::uint32_t cycles) {
    std::lock_guard<std::mutex> lock(sound_mutex);
    if (emulating)
        mixer.sync(cycles);
    return {mixer.rate(),     mixer.phase_state(), mixer.base_clock(), mixer.sample_remainder(),
            mixer.channels(), mixer.filter_state()};
}

bool RestoreSoundPhase(const rxpce_audio::PsgPhaseState& phase, std::uint32_t cycles,
                       const rxpce_audio::PsgState& state) {
    std::function<void(float*, std::size_t, unsigned)> source;
    rxpce_audio::OutputSettings settings;
    HWND window;
    {
        std::lock_guard<std::mutex> lock(sound_mutex);
        if (emulating && (initialized || !active_settings.enabled)) {
            mixer.reset(cycles, state);
            mixer.restore_phase(phase.voices);
            mixer.restore_clock(phase.rate, phase.base_clock, phase.remainder);
            return mixer.restore_render(phase.render, phase.channels, phase.base_clock);
        }
        source = mixer.extra_source();
        settings = active_settings;
        window = output_window;
    }
    return InitializeSound(settings, window, state, cycles, &phase, std::move(source));
}

SoundQueueStatistics SyncSoundCycles(std::uint32_t cycles) {
    const auto begin = WallClock::now();
    std::lock_guard<std::mutex> lock(sound_mutex);
    if (last_sync_end != WallClock::time_point{})
        timing.gap_us = (std::max)(timing.gap_us, elapsed_us(last_sync_end, begin));
    if (emulating)
        mixer.sync(cycles);
    last_sync_end = WallClock::now();
    timing.sync_us = (std::max)(timing.sync_us, elapsed_us(begin, last_sync_end));
    if (requested_frames >= std::uint64_t(active_settings.sample_rate) * 2) {
        timing.requested = requested_frames;
        timing.missing = missing_frames;
        timing.events = missing_events;
        timing.clock_resets = mixer.clock_discontinuities() - reported_clock_resets;
        timing.output_empty = output_empty.exchange(0, std::memory_order_relaxed);
        timing.deferred = output_deferred.exchange(0, std::memory_order_relaxed);
        reported_clock_resets = mixer.clock_discontinuities();
        const auto result = timing;
        timing = {};
        requested_frames = missing_frames = missing_events = 0;
        return result;
    }
    return {};
}

void ResetSound(std::uint32_t cycles, const rxpce_audio::PsgState& state) {
    std::lock_guard<std::mutex> lock(sound_mutex);
    if (emulating)
        mixer.reset(cycles, state);
}

std::size_t AvailableMixedAudioFrames() {
    std::lock_guard<std::mutex> lock(sound_mutex);
    return initialized ? mixer.queued_frames() : 0;
}
void DiscardSoundOutput(std::uint32_t cycles) {
    std::lock_guard<std::mutex> lock(sound_mutex);
    if (emulating)
        mixer.sync(cycles);
    mixer.discard_output();
}
void ReportAudioOutput(bool empty, bool deferred) noexcept {
    if (empty)
        output_empty.fetch_add(1, std::memory_order_relaxed);
    if (deferred)
        output_deferred.fetch_add(1, std::memory_order_relaxed);
}
void ReadMixedAudio(float* output, std::size_t frames) {
    if (!frames)
        return;
    std::lock_guard<std::mutex> lock(sound_mutex);
    if (initialized) {
        const auto missing = mixer.mix(output, frames);
        requested_frames += frames;
        missing_frames += missing;
        missing_events += missing != 0;

    } else
        std::fill_n(output, frames * output_channels, 0.0f);
}
#endif // SOUND
