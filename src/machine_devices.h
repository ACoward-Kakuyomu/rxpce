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
    void (*psg_changed)(void*, std::uint8_t reg, std::uint8_t value) = nullptr;
    void (*controller_read)(void*, std::uint8_t) = nullptr;
    void (*trace)(void*, const char*, std::va_list) = nullptr;
    void (*sync_audio)(void*, std::uint32_t) = nullptr;
    bool (*begin_frame)(void*) = nullptr;
    void (*present)(void*, FrameSurface, DisplayRectangle) = nullptr;
    bool (*frame_start)(void*, std::uint64_t master_time) = nullptr;
};
void WriteMachineIo(Machine&, const DeviceHost&, std::uint16_t address, std::uint8_t value);
std::uint8_t ReadMachineIo(Machine&, const DeviceHost&, std::uint16_t address);
void TickMachineLine(Machine&, const DeviceHost&, FrameSurface, std::uint64_t time);
void AttachMachineVideo(Machine&, const DeviceHost&, FrameSurface);
void RenderMachineScanline(Machine&, FrameSurface, int row);
void PresentMachineFrame(Machine&, const DeviceHost&, FrameSurface, std::uint64_t master_time);
void TickMachineTimer(Machine&);
void ResetVideoAfterLoad(Machine&, FrameSurface);
DisplayRectangle VisibleRectangle(const Machine&);
} // namespace rxpce_machine
