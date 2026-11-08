#pragma once
#include "replay_codec.h"
#include <string_view>
namespace rxpce_machine { class Machine; }

namespace rxpce_replay {
struct MediaIdentity {
    MediaKind kind;
    Digest media;
    Digest bios;
};
Digest hash_bytes(const void* bytes, std::size_t size);
MediaIdentity identify_hucard(const unsigned char* bytes, std::size_t size);
MediaIdentity identify_cd(const std::filesystem::path& path, std::string_view disc_hash,
                          std::string_view bios_hash);
bool matches_media(const Recording& recording, const MediaIdentity& identity, std::string& error);
MediaIdentity current_media(const rxpce_machine::Machine& machine);
} // namespace rxpce_replay
