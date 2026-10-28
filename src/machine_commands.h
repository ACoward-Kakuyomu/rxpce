#pragma once
#include "command_channel.h"

// Host-side command endpoint for this application's single emulation worker.
// UI owns initialization/reset; the worker only consumes notifications.
rxpce_host::CommandChannel& MachineCommands();

enum : DWORD {
    EV_SAVE = 0x0002,
    EV_LOAD = 0x0004,
    EV_RESET = 0x0040,
    EV_DOWNSKIPRATE = 0x0100,
    EV_UPSKIPRATE = 0x0200,
    EV_AUTOFRAMESKIP = 0x0400
};
