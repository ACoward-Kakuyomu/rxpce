// VDC rendering extracted from the original rxpce implementation.
// Portable PC-Engine Emulator, 1998 BERO; modified 1998 hmmx.
#include "vdc_render.h"
#include "machine.h"
#include "vdc_pixels.h"
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
void CheckFrame(Machine& machine, FrameSurface frame) {
    if (!frame.pixels || frame.count < frame_width * frame_height || !machine.graphics.vram() ||
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
std::uint32_t SpriteRowMask(const Machine& machine, const Sprite& sprite, int row) {
    const int width = SpriteWidth(sprite), height = SpriteHeight(sprite);
    int y = row - ((sprite.y & 1023) - 64);
    if (y < 0 || y >= height)
        return 0;
    if (sprite.atr & 0x8000)
        y = height - 1 - y;
    const unsigned alignment = (height / 16 - 1) * 2 + (width / 16 - 1);
    const unsigned base = ((sprite.no & 2047) >> 1) & ~alignment;
    std::uint32_t mask = 0;
    for (int cell = 0; cell < width / 16; ++cell) {
        const auto* tile = machine.graphics.vram() + (base + (y / 16) * 2 + cell) * 128;
        std::uint16_t opaque = 0;
        for (unsigned plane = 0; plane < 4; ++plane)
            opaque |= ReadWord(tile + plane * 32 + (y % 16) * 2);
        mask |= (ReverseBits(opaque) >> 16) << (cell * 16);
    }
    return (sprite.atr & 0x800) ? ReverseBits(mask) >> (32 - width) : mask;
}
} // namespace
bool SpritesCollide(const Machine& machine) {
    const int row = machine.scanline;
    if (!machine.graphics.vram() || machine.io.screen_w <= 0 || row < machine.io.minline ||
        row > machine.io.maxline)
        return false;
    const auto first = ReadSprite(machine, 0);
    const int x0 = (first.x & 1023) - 32, w0 = SpriteWidth(first);
    if (x0 >= machine.io.screen_w || x0 + w0 <= 0)
        return false;
    const auto mask0 = SpriteRowMask(machine, first, row);
    if (!mask0)
        return false;
    int remaining = 16 - w0 / 16;
    for (unsigned i = 1; i < 64 && remaining; ++i) {
        const auto spr = ReadSprite(machine, i);
        const int y = (spr.y & 1023) - 64;
        if (row < y || row >= y + SpriteHeight(spr))
            continue;
        const int cells = std::min(remaining, SpriteWidth(spr) / 16);
        remaining -=
            cells; // Y selection consumes cells even when X is offscreen or pixels are zero.
        const int x = (spr.x & 1023) - 32;
        const int left = std::max({0, x0, x});
        const int end = std::min({machine.io.screen_w, x0 + w0, x + cells * 16});
        if (left >= end)
            continue;
        const auto overlap = static_cast<std::uint32_t>((std::uint64_t{1} << (end - left)) - 1);
        if (((mask0 >> (left - x0)) & (SpriteRowMask(machine, spr, row) >> (left - x)) & overlap) !=
            0)
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
#define SPal (machine.video.palette.data() + 256)
#define FC_W machine.io.screen_w
#define FC_H 256

void RenderBackground(Machine& machine, FrameSurface frame, int Y1, int Y2) {
    CheckFrame(machine, frame);
    if (Y1 < 0 || Y1 > 256 || Y2 >= 256)
        throw std::invalid_argument("Invalid background rows");
    if (Y2 < Y1 || machine.io.screen_w == 0)
        return;
    int X1, XW, Line;
    int x, y, h, offset, Shift;

    rxpce_video::Pixel* PP;

    Y2++;
    PP = frame.pixels + render_stride * (render_rows - FC_H) / 2 + (render_stride - FC_W) / 2 +
         render_stride * Y1;

    if ((machine.io.vdc[CR].value & 0x80) && machine.video.background_enabled) {

        y = Y1 + machine.io.vdc[BYR].value - machine.video.scroll_y_difference;
        offset = y & 7;
        h = 8 - offset;
        if (h > Y2 - Y1)
            h = Y2 - Y1;
        y >>= 3;
        PP -= machine.io.vdc[BXR].value & 7;
        XW = machine.io.screen_w / 8 + 1;
        Shift = machine.io.vdc[BXR].value & 7;

        for (Line = Y1; Line < Y2; y++) {

            x = machine.io.vdc[BXR].value / 8;
            y &= machine.io.bg_h - 1;
            for (X1 = 0; X1 < XW; X1++, x++, PP += 8) {
                rxpce_video::Pixel *R, *P;
                byte* C;
                std::uint32_t* C2;
                int no, i;
                x &= machine.io.bg_w - 1;
                no = ReadWord(machine.graphics.vram() + 2 * (x + y * machine.io.bg_w));
                R = &machine.video.palette.data()[(no >> 12) * 16];
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
                    if (!J)
                        continue;

                    L = C2[0];
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

static void PutSprite(rxpce_video::Pixel* output, const byte* source, const std::uint32_t* cache,
                      rxpce_video::Pixel* R, int h, int inc) {
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

static void PutSpriteHflip(rxpce_video::Pixel* output, const byte* source,
                           const std::uint32_t* cache, rxpce_video::Pixel* R, int h, int inc) {
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

static void PutSpriteM(rxpce_video::Pixel* output, const byte* source, const std::uint32_t* cache,
                       rxpce_video::Pixel* R, int h, int inc, byte* mask, byte pr) {
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

static void PutSpriteHflipM(rxpce_video::Pixel* output, const byte* source,
                            const std::uint32_t* cache, rxpce_video::Pixel* R, int h, int inc,
                            byte* mask, byte pr) {
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

static void PutSpriteMakeMask(rxpce_video::Pixel* output, const byte* source,
                              const std::uint32_t* cache, rxpce_video::Pixel* R, int h, int inc,
                              byte* mask, byte pr) {
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

static void PutSpriteHflipMakeMask(rxpce_video::Pixel* output, const byte* source,
                                   const std::uint32_t* cache, rxpce_video::Pixel* R, int h,
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

void RenderSprites(Machine& machine, FrameSurface frame, int Y1, int Y2, unsigned bg) {
    CheckFrame(machine, frame);
    if (Y1 < 0 || Y2 > 256 || Y2 < 0 || Y1 > 256 || bg > 1)
        throw std::invalid_argument("Invalid sprite rows");
    if (Y2 <= Y1 || machine.io.screen_w == 0)
        return;
    int n;

    if (bg == 0)
        machine.video.uses_sprite_priority = 0;

    for (n = 0; n < 64; n++) {
        const auto spr = ReadSprite(machine, static_cast<unsigned>(63 - n));
        int x, y, no, atr, inc, cgx, cgy;
        rxpce_video::Pixel* R;
        byte* C;
        std::uint32_t* C2;
        int pos;
        int h, i, j;
        unsigned spbg;

        atr = spr.atr;
        spbg = static_cast<unsigned>((atr >> 7) & 1);
        const auto priority = static_cast<byte>(n);
        if (spbg != bg)
            continue;
        y = (spr.y & 1023) - 64;
        x = (spr.x & 1023) - 32;
        no = spr.no & 2047;
        cgx = (atr >> 8) & 1;
        cgy = (atr >> 12) & 3;
        cgy |= cgy >> 1;
        no = (no >> 1) & ~(cgy * 2 + cgx);
        if (y >= Y2 || y + (cgy + 1) * 16 < Y1 || x >= FC_W || x + (cgx + 1) * 16 < 0)
            continue;

        R = &SPal[(atr & 15) * 16];
        for (i = 0; i < cgy * 2 + cgx + 1; i++) {
            if (machine.graphics.sprite_dirty()[no + i]) {
                machine.graphics.sprite_dirty()[no + i] = 0;
                DecodeSpriteTile(machine, no + i);
            }
            if (!cgx)
                i++;
        }
        inc = (atr & V_FLIP) ? -2 : 2;

        for (i = 0; i <= cgy; i++) {
            const int top = y + i * 16;
            const int first = std::max(Y1, top), end = std::min(Y2, top + 16);
            if (first >= end)
                continue;
            const int block = (atr & V_FLIP) ? cgy - i : i;
            const int row = (atr & V_FLIP) ? 15 - (first - top) : first - top;
            C = machine.graphics.vram() + no * 128 + block * 256 + row * 2;
            C2 = machine.graphics.sprites() + no * 32 + block * 64 + row * 2;
            pos = (render_stride - FC_W) / 2 + render_stride * first + x;
            h = end - first;
            if (spbg == 0) {
                machine.video.uses_sprite_priority = 1;
                if (atr & H_FLIP) {
                    for (j = 0; j <= cgx; j++)
                        PutSpriteHflipMakeMask(
                            frame.pixels + pos + (cgx - j) * 16, C + j * 128, C2 + j * 32, R, h,
                            inc, machine.video.priority.data() + pos + (cgx - j) * 16, priority);
                } else {
                    for (j = 0; j <= cgx; j++)
                        PutSpriteMakeMask(frame.pixels + pos + j * 16, C + j * 128, C2 + j * 32, R,
                                          h, inc, machine.video.priority.data() + pos + j * 16,
                                          priority);
                }
            } else if (machine.video.uses_sprite_priority) {
                if (atr & H_FLIP) {
                    for (j = 0; j <= cgx; j++)
                        PutSpriteHflipM(
                            frame.pixels + pos + (cgx - j) * 16, C + j * 128, C2 + j * 32, R, h,
                            inc, machine.video.priority.data() + pos + (cgx - j) * 16, priority);
                } else {
                    for (j = 0; j <= cgx; j++)
                        PutSpriteM(frame.pixels + pos + j * 16, C + j * 128, C2 + j * 32, R, h, inc,
                                   machine.video.priority.data() + pos + j * 16, priority);
                }
            } else {
                if (atr & H_FLIP) {
                    for (j = 0; j <= cgx; j++)
                        PutSpriteHflip(frame.pixels + pos + (cgx - j) * 16, C + j * 128,
                                       C2 + j * 32, R, h, inc);
                } else {
                    for (j = 0; j <= cgx; j++)
                        PutSprite(frame.pixels + pos + j * 16, C + j * 128, C2 + j * 32, R, h, inc);
                }
            }
        }
    }
}

void ClearVideoRows(Machine& machine, FrameSurface frame, int first, int last) {
    if (!frame.pixels || frame.count < frame_width * frame_height || first < 0 || first > 256 ||
        last >= 256)
        throw std::invalid_argument("Invalid video clear range");
    if (last < first)
        return;
    std::fill_n(frame.pixels + first * frame_width, (last - first + 1) * frame_width,
                machine.video.palette[0]);
    std::fill_n(machine.video.priority.data() + first * frame_width,
                (last - first + 1) * frame_width, 0);
}
} // namespace rxpce_machine
