#pragma once
#include "cd_controller.h"
#include "audio_state.h"
#include <memory>
#include <filesystem>
namespace rxpce_cd {
struct MachineState {
    Controller device;
    std::array<unsigned char, 65536> ram;
    std::array<unsigned char, 196608> super_ram;
    rxpce_audio::PsgPhaseState sound;
};
} // namespace rxpce_cd
std::size_t CdStateBytes();
std::uint32_t CdStateFlags();
std::wstring CdStatePath(const std::filesystem::path& folder);
void SynchronizeCdState();
void AppendCdState(std::vector<char>& payload, const rxpce_audio::PsgPhaseState& sound);
std::unique_ptr<rxpce_cd::MachineState> PrepareCdState(const char* data, std::size_t size,
                                                       std::uint64_t time, std::string& error);
void ApplyCdState(rxpce_cd::MachineState&& state);
