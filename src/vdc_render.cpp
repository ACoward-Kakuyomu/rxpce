// VDC rendering extracted from the original rxpce implementation.
// Portable PC-Engine Emulator, 1998 BERO; modified 1998 hmmx.
#include "vdc_render.h"
#include "palette_line.h"
#include "machine.h"
#include "vdc_pixels.h"
#include "vdc_planes.h"
#include "vdc_registers.h"
#include <algorithm>
#include <stdexcept>

namespace rxpce_machine {
namespace {
constexpr int render_stride = frame_width, render_rows = frame_height;
using namespace vdc;
struct Sprite {
    std::int16_t y, x, no, atr;
};
Sprite ReadSprite(const Machine& machine, unsigned index) {
    const auto* words = machine.video.sprites.data() + index * 4;
    return {words[0], words[1], words[2], words[3]};
}
std::uint16_t ReadWord(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0] | (std::uint16_t(bytes[1]) << 8));
}
template <class T> struct DrawSurface {
    T* pixels;
    std::size_t count;
    int first_row = 0, rows = frame_height;
};
template <class T> void CheckFrame(Machine& machine, DrawSurface<T> frame) {
    if (!frame.pixels || frame.first_row < 0 || frame.rows <= 0 ||
        frame.first_row > int(frame_height) - frame.rows ||
        frame.count < frame_width * unsigned(frame.rows) || !machine.graphics.vram() ||
        machine.io.screen_w < 0 || machine.io.screen_w > 512 || machine.io.screen_w % 8)
        throw std::invalid_argument("Invalid video frame or geometry");
}
int SpriteWidth(const Sprite& sprite) {
    return (sprite.atr & 0x100) ? 32 : 16;
}
int SpriteHeight(const Sprite& sprite) {
    constexpr int heights[] = {16, 32, 64, 64};
    return heights[(sprite.atr >> 12) & 3];
}
std::uint32_t ReverseBits(std::uint32_t bits) {
    bits = ((bits & 0x55555555u) << 1) | ((bits >> 1) & 0x55555555u);
    bits = ((bits & 0x33333333u) << 2) | ((bits >> 2) & 0x33333333u);
    bits = ((bits & 0x0f0f0f0fu) << 4) | ((bits >> 4) & 0x0f0f0f0fu);
    bits = ((bits & 0x00ff00ffu) << 8) | ((bits >> 8) & 0x00ff00ffu);
    return (bits << 16) | (bits >> 16);
}
// Bit zero is the leftmost screen pixel, independent of palette and drawing caches.
std::uint32_t SpriteRowMask(const Machine& machine, const Sprite& sprite, int row,
                            std::uint16_t memory) {
    const int width = SpriteWidth(sprite), height = SpriteHeight(sprite);
    int y = row - ((sprite.y & 1023) - 64);
    if (y < 0 || y >= height)
        return 0;
    if (sprite.atr & 0x8000)
        y = height - 1 - y;
    const unsigned alignment = (height / 16 - 1) * 2 + (width / 16 - 1);
    const unsigned base = ((sprite.no & 2047) >> 1) & ~alignment;
    std::uint32_t mask = 0;
    const auto planes = rxpce_video::SpritePlanes(memory, sprite.no);
    for (int cell = 0; cell < width / 16; ++cell) {
        const auto* tile = machine.graphics.vram() + (base + (y / 16) * 2 + cell) * 128;
        std::uint16_t opaque = 0;
        for (unsigned plane = planes.first; plane < planes.first + planes.count; ++plane)
            opaque |= ReadWord(tile + plane * 32 + (y % 16) * 2);
        mask |= (ReverseBits(opaque) >> 16) << (cell * 16);
    }
    return (sprite.atr & 0x800) ? ReverseBits(mask) >> (32 - width) : mask;
}
} // namespace
rxpce_video::SpriteLinePlan EvaluateMachineSprites(const Machine& machine, int row) {
    const auto& a = machine.vdc.scan.state().active;
    return rxpce_video::EvaluateSpriteLine(
        machine.video.sprites, row, rxpce_video::SpriteLimitsFromRegisters(a.hsr, a.hdr, a.memory));
}
rxpce_video::SpriteLinePlan EvaluateMachineDrawingSprites(const Machine& machine, int row) {
    if (machine.hardware_sprite_limit)
        return EvaluateMachineSprites(machine, row);
    return rxpce_video::EvaluateSpriteLine(machine.video.sprites, row, {}, false);
}
bool SpritesCollide(const Machine& machine, const rxpce_video::SpriteLinePlan& plan,
                    const SpriteDisplay& display) {
    const int row = display.row;
    if (!machine.graphics.vram() || display.width <= 0 || row < display.first || row > display.last)
        return false;
    const auto first = ReadSprite(machine, 0);
    const int x0 = (first.x & 1023) - 32, w0 = SpriteWidth(first);
    const auto full0 = SpriteRowMask(machine, first, row, display.memory);
    std::uint32_t mask0 = 0;
    for (unsigned i = 0; i < plan.count; ++i)
        if (plan.cells[i].satb_index == 0)
            mask0 |= full0 & (0xffffu << (plan.cells[i].screen_cell * 16));
    if (!mask0)
        return false;
    for (unsigned i = 0; i < plan.count; ++i) {
        const auto cell = plan.cells[i];
        if (!cell.satb_index)
            continue;
        const auto spr = ReadSprite(machine, cell.satb_index);
        const int x = (spr.x & 1023) - 32;
        const int left = std::max({0, x0, x + int(cell.screen_cell) * 16});
        const int end = std::min({display.width, x0 + w0, x + (int(cell.screen_cell) + 1) * 16});
        if (left >= end)
            continue;
        const auto overlap = static_cast<std::uint32_t>((std::uint64_t{1} << (end - left)) - 1);
        if (((mask0 >> (left - x0)) &
             (SpriteRowMask(machine, spr, row, display.memory) >> (left - x)) & overlap) != 0)
            return true;
    }
    return false;
}
static void DecodeBackgroundTile(Machine& machine, int no) {
    rxpce_machine::background_pixels(machine.graphics.vram() + no * 32,
                                     machine.graphics.background() + no * 8);
}

static void DecodeSpriteTile(Machine& machine, int no) {
    rxpce_machine::sprite_pixels(machine.graphics.vram() + no * 128,
                                 machine.graphics.sprites() + no * 32);
}
#define PAL(c) R[c]
#define FC_W machine.io.screen_w
#define FC_H 256

template <class T>
void RenderBackgroundImpl(Machine& machine, DrawSurface<T> frame, int Y1, int Y2,
                          const BackgroundRegisters& registers, const T* palette) {
    CheckFrame(machine, frame);
    if (Y1 - machine.video.row_bias < frame.first_row || Y1 - machine.video.row_bias > 256 ||
        Y2 - machine.video.row_bias >= frame.first_row + frame.rows)
        throw std::invalid_argument("Invalid background rows");
    if (Y2 < Y1 || machine.io.screen_w == 0)
        return;
    int X1, XW, Line;
    int x, y, h, offset, Shift;

    T* PP;

    Y2++;
    PP = frame.pixels + render_stride * (render_rows - FC_H) / 2 + (render_stride - FC_W) / 2 +
         render_stride * (Y1 - machine.video.row_bias - frame.first_row);

    if ((registers.control & 0x80) && machine.video.background_enabled) {
        const auto planes = rxpce_video::BackgroundPlanes(registers.memory);

        y = Y1 + registers.y_offset;
        offset = y & 7;
        h = 8 - offset;
        if (h > Y2 - Y1)
            h = Y2 - Y1;
        y >>= 3;
        PP -= registers.x_scroll & 7;
        XW = machine.io.screen_w / 8 + 1;
        Shift = registers.x_scroll & 7;

        for (Line = Y1; Line < Y2; y++) {

            x = registers.x_scroll / 8;
            y &= machine.io.bg_h - 1;
            for (X1 = 0; X1 < XW; X1++, x++, PP += 8) {
                const T* R;
                T* P;
                byte* C;
                std::uint32_t* C2;
                int no, i;
                x &= machine.io.bg_w - 1;
                no = ReadWord(machine.graphics.vram() + 2 * (x + y * machine.io.bg_w));
                R = &palette[(no >> 12) * 16];
                no &= 0xFFF;
                if (machine.graphics.background_dirty()[no]) {
                    machine.graphics.background_dirty()[no] = 0;
                    DecodeBackgroundTile(machine, no);
                }
                C2 = &machine.graphics.background()[no * 8 + offset];
                C = &machine.graphics.vram()[no * 32 + offset * 2];
                P = PP;

                for (i = 0; i < h; i++, C2++, C += 2 /*,Z+=ZW*/) {
                    P = PP + i * render_stride;
                    std::uint32_t L;
                    std::uint8_t J;

                    J = (C[0] | C[1] | C[16] | C[17]);
                    if (planes.count == 2)
                        J = planes.first ? (C[16] | C[17]) : (C[0] | C[1]);
                    if (!J)
                        continue;

                    L = planes.packed(C2[0]);
                    if (J & 0x80)
                        P[0] = PAL((L >> 4) & 15);
                    if (J & 0x40)
                        P[1] = PAL((L >> 12) & 15);
                    if (J & 0x20)
                        P[2] = PAL((L >> 20) & 15);
                    if (J & 0x10)
                        P[3] = PAL((L >> 28));
                    if (J & 0x08)
                        P[4] = PAL((L) & 15);
                    if (J & 0x04)
                        P[5] = PAL((L >> 8) & 15);
                    if (J & 0x02)
                        P[6] = PAL((L >> 16) & 15);
                    if (J & 0x01)
                        P[7] = PAL((L >> 24) & 15);
                }
            }
            Line += h;
            if (Line < Y2)
                PP += render_stride * h - XW * 8;
            offset = 0;
            h = Y2 - Line;
            if (h > 8)
                h = 8;
        }
    }
    /* Refresh Sprite */
}

#define V_FLIP 0x8000
#define H_FLIP 0x0800
#define SPBG 0x80
#define CGX 0x100

template <class T>
static void PutSprite(T* output, const byte* source, const std::uint32_t* cache, const T* R, int h,
                      int inc) {
    int i, J;
    std::uint32_t L;
    for (i = 0; i < h; ++i) {
        auto* P = output + i * render_stride;
        const auto* C = source + i * inc;
        const auto* C2 = cache + i * inc;
        J = ReadWord(C) | ReadWord(C + 32) | ReadWord(C + 64) | ReadWord(C + 96);
        if (!J)
            continue;
        L = C2[1];
        if (J & 0x8000)
            P[0] = PAL((L >> 4) & 15);
        if (J & 0x4000)
            P[1] = PAL((L >> 12) & 15);
        if (J & 0x2000)
            P[2] = PAL((L >> 20) & 15);
        if (J & 0x1000)
            P[3] = PAL((L >> 28));
        if (J & 0x0800)
            P[4] = PAL((L) & 15);
        if (J & 0x0400)
            P[5] = PAL((L >> 8) & 15);
        if (J & 0x0200)
            P[6] = PAL((L >> 16) & 15);
        if (J & 0x0100)
            P[7] = PAL((L >> 24) & 15);
        L = C2[0];
        if (J & 0x80)
            P[8] = PAL((L >> 4) & 15);
        if (J & 0x40)
            P[9] = PAL((L >> 12) & 15);
        if (J & 0x20)
            P[10] = PAL((L >> 20) & 15);
        if (J & 0x10)
            P[11] = PAL((L >> 28));
        if (J & 0x08)
            P[12] = PAL((L) & 15);
        if (J & 0x04)
            P[13] = PAL((L >> 8) & 15);
        if (J & 0x02)
            P[14] = PAL((L >> 16) & 15);
        if (J & 0x01)
            P[15] = PAL((L >> 24) & 15);
    }
}

template <class T>
static void PutSpriteHflip(T* output, const byte* source, const std::uint32_t* cache, const T* R,
                           int h, int inc) {
    int i, J;
    std::uint32_t L;
    for (i = 0; i < h; ++i) {
        auto* P = output + i * render_stride;
        const auto* C = source + i * inc;
        const auto* C2 = cache + i * inc;
        J = ReadWord(C) | ReadWord(C + 32) | ReadWord(C + 64) | ReadWord(C + 96);
        if (!J)
            continue;
        L = C2[1];
        if (J & 0x8000)
            P[15] = PAL((L >> 4) & 15);
        if (J & 0x4000)
            P[14] = PAL((L >> 12) & 15);
        if (J & 0x2000)
            P[13] = PAL((L >> 20) & 15);
        if (J & 0x1000)
            P[12] = PAL((L >> 28));
        if (J & 0x0800)
            P[11] = PAL((L) & 15);
        if (J & 0x0400)
            P[10] = PAL((L >> 8) & 15);
        if (J & 0x0200)
            P[9] = PAL((L >> 16) & 15);
        if (J & 0x0100)
            P[8] = PAL((L >> 24) & 15);
        L = C2[0];
        if (J & 0x80)
            P[7] = PAL((L >> 4) & 15);
        if (J & 0x40)
            P[6] = PAL((L >> 12) & 15);
        if (J & 0x20)
            P[5] = PAL((L >> 20) & 15);
        if (J & 0x10)
            P[4] = PAL((L >> 28));
        if (J & 0x08)
            P[3] = PAL((L) & 15);
        if (J & 0x04)
            P[2] = PAL((L >> 8) & 15);
        if (J & 0x02)
            P[1] = PAL((L >> 16) & 15);
        if (J & 0x01)
            P[0] = PAL((L >> 24) & 15);
    }
}

template <class T>
static void PutSpriteM(T* output, const byte* source, const std::uint32_t* cache, const T* R, int h,
                       int inc, byte* mask, byte pr) {
    int i, J;
    std::uint32_t L;
    for (i = 0; i < h; ++i) {
        auto* P = output + i * render_stride;
        const auto* C = source + i * inc;
        const auto* C2 = cache + i * inc;
        auto* M = mask + i * render_stride;
        J = ReadWord(C) | ReadWord(C + 32) | ReadWord(C + 64) | ReadWord(C + 96);
        if (!J)
            continue;
        L = C2[1];
        if ((J & 0x8000) && M[0] <= pr)
            P[0] = PAL((L >> 4) & 15);
        if ((J & 0x4000) && M[1] <= pr)
            P[1] = PAL((L >> 12) & 15);
        if ((J & 0x2000) && M[2] <= pr)
            P[2] = PAL((L >> 20) & 15);
        if ((J & 0x1000) && M[3] <= pr)
            P[3] = PAL((L >> 28));
        if ((J & 0x0800) && M[4] <= pr)
            P[4] = PAL((L) & 15);
        if ((J & 0x0400) && M[5] <= pr)
            P[5] = PAL((L >> 8) & 15);
        if ((J & 0x0200) && M[6] <= pr)
            P[6] = PAL((L >> 16) & 15);
        if ((J & 0x0100) && M[7] <= pr)
            P[7] = PAL((L >> 24) & 15);
        L = C2[0];
        if ((J & 0x80) && M[8] <= pr)
            P[8] = PAL((L >> 4) & 15);
        if ((J & 0x40) && M[9] <= pr)
            P[9] = PAL((L >> 12) & 15);
        if ((J & 0x20) && M[10] <= pr)
            P[10] = PAL((L >> 20) & 15);
        if ((J & 0x10) && M[11] <= pr)
            P[11] = PAL((L >> 28));
        if ((J & 0x08) && M[12] <= pr)
            P[12] = PAL((L) & 15);
        if ((J & 0x04) && M[13] <= pr)
            P[13] = PAL((L >> 8) & 15);
        if ((J & 0x02) && M[14] <= pr)
            P[14] = PAL((L >> 16) & 15);
        if ((J & 0x01) && M[15] <= pr)
            P[15] = PAL((L >> 24) & 15);
    }
}

template <class T>
static void PutSpriteHflipM(T* output, const byte* source, const std::uint32_t* cache, const T* R,
                            int h, int inc, byte* mask, byte pr) {
    int i, J;
    std::uint32_t L;
    for (i = 0; i < h; ++i) {
        auto* P = output + i * render_stride;
        const auto* C = source + i * inc;
        const auto* C2 = cache + i * inc;
        auto* M = mask + i * render_stride;
        J = ReadWord(C) | ReadWord(C + 32) | ReadWord(C + 64) | ReadWord(C + 96);
        if (!J)
            continue;
        L = C2[1];
        if ((J & 0x8000) && M[15] <= pr)
            P[15] = PAL((L >> 4) & 15);
        if ((J & 0x4000) && M[14] <= pr)
            P[14] = PAL((L >> 12) & 15);
        if ((J & 0x2000) && M[13] <= pr)
            P[13] = PAL((L >> 20) & 15);
        if ((J & 0x1000) && M[12] <= pr)
            P[12] = PAL((L >> 28));
        if ((J & 0x0800) && M[11] <= pr)
            P[11] = PAL((L) & 15);
        if ((J & 0x0400) && M[10] <= pr)
            P[10] = PAL((L >> 8) & 15);
        if ((J & 0x0200) && M[9] <= pr)
            P[9] = PAL((L >> 16) & 15);
        if ((J & 0x0100) && M[8] <= pr)
            P[8] = PAL((L >> 24) & 15);
        L = C2[0];
        if ((J & 0x80) && M[7] <= pr)
            P[7] = PAL((L >> 4) & 15);
        if ((J & 0x40) && M[6] <= pr)
            P[6] = PAL((L >> 12) & 15);
        if ((J & 0x20) && M[5] <= pr)
            P[5] = PAL((L >> 20) & 15);
        if ((J & 0x10) && M[4] <= pr)
            P[4] = PAL((L >> 28));
        if ((J & 0x08) && M[3] <= pr)
            P[3] = PAL((L) & 15);
        if ((J & 0x04) && M[2] <= pr)
            P[2] = PAL((L >> 8) & 15);
        if ((J & 0x02) && M[1] <= pr)
            P[1] = PAL((L >> 16) & 15);
        if ((J & 0x01) && M[0] <= pr)
            P[0] = PAL((L >> 24) & 15);
    }
}

template <class T>
static void PutSpriteMakeMask(T* output, const byte* source, const std::uint32_t* cache, const T* R,
                              int h, int inc, byte* mask, byte pr) {
    int i, J;
    std::uint32_t L;
    for (i = 0; i < h; ++i) {
        auto* P = output + i * render_stride;
        const auto* C = source + i * inc;
        const auto* C2 = cache + i * inc;
        auto* M = mask + i * render_stride;
        J = ReadWord(C) | ReadWord(C + 32) | ReadWord(C + 64) | ReadWord(C + 96);
        if (!J)
            continue;
        L = C2[1];
        if (J & 0x8000) {
            P[0] = PAL((L >> 4) & 15);
            M[0] = pr;
        }
        if (J & 0x4000) {
            P[1] = PAL((L >> 12) & 15);
            M[1] = pr;
        }
        if (J & 0x2000) {
            P[2] = PAL((L >> 20) & 15);
            M[2] = pr;
        }
        if (J & 0x1000) {
            P[3] = PAL((L >> 28));
            M[3] = pr;
        }
        if (J & 0x0800) {
            P[4] = PAL((L) & 15);
            M[4] = pr;
        }
        if (J & 0x0400) {
            P[5] = PAL((L >> 8) & 15);
            M[5] = pr;
        }
        if (J & 0x0200) {
            P[6] = PAL((L >> 16) & 15);
            M[6] = pr;
        }
        if (J & 0x0100) {
            P[7] = PAL((L >> 24) & 15);
            M[7] = pr;
        }
        L = C2[0];
        if (J & 0x80) {
            P[8] = PAL((L >> 4) & 15);
            M[8] = pr;
        }
        if (J & 0x40) {
            P[9] = PAL((L >> 12) & 15);
            M[9] = pr;
        }
        if (J & 0x20) {
            P[10] = PAL((L >> 20) & 15);
            M[10] = pr;
        }
        if (J & 0x10) {
            P[11] = PAL((L >> 28));
            M[11] = pr;
        }
        if (J & 0x08) {
            P[12] = PAL((L) & 15);
            M[12] = pr;
        }
        if (J & 0x04) {
            P[13] = PAL((L >> 8) & 15);
            M[13] = pr;
        }
        if (J & 0x02) {
            P[14] = PAL((L >> 16) & 15);
            M[14] = pr;
        }
        if (J & 0x01) {
            P[15] = PAL((L >> 24) & 15);
            M[15] = pr;
        }
    }
}

template <class T>
static void PutSpriteHflipMakeMask(T* output, const byte* source, const std::uint32_t* cache,
                                   const T* R, int h, int inc, byte* mask, byte pr) {
    int i, J;
    std::uint32_t L;
    for (i = 0; i < h; ++i) {
        auto* P = output + i * render_stride;
        const auto* C = source + i * inc;
        const auto* C2 = cache + i * inc;
        auto* M = mask + i * render_stride;
        J = ReadWord(C) | ReadWord(C + 32) | ReadWord(C + 64) | ReadWord(C + 96);
        if (!J)
            continue;
        L = C2[1];
        if (J & 0x8000) {
            P[15] = PAL((L >> 4) & 15);
            M[15] = pr;
        }
        if (J & 0x4000) {
            P[14] = PAL((L >> 12) & 15);
            M[14] = pr;
        }
        if (J & 0x2000) {
            P[13] = PAL((L >> 20) & 15);
            M[13] = pr;
        }
        if (J & 0x1000) {
            P[12] = PAL((L >> 28));
            M[12] = pr;
        }
        if (J & 0x0800) {
            P[11] = PAL((L) & 15);
            M[11] = pr;
        }
        if (J & 0x0400) {
            P[10] = PAL((L >> 8) & 15);
            M[10] = pr;
        }
        if (J & 0x0200) {
            P[9] = PAL((L >> 16) & 15);
            M[9] = pr;
        }
        if (J & 0x0100) {
            P[8] = PAL((L >> 24) & 15);
            M[8] = pr;
        }
        L = C2[0];
        if (J & 0x80) {
            P[7] = PAL((L >> 4) & 15);
            M[7] = pr;
        }
        if (J & 0x40) {
            P[6] = PAL((L >> 12) & 15);
            M[6] = pr;
        }
        if (J & 0x20) {
            P[5] = PAL((L >> 20) & 15);
            M[5] = pr;
        }
        if (J & 0x10) {
            P[4] = PAL((L >> 28));
            M[4] = pr;
        }
        if (J & 0x08) {
            P[3] = PAL((L) & 15);
            M[3] = pr;
        }
        if (J & 0x04) {
            P[2] = PAL((L >> 8) & 15);
            M[2] = pr;
        }
        if (J & 0x02) {
            P[1] = PAL((L >> 16) & 15);
            M[1] = pr;
        }
        if (J & 0x01) {
            P[0] = PAL((L >> 24) & 15);
            M[0] = pr;
        }
    }
}

template <class T>
void RenderSpriteLineImpl(Machine& machine, DrawSurface<T> frame, int row, unsigned bg,
                          const rxpce_video::SpriteLinePlan& plan, std::uint16_t memory,
                          const T* palette) {
    CheckFrame(machine, frame);
    if (row - machine.video.row_bias < frame.first_row ||
        row - machine.video.row_bias >= frame.first_row + frame.rows || bg > 1)
        throw std::invalid_argument("Invalid sprite row");
    for (unsigned i = plan.count; i > 0; --i) {
        const auto cell = plan.cells[i - 1];
        const auto spr = ReadSprite(machine, cell.satb_index);
        const unsigned attr = static_cast<std::uint16_t>(spr.atr);
        if (((attr >> 7) & 1) != bg)
            continue;
        const int sprite_x = (spr.x & 1023) - 32;
        if (sprite_x >= machine.io.screen_w || sprite_x + SpriteWidth(spr) <= 0)
            continue;
        const int x = sprite_x + cell.screen_cell * 16;
        const unsigned alignment = (SpriteHeight(spr) / 16 - 1) * 2 + (SpriteWidth(spr) / 16 - 1);
        const unsigned tile =
            (((spr.no & 2047) >> 1) & ~alignment) + (cell.row / 16) * 2 + cell.source_cell;
        if (machine.graphics.sprite_dirty()[tile]) {
            machine.graphics.sprite_dirty()[tile] = 0;
            DecodeSpriteTile(machine, tile);
        }
        auto* C = machine.graphics.vram() + tile * 128 + (cell.row % 16) * 2;
        auto* C2 = machine.graphics.sprites() + tile * 32 + (cell.row % 16) * 2;
        const auto planes = rxpce_video::SpritePlanes(memory, spr.no);
        // Keep the shared cache in four-plane form. Only SM1 uses this temporary row;
        // both opacity and packed colors presented to the legacy blitters are selected.
        std::array<byte, 128> selected_source;
        std::array<std::uint32_t, 2> selected_cache;
        if (planes.count == 2) {
            selected_source.fill(0);
            for (unsigned p = 0; p < 2; ++p)
                for (unsigned b = 0; b < 2; ++b)
                    selected_source[p * 32 + b] = C[(planes.first + p) * 32 + b];
            selected_cache = {planes.packed(C2[0]), planes.packed(C2[1])};
            C = selected_source.data();
            C2 = selected_cache.data();
        }
        auto* R = &palette[256 + (attr & 15) * 16];
        const auto pos = (row - machine.video.row_bias) * render_stride +
                         (render_stride - machine.io.screen_w) / 2 + x;
        auto* P = frame.pixels + pos - frame.first_row * render_stride;
        auto* M = machine.video.priority.data() + pos;
        const auto priority = static_cast<byte>(63 - cell.satb_index);
        if (bg == 0) {
            machine.video.uses_sprite_priority = 1;
            if (attr & H_FLIP)
                PutSpriteHflipMakeMask(P, C, C2, R, 1, 2, M, priority);
            else
                PutSpriteMakeMask(P, C, C2, R, 1, 2, M, priority);
        } else if (machine.video.uses_sprite_priority) {
            if (attr & H_FLIP)
                PutSpriteHflipM(P, C, C2, R, 1, 2, M, priority);
            else
                PutSpriteM(P, C, C2, R, 1, 2, M, priority);
        } else {
            if (attr & H_FLIP)
                PutSpriteHflip(P, C, C2, R, 1, 2);
            else
                PutSprite(P, C, C2, R, 1, 2);
        }
    }
}
template <class T>
void ClearVideoRowsImpl(Machine& machine, DrawSurface<T> frame, int first, int last,
                        const T* palette) {
    first -= machine.video.row_bias;
    last -= machine.video.row_bias;
    if (!frame.pixels || frame.count < frame_width * unsigned(frame.rows) ||
        first < frame.first_row || first > 256 || last >= frame.first_row + frame.rows)
        throw std::invalid_argument("Invalid video clear range");
    if (last < first)
        return;
    std::fill_n(frame.pixels + (first - frame.first_row) * frame_width,
                (last - first + 1) * frame_width, palette[0]);
    std::fill_n(machine.video.priority.data() + first * frame_width,
                (last - first + 1) * frame_width, 0);
}

void RenderBackground(Machine& m, FrameSurface f, int first, int last,
                      const BackgroundRegisters& r) {
    RenderBackgroundImpl(m, DrawSurface<rxpce_video::Pixel>{f.pixels, f.count}, first, last, r,
                         m.video.palette.data());
}
void RenderSpriteLine(Machine& m, FrameSurface f, int row, unsigned bg,
                      const rxpce_video::SpriteLinePlan& p, std::uint16_t memory) {
    RenderSpriteLineImpl(m, DrawSurface<rxpce_video::Pixel>{f.pixels, f.count}, row, bg, p, memory,
                         m.video.palette.data());
}
void ClearVideoRows(Machine& m, FrameSurface f, int first, int last) {
    ClearVideoRowsImpl(m, DrawSurface<rxpce_video::Pixel>{f.pixels, f.count}, first, last,
                       m.video.palette.data());
}
namespace {
constexpr auto entry_palette() {
    std::array<PaletteEntry, 512> p{};
    for (unsigned i = 0; i < 512; ++i)
        p[i] = PaletteEntry(i < 256 && !(i & 15) ? 0 : i);
    return p;
}
constexpr auto entries = entry_palette();
} // namespace
void ComposePaletteLine(Machine& m, PaletteLine& line, int row) {
    static_assert(palette_line_pixels == frame_width, "palette line geometry");
    line.active = false;
    const int y = row - m.video.row_bias;
    if (y < 0 || y >= int(frame_height))
        throw std::invalid_argument("Invalid palette row");
    DrawSurface<PaletteEntry> surface{line.entries.data(), line.entries.size(), y, 1};
    const auto& s = m.vdc.scan.state();
    const auto control = static_cast<std::uint16_t>(s.burst ? 0 : s.active.control);
    const auto plan =
        (control & 0x40) ? EvaluateMachineDrawingSprites(m, row) : rxpce_video::SpriteLinePlan{};
    ClearVideoRowsImpl(m, surface, row, row, entries.data());
    if (s.burst)
        line.entries.fill(PaletteEntry(256)); // VD0..7=0, SPBG=1; no display VRAM fetches.
    m.video.uses_sprite_priority = 0;
    if ((control & 0x40) && m.video.sprites_enabled)
        RenderSpriteLineImpl(m, surface, row, 0, plan, s.active.memory, entries.data());
    RenderBackgroundImpl(m, surface, row, row,
                         {control, s.active.x_scroll, int(s.y_counter) - row, s.active.memory},
                         entries.data());
    if ((control & 0x40) && m.video.sprites_enabled)
        RenderSpriteLineImpl(m, surface, row, 1, plan, s.active.memory, entries.data());
    line.output_row = y;
    line.width = static_cast<unsigned>(m.io.screen_w);
    line.start_master = m.vce_clock.state().master_time;
    constexpr unsigned dividers[] = {4, 3, 2, 2};
    line.divider = dividers[m.vce_clock.state().control & 3];
    line.monochrome = (m.vce_clock.state().control & 128) != 0;
    line.committed = 0;
    line.used_valid = false;
    line.active = true;
}
void ResolvePaletteLine(PaletteLine& line, FrameSurface frame,
                        const std::array<rxpce_video::Pixel, 512>& palette, unsigned end) {
    if (!line.active || line.output_row < 0 || line.output_row >= int(frame_height) ||
        end > frame_width || end < line.committed || !frame.pixels ||
        frame.count < frame_width * frame_height)
        throw std::invalid_argument("Invalid palette prefix");
    auto* out = frame.pixels + line.output_row * frame_width;
    const auto attribute = line.monochrome ? rxpce_video::monochrome_bit : 0;
    for (unsigned x = line.committed; x < end; ++x)
        out[x] = static_cast<rxpce_video::Pixel>(palette[line.entries[x].index()] | attribute);
    line.committed = end;
}
} // namespace rxpce_machine
