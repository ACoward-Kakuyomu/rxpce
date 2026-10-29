// Device behavior extracted from the original rxpce implementation.
// Portable PC-Engine Emulator, 1998 BERO; modified 1998 hmmx.
#include "machine_devices.h"
#include "machine.h"
#include "vdc_registers.h"
#include "vram_dma.h"
#include <algorithm>
#include <cstdarg>

namespace rxpce_machine {
using namespace vdc;
namespace {
constexpr int MAXDISP = 227;
enum PceEvent { PceNone, PceIrq };
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
void FlushVideoRows(Machine& machine, FrameSurface frame, int end) {
    const auto area = VisibleRectangle(machine);
    const int first = std::max(machine.video.previous_line, area.y);
    end = std::min(end, area.y + area.height);
    if (end <= first || !machine.video.render_frame)
        return;
    ClearVideoRows(machine, frame, first, end - 1);
    if ((machine.io.vdc[CR].value & 0x40) && machine.video.sprites_enabled)
        RenderSprites(machine, frame, first, end, 0);
    RenderBackground(machine, frame, first, end - 1);
    if ((machine.io.vdc[CR].value & 0x40) && machine.video.sprites_enabled)
        RenderSprites(machine, frame, first, end, 1);
    machine.video.previous_line = end;
}
void FlushScrollChange(Machine& machine, FrameSurface frame, int end) {
    if (!machine.video.scroll_changed)
        return;
    const auto x = machine.io.vdc[BXR].value, y = machine.io.vdc[BYR].value;
    const auto difference = machine.video.scroll_y_difference;
    machine.io.vdc[BXR].value = machine.video.old_scroll_x;
    machine.io.vdc[BYR].value = machine.video.old_scroll_y;
    machine.video.scroll_y_difference = machine.video.old_scroll_y_difference;
    FlushVideoRows(machine, frame, end);
    machine.io.vdc[BXR].value = x;
    machine.io.vdc[BYR].value = y;
    machine.video.scroll_y_difference = difference;
    machine.video.scroll_changed = 0;
}
void FlushBeforePaletteWrite(Machine& machine, const DeviceHost& host) {
    if (!host.frame || !machine.video.render_frame || machine.scanline < machine.io.minline ||
        machine.scanline > machine.io.maxline)
        return;
    const auto frame = host.frame(host.context);
    FlushScrollChange(machine, frame, machine.scanline);
    FlushVideoRows(machine, frame, machine.scanline);
}
void PresentScreen(Machine& machine, const DeviceHost& host, FrameSurface frame) {
    if (host.present)
        host.present(host.context, frame, VisibleRectangle(machine));
}
} // namespace
void ResetVideoAfterLoad(Machine& machine, FrameSurface frame) {
    auto& v = machine.video;
    v.render_frame = false;
    v.previous_line = 0;
    v.scroll_changed = v.scroll_y_difference = v.old_scroll_y_difference = 0;
    v.old_scroll_x = v.old_scroll_y = 0;
    v.uses_sprite_priority = 0;
    v.priority.fill(0);
    machine.frame_boundary_pending = false;
    std::fill_n(machine.graphics.background_dirty(), GraphicsMemory::background_tiles, 1);
    std::fill_n(machine.graphics.sprite_dirty(), GraphicsMemory::sprite_tiles, 1);
    if (frame.pixels)
        std::fill_n(frame.pixels, frame.count, v.palette[0]);
}
DisplayRectangle VisibleRectangle(const Machine& machine) {
    const int first =
        machine.io.maxline - machine.io.minline > MAXDISP
            ? machine.io.minline + ((machine.io.maxline - machine.io.minline - MAXDISP + 1) >> 1)
            : machine.io.minline;
    const int last =
        machine.io.maxline - machine.io.minline > MAXDISP
            ? machine.io.maxline - ((machine.io.maxline - machine.io.minline - MAXDISP + 1) >> 1)
            : machine.io.maxline;
    return {(int(frame_width) - machine.io.screen_w) / 2, first, machine.io.screen_w,
            last - first + 1};
}
void WriteMachineIo(Machine& machine, const DeviceHost& host, std::uint16_t A, std::uint8_t V) {

    switch (A & 0x1C00) {
    case 0x0000: /* VDC */
        switch (A & 3) {
        case 0:
            machine.io.vdc_reg = V & 31;
            return;
        case 1:
            return;
        case 2:

            switch (machine.io.vdc_reg) {
            case VWR:
                machine.io.vdc_ratch = V;

                return;
            case HDR:
                machine.io.screen_w = std::min((V + 1) * 8, 512);

                break;
            case MWR: {
                static constexpr byte bgw[] = {32, 64, 128, 128};
                machine.io.bg_h = (V & 0x40) ? 64 : 32;
                machine.io.bg_w = bgw[(V >> 4) & 3];
            }
                TRACE("bg:%dx%d, V:%X\n", machine.io.bg_w, machine.io.bg_h, V);

                break;
            case BYR:
                if (!machine.video.scroll_changed) {
                    machine.video.old_scroll_x = machine.io.vdc[BXR].value;
                    machine.video.old_scroll_y = machine.io.vdc[BYR].value;
                    machine.video.old_scroll_y_difference = machine.video.scroll_y_difference;
                }
                machine.io.vdc[BYR].set_low(V);
                machine.video.scroll_changed = 1;

                machine.video.scroll_y_difference = machine.scanline - 1;

                return;
            case BXR:
                if (!machine.video.scroll_changed) {
                    machine.video.old_scroll_x = machine.io.vdc[BXR].value;
                    machine.video.old_scroll_y = machine.io.vdc[BYR].value;
                    machine.video.old_scroll_y_difference = machine.video.scroll_y_difference;
                }
                machine.io.vdc[BXR].set_low(V);
                machine.video.scroll_changed = 1;
                return;

#define PRINT_VDC_L(REG)                                                                           \
    case REG:                                                                                      \
        TRACE(#REG "l: %X\n", V);                                                                  \
        break;

                PRINT_VDC_L(VPR);
                PRINT_VDC_L(VDW);
                PRINT_VDC_L(VCR);
                PRINT_VDC_L(DCR);
            }
            machine.io.vdc[machine.io.vdc_reg].set_low(V);

            if (machine.io.vdc_reg > 19) {
                TRACE("ignore write lo vdc%d,%02x\n", machine.io.vdc_reg, V);
            }
            return;
        case 3:

            switch (machine.io.vdc_reg) {
            case VWR:

                machine.graphics.vram()[machine.io.vdc[MAWR].value * 2] = machine.io.vdc_ratch;
                machine.graphics.vram()[machine.io.vdc[MAWR].value * 2 + 1] = V;

                machine.graphics.background_dirty()[machine.io.vdc[MAWR].value / 16] = 1;
                machine.graphics.sprite_dirty()[machine.io.vdc[MAWR].value / 64] = 1;
                machine.io.vdc[MAWR].value += machine.io.vdc_inc;
                return;
            case VDW:

                machine.io.vdc[VDW].set_high(V);
                machine.io.screen_h = std::min((machine.io.vdc[VDW].value & 511) + 1, 256);
                machine.io.maxline = machine.io.screen_h - 1;
                TRACE("VDWh: %X\n", machine.io.vdc[VDW].value);
                return;
            case LENR:

                machine.io.vdc[LENR].set_high(V);
                /* machine.graphics.vram() to machine.graphics.vram() DMA: overlapping words are transferred in order. */
                rxpce_video::transfer_vram(machine.graphics.vram(),
                                           machine.graphics.background_dirty(),
                                           machine.graphics.sprite_dirty(),
                                           machine.io.vdc[SOUR].value, machine.io.vdc[DISTR].value,
                                           machine.io.vdc[LENR].value, machine.io.vdc[DCR].value);
                if ((machine.io.vdc[DCR].value & 0x02)) {
                    machine.io.vdc_status |= VDC_DMAfinish;
                    machine.io.irq_status |= IRQ1;
                }
                return;
            case CR: {
                static constexpr byte incsize[] = {1, 32, 64, 128};
                machine.io.vdc_inc = incsize[(V >> 3) & 3];

            } break;
            case HDR:

                TRACE("HDRh\n");
                break;
            case BYR:
                if (!machine.video.scroll_changed) {
                    machine.video.old_scroll_x = machine.io.vdc[BXR].value;
                    machine.video.old_scroll_y = machine.io.vdc[BYR].value;
                    machine.video.old_scroll_y_difference = machine.video.scroll_y_difference;
                }
                machine.io.vdc[BYR].set_high(V & 1);
                machine.video.scroll_changed = 1;

                {
                    machine.video.scroll_y_difference = machine.scanline - 1;
                }

                return;
            case SATB:
                machine.io.vdc[SATB].set_high(V);

                machine.io.vdc_satb = 1;
                machine.io.vdc_status &= ~VDC_SATBfinish;
                return;
            case BXR:
                if (!machine.video.scroll_changed) {
                    machine.video.old_scroll_x = machine.io.vdc[BXR].value;
                    machine.video.old_scroll_y = machine.io.vdc[BYR].value;
                    machine.video.old_scroll_y_difference = machine.video.scroll_y_difference;
                }
                machine.io.vdc[BXR].set_high(V & 3);
                machine.video.scroll_changed = 1;

                return;

#define PRINT_VDC_H(REG)                                                                           \
    case REG:                                                                                      \
        TRACE(#REG "h: %X\n", V);                                                                  \
        break;
                PRINT_VDC_H(VPR);
                PRINT_VDC_H(VCR);
                PRINT_VDC_H(DCR);
            }

            machine.io.vdc[machine.io.vdc_reg].set_high(V);

            if (machine.io.vdc_reg > 19) {
                TRACE("ignore write hi vdc%d,%02x\n", machine.io.vdc_reg, V);
            }
            return;
        }
        break;

    case 0x0400: /* VCE */
        switch (A & 7) {
        case 0: /*??*/
            TRACE("VCE 0, V=%X\n", V);
            return;
        case 2:
            machine.io.vce_reg.set_low(V);
            return;
        case 3:
            machine.io.vce_reg.set_high(V & 1);
            return;
        case 4:
            FlushBeforePaletteWrite(machine, host);
            machine.io.vce[machine.io.vce_reg.value].set_low(V);
            rxpce_video::set_palette_color(machine.video.palette.data(), machine.io.vce_reg.value,
                                           machine.io.vce[machine.io.vce_reg.value].value);
            return;
        case 5:
            FlushBeforePaletteWrite(machine, host);
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
            host.psg_changed(host.context);
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
    byte ret;

    switch (A & 0x1C00) {
    case 0x0000: /* VDC */
        switch (A & 3) {
        case 0:
            ret = machine.io.vdc_status;
            machine.io.irq_status &= ~IRQ1;
            machine.io.vdc_status = 0;
            return ret;
        case 1:
            return 0;
        case 2:
            if (machine.io.vdc_reg == VRR)
                return machine.graphics.vram()[machine.io.vdc[MARR].value * 2];
            else
                return machine.io.vdc[machine.io.vdc_reg].low();
        case 3:
            if (machine.io.vdc_reg == VRR) {
                ret = machine.graphics.vram()[machine.io.vdc[MARR].value * 2 + 1];
                machine.io.vdc[MARR].value += machine.io.vdc_inc;

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
    if (host.sync_audio)
        host.sync_audio(host.context, static_cast<std::uint32_t>(time));
    int dispmin, dispmax;
    int ret;

    dispmin =
        (machine.io.maxline - machine.io.minline > MAXDISP
             ? machine.io.minline + ((machine.io.maxline - machine.io.minline - MAXDISP + 1) >> 1)
             : machine.io.minline);
    dispmax =
        (machine.io.maxline - machine.io.minline > MAXDISP
             ? machine.io.maxline - ((machine.io.maxline - machine.io.minline - MAXDISP + 1) >> 1)
             : machine.io.maxline);

    machine.scanline = (machine.scanline + 1) % machine.frame_lines;

    ret = PceNone;
    machine.io.vdc_status &= ~VDC_RasHit;
    if (machine.scanline > machine.io.maxline)
        machine.io.vdc_status |= VDC_InVBlank;

    if (machine.scanline >= machine.io.minline && machine.scanline <= machine.io.maxline)
        FlushScrollChange(machine, frame, machine.scanline - 1);

    if (machine.scanline == machine.io.minline) {
        machine.video.render_frame = !host.begin_frame || host.begin_frame(host.context);
        machine.io.vdc_status &= ~VDC_InVBlank;
        machine.video.previous_line = dispmin;
        machine.video.scroll_y_difference = 0;
        machine.video.old_scroll_y_difference = 0;

    } else if (machine.scanline == machine.io.maxline) {
        if (machine.video.render_frame) {
            FlushVideoRows(machine, frame, dispmax + 1);
            PresentScreen(machine, host, frame);
        }
    }
    if (machine.scanline >= machine.io.minline && machine.scanline <= machine.io.maxline) {
        if ((machine.io.vdc[CR].value & 0x01) && rxpce_machine::SpritesCollide(machine)) {
            machine.io.vdc_status |= VDC_SpHit;
            ret = PceIrq;
        }
        if (machine.scanline == (machine.io.vdc[RCR].value & 1023) - 64) {
            if ((machine.io.vdc[CR].value & 0x04) && machine.video.render_frame &&
                dispmin <= machine.scanline && machine.scanline <= dispmax) {
                FlushVideoRows(machine, frame, machine.scanline);
            }
            machine.io.vdc_status |= VDC_RasHit;
            if ((machine.io.vdc[CR].value & 0x04)) {

                ret = PceIrq;
            }
        }
    } else {
        int rcr = (machine.io.vdc[RCR].value & 1023) - 64;
        if (machine.scanline == rcr) {

            if ((machine.io.vdc[CR].value & 0x04)) {

                machine.io.vdc_status |= VDC_RasHit;
                ret = PceIrq;
            }
        }
    }
    machine.video.scroll_changed = 0;
    if (machine.scanline == machine.io.maxline + 1) {

        machine.frame_boundary_pending = true;

        /* machine.graphics.vram() to SATB DMA */
        if (machine.io.vdc_satb == 1 || machine.io.vdc[DCR].value & 0x0010) {
            rxpce_video::transfer_satb(machine.graphics.vram(), machine.video.sprites,
                                       machine.io.vdc[SATB].value);
            machine.io.vdc_satb = 1;
            machine.io.vdc_status &= ~VDC_SATBfinish;
        }
        if (ret == PceIrq)
            machine.io.vdc_pendvsync = 1;
        else {

            if ((machine.io.vdc[CR].value & 0x08)) {

                ret = PceIrq;
            }
        }
    } else if (machine.scanline == std::min(machine.io.maxline + 5, machine.frame_lines - 1)) {
        if (machine.io.vdc_satb) {
            machine.io.vdc_satb = 0;
            if ((machine.io.vdc[DCR].value & 0x01)) {
                machine.io.vdc_status |= VDC_SATBfinish;

                ret = PceIrq;
            }
        }
    } else if (machine.io.vdc_pendvsync && ret != PceIrq) {
        machine.io.vdc_pendvsync = 0;

        if ((machine.io.vdc[CR].value & 0x08)) {

            ret = PceIrq;
        }
    }
    if (ret == PceIrq)
        machine.io.irq_status |= IRQ1;
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
