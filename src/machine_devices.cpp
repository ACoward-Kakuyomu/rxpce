// Device behavior extracted from the original rxpce implementation.
// Portable PC-Engine Emulator, 1998 BERO; modified 1998 hmmx.
#include "machine_devices.h"
#include "machine.h"
#include "machine_vdc.h"
#include "vdc_registers.h"
#include <algorithm>
#include <cstdarg>

namespace rxpce_machine {
using namespace vdc;
namespace {
constexpr int MAXDISP = 227;
#ifdef _DEBUG
void TraceHost(const DeviceHost& host, const char* format, ...) {
    if (!host.trace)
        return;
    std::va_list args;
    va_start(args, format);
    host.trace(host.context, format, args);
    va_end(args);
}
#define TRACE(...) TraceHost(host, __VA_ARGS__)
#else
#define TRACE(...) ((void)0)
#endif
} // namespace
void AttachMachineVideo(Machine& m, const DeviceHost& host, FrameSurface frame) {
    m.video_ports = host;
    m.video_surface = frame;
    m.video.output_bound = true;
    m.video.next_render = !host.begin_frame || host.begin_frame(host.context);
    m.video.render_frame = m.video.next_render;
}
void ResetVideoAfterLoad(Machine& m, FrameSurface frame) {
    m.video.render_frame = m.video.next_render = false;
    m.video.row_bias =
        (std::max)(0, ((std::min)(m.io.screen_h, m.frame_lines) - int(frame_height)) / 2);
    m.video.priority.fill(0);
    m.video.uses_sprite_priority = 0;
    m.frame_boundary_pending = false;
    std::fill_n(m.graphics.background_dirty(), GraphicsMemory::background_tiles, 1);
    std::fill_n(m.graphics.sprite_dirty(), GraphicsMemory::sprite_tiles, 1);
    if (frame.pixels)
        std::fill_n(frame.pixels, frame.count, m.video.palette[0]);
}
DisplayRectangle VisibleRectangle(const Machine& m) {
    const auto rows = (std::min)({int(frame_height), m.io.screen_h, m.frame_lines});
    const auto height = (std::min)(MAXDISP + 1, rows);
    return {(int(frame_width) - m.io.screen_w) / 2, (rows - height) / 2, m.io.screen_w, height};
}
void RenderMachineScanline(Machine& m, FrameSurface frame, int row) {
    if (!m.video.render_frame || !frame.pixels || row < m.video.row_bias ||
        row >= m.video.row_bias + int(frame_height))
        return;
    const auto area = VisibleRectangle(m);
    const auto output = row - m.video.row_bias;
    if (output < area.y || output >= area.y + area.height)
        return;
    const auto control = m.vdc.scan.state().active.control;
    const auto plan =
        (control & 0x40) ? EvaluateMachineSprites(m, row) : rxpce_video::SpriteLinePlan{};
    ClearVideoRows(m, frame, row, row);
    m.video.uses_sprite_priority = 0;
    if ((control & 0x40) && m.video.sprites_enabled)
        RenderSpriteLine(m, frame, row, 0, plan);
    const auto& scan = m.vdc.scan.state();
    RenderBackground(m, frame, row, row,
                     {scan.active.control, scan.active.x_scroll, int(scan.y_counter) - row});
    if ((control & 0x40) && m.video.sprites_enabled)
        RenderSpriteLine(m, frame, row, 1, plan);
    if (m.vce_clock.state().control & 0x80) {
        auto* pixels = frame.pixels + output * frame_width;
        for (unsigned x = 0; x < frame_width; ++x)
            pixels[x] |= rxpce_video::monochrome_bit;
    }
}
void PresentMachineFrame(Machine& m, const DeviceHost& host, FrameSurface frame,
                         std::uint64_t master) {
    if (host.sync_audio)
        host.sync_audio(host.context,
                        static_cast<std::uint32_t>(m.video_clock.cpu_deadline(master)));
    if (m.video.render_frame && host.present && frame.pixels)
        host.present(host.context, frame, VisibleRectangle(m));
    m.video.next_render = host.frame_start ? host.frame_start(host.context, master)
                                           : (!host.begin_frame || host.begin_frame(host.context));
}
void WriteMachineIo(Machine& machine, const DeviceHost& host, std::uint16_t A, std::uint8_t V) {
    AdvanceMachineVdc(machine, machine.cpu.cpu.elapsed);

    switch (A & 0x1C00) {
    case 0x0000: {
        const auto port = A & 3;
        if (port == 0) {
            machine.io.vdc_reg = V & 31;
            return;
        }
        if (port == 1)
            return;
        const auto reg = machine.io.vdc_reg;
        if (reg == VWR) {
            if (port == 2) {
                machine.io.vdc_ratch = V;
                return;
            }
            const auto value =
                static_cast<std::uint16_t>(machine.io.vdc_ratch | (unsigned(V) << 8));
            if (machine.vdc_mode == rxpce_video::VdcMode::fast) {
                WriteVramDirect(machine, machine.io.vdc[MAWR].value, value);
                machine.io.vdc[MAWR].value += machine.io.vdc_inc;
            } else
                QueueVramWrite(machine, machine.io.vdc[MAWR].value, value);
            return;
        }
        if (port == 2)
            machine.io.vdc[reg].set_low(V);
        else
            machine.io.vdc[reg].set_high(V);
        WriteMachineVdcRegister(machine, reg);
        if (port == 3) {
            if (reg == MARR && machine.vdc_mode == rxpce_video::VdcMode::accurate)
                QueueVramRead(machine, machine.io.vdc[MARR].value);
            if (reg == CR) {
                constexpr byte increments[] = {1, 32, 64, 128};
                machine.io.vdc_inc = increments[(V >> 3) & 3];
            }
            if (reg == LENR) {
                if (machine.vdc_mode == rxpce_video::VdcMode::fast)
                    TransferFastVram(machine);
                else
                    machine.vdc.dma.request_vram(
                        machine.io.vdc[SOUR].value, machine.io.vdc[DISTR].value,
                        machine.io.vdc[LENR].value, machine.io.vdc[DCR].value);
            }
            if (reg == SATB)
                machine.vdc.dma.request_satb(machine.io.vdc[SATB].value);
        }
        return;
    }

    case 0x0400: /* VCE */
        switch (A & 7) {
        case 0:
            machine.vce_clock.write_control(V);
            return;
        case 2:
            machine.io.vce_reg.set_low(V);
            return;
        case 3:
            machine.io.vce_reg.set_high(V & 1);
            return;
        case 4:
            machine.io.vce[machine.io.vce_reg.value].set_low(V);
            rxpce_video::set_palette_color(machine.video.palette.data(), machine.io.vce_reg.value,
                                           machine.io.vce[machine.io.vce_reg.value].value);
            return;
        case 5:
            machine.io.vce[machine.io.vce_reg.value].set_high(V);
            rxpce_video::set_palette_color(machine.video.palette.data(), machine.io.vce_reg.value,
                                           machine.io.vce[machine.io.vce_reg.value].value);
            machine.io.vce_reg.value = (machine.io.vce_reg.value + 1) & 0x1FF;
            return;

        case 1:
            TRACE("VCE 1, V=%X\n", V);
            return;
        case 6:
            TRACE("VCE 6, V=%X\n", V);
            return;
        case 7:
            TRACE("VCE 7, V=%X\n", V);
            return;
        }
        break;

    case 0x0800: /* PSG */
        if ((A & 15) >= 2 && (A & 15) <= 7 && machine.io.psg_ch >= 6)
            return;
        switch (A & 15) {
        case 0:
            machine.io.psg_ch = V & 7;
            break;
        case 1:
            machine.io.psg_volume = V;
            break;
        case 2:
            machine.io.psg[machine.io.psg_ch][2] = V;
            break;
        case 3:
            machine.io.psg[machine.io.psg_ch][3] = V & 15;
            break;
        case 4:
            if ((V & 0xc0) == 0x40 ||
                ((machine.io.psg[machine.io.psg_ch][4] & 0x40) && !(V & 0x40)))
                machine.io.wavofs[machine.io.psg_ch] = 0;
            machine.io.psg[machine.io.psg_ch][4] = V;
            break;
        case 5:
            machine.io.psg[machine.io.psg_ch][5] = V;
            break;
        case 6:
            if (machine.io.psg[machine.io.psg_ch][4] & 0x40) {
                machine.io.psg[machine.io.psg_ch][6] = V & 31;
            } else if (!(machine.io.psg[machine.io.psg_ch][4] & 0x80)) {
                machine.io.wave[machine.io.psg_ch][machine.io.wavofs[machine.io.psg_ch]] = V & 31;
                machine.io.wavofs[machine.io.psg_ch] =
                    (machine.io.wavofs[machine.io.psg_ch] + 1) & 31;
            }
            break;
        case 7:
            machine.io.psg[machine.io.psg_ch][7] = V;
            break;
        case 8:
            machine.io.psg_lfo_freq = V;
            break;
        case 9:
            machine.io.psg_lfo_ctrl = V;
            break;
        default:
            TRACE("ignored PSG write\n");
        }
        if (host.psg_changed)
            host.psg_changed(host.context, static_cast<std::uint8_t>(A & 15), V);
        return;

    case 0x0c00: /* timer */

        switch (A & 1) {
        case 0:
            machine.io.timer_reload = V & 127;
            return;
        case 1:
            V &= 1;
            if (V && !machine.io.timer_start) {
                machine.io.timer_counter = machine.io.timer_reload;
                machine.cpu.timer_remaining = machine.cpu.timer_period;
            }
            machine.io.timer_start = V;
            return;
        }
        break;

    case 0x1000: /* joypad */

        machine.io.controllers.write(V);
        return;

    case 0x1400: /* IRQ */
        switch (A & 15) {
        case 2:
            machine.io.irq_mask = V & 7;
            return;
        case 3:
            machine.io.irq_status &= ~TIRQ;
            return;
        }
        break;

    case 0x1800:
        if ((A & 15) == 7) {
            machine.io.backup.unlock(V);
            return;
        }
        if (machine.cd_session) {
            machine.cd_controller.write(A & 15, V, machine.cpu.cpu.elapsed);
            machine.io.irq_status =
                (machine.io.irq_status & ~IRQ2) | (machine.cd_controller.irq() ? IRQ2 : 0);
            return;
        }
        break;
    }
    TRACE("ignore I/O write %04x,%02x\n", A, V);
}

/* read */
std::uint8_t ReadMachineIo(Machine& machine, const DeviceHost& host, std::uint16_t A) {
    AdvanceMachineVdc(machine, machine.cpu.cpu.elapsed);
    byte ret;

    switch (A & 0x1C00) {
    case 0x0000: /* VDC */
        switch (A & 3) {
        case 0:
            ret = static_cast<byte>(
                (machine.io.vdc_status & ~0x40) |
                (machine.vdc.cpu.kind != rxpce_video::CpuVramKind::none ? 0x40 : 0));
            machine.io.irq_status &= ~IRQ1;
            machine.io.vdc_status = 0;
            return ret;
        case 1:
            return 0;
        case 2:
            if (machine.io.vdc_reg == VRR)
                return static_cast<byte>(machine.vdc_mode == rxpce_video::VdcMode::fast
                                             ? ReadVramDirect(machine, machine.io.vdc[MARR].value)
                                             : machine.vdc.cpu.read_buffer);
            else
                return machine.io.vdc[machine.io.vdc_reg].low();
        case 3:
            if (machine.io.vdc_reg == VRR) {
                ret = static_cast<byte>((machine.vdc_mode == rxpce_video::VdcMode::fast
                                             ? ReadVramDirect(machine, machine.io.vdc[MARR].value)
                                             : machine.vdc.cpu.read_buffer) >>
                                        8);
                machine.io.vdc[MARR].value += machine.io.vdc_inc;
                if (machine.vdc_mode == rxpce_video::VdcMode::accurate)
                    QueueVramRead(machine, machine.io.vdc[MARR].value);

                return ret;
            } else
                return machine.io.vdc[machine.io.vdc_reg].high();
        }
        break;

    case 0x0400: /* VCE */
        switch (A & 7) {
        case 4:
            return machine.io.vce[machine.io.vce_reg.value & 0x1ff].low();
        case 5: {
            const auto value = machine.io.vce[machine.io.vce_reg.value & 0x1ff].high();
            machine.io.vce_reg.value = word((machine.io.vce_reg.value + 1) & 0x1ff);
            return value;
        }
        }
        break;

    case 0x0800: /* PSG */
        if ((A & 15) >= 2 && (A & 15) <= 7 && machine.io.psg_ch >= 6)
            return NODATA;
        switch (A & 15) {
        case 0:
            return machine.io.psg_ch;
        case 1:
            return machine.io.psg_volume;
        case 2:
            return machine.io.psg[machine.io.psg_ch][2];
        case 3:
            return machine.io.psg[machine.io.psg_ch][3];
        case 4:
            return machine.io.psg[machine.io.psg_ch][4];
        case 5:
            return machine.io.psg[machine.io.psg_ch][5];
        case 6: {
            int ofs = machine.io.wavofs[machine.io.psg_ch];
            machine.io.wavofs[machine.io.psg_ch] = (machine.io.wavofs[machine.io.psg_ch] + 1) & 31;
            return machine.io.wave[machine.io.psg_ch][ofs];
        }
        case 7:
            return machine.io.psg[machine.io.psg_ch][7];
        case 8:
            return machine.io.psg_lfo_freq;
        case 9:
            return machine.io.psg_lfo_ctrl;
        default:
            return NODATA;
        }
        break;

    case 0x0c00: /* timer */
        return machine.io.timer_counter;

    case 0x1000: /* joypad */
    {
        const byte value = machine.io.controllers.read();
        if (host.controller_read)
            host.controller_read(host.context, value);
        return value | machine.configuration.joy_signature();
    }

    case 0x1400: /* IRQ */
        switch (A & 15) {
        case 2:
            return machine.io.irq_mask;
        case 3:
            return machine.io.irq_status;
        }
        break;

    case 0x1800:
        if (machine.cd_session && machine.configuration.kind == rxpce_machine::Kind::super_cdrom &&
            (A & 0xc8) == 0xc0) {
            switch (A & 7) {
            case 1:
            case 5:
                return 0xaa;
            case 2:
            case 6:
                return 0x55;
            case 7:
                return 3;
            default:
                return 0;
            }
        }
        if ((A & 15) == 3)
            machine.io.backup.lock();
        if ((A & 15) == 7)
            return machine.io.backup.status();
        if (machine.cd_session) {
            const auto result = machine.cd_controller.read(A & 15, machine.cpu.cpu.elapsed);
            machine.io.irq_status =
                (machine.io.irq_status & ~IRQ2) | (machine.cd_controller.irq() ? IRQ2 : 0);
            return result;
        }
        if ((A & 15) == 3)
            return 0;
        break;
    }
    TRACE("ignore I/O read %04x\n", A);
    return NODATA;
}

void TickMachineLine(Machine& machine, const DeviceHost& host, FrameSurface frame,
                     std::uint64_t time) {
    if (!machine.video.output_bound)
        AttachMachineVideo(machine, host, frame);
    else {
        machine.video_ports = host;
        machine.video_surface = frame;
    }
    AdvanceMachineVdc(machine, time);
    if (host.sync_audio)
        host.sync_audio(host.context, static_cast<std::uint32_t>(time));
}
void TickMachineTimer(Machine& machine) {
    if (!machine.io.timer_start)
        return;
    if (machine.io.timer_counter == 0) {
        machine.io.timer_counter = machine.io.timer_reload;
        machine.io.irq_status |= TIRQ;
    } else
        --machine.io.timer_counter;
}
} // namespace rxpce_machine
