#include "replay_media.h"
#include "cartridge.h"
#include <stdexcept>

namespace rxpce_replay {
namespace {
unsigned hex_digit(char ch) {
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    throw std::invalid_argument("Invalid media SHA-256 digit");
}
Digest digest(std::string_view text) {
    Digest bytes{};
    if (text.size() != bytes.size() * 2)
        throw std::invalid_argument("Invalid media SHA-256 length");
    for (std::size_t i = 0; i < bytes.size(); ++i)
        bytes[i] = static_cast<std::uint8_t>((hex_digit(text[i * 2]) << 4) |
                                             hex_digit(text[i * 2 + 1]));
    return bytes;
}
} // namespace
Digest hash_bytes(const void* bytes, std::size_t size) {
    return digest(rxpce_rom::sha256(static_cast<const unsigned char*>(bytes), size));
}
MediaIdentity identify_hucard(const unsigned char* bytes, std::size_t size) {
    if (!bytes || !size)
        throw std::invalid_argument("No HuCard data");
    return {MediaKind::hucard, hash_bytes(bytes, size), {}};
}
MediaIdentity identify_cd(const std::filesystem::path& path, std::string_view disc_hash,
                          std::string_view bios_hash) {
    auto extension = path.extension().wstring();
    for (auto& ch : extension)
        if (ch >= L'A' && ch <= L'Z')
            ch += L'a' - L'A';
    if (extension != L".chd" && extension != L".cue")
        throw std::invalid_argument("Expected a CUE or CHD replay image");
    return {extension == L".chd" ? MediaKind::chd : MediaKind::cue, digest(disc_hash),
            digest(bios_hash)};
}
bool matches_media(const Recording& recording, const MediaIdentity& identity, std::string& error) {
    if (recording.kind != identity.kind)
        error = "Replay media format does not match";
    else if (recording.media != identity.media)
        error = "Replay ROM or disc identity does not match";
    else if (recording.bios != identity.bios)
        error = "Replay requires the System Card BIOS used during recording";
    else {
        error.clear();
        return true;
    }
    return false;
}
} // namespace rxpce_replay
