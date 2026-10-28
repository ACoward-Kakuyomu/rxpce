#pragma once
#include <cstdint>

namespace rxpce_machine::vdc {
enum Register : unsigned {
    MAWR,
    MARR,
    VWR,
    vdc3,
    vdc4,
    CR,
    RCR,
    BXR,
    BYR,
    MWR,
    HSR,
    HDR,
    VPR,
    VDW,
    VCR,
    DCR,
    SOUR,
    DISTR,
    LENR,
    SATB
};
constexpr unsigned VRR = 2, NODATA = 0xff;
constexpr std::uint8_t VDC_SpHit = 0x01, VDC_Over = 0x02, VDC_RasHit = 0x04, VDC_SATBfinish = 0x08,
                       VDC_DMAfinish = 0x10, VDC_InVBlank = 0x20;
constexpr std::uint8_t IRQ2 = 1, IRQ1 = 2, TIRQ = 4;
} // namespace rxpce_machine::vdc
