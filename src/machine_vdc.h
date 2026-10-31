#pragma once
#include <cstdint>
namespace rxpce_machine {
class Machine;
void ResetMachineVdc(Machine&);
std::uint64_t NextMachineLine(const Machine&);
std::uint16_t ReadVramDirect(Machine&, std::uint16_t address);
void WriteVramDirect(Machine&, std::uint16_t address, std::uint16_t value);
void TransferFastVram(Machine&);
void WriteMachineVdcRegister(Machine&, unsigned reg);
void AdvanceMachineVdc(Machine&, std::uint64_t time);
void QueueVramRead(Machine&, std::uint16_t address);
void QueueVramWrite(Machine&, std::uint16_t address, std::uint16_t value);
unsigned MachineVdcReady(Machine&, std::uint32_t address, bool write);
} // namespace rxpce_machine
