#pragma once
#include "machine_devices.h"
#include <filesystem>
namespace rxpce_replay { class Session; }
namespace rxpce_cheats { class Session; }

namespace rxpce_host {
// Single emulation worker. Frame memory is valid from successful InitializeHost
// until ShutdownHost; consumers receive a bounded view, never own the pixels.
int RunEmulation(const std::filesystem::path& backup_file, rxpce_replay::Session* replay = nullptr,
                 rxpce_cheats::Session* cheats = nullptr);
bool InitializeHost(rxpce_machine::Machine& machine);
void ShutdownHost();
rxpce_machine::FrameSurface HostFrame();
void PresentFrame(rxpce_machine::FrameSurface frame, rxpce_machine::DisplayRectangle area);
bool PollHostCommands(rxpce_machine::Machine& machine, rxpce_replay::Session& replay,
                      rxpce_cheats::Session& cheats);
} // namespace rxpce_host
void RequestMachineStop();
void ResetMachineStop();
bool IsMachineStopRequested();
