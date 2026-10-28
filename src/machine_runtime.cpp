#include "machine_runtime.h"

namespace rxpce_host {
rxpce_machine::Machine& CurrentMachine() {
    static auto machine = std::make_unique<rxpce_machine::Machine>();
    return *machine;
}
} // namespace rxpce_host
