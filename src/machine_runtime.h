#pragma once
#include "machine.h"

namespace rxpce_host {
// Transitional host binding. Access only on the emulation worker or while it
// is stopped; the UI submits commands instead of accessing device state.
rxpce_machine::Machine& CurrentMachine();
} // namespace rxpce_host
