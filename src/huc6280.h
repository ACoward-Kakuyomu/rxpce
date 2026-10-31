#pragma once
#include <array>
#include <cstdint>
extern "C" {
#include "MCS6502.h"
}

namespace rxpce_cpu {
enum Flag : std::uint8_t { C = 1, Z = 2, I = 4, D = 8, B = 16, T = 32, V = 64, N = 128 };
enum Irq : std::uint8_t { Irq2 = 1, Irq1 = 2, Timer = 4 };
struct TransferState {
    std::uint8_t opcode = 0; // Zero means inactive; remaining==0 means finish is pending.
    std::uint16_t source = 0, destination = 0;
    std::uint32_t remaining = 0;
    bool alternate = false;
};
// Only this pointer-free state is eligible for a future save-state format.
struct State {
    std::uint16_t pc = 0;
    std::uint8_t a = 0, x = 0, y = 0, sp = 0xff, p = I | B;
    std::array<std::uint8_t, 8> mpr{};
    std::uint8_t mpr_latch = 0, irq_lines = 0, irq_mask = 0, irq_delay = 0;
    bool irq_sampled_before_sei = false;
    bool nmi_line = false, nmi_pending = false, low_speed = true;
    std::uint64_t elapsed = 0;
    TransferState transfer;
};
struct Bus {
    std::uint8_t (*read)(std::uint32_t physical, void* context);
    void (*write)(std::uint32_t physical, std::uint8_t value, void* context);
    void* context;
    // Optional extra CPU clocks for a physical access, e.g. VDC/VCE waits.
    unsigned (*wait)(std::uint32_t physical, bool write, void* context) = nullptr;
    // Optional elapsed-prefix synchronization and additional readiness waits.
    void (*synchronize)(std::uint64_t time, void* context) = nullptr;
    unsigned (*ready)(std::uint32_t physical, bool write, void* context) = nullptr;
    // Scheduler-only fallback for timed writes when the normal bus sync is absent.
    // Separate context preserves direct Fast-mode read/write callbacks.
    void (*synchronize_prefix)(std::uint64_t time, void* context) = nullptr;
    void* prefix_context = nullptr;
};
struct StepResult {
    unsigned cycles;  // CPU clocks, including bus waits
    unsigned elapsed; // high-speed clock units (low speed multiplies by 4)
    bool valid;
    bool interrupt;
    bool instruction_complete = true;
};
class HuC6280 {
  public:
    explicit HuC6280(Bus bus);
    HuC6280(Bus bus, State& state);
    HuC6280(const HuC6280&) = delete;
    HuC6280& operator=(const HuC6280&) = delete;
    State& state() {
        return state_;
    }
    const State& state() const {
        return state_;
    }
    void reset(const std::array<std::uint8_t, 8>& mapping);
    void set_irq_lines(std::uint8_t lines);
    void set_irq_mask(std::uint8_t mask);
    void set_nmi_line(bool asserted);
    StepResult step();

  private:
    State storage_;
    State& state_;
    Bus bus_;
    MCS6502ExecutionContext base_{};
    unsigned waits_ = 0;
    int cached_opcode_ = -1, injected_operand_ = -1;
    unsigned delegated_write_cycle_ = 0;
    std::uint64_t instruction_time_ = 0;
    unsigned instruction_divisor_ = 1;
    bool in_step_ = false;
    void bus_wait(std::uint32_t, bool, unsigned base_cycle = 0);
    static std::uint8_t read_base(std::uint16_t, void*);
    static void write_base(std::uint16_t, std::uint8_t, void*);
    std::uint32_t physical(std::uint16_t) const;
    std::uint8_t read(std::uint16_t);
    void write(std::uint16_t, std::uint8_t, unsigned base_cycle = 0);
    void write_physical(std::uint32_t, std::uint8_t, unsigned base_cycle = 0);
    std::uint16_t word(std::uint16_t);
    std::uint16_t direct_word(std::uint8_t);
    void push(std::uint8_t);
    std::uint8_t pull();
    void nz(std::uint8_t);
    unsigned enter_interrupt(std::uint16_t, bool software);
    unsigned execute(std::uint8_t opcode, bool& valid);
    unsigned alu(std::uint8_t opcode);
    unsigned rmw(std::uint8_t opcode);
    unsigned transfer(std::uint8_t opcode);
    unsigned continue_transfer();
    bool delegate(std::uint8_t opcode, int operand = -1, unsigned write_cycle = 0);
    void initialize_base();
};
} // namespace rxpce_cpu
