#define NOMINMAX
#include "transport.h"
#include <windows.h>
#include <ntddcdrm.h>
#include <ntddscsi.h>
#include <setupapi.h>
#include <winioctl.h>
#include <algorithm>
#include <cstddef>
#include <stdexcept>

namespace rxpce_cd_dump {
namespace {
[[noreturn]] void win_error(const char* what) {
    throw std::runtime_error(std::string(what) + ": Win32 error " + std::to_string(GetLastError()));
}
struct DeviceSet {
    HDEVINFO value;
    explicit DeviceSet(HDEVINFO handle):value(handle) {}
    DeviceSet(const DeviceSet&)=delete;
    DeviceSet& operator=(const DeviceSet&)=delete;
    ~DeviceSet() { if (value != INVALID_HANDLE_VALUE) SetupDiDestroyDeviceInfoList(value); }
};
struct TransferBuffer {
    void* value=nullptr;
    explicit TransferBuffer(unsigned bytes)
    {
        if(!bytes) return;
        value=VirtualAlloc(nullptr,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
        if (!value) win_error("Allocate SCSI transfer buffer");
    }
    TransferBuffer(const TransferBuffer&)=delete;
    TransferBuffer& operator=(const TransferBuffer&)=delete;
    ~TransferBuffer() { if(value) VirtualFree(value, 0, MEM_RELEASE); }
};
}
std::string utf8(const std::wstring& text) {
    if (text.empty()) return {};
    const auto length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!length) win_error("UTF-8 conversion");
    std::string result(static_cast<std::size_t>(length), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), result.data(), length, nullptr, nullptr)) win_error("UTF-8 conversion");
    return result;
}
std::vector<DriveInfo> list_drives() {
    DeviceSet devices{SetupDiGetClassDevsW(&GUID_DEVINTERFACE_CDROM, nullptr, nullptr,
                                         DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)};
    if (devices.value == INVALID_HANDLE_VALUE) win_error("Enumerate optical drives");
    std::vector<DriveInfo> result;
    for (DWORD n = 0;; ++n) {
        SP_DEVICE_INTERFACE_DATA item{}; item.cbSize = sizeof(item);
        if (!SetupDiEnumDeviceInterfaces(devices.value, nullptr, &GUID_DEVINTERFACE_CDROM, n, &item)) {
            if (GetLastError() == ERROR_NO_MORE_ITEMS) break;
            win_error("Read optical device interface");
        }
        DWORD size = 0;
        SetupDiGetDeviceInterfaceDetailW(devices.value, &item, nullptr, 0, &size, nullptr);
        if (size < sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) || size > 1024 * 1024)
            throw std::runtime_error("Invalid optical device path length");
        std::vector<unsigned char> buffer(size, 0);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
        detail->cbSize = sizeof(*detail);
        SP_DEVINFO_DATA info{}; info.cbSize = sizeof(info);
        if (!SetupDiGetDeviceInterfaceDetailW(devices.value, &item, detail, size, nullptr, &info))
            win_error("Read optical device path");
        const auto capacity = (size - offsetof(SP_DEVICE_INTERFACE_DETAIL_DATA_W, DevicePath)) / sizeof(wchar_t);
        const auto end = std::find(detail->DevicePath, detail->DevicePath + capacity, L'\0');
        if (end == detail->DevicePath + capacity) throw std::runtime_error("Unterminated optical device path");
        wchar_t name[512]{};
        if (!SetupDiGetDeviceRegistryPropertyW(devices.value, &info, SPDRP_FRIENDLYNAME, nullptr,
                reinterpret_cast<BYTE*>(name), sizeof(name) - sizeof(wchar_t), nullptr))
            SetupDiGetDeviceRegistryPropertyW(devices.value, &info, SPDRP_DEVICEDESC, nullptr,
                reinterpret_cast<BYTE*>(name), sizeof(name) - sizeof(wchar_t), nullptr);
        result.push_back({std::wstring(detail->DevicePath, end), utf8(name)});
    }
    return result;
}
struct WindowsDrive::Impl {
    HANDLE handle = INVALID_HANDLE_VALUE;
    bool locked = false;
    ~Impl() { if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle); }
};
WindowsDrive::WindowsDrive(const std::wstring& path) : impl_(std::make_unique<Impl>()) {
    const auto device = path.size() == 2 && path[1] == L':' ? L"\\\\.\\" + path : path;
    if (device.rfind(L"\\\\", 0) != 0) throw std::runtime_error("Use a drive letter (E:) or a device interface path");
    impl_->handle = CreateFileW(device.c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (impl_->handle == INVALID_HANDLE_VALUE) win_error("Open optical drive for SPTI");
}
WindowsDrive::~WindowsDrive() {
    if (impl_ && impl_->locked) {
        try { execute({0x1e,0,0,0,0,0},0); } catch (...) {}
    }
}
bool WindowsDrive::lock_media() {
    if (impl_->locked) return true;
    try { execute({0x1e,0,0,0,1,0},0);impl_->locked=true;return true; }
    catch (const std::exception&) { return false; }
}
std::optional<std::uint32_t> WindowsDrive::media_generation() {
    DWORD generation=0,returned=0;
    if (!DeviceIoControl(impl_->handle,IOCTL_STORAGE_CHECK_VERIFY2,nullptr,0,
                         &generation,sizeof(generation),&returned,nullptr)) {
        const auto error=GetLastError();
        if (error==ERROR_INVALID_FUNCTION || error==ERROR_NOT_SUPPORTED) return std::nullopt;
        SetLastError(error);win_error("Verify optical media");
    }
    if (returned==0) return std::nullopt;
    if (returned!=sizeof(generation)) throw std::runtime_error("Invalid media generation response");
    return generation;
}
Bytes WindowsDrive::execute(const Bytes& command, unsigned expected) {
    if (command.empty() || command.size() > 16 || expected > 128 * 1024)
        throw std::runtime_error("Invalid SCSI command size");
    // Do not expose arbitrary commands, eject or media writes through this tool.
    if (command[0] != 0x12 && command[0] != 0x43 && command[0] != 0x46 &&
        command[0] != 0xbe && command[0] != 0x1e)
        throw std::runtime_error("SCSI command is not allowed");
    struct Packet { SCSI_PASS_THROUGH_DIRECT spti{}; unsigned char sense[64]{}; } packet;
    TransferBuffer buffer(expected);
    auto& s = packet.spti;
    s.Length = sizeof(s);
    s.CdbLength = static_cast<UCHAR>(command.size());
    s.SenseInfoLength = sizeof(packet.sense);
    s.DataIn = expected ? SCSI_IOCTL_DATA_IN : SCSI_IOCTL_DATA_UNSPECIFIED;
    s.DataTransferLength = expected;
    s.TimeOutValue = 10;
    s.DataBuffer = buffer.value;
    s.SenseInfoOffset = offsetof(Packet, sense);
    std::copy(command.begin(), command.end(), s.Cdb);
    DWORD returned = 0;
    if (!DeviceIoControl(impl_->handle, IOCTL_SCSI_PASS_THROUGH_DIRECT,
        &packet, sizeof(packet), &packet, sizeof(packet), &returned, nullptr)) win_error("SCSI pass-through");
    if (returned < sizeof(s) || s.DataTransferLength > expected)
        throw std::runtime_error("Invalid SCSI response length");
    if (s.ScsiStatus) {
        const auto senseBytes = (std::min)(static_cast<std::size_t>(s.SenseInfoLength), sizeof(packet.sense));
        throw ScsiFailure(s.ScsiStatus,Bytes(packet.sense,packet.sense+senseBytes));
    }
    if(!s.DataTransferLength) return {};
    const auto* begin = static_cast<const unsigned char*>(buffer.value);
    return Bytes(begin, begin + s.DataTransferLength);
}
std::string WindowsDrive::inquiry() {
    const auto bytes = execute({0x12,0,0,0,96,0}, 96);
    if (bytes.size() < 36 || (bytes[0] & 31) != 5) throw std::runtime_error("Device is not an optical drive");
    return std::string(bytes.begin() + 8, bytes.begin() + 36);
}
}
