#pragma once
#include "vdc_mode.h"
#include "state_codec.h"
#include <string>
namespace rxpce_cd {
struct StateBinding {
    std::uint32_t kind = 0, clock = 0;
    std::string bios, disc;
};
constexpr std::size_t binding_bytes = 136;
void write_binding(rxpce_state::Writer& output, const StateBinding& binding);
bool read_binding(rxpce_state::Reader& input, const StateBinding& expected, std::string& error);
std::wstring state_path(const std::wstring& image, const std::wstring& folder,
                        rxpce_video::VdcMode mode = rxpce_video::VdcMode::accurate);
bool read_state_file(const std::wstring& path, std::size_t size, std::vector<char>& output,
                     std::string& error);
// Read and validate the outer GUID header, returning only the payload.
bool read_state_payload(const std::wstring& path, std::size_t payload_size, std::uint32_t flags,
                        std::vector<char>& output, std::string& error,
                        rxpce_video::VdcMode mode = rxpce_video::VdcMode::accurate);
bool write_state_file(const std::wstring& path, const std::vector<char>& data, std::string& error);
} // namespace rxpce_cd
