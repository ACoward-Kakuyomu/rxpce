#pragma once
#include "memory_search.h"

namespace rxpce_cheats::detail {
struct Ram {
    std::uint8_t* data = nullptr;
    std::size_t size = 0;
};
// Worker-only physical RAM resolution. Never returns persistent or I/O memory.
Ram resolve_ram(rxpce_machine::Machine& machine, rxpce_search::RegionId region);
}
