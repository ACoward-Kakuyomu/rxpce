#pragma once
#include "vdc_render.h"
#include <cstdarg>
#include <cstdint>

namespace rxpce_machine {
struct DisplayRectangle {
    int x, y, width, height;
};
struct DeviceHost {
    void* context = nullptr;
    void (*psg_changed)(void*) = nullptr;
    void (*controller_read)(void*, std::uint8_t) = nullptr;
    void (*trace)(void*, const char*, std::va_list) = nullptr;
    void (*sync_audio)(void*, std::uint32_t) = nullptr;
    bool (*begin_frame)(void*) = nullptr;
    void (*present)(void*, FrameSurface, DisplayRectangle) = nullptr;
    FrameSurface (*frame)(void*) = nullptr;
};
void WriteMachineIo(Machine&, const DeviceHost&, std::uint16_t address, std::uint8_t value);
std::uint8_t ReadMachineIo(Machine&, const DeviceHost&, std::uint16_t address);
void TickMachineLine(Machine&, const DeviceHost&, FrameSurface, std::uint64_t time);
void TickMachineTimer(Machine&);
void ResetVideoAfterLoad(Machine&, FrameSurface);
DisplayRectangle VisibleRectangle(const Machine&);
} // namespace rxpce_machine
