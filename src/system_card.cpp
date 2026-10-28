#define NOMINMAX
#include "system_card.h"
#include "cartridge.h"
#include <fstream>
#include <exception>
#include <utility>

namespace rxpce_cd {
SystemProfile lookup_system_card(std::size_t size, const std::string& prefix) {
    struct Entry {
        const char* prefix;
        const char* full;
    };
    static const Entry entries[] = {
        {"1ab44feb4c448d38620754e31441f5fe6210ad79f35e368bd890ea3a5ca222ac",
         "afe9f27f91ac918348555b86298b4f984643eafa2773196f2c5441ea84f0c3bb"},
        {"248dccdfe6db3802f8ac19a6596492b4cf1effb475cd90f986d4a985bb74e332",
         "909f2153e624b76392684434ba5aa3e394cbba0318a0dda350140cb61ce9bc49"},
        {"cb688fa4b9ea4d205ea3e26e805f79421b65c8a67f59863e91fdc8f4db9d1aab",
         "0deb13845c7e44ea78a25bbbe324afd60a0ec29ea5a4cf5780349f1598d24cd3"},
        {"13e5b723baec78ed98077f272d3a062a727cbfdd7a20733213e7a2c1c9654f0c",
         "e11527b3b96ce112a037138988ca72fd117a6b0779c2480d9e03eaebece3d9ce"}};
    for (unsigned n = 0; n < 4; ++n)
        if (size == 262144 && prefix == entries[n].prefix)
            return {static_cast<SystemVersion>(n + 1),
                    n == 3 ? rxpce_machine::Kind::super_cdrom : rxpce_machine::Kind::cdrom,
                    entries[n].full};
    return {};
}
bool decode_system_card(const std::vector<unsigned char>& file, SystemCard& output,
                        std::string& error) {
    const std::size_t header = file.size() % 8192, body = file.size() - header;
    if ((header != 0 && header != 512) || body != 262144) {
        error = "Invalid System Card size or header";
        return false;
    }
    try {
        SystemCard next;
        const auto first = file.data() + header;
        next.profile = lookup_system_card(body, rxpce_rom::sha256(first, 8192));
        if (next.profile.version == SystemVersion::unknown) {
            error = "Unsupported System Card BIOS";
            return false;
        }
        if (rxpce_rom::sha256(first, body) != next.profile.rom_sha256) {
            error = "System Card whole-ROM hash mismatch";
            return false;
        }
        next.bytes.assign(first, first + body);
        output = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception&) {
        error = "Cannot identify System Card BIOS";
        return false;
    }
}
bool load_system_card(const std::wstring& path, SystemCard& output, std::string& error) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        error = "Cannot open System Card BIOS";
        return false;
    }
    const auto size = file.tellg();
    if (size != 262144 && size != 262656) {
        error = "Invalid System Card file size";
        return false;
    }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(bytes.data()), bytes.size())) {
        error = "Cannot read System Card BIOS";
        return false;
    }
    return decode_system_card(bytes, output, error);
}
} // namespace rxpce_cd
