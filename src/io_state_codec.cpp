#include "io_state_codec.h"
#include "state_codec.h"
#include <algorithm>
#include <utility>

namespace rxpce_state {
bool valid_io(const rxpce_machine::IoState& io, std::string& error) {
    auto reject = [&](const char* reason) {
        error = reason;
        return false;
    };
    if (!io.backup.valid())
        return reject("invalid backup gate");
    if (!io.controllers.valid())
        return reject("invalid controller bus");
    if (io.vdc_reg >= 32 || io.vce_reg.value >= 512)
        return reject("invalid video register index");
    if (io.vdc_inc != 1 && io.vdc_inc != 32 && io.vdc_inc != 64 && io.vdc_inc != 128)
        return reject("invalid VRAM increment");
    if (io.vdc_satb > 1 || io.vdc_pendvsync > 1)
        return reject("invalid VDC pending state");
    if (io.screen_w < 8 || io.screen_w > 512 || io.screen_w % 8 || io.screen_h < 1 ||
        io.screen_h > 256 || io.minline < 0 || io.maxline >= 256 || io.maxline < io.minline)
        return reject("invalid state dimensions");
    const bool width = io.bg_w == 32 || io.bg_w == 64 || io.bg_w == 128;
    const bool height = io.bg_h == 32 || io.bg_h == 64;
    if ((io.bg_w == 0) != (io.bg_h == 0) || (!width && io.bg_w != 0) || (!height && io.bg_h != 0))
        return reject("invalid background dimensions");
    if (io.psg_ch > 7)
        return reject("invalid PSG channel");
    for (unsigned ch = 0; ch < 6; ++ch) {
        if (io.wavofs[ch] >= 32 || io.psg[ch][3] > 15)
            return reject("invalid PSG position or frequency");
        for (auto value : io.wave[ch])
            if (value > 31)
                return reject("invalid PSG wave sample");
    }
    if (io.timer_reload > 127 || io.timer_counter > 127 || io.timer_start > 1 || io.irq_mask > 7 ||
        io.irq_status > 7)
        return reject("invalid timer or IRQ state");
    error.clear();
    return true;
}
std::vector<char> encode_io(const rxpce_machine::IoState& io) {
    std::string error;
    if (!valid_io(io, error))
        throw std::invalid_argument(error);
    Writer w;
    w.data.reserve(io_wire::size);
    for (const auto& value : io.vdc)
        w.number(value.value, 2);
    for (const auto& value : io.vce)
        w.number(value.value, 2);
    w.number(io.vce_reg.value, 2);
    w.number(io.vdc_inc, 2);
    for (auto value : {io.vdc_reg, io.vdc_status, io.vdc_ratch, io.vdc_satb, io.vdc_pendvsync})
        w.number(value, 1);
    for (auto value : {io.bg_h, io.bg_w, io.screen_w, io.screen_h, io.minline, io.maxline})
        w.number(static_cast<std::uint32_t>(value), 4);
    for (auto value : io.controllers.buttons)
        w.number(value, 2);
    w.block(io.controllers.modes.data(), 5);
    w.block(io.controllers.phase.data(), 5);
    for (auto value :
         {io.controllers.count, io.controllers.port, io.controllers.sel, io.controllers.clr})
        w.number(value, 1);
    for (const auto& row : io.psg)
        w.block(row.data(), 8);
    for (const auto& row : io.wave)
        w.block(row.data(), 32);
    w.block(io.wavofs.data(), 6);
    for (auto value : {io.psg_ch, io.psg_volume, io.psg_lfo_freq, io.psg_lfo_ctrl, io.timer_reload,
                       io.timer_start, io.timer_counter, io.irq_mask, io.irq_status,
                       io.backup.latch, io.backup.unlocked})
        w.number(value, 1);
    return std::move(w.data);
}
bool decode_io(const char* data, std::size_t size, rxpce_machine::IoState& output,
               std::string& error) {
    if (!data || size != io_wire::size) {
        error = "invalid I/O state length";
        return false;
    }
    try {
        Reader r(data, size);
        rxpce_machine::IoState next;
        auto byte = [&] { return static_cast<std::uint8_t>(r.number(1)); };
        auto word = [&] { return static_cast<std::uint16_t>(r.number(2)); };
        for (auto& value : next.vdc)
            value.value = word();
        for (auto& value : next.vce)
            value.value = word();
        next.vce_reg.value = word();
        next.vdc_inc = word();
        next.vdc_reg = byte();
        next.vdc_status = byte();
        next.vdc_ratch = byte();
        next.vdc_satb = byte();
        next.vdc_pendvsync = byte();
        next.bg_h = static_cast<std::int32_t>(r.signed_number(4));
        next.bg_w = static_cast<std::int32_t>(r.signed_number(4));
        next.screen_w = static_cast<std::int32_t>(r.signed_number(4));
        next.screen_h = static_cast<std::int32_t>(r.signed_number(4));
        next.minline = static_cast<std::int32_t>(r.signed_number(4));
        next.maxline = static_cast<std::int32_t>(r.signed_number(4));
        for (auto& value : next.controllers.buttons)
            value = word();
        r.block(next.controllers.modes.data(), 5);
        r.block(next.controllers.phase.data(), 5);
        next.controllers.count = byte();
        next.controllers.port = byte();
        next.controllers.sel = byte();
        next.controllers.clr = byte();
        for (auto& row : next.psg)
            r.block(row.data(), 8);
        for (auto& row : next.wave)
            r.block(row.data(), 32);
        r.block(next.wavofs.data(), 6);
        next.psg_ch = byte();
        next.psg_volume = byte();
        next.psg_lfo_freq = byte();
        next.psg_lfo_ctrl = byte();
        next.timer_reload = byte();
        next.timer_start = byte();
        next.timer_counter = byte();
        next.irq_mask = byte();
        next.irq_status = byte();
        next.backup.latch = byte();
        next.backup.unlocked = byte();
        if (r.remaining) {
            error = "trailing I/O state";
            return false;
        }
        if (!valid_io(next, error))
            return false;
        output = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception& failure) {
        error = failure.what();
        return false;
    }
}
} // namespace rxpce_state
