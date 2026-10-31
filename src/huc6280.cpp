#include "huc6280.h"
#include <utility>
namespace rxpce_cpu {
HuC6280::HuC6280(Bus bus) : state_(storage_), bus_(bus) {
    initialize_base();
}
HuC6280::HuC6280(Bus bus, State& state) : state_(state), bus_(bus) {
    initialize_base();
}
void HuC6280::initialize_base() {
    MCS6502Init(&base_, read_base, write_base, this);
    base_.directPageBase = 0x2000;
    base_.stackPageBase = 0x2100;
}
std::uint32_t HuC6280::physical(std::uint16_t a) const {
    return (std::uint32_t(state_.mpr[a >> 13]) << 13) | (a & 8191);
}
std::uint8_t HuC6280::read(std::uint16_t a) {
    const auto p = physical(a);
    bus_wait(p, false);
    return bus_.read(p, bus_.context);
}
void HuC6280::write_physical(std::uint32_t p, std::uint8_t value, unsigned base_cycle) {
    bus_wait(p, true, base_cycle);
    bus_.write(p, value, bus_.context);
}
void HuC6280::bus_wait(std::uint32_t physical, bool write, unsigned base_cycle) {
    auto sync = [&] {
        if (in_step_ && (bus_.synchronize || bus_.ready || base_cycle)) {
            state_.elapsed =
                instruction_time_ + (std::uint64_t(base_cycle) + waits_) * instruction_divisor_;
            if (bus_.synchronize)
                bus_.synchronize(state_.elapsed, bus_.context);
            else if (base_cycle && bus_.synchronize_prefix)
                bus_.synchronize_prefix(state_.elapsed, bus_.prefix_context);
        }
    };
    sync();
    if (bus_.wait)
        waits_ += bus_.wait(physical, write, bus_.context);
    sync();
    if (in_step_ && bus_.ready) {
        unsigned extra;
        while ((extra = bus_.ready(physical, write, bus_.context)) != 0) {
            waits_ += extra;
            sync();
        }
    }
}
void HuC6280::write(std::uint16_t a, std::uint8_t v, unsigned base_cycle) {
    write_physical(physical(a), v, base_cycle);
}
std::uint16_t HuC6280::word(std::uint16_t a) {
    const unsigned lo = read(a);
    return std::uint16_t(lo | (unsigned(read(std::uint16_t(a + 1))) << 8));
}
std::uint16_t HuC6280::direct_word(std::uint8_t offset) {
    const unsigned lo = read(0x2000 + offset);
    return std::uint16_t(lo | (unsigned(read(0x2000 + std::uint8_t(offset + 1))) << 8));
}
void HuC6280::push(std::uint8_t v) {
    write(0x2100 + state_.sp, v);
    --state_.sp;
}
std::uint8_t HuC6280::pull() {
    ++state_.sp;
    return read(0x2100 + state_.sp);
}
void HuC6280::nz(std::uint8_t v) {
    state_.p = std::uint8_t((state_.p & ~(N | Z)) | (v & N) | (v ? 0 : Z));
}
std::uint8_t HuC6280::read_base(std::uint16_t a, void* p) {
    auto& self = *static_cast<HuC6280*>(p);
    // These substitutions belong to one delegated fetch each. A later data or
    // stack read at the same address must reach the real bus (including waits).
    if (self.cached_opcode_ >= 0 && a == self.state_.pc)
        return std::uint8_t(std::exchange(self.cached_opcode_, -1));
    if (self.injected_operand_ >= 0 && a == std::uint16_t(self.state_.pc + 1))
        return std::uint8_t(std::exchange(self.injected_operand_, -1));
    return self.read(a);
}
void HuC6280::write_base(std::uint16_t a, std::uint8_t v, void* p) {
    auto& self = *static_cast<HuC6280*>(p);
    self.write(a, v, self.delegated_write_cycle_);
}
bool HuC6280::delegate(std::uint8_t opcode, int operand, unsigned write_cycle) {
    auto& s = state_;
    base_.a = s.a;
    base_.x = s.x;
    base_.y = s.y;
    base_.sp = s.sp;
    base_.p = s.p;
    base_.pc = s.pc;
    cached_opcode_ = opcode;
    injected_operand_ = operand;
    delegated_write_cycle_ = write_cycle;
    const auto result = MCS6502ExecNext(&base_);
    cached_opcode_ = injected_operand_ = -1;
    delegated_write_cycle_ = 0;
    s.a = base_.a;
    s.x = base_.x;
    s.y = base_.y;
    s.sp = base_.sp;
    s.p = base_.p;
    s.pc = base_.pc;
    // A self-loop is valid execution, not a request to stop the host.
    return result != MCS6502ExecResultInvalidOperation;
}
void HuC6280::reset(const std::array<std::uint8_t, 8>& mapping) {
    const auto initial_mapping = mapping; // The caller may pass state().mpr.
    state_ = State{};
    state_.mpr = initial_mapping;
    waits_ = 0;
    state_.pc = word(0xfffe);
    waits_ = 0;
}
void HuC6280::set_irq_lines(std::uint8_t lines) {
    lines &= 7;
    if (lines & ~state_.irq_lines)
        state_.irq_delay = 1;
    state_.irq_lines = lines;
}
void HuC6280::set_irq_mask(std::uint8_t mask) {
    mask &= 7;
    if (state_.irq_mask & ~mask)
        state_.irq_delay = 1;
    state_.irq_mask = mask;
}
void HuC6280::set_nmi_line(bool asserted) {
    if (asserted && !state_.nmi_line)
        state_.nmi_pending = true;
    state_.nmi_line = asserted;
}
unsigned HuC6280::enter_interrupt(std::uint16_t vector, bool software) {
    auto& s = state_;
    push(std::uint8_t(s.pc >> 8));
    push(std::uint8_t(s.pc));
    push(std::uint8_t(software ? s.p | B : s.p & ~B));
    s.p = std::uint8_t((s.p & ~(D | T | B)) | I | (software ? B : 0));
    s.pc = word(vector);
    return software ? 8 : 7;
}
StepResult HuC6280::step() {
    waits_ = 0;
    const unsigned divisor = state_.low_speed ? 4 : 1;
    instruction_time_ = state_.elapsed;
    instruction_divisor_ = divisor;
    in_step_ = true;
    if (state_.transfer.opcode) {
        unsigned cycles = continue_transfer();
        cycles += waits_;
        const unsigned elapsed = cycles * divisor;
        state_.elapsed = instruction_time_ + elapsed;
        in_step_ = false;
        return {cycles, elapsed, true, false, state_.transfer.opcode == 0};
    }
    unsigned cycles = 0;
    bool interrupt = false, valid = true;
    const bool irq_enabled = !(state_.p & I) || state_.irq_sampled_before_sei;
    state_.irq_sampled_before_sei = false;
    const bool delayed = state_.irq_delay != 0;
    if (delayed)
        --state_.irq_delay;
    if (state_.nmi_pending) {
        state_.nmi_pending = false;
        cycles = enter_interrupt(0xfffc, false);
        interrupt = true;
    } else if (!delayed && irq_enabled && (state_.irq_lines & ~state_.irq_mask & 7)) {
        const unsigned active = state_.irq_lines & ~state_.irq_mask;
        cycles = enter_interrupt((active & Timer)  ? 0xfffa
                                 : (active & Irq1) ? 0xfff8
                                                   : 0xfff6,
                                 false);
        interrupt = true;
    } else {
        const auto opcode = read(state_.pc);
        cycles = execute(opcode, valid);
    }
    cycles += waits_;
    const unsigned elapsed = cycles * divisor;
    state_.elapsed = instruction_time_ + elapsed;
    in_step_ = false;
    return {cycles, elapsed, valid, interrupt, state_.transfer.opcode == 0};
}

// Group-1 ALU modes, plus 65C02 ($zp). Decode the operand before redirecting
// A to direct-page X; ordinary binary arithmetic is executed by MCS6502.
unsigned HuC6280::alu(std::uint8_t op) {
    auto& s = state_;
    const auto pc = s.pc;
    const unsigned mode = op & 31, group = op >> 5;
    unsigned cycles = 0, length = 2;
    std::uint16_t address = 0;
    switch (mode) {
    case 0x09:
        address = std::uint16_t(pc + 1);
        cycles = 2;
        break;
    case 0x05:
        address = 0x2000 + read(std::uint16_t(pc + 1));
        cycles = 4;
        break;
    case 0x15:
        address = 0x2000 + std::uint8_t(read(std::uint16_t(pc + 1)) + s.x);
        cycles = 4;
        break;
    case 0x0d:
        address = word(std::uint16_t(pc + 1));
        cycles = 5;
        length = 3;
        break;
    case 0x1d:
        address = std::uint16_t(word(std::uint16_t(pc + 1)) + s.x);
        cycles = 5;
        length = 3;
        break;
    case 0x19:
        address = std::uint16_t(word(std::uint16_t(pc + 1)) + s.y);
        cycles = 5;
        length = 3;
        break;
    case 0x01:
        address = direct_word(std::uint8_t(read(std::uint16_t(pc + 1)) + s.x));
        cycles = 7;
        break;
    case 0x11:
        address = std::uint16_t(direct_word(read(std::uint16_t(pc + 1))) + s.y);
        cycles = 7;
        break;
    case 0x12:
        address = direct_word(read(std::uint16_t(pc + 1)));
        cycles = 7;
        break;
    }
    const bool transfer = (s.p & T) && (group <= 3 || group == 7);
    s.p &= ~T;
    if (group == 4) {
        write(address, s.a, cycles);
    } else {
        const auto operand = read(address);
        const auto saved_a = s.a;
        if (transfer) {
            s.a = read(0x2000 + s.x);
            cycles += 3;
        }
        if ((group == 3 || group == 7) && (s.p & D)) {
            // Digit adjustment also avoids the upstream decimal-to-integer
            // conversion's fallback-to-zero on non-BCD inputs.
            const int carry = (s.p & C) ? 1 : 0;
            int lo, hi;
            bool carry_out;
            if (group == 3) {
                lo = (s.a & 15) + (operand & 15) + carry;
                if (lo > 9)
                    lo += 6;
                hi = (s.a >> 4) + (operand >> 4) + (lo > 15);
                if (hi > 9)
                    hi += 6;
                carry_out = hi > 15;
            } else {
                lo = (s.a & 15) - (operand & 15) - (1 - carry);
                hi = (s.a >> 4) - (operand >> 4);
                if (lo < 0) {
                    lo -= 6;
                    --hi;
                }
                if (hi < 0)
                    hi -= 6;
                carry_out = int(s.a) - int(operand) - (1 - carry) >= 0;
            }
            s.a = std::uint8_t((hi * 16 & 0xf0) | (lo & 15));
            s.p = std::uint8_t((s.p & ~C) | (carry_out ? C : 0));
            nz(s.a);
            ++cycles;
        } else {
            delegate(std::uint8_t((op & 0xe0) | 0x09), operand);
        }
        if (transfer) {
            write(0x2000 + s.x, s.a);
            s.a = saved_a;
        }
    }
    s.pc = std::uint16_t(pc + length);
    return cycles;
}

unsigned HuC6280::transfer(std::uint8_t op) {
    auto& s = state_;
    // Decode everything before writes, including transfers over their own operands.
    const auto source = word(std::uint16_t(s.pc + 1));
    const auto dest = word(std::uint16_t(s.pc + 3));
    const auto count = word(std::uint16_t(s.pc + 5));
    s.pc = std::uint16_t(s.pc + 7);
    push(s.y);
    push(s.a);
    push(s.x);
    s.transfer = {op, source, dest, count ? unsigned(count) : 65536u, false};
    // Approximate access placement: setup 14, each byte 6, finish 3 clocks.
    return 14;
}
unsigned HuC6280::continue_transfer() {
    auto& t = state_.transfer;
    if (!t.remaining) {
        // Read the actual stack: the transfer may have overwritten it.
        state_.x = pull();
        state_.a = pull();
        state_.y = pull();
        t = {};
        return 3;
    }
    const auto from = std::uint16_t(t.source + (t.opcode == 0xf3 && t.alternate ? 1 : 0));
    const auto to = std::uint16_t(t.destination + (t.opcode == 0xe3 && t.alternate ? 1 : 0));
    const auto value = read(from);
    write(to, value);
    if (t.opcode == 0xc3) {
        --t.source;
        --t.destination;
    } else {
        if (t.opcode != 0xf3)
            ++t.source;
        if (t.opcode == 0x73 || t.opcode == 0xf3)
            ++t.destination;
    }
    --t.remaining;
    if (t.opcode == 0xe3 || t.opcode == 0xf3)
        t.alternate = !t.alternate;
    return 6;
}

// Keep the decoded address local to this instruction. Data reads must still reach
// the bus when they alias opcode/operand addresses or another MPR's physical bank.
unsigned HuC6280::rmw(std::uint8_t op) {
    auto& s = state_;
    const auto pc = s.pc;
    const unsigned mode = op & 31;
    const bool absolute = mode == 14 || mode == 30;
    std::uint16_t address;
    if (absolute) {
        address = word(std::uint16_t(pc + 1));
        if (mode == 30)
            address = std::uint16_t(address + s.x);
    } else {
        const auto offset = read(std::uint16_t(pc + 1));
        address = std::uint16_t(0x2000 + std::uint8_t(offset + (mode == 22 ? s.x : 0)));
    }
    const auto input = read(address);
    const unsigned carry = s.p & C;
    std::uint8_t value = input;
    switch (op >> 5) {
    case 0: // ASL
        value = std::uint8_t(input << 1);
        s.p = std::uint8_t((s.p & ~C) | (input >> 7));
        break;
    case 1: // ROL
        value = std::uint8_t((input << 1) | carry);
        s.p = std::uint8_t((s.p & ~C) | (input >> 7));
        break;
    case 2: // LSR
        value = std::uint8_t(input >> 1);
        s.p = std::uint8_t((s.p & ~C) | (input & 1));
        break;
    case 3: // ROR
        value = std::uint8_t((input >> 1) | (carry << 7));
        s.p = std::uint8_t((s.p & ~C) | (input & 1));
        break;
    case 6: // DEC
        --value;
        break;
    case 7: // INC
        ++value;
        break;
    }
    nz(value);
    write(address, value);
    s.pc = std::uint16_t(pc + (absolute ? 3 : 2));
    return absolute ? 7 : 6;
}

unsigned HuC6280::execute(std::uint8_t op, bool& valid) {
    auto& s = state_;
    const auto pc = s.pc;
    const unsigned mode = op & 31;
    if (op != 0x89 && (mode == 1 || mode == 5 || mode == 9 || mode == 13 || mode == 17 ||
                       mode == 18 || mode == 21 || mode == 25 || mode == 29))
        return alu(op);
    const auto old_p = s.p;
    s.p &= ~T;
    const unsigned family = op & 0xe0;
    if ((family <= 0x60 || family >= 0xc0) && (mode == 6 || mode == 22 || mode == 14 || mode == 30))
        return rmw(op);
    if ((op & 15) == 7) {
        const auto address = std::uint16_t(0x2000 + read(std::uint16_t(pc + 1)));
        const auto value = read(address);
        const unsigned bit = 1u << ((op >> 4) & 7);
        write(address, std::uint8_t((op & 0x80) ? value | bit : value & ~bit));
        s.pc = std::uint16_t(pc + 2);
        return 7;
    }
    if ((op & 15) == 15) {
        const auto value = read(0x2000 + read(std::uint16_t(pc + 1)));
        const auto offset = static_cast<std::int8_t>(read(std::uint16_t(pc + 2)));
        const bool taken = bool(value & (1u << ((op >> 4) & 7))) == bool(op & 0x80);
        s.pc = std::uint16_t(pc + 3 + (taken ? offset : 0));
        return taken ? 8 : 6;
    }
    if ((op & 31) == 16) {
        const unsigned flags[] = {N, V, C, Z};
        const bool taken = bool(s.p & flags[op >> 6]) == bool(op & 0x20);
        const auto offset = static_cast<std::int8_t>(read(std::uint16_t(pc + 1)));
        s.pc = std::uint16_t(pc + 2 + (taken ? offset : 0));
        return taken ? 4 : 2;
    }
    switch (op) {
    case 0x02:
        std::swap(s.x, s.y);
        ++s.pc;
        return 3;
    case 0x22:
        std::swap(s.a, s.x);
        ++s.pc;
        return 3;
    case 0x42:
        std::swap(s.a, s.y);
        ++s.pc;
        return 3;
    case 0x62:
        s.a = 0;
        ++s.pc;
        return 2;
    case 0x82:
        s.x = 0;
        ++s.pc;
        return 2;
    case 0xc2:
        s.y = 0;
        ++s.pc;
        return 2;
    case 0x03:
    case 0x13:
    case 0x23: {
        const auto value = read(std::uint16_t(pc + 1));
        write_physical(0x1fe000 + (op == 3 ? 0 : op == 0x13 ? 2 : 3), value, 4);
        s.pc = std::uint16_t(pc + 2);
        return 4;
    }
    case 0x43:
    case 0x53: {
        const auto mask = read(std::uint16_t(pc + 1));
        if (op == 0x53) {
            if (mask)
                s.mpr_latch = s.a;
            for (unsigned n = 0; n < 8; ++n)
                if (mask & (1u << n))
                    s.mpr[n] = s.a;
        } else {
            s.a = s.mpr_latch;
            // Multi-bit TMA is unspecified; choose the highest selected MPR.
            for (unsigned n = 0; n < 8; ++n)
                if (mask & (1u << n))
                    s.a = s.mpr[n];
        }
        s.pc = std::uint16_t(pc + 2);
        return op == 0x43 ? 4 : 5;
    }
    case 0x54:
    case 0xd4:
        s.low_speed = op == 0x54;
        ++s.pc;
        return 3;
    case 0xf4:
        s.p |= T;
        ++s.pc;
        return 2;
    case 0x1a:
    case 0x3a:
        s.a = std::uint8_t(s.a + (op == 0x1a ? 1 : -1));
        nz(s.a);
        ++s.pc;
        return 2;
    case 0xda:
    case 0x5a:
        push(op == 0xda ? s.x : s.y);
        ++s.pc;
        return 3;
    case 0xfa:
    case 0x7a: {
        const auto value = pull();
        if (op == 0xfa)
            s.x = value;
        else
            s.y = value;
        nz(value);
        ++s.pc;
        return 4;
    }
    case 0x08:
        push(s.p);
        ++s.pc;
        return 3;
    case 0x28:
    case 0x40: {
        s.p = pull() | B;
        if (op == 0x40) {
            const unsigned lo = pull();
            s.pc = std::uint16_t(lo | (unsigned(pull()) << 8));
        } else
            ++s.pc;
        if (!(s.p & I))
            s.irq_delay = 1;
        return op == 0x28 ? 4 : 7;
    }
    case 0x00:
        s.pc = std::uint16_t(pc + 2);
        return enter_interrupt(0xfff6, true);
    case 0x80:
    case 0x44: {
        const auto offset = static_cast<std::int8_t>(read(std::uint16_t(pc + 1)));
        if (op == 0x44) {
            push(std::uint8_t((pc + 1) >> 8));
            push(std::uint8_t(pc + 1));
        }
        s.pc = std::uint16_t(pc + 2 + offset);
        return op == 0x44 ? 8 : 4;
    }
    case 0x6c:
    case 0x7c: {
        auto address = word(std::uint16_t(pc + 1));
        if (op == 0x7c)
            address = std::uint16_t(address + s.x);
        s.pc = word(address);
        return 7;
    }
    case 0x64:
    case 0x74:
    case 0x9c:
    case 0x9e: {
        auto address = (op & 8) ? word(std::uint16_t(pc + 1))
                                : std::uint16_t(0x2000 + read(std::uint16_t(pc + 1)));
        if (op == 0x74)
            address = 0x2000 + std::uint8_t(address + s.x);
        if (op == 0x9e)
            address = std::uint16_t(address + s.x);
        write(address, 0, (op & 8) ? 5 : 4);
        s.pc = std::uint16_t(pc + ((op & 8) ? 3 : 2));
        return (op & 8) ? 5 : 4;
    }
    case 0x04:
    case 0x14:
    case 0x0c:
    case 0x1c: {
        const auto address = (op & 8) ? word(std::uint16_t(pc + 1))
                                      : std::uint16_t(0x2000 + read(std::uint16_t(pc + 1)));
        const auto value = read(address);
        const auto result = std::uint8_t((op & 0x10) ? value & ~s.a : value | s.a);
        s.p = std::uint8_t((s.p & ~(N | V | Z)) | (value & (N | V)) | (result ? 0 : Z));
        write(address, result);
        s.pc = std::uint16_t(pc + ((op & 8) ? 3 : 2));
        return (op & 8) ? 7 : 6;
    }
    case 0x89:
    case 0x34:
    case 0x3c: {
        std::uint16_t address = std::uint16_t(pc + 1);
        if (op == 0x34)
            address = 0x2000 + std::uint8_t(read(address) + s.x);
        if (op == 0x3c)
            address = std::uint16_t(word(address) + s.x);
        const auto value = read(address);
        s.p = std::uint8_t((s.p & ~(N | V | Z)) | (value & (N | V)) | ((s.a & value) ? 0 : Z));
        s.pc = std::uint16_t(pc + (op == 0x3c ? 3 : 2));
        return op == 0x89 ? 2 : op == 0x34 ? 4 : 5;
    }
    case 0x83:
    case 0xa3:
    case 0x93:
    case 0xb3: {
        const auto mask = read(std::uint16_t(pc + 1));
        auto address = (op & 0x10) ? word(std::uint16_t(pc + 2))
                                   : std::uint16_t(0x2000 + read(std::uint16_t(pc + 2)));
        if (op & 0x20)
            address = (op & 0x10) ? std::uint16_t(address + s.x)
                                  : std::uint16_t(0x2000 + std::uint8_t(address + s.x));
        const auto value = read(address);
        s.p = std::uint8_t((s.p & ~(N | V | Z)) | (value & (N | V)) | ((mask & value) ? 0 : Z));
        s.pc = std::uint16_t(pc + ((op & 0x10) ? 4 : 3));
        return (op & 0x10) ? 8 : 7;
    }
    case 0x73:
    case 0xc3:
    case 0xd3:
    case 0xe3:
    case 0xf3:
        return transfer(op);
    }
    // Only ordinary STX/STY delegate a write at the instruction's trailing edge.
    // RMW, stack and all reads retain their separately documented models.
    const unsigned write_cycle = op == 0x8c || op == 0x8e                               ? 5
                                 : op == 0x84 || op == 0x94 || op == 0x86 || op == 0x96 ? 4
                                                                                        : 0;
    if (!delegate(op, -1, write_cycle)) {
        s.p = old_p;
        valid = false;
        return 0;
    }
    if (op == 0x58 && (old_p & I))
        s.irq_delay = 1;
    // SEI cannot revoke an IRQ sampled while the previous I flag was clear.
    if (op == 0x78 && !(old_p & I) && (s.irq_lines & ~s.irq_mask & 7))
        s.irq_sampled_before_sei = true;
    if (op == 0x20 || op == 0x60)
        return 7;
    if (op == 0x4c)
        return 4;
    switch (op) {
    case 0xc0:
        return 2; // CPY immediate: upstream 6502 table has an extra cycle.
    case 0x24:
    case 0x84:
    case 0x94:
    case 0x86:
    case 0x96:
    case 0xa4:
    case 0xb4:
    case 0xa6:
    case 0xb6:
    case 0xc4:
    case 0xe4:
        return 4;
    case 0x2c:
    case 0x8c:
    case 0x8e:
    case 0xac:
    case 0xbc:
    case 0xae:
    case 0xbe:
    case 0xcc:
    case 0xec:
        return 5;
    default:
        return base_.timingForLastOperation;
    }
}
} // namespace rxpce_cpu
