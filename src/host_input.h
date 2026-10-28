#pragma once
namespace rxpce_machine {
class Machine;
}
namespace rxpce_host {
// Emulation worker only; the UI publishes key state and input configuration.
void InitializeInput();
void ResetInput();
void PollInput(rxpce_machine::Machine& machine);
} // namespace rxpce_host
