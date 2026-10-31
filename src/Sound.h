// Sound.h

#ifndef HDEF_SOUND_H
#define HDEF_SOUND_H

#include <windows.h>
#include "audio_output.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include "audio_state.h"
#include "psg_state.h"

bool InitSound(rxpce_audio::OutputSettings settings, HWND window,
               const rxpce_audio::PsgState& state);
void TrashSound(void);
std::optional<rxpce_audio::PsgState> SoundPsgWrite(std::uint32_t cycles, unsigned reg,
                                                   std::uint8_t value);
struct SoundQueueStatistics {
    std::uint64_t requested = 0, missing = 0, events = 0;
    std::uint64_t sync_us = 0, gap_us = 0, write_us = 0, clock_resets = 0;
    std::uint64_t output_empty = 0, deferred = 0;
};
// Producer-thread diagnostics only. The caller writes logs after the audio lock is released.
SoundQueueStatistics SyncSoundCycles(std::uint32_t cycles);
void ResetSound(std::uint32_t cycles, const rxpce_audio::PsgState& state);
void ReadMixedAudio(float* output, std::size_t frames);
std::size_t AvailableMixedAudioFrames();
void ReportAudioOutput(bool empty, bool deferred) noexcept;
void DiscardSoundOutput(std::uint32_t cycles);
// Producer-thread callback; the playback thread only consumes the finished mix.
void SetSoundExtraSource(std::function<void(float*, std::size_t, unsigned)> source);
rxpce_audio::PsgPhaseState CaptureSoundPhase(std::uint32_t cycles);
bool RestoreSoundPhase(const rxpce_audio::PsgPhaseState& phase, std::uint32_t cycles,
                       const rxpce_audio::PsgState& state);

#endif // HDEF_SOUND_H
