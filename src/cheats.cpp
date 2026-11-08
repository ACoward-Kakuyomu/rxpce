#define NOMINMAX
#include "cheats.h"
#include <windows.h>
#include <picojson.h>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>

namespace rxpce_cheats {
namespace {
using Object = picojson::object;
using Value = picojson::value;
void require(bool ok, const char* text) {
    if (!ok) throw std::invalid_argument(text);
}
const Object& object(const Value& value, std::initializer_list<const char*> keys) {
    require(value.is<Object>(), "Expected a cheat JSON object");
    const auto& result = value.get<Object>();
    for (const auto& field : result)
        require(std::any_of(keys.begin(), keys.end(), [&](const auto key) { return field.first == key; }),
                "Unknown cheat JSON field");
    return result;
}
template<class T> const T& get(const Object& object, const char* key) {
    const auto it = object.find(key);
    require(it != object.end() && it->second.is<T>(), key);
    return it->second.get<T>();
}
std::uint32_t integer(const Object& object, const char* key) {
    const double value = get<double>(object, key);
    require(std::isfinite(value) && value >= 0 && value <= UINT32_MAX && std::floor(value) == value,
            "Cheat number must be an unsigned 32-bit integer");
    return static_cast<std::uint32_t>(value);
}
Value number(std::uint32_t value) { return Value(static_cast<double>(value)); }
std::string hex(const rxpce_replay::Digest& digest) {
    const char digits[] = "0123456789abcdef";
    std::string text;
    for (auto byte : digest) {
        text += digits[byte >> 4];
        text += digits[byte & 15];
    }
    return text;
}
rxpce_replay::Digest digest(const std::string& text) {
    require(text.size() == 64 && text.find_first_not_of("0123456789abcdef") == std::string::npos,
            "Cheat media hash must contain 64 lowercase hexadecimal digits");
    const auto digit = [](char ch) { return ch <= '9' ? ch - '0' : ch - 'a' + 10; };
    rxpce_replay::Digest result{};
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<std::uint8_t>((digit(text[2 * i]) << 4) | digit(text[2 * i + 1]));
    return result;
}
const char* kind_name(rxpce_replay::MediaKind kind) {
    switch (kind) {
    case rxpce_replay::MediaKind::hucard: return "hucard";
    case rxpce_replay::MediaKind::cue: return "cue";
    case rxpce_replay::MediaKind::chd: return "chd";
    default: throw std::invalid_argument("Unknown cheat media kind");
    }
}
void validate_name(const std::string& name) {
    require(!name.empty() && name.size() <= 256, "Name must contain 1 to 64 characters");
    const auto size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(),
                                         static_cast<int>(name.size()), nullptr, 0);
    require(size > 0, "Invalid UTF-8 in cheat name");
    std::wstring text(size, L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, name.data(), static_cast<int>(name.size()), text.data(), size);
    unsigned count = 0;
    for (const auto unit : text) {
        require(unit >= 0x20 && !(unit >= 0x7f && unit <= 0x9f), "Control character in cheat name");
        if (unit < 0xdc00 || unit > 0xdfff) ++count;
    }
    require(count <= 64, "Name exceeds 64 characters");
}
}
bool validate(const Document& document, const rxpce_search::Snapshot& ram, std::string& error) {
    try {
        kind_name(document.media.kind);
        if (document.media.kind == rxpce_replay::MediaKind::hucard)
            require(document.media.bios == rxpce_replay::Digest{}, "HuCard cheats cannot specify a BIOS");
        require(document.cheats.size() <= max_entries, "Cheat list exceeds 1024 entries");
        for (std::size_t i = 0; i < document.cheats.size(); ++i) {
            const auto& entry = document.cheats[i];
            validate_name(entry.name);
            require(entry.bytes == 1 || entry.bytes == 2 || entry.bytes == 4, "Bytes must be 1, 2 or 4");
            const auto size = rxpce_search::writable_bytes(entry.region);
            const auto region = std::find_if(ram.regions.begin(), ram.regions.end(),
                [&](const auto& region) { return region.id == entry.region && region.bytes.size() == size; });
            require(size && region != ram.regions.end(), "RAM region is readonly or unavailable");
            require(entry.offset <= size && entry.bytes <= size - entry.offset, "Cheat crosses the RAM boundary");
            require(entry.value <= (UINT32_MAX >> ((4 - entry.bytes) * 8)), "Value exceeds the selected size");
            if (!entry.enabled || !entry.always_apply) continue;
            for (std::size_t j = 0; j < i; ++j) {
                const auto& before = document.cheats[j];
                require(!before.enabled || !before.always_apply || before.region != entry.region ||
                            before.offset + before.bytes <= entry.offset || entry.offset + entry.bytes <= before.offset,
                        "Enabled Always Apply codes overlap");
            }
        }
        error.clear();
        return true;
    } catch (const std::exception& failure) { error = failure.what(); return false; }
}
std::string encode(const Document& document) {
    Object media;
    media["kind"] = Value(std::string(kind_name(document.media.kind)));
    media["sha256"] = Value(hex(document.media.media));
    if (document.media.kind != rxpce_replay::MediaKind::hucard)
        media["bios_sha256"] = Value(hex(document.media.bios));
    picojson::array entries;
    for (const auto& entry : document.cheats) {
        Object item;
        item["name"] = Value(entry.name);
        item["enabled"] = Value(entry.enabled);
        item["region"] = Value(std::string(rxpce_search::region_key(entry.region)));
        item["offset"] = number(entry.offset);
        item["bytes"] = number(entry.bytes);
        item["value"] = number(entry.value);
        item["always_apply"] = Value(entry.always_apply);
        entries.emplace_back(item);
    }
    return Value(Object{{"version", number(1)}, {"media", Value(media)}, {"cheats", Value(entries)}}).serialize(true) + "\n";
}
bool decode(const std::string& json, const rxpce_replay::MediaIdentity& expected,
            const rxpce_search::Snapshot& ram, Document& output, std::string& error) {
    try {
        require(json.size() <= max_file_bytes, "Cheat JSON exceeds 1 MiB");
        Value root;
        std::string problem;
        const auto end = picojson::parse(root, json.begin(), json.end(), &problem);
        require(problem.empty() && std::all_of(end, json.end(),
            [](char ch) { return ch == ' ' || ch == '\r' || ch == '\n' || ch == '\t'; }),
            "Invalid cheat JSON or trailing data");
        const auto& object_root = object(root, {"version", "media", "cheats"});
        require(integer(object_root, "version") == 1, "Unsupported cheat JSON version");
        const auto media_it = object_root.find("media");
        require(media_it != object_root.end(), "Missing media identity");
        const auto& media = object(media_it->second, {"kind", "sha256", "bios_sha256"});
        Document next;
        const auto& kind = get<std::string>(media, "kind");
        require(kind == "hucard" || kind == "cue" || kind == "chd", "Unknown cheat media kind");
        next.media.kind = kind == "hucard" ? rxpce_replay::MediaKind::hucard :
                          kind == "cue" ? rxpce_replay::MediaKind::cue : rxpce_replay::MediaKind::chd;
        next.media.media = digest(get<std::string>(media, "sha256"));
        if (next.media.kind == rxpce_replay::MediaKind::hucard)
            require(media.count("bios_sha256") == 0, "HuCard cheats cannot specify a BIOS");
        else next.media.bios = digest(get<std::string>(media, "bios_sha256"));
        require(next.media.kind == expected.kind && next.media.media == expected.media && next.media.bios == expected.bios,
                "Cheat media or BIOS identity does not match");
        const auto& entries = get<picojson::array>(object_root, "cheats");
        require(entries.size() <= max_entries, "Cheat list exceeds 1024 entries");
        for (const auto& raw : entries) {
            const auto& item = object(raw, {"name", "enabled", "region", "offset", "bytes", "value", "always_apply"});
            Entry entry;
            entry.name = get<std::string>(item, "name");
            entry.enabled = get<bool>(item, "enabled");
            entry.always_apply = get<bool>(item, "always_apply");
            const auto& key = get<std::string>(item, "region");
            for (auto region : {rxpce_search::RegionId::main_ram, rxpce_search::RegionId::cd_ram, rxpce_search::RegionId::super_ram})
                if (key == rxpce_search::region_key(region)) entry.region = region;
            require(key == rxpce_search::region_key(entry.region), "Unknown writable RAM region");
            entry.offset = integer(item, "offset");
            entry.bytes = integer(item, "bytes");
            entry.value = integer(item, "value");
            next.cheats.push_back(std::move(entry));
        }
        if (!validate(next, ram, error)) return false;
        output = std::move(next);
        error.clear();
        return true;
    } catch (const std::exception& failure) { error = failure.what(); return false; }
}
} // namespace rxpce_cheats
