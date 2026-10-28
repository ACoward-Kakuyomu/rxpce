#pragma once
#include "machine_memory.h"
#include <string>
#include <vector>

namespace rxpce_cd {
enum class SystemVersion { unknown, v10, v20, v21, v30 };
struct SystemProfile {
    SystemVersion version = SystemVersion::unknown;
    rxpce_machine::Kind kind = rxpce_machine::Kind::cdrom;
    std::string rom_sha256;
};
struct SystemCard {
    SystemProfile profile;
    std::vector<unsigned char> bytes;
};
SystemProfile lookup_system_card(std::size_t body_bytes, const std::string& prefix_sha256);
bool decode_system_card(const std::vector<unsigned char>& file, SystemCard& output,
                        std::string& error);
bool load_system_card(const std::wstring& path, SystemCard& output, std::string& error);
} // namespace rxpce_cd
