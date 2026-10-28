// pce.h
#pragma once

#include "pce_cpu.h"

std::uint32_t CartridgeFlags();
std::uint8_t CartridgeBank();
bool ValidCartridgeBank(unsigned bank);
void RestoreCartridgeBank(unsigned bank);
void ResetPCE(PceCpu* M);
