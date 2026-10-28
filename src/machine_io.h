#pragma once
#include "pce_types.h"
#include "controller_bus.h"
#include "machine_memory.h"
#include <array>
#include <cstdint>

namespace rxpce_machine {
struct IoState {
    std::array<Register16, 32> vdc{};
    std::array<Register16, 512> vce{};
    Register16 vce_reg;
    std::uint16_t vdc_inc = 1;
    std::uint8_t vdc_reg = 0, vdc_status = 0, vdc_ratch = 0, vdc_satb = 0, vdc_pendvsync = 0;
    std::int32_t bg_h = 0, bg_w = 0, screen_w = 0, screen_h = 0, minline = 0, maxline = 255;
    rxpce_input::ControllerBus controllers;
    std::array<std::array<std::uint8_t, 8>, 6> psg{};
    std::array<std::array<std::uint8_t, 32>, 6> wave{};
    std::array<std::uint8_t, 6> wavofs{};
    std::uint8_t psg_ch = 0, psg_volume = 0, psg_lfo_freq = 0, psg_lfo_ctrl = 0;
    std::uint8_t timer_reload = 0, timer_start = 0, timer_counter = 0, irq_mask = 0, irq_status = 0;
    BackupGate backup;
};
} // namespace rxpce_machine
