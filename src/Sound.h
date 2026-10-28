// Sound.h

#ifndef HDEF_SOUND_H
#define HDEF_SOUND_H

#include <windows.h>
#include "audio_output.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include "audio_state.h"
#include "psg_state.h"

bool InitSound(rxpce_audio::OutputSettings settings, HWND window,
               const rxpce_audio::PsgState& state);
void TrashSound(void);
void SoundPsgChanged(std::uint32_t cycles, const rxpce_audio::PsgState& state);
void SyncSoundCycles(std::uint32_t cycles);
void ResetSound(std::uint32_t cycles, const rxpce_audio::PsgState& state);
void ReadMixedAudio(float* output, std::size_t frames);
// Producer-thread callback; the playback thread only consumes the finished mix.
void SetSoundExtraSource(std::function<void(float*, std::size_t, unsigned)> source);
rxpce_audio::PsgPhaseState CaptureSoundPhase(std::uint32_t cycles);
bool RestoreSoundPhase(const rxpce_audio::PsgPhaseState& phase, std::uint32_t cycles,
                       const rxpce_audio::PsgState& state);

#endif // HDEF_SOUND_H
