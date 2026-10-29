#include "pce_cpu_state.h"
namespace rxpce_state {
std::vector<char> encode_cpu(const PceCpu& value) {
    std::vector<char> data;
    auto put = [&](std::uint64_t v, unsigned count) {
        for (unsigned n = 0; n < count; ++n)
            data.push_back(char(v >> (8 * n)));
    };
    const auto& s = value.cpu;
    put(s.pc, 2);
    put(s.a, 1);
    put(s.x, 1);
    put(s.y, 1);
    put(s.sp, 1);
    put(s.p, 1);
    for (auto v : s.mpr)
        put(v, 1);
    put(s.mpr_latch, 1);
    put(s.irq_lines, 1);
    put(s.irq_mask, 1);
    put(s.irq_delay, 1);
    put((s.irq_sampled_before_sei ? 1 : 0) | (s.nmi_line ? 2 : 0) | (s.nmi_pending ? 4 : 0) |
            (s.low_speed ? 8 : 0),
        1);
    put(s.elapsed, 8);
    put(std::uint32_t(value.line_period), 4);
    put(std::uint32_t(value.line_remaining), 4);
    put(std::uint32_t(value.timer_period), 4);
    put(std::uint32_t(value.timer_remaining), 4);
    put(value.Trace ? 1 : 0, 1);
    put(value.Trap, 2);
    put(s.transfer.opcode, 1);
    put(s.transfer.source, 2);
    put(s.transfer.destination, 2);
    put(s.transfer.remaining, 4);
    put(s.transfer.alternate ? 1 : 0, 1);
    return data;
}
bool decode_cpu(const char* data, std::size_t size, int line, int timer, PceCpu& result) {
    if (!data || size != cpu_state_size || line <= 0 || timer <= 0)
        return false;
    PceCpu value;
    unsigned offset = 0;
    auto get = [&](unsigned count) {
        std::uint64_t v = 0;
        for (unsigned n = 0; n < count; ++n)
            v |= std::uint64_t(static_cast<byte>(data[offset++])) << (8 * n);
        return v;
    };
    auto& s = value.cpu;
    s.pc = word(get(2));
    s.a = byte(get(1));
    s.x = byte(get(1));
    s.y = byte(get(1));
    s.sp = byte(get(1));
    s.p = byte(get(1));
    for (auto& v : s.mpr)
        v = byte(get(1));
    s.mpr_latch = byte(get(1));
    s.irq_lines = byte(get(1));
    s.irq_mask = byte(get(1));
    s.irq_delay = byte(get(1));
    const auto flags = get(1);
    if (flags > 15 || s.irq_lines > 7 || s.irq_mask > 7 || s.irq_delay > 1)
        return false;
    s.irq_sampled_before_sei = (flags & 1) != 0;
    s.nmi_line = (flags & 2) != 0;
    s.nmi_pending = (flags & 4) != 0;
    s.low_speed = (flags & 8) != 0;
    s.elapsed = get(8);
    // Decode signed clock debt without implementation-defined unsigned casts.
    auto signed_clock = [&]() {
        const auto v = get(4);
        return v <= 0x7fffffffu ? int(v) : int(std::int64_t(v) - 0x100000000ll);
    };
    value.line_period = signed_clock();
    value.line_remaining = signed_clock();
    value.timer_period = signed_clock();
    value.timer_remaining = signed_clock();
    const auto trace = get(1);
    value.Trace = trace != 0;
    value.Trap = word(get(2));
    auto& t = s.transfer;
    t.opcode = byte(get(1));
    t.source = word(get(2));
    t.destination = word(get(2));
    t.remaining = std::uint32_t(get(4));
    const auto alternate = get(1);
    t.alternate = alternate != 0;
    const bool alternating = t.opcode == 0xe3 || t.opcode == 0xf3;
    if (alternate > 1 || t.remaining > 65536 || (!alternating && t.alternate))
        return false;
    switch (t.opcode) {
    case 0:
        if (t.source || t.destination || t.remaining || t.alternate)
            return false;
        break;
    case 0x73:
    case 0xc3:
    case 0xd3:
    case 0xe3:
    case 0xf3:
        break;
    default:
        return false;
    }
    if (trace > 1 || value.line_period != line || value.timer_period != timer ||
        value.line_remaining <= 0 || value.line_remaining > line || value.timer_remaining <= 0 ||
        value.timer_remaining > timer)
        return false;
    result = value;
    return true;
}
} // namespace rxpce_state
