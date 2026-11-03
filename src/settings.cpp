#define NOMINMAX
#include "settings.h"
#include "keyboard_input.h"
#include "input_device.h"
#include <windows.h>
#include "unique_handle.h"
#include <picojson.h>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <algorithm>

namespace rxpce_config {
std::atomic_bool dialog_open{false};
static std::atomic_bool input_release_pending{false};
void begin_dialog_input() {
    dialog_open = true;
    rxpce_input::keyboard_state().reset();
}
void end_dialog_input() {
    rxpce_input::keyboard_state().reset();
    input_release_pending = true;
    dialog_open = false;
}
bool block_game_input(const unsigned char* keys, bool pad_button_down) {
    if (dialog_open)
        return true;
    if (!input_release_pending)
        return false;
    if (!pad_button_down &&
        std::none_of(keys, keys + 256, [](unsigned char key) { return (key & 0x80) != 0; }))
        input_release_pending = false;
    return true;
}
Settings::Input active_input;
namespace {
constexpr std::size_t limit = 128 * 1024;
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
const picojson::object& object(const picojson::value& value) {
    require(value.is<picojson::object>(), "Expected a JSON object");
    return value.get<picojson::object>();
}
const picojson::value& field(const picojson::object& obj, const char* key) {
    static const picojson::value empty(picojson::object{});
    const auto it = obj.find(key);
    return it == obj.end() ? empty : it->second;
}
template <class T> void read(const picojson::object& obj, const char* key, T& target) {
    const auto it = obj.find(key);
    if (it == obj.end())
        return;
    require(it->second.is<T>(), key);
    target = it->second.get<T>();
}
void read(const picojson::object& obj, const char* key, int& target) {
    const auto it = obj.find(key);
    if (it == obj.end())
        return;
    require(it->second.is<double>(), key);
    const double n = it->second.get<double>();
    require(std::isfinite(n) && n == std::floor(n) && n >= -2147483647.0 && n <= 2147483647.0, key);
    target = static_cast<int>(n);
}
template <std::size_t N>
void read(const picojson::object& obj, const char* key, std::array<int, N>& target) {
    const auto it = obj.find(key);
    if (it == obj.end())
        return;
    require(it->second.is<picojson::array>(), key);
    const auto& array = it->second.get<picojson::array>();
    require(array.size() == N, key);
    for (std::size_t i = 0; i < N; ++i) {
        picojson::object entry;
        entry["n"] = array[i];
        read(entry, "n", target[i]);
    }
}
picojson::value val(int n) {
    return picojson::value(static_cast<double>(n));
}
picojson::value val(bool b) {
    return picojson::value(b);
}
picojson::value val(const std::string& s) {
    return picojson::value(s);
}
template <std::size_t N> picojson::value val(const std::array<int, N>& array) {
    picojson::array result;
    for (int n : array)
        result.push_back(val(n));
    return picojson::value(result);
}
std::wstring wide(const std::string& text, UINT codepage) {
    if (text.empty())
        return {};
    const int size = MultiByteToWideChar(codepage, codepage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0,
                                         text.data(), static_cast<int>(text.size()), nullptr, 0);
    require(size > 0, "Invalid text encoding");
    std::wstring out(size, L'\0');
    MultiByteToWideChar(codepage, codepage == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0, text.data(),
                        static_cast<int>(text.size()), &out[0], size);
    return out;
}
std::string narrow(const std::wstring& text, UINT codepage) {
    if (text.empty())
        return {};
    BOOL substituted = FALSE;
    const bool utf8 = codepage == CP_UTF8 || GetACP() == CP_UTF8;
    const DWORD flags = utf8 ? WC_ERR_INVALID_CHARS : WC_NO_BEST_FIT_CHARS;
    const int size =
        WideCharToMultiByte(codepage, flags, text.data(), static_cast<int>(text.size()), nullptr, 0,
                            nullptr, utf8 ? nullptr : &substituted);
    require(size > 0 && !substituted, "Path cannot be represented by the current code page");
    std::string out(size, '\0');
    WideCharToMultiByte(codepage, flags, text.data(), static_cast<int>(text.size()), &out[0], size,
                        nullptr, utf8 ? nullptr : &substituted);
    require(!substituted, "Path cannot be represented by the current code page");
    return out;
}
bool valid_slot(int slot) {
    return slot >= 1 && slot <= Settings::input_profile_count;
}
void validate_profile_name(const std::string& name) {
    require(name.size() <= 120, "Input profile name exceeds 30 characters");
    const auto text = wide(name, CP_UTF8);
    int length = 0;
    for (wchar_t unit : text) {
        require(unit >= 0x20 && unit != 0x7f, "Control character in input profile name");
        if (unit < 0xdc00 || unit > 0xdfff)
            ++length;
    }
    require(length <= 30, "Input profile name exceeds 30 characters");
}
void validate_mapping(const Settings::PlayerInput& mapping) {
    require(mapping.count >= 1 && mapping.count <= 5, "Invalid controller count");
    for (const auto& player : mapping.players) {
        require(player.buttons == 2 || player.buttons == 6, "Invalid controller button mode");
        for (int n : player.keyboard)
            require(n > 0 && n <= 255, "Invalid keyboard mapping");
        for (int n : player.pad)
            require(n >= 0 && n < 32, "Invalid pad mapping");
        for (int n : player.xinput)
            require(n >= 0 && n < 12, "Invalid XInput mapping");
        if (player.device != "keyboard" && rxpce_input::xinput_slot(player.device) < 0) {
            require(player.device.size() == 38 && player.device.front() == '{' &&
                        player.device.back() == '}',
                    "Invalid input device GUID");
            for (std::size_t n = 1; n < 37; ++n) {
                const char c = player.device[n];
                const bool hyphen = n == 9 || n == 14 || n == 19 || n == 24;
                require(hyphen ? c == '-'
                               : ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                                  (c >= 'A' && c <= 'F')),
                        "Invalid input device GUID");
            }
        }
    }
}
void read_mapping(const picojson::object& i, Settings::PlayerInput& mapping) {
    read(i, "count", mapping.count);
    const auto players = i.find("players");
    if (players != i.end()) {
        require(players->second.is<picojson::array>(), "Invalid players array");
        const auto& array = players->second.get<picojson::array>();
        require(array.size() == 5, "Expected five player profiles");
        for (std::size_t n = 0; n < 5; ++n) {
            const auto& p = object(array[n]);
            auto& player = mapping.players[n];
            read(p, "device", player.device);
            read(p, "buttons", player.buttons);
            read(p, "keyboard", player.keyboard);
            read(p, "pad", player.pad);
            read(p, "xinput", player.xinput);
        }
    }
}
picojson::value encode_profile(const std::string& name, const Settings::PlayerInput& mapping) {
    picojson::object profile;
    profile["name"] = val(name);
    picojson::array players;
    for (const auto& player : mapping.players) {
        picojson::object p;
        p["device"] = val(player.device);
        p["buttons"] = val(player.buttons);
        p["keyboard"] = val(player.keyboard);
        p["pad"] = val(player.pad);
        p["xinput"] = val(player.xinput);
        players.push_back(picojson::value(p));
    }
    profile["count"] = val(mapping.count);
    profile["players"] = picojson::value(players);
    return picojson::value(profile);
}
} // namespace
std::wstring to_wide(const std::string& s) {
    return wide(s, CP_UTF8);
}
std::string to_utf8(const std::wstring& s) {
    return narrow(s, CP_UTF8);
}
std::string from_native(const std::string& s) {
    return to_utf8(wide(s, CP_ACP));
}
std::filesystem::path config_path(const std::filesystem::path& executable) {
    return executable.parent_path() / L"rxpce.json";
}
void merge_runtime(Settings& p, const Settings& r, const RuntimeOverrides& o) {
    p.video.zoom = r.video.zoom;
    p.video.aspect_mode = r.video.aspect_mode;
    if (!o.auto_skip)
        p.video.auto_frame_skip = r.video.auto_frame_skip;
    if (!o.skip)
        p.video.frame_skip = r.video.frame_skip;
    if (!o.fps)
        p.advanced.show_fps = r.advanced.show_fps;
    if (!o.rom)
        p.folders.rom = r.folders.rom;
}
bool select_input_profile(Settings& s, int slot) {
    if (!valid_slot(slot) || !valid_slot(s.selected_profile))
        return false;
    if (slot == s.selected_profile)
        return true;

    auto& target = s.inactive_profiles[slot - 1];
    s.inactive_profiles[s.selected_profile - 1] = Settings::InputProfile{s.profile_name, s.input};
    if (target) {
        static_cast<Settings::PlayerInput&>(s.input) = std::move(target->mapping);
        s.profile_name = std::move(target->name);
        target.reset();
    } else {
        s.profile_name.clear();
    }
    s.selected_profile = slot;
    return true;
}
bool validate(const Settings& s, std::string& error) {
    try {
        require(s.audio.backend == "directsound" || s.audio.backend == "wasapi",
                "Invalid audio backend");
        require(rxpce_video::valid_vdc_mode(s.advanced.vdc_emulation),
                "Invalid VDC emulation mode");
        require(rxpce_video::valid_aspect_mode(s.video.aspect_mode), "Invalid aspect ratio");
        require(s.video.adapter >= -2 && s.video.zoom >= 1 && s.video.zoom <= 4 &&
                    s.video.frame_skip >= 0 && s.video.frame_skip <= 15,
                "Invalid video setting");
        const int rates[] = {48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000};
        require(std::find(std::begin(rates), std::end(rates), s.audio.sample_rate) !=
                    std::end(rates),
                "Unsupported sample rate");
        require(s.audio.channels >= 1 && s.audio.channels <= 2 && s.audio.latency_ms >= 20 &&
                    s.audio.latency_ms <= 160 && s.audio.cooperative_level >= 0 &&
                    s.audio.cooperative_level <= 2,
                "Invalid audio setting");
        require(s.advanced.base_clock_hz >= 1000000 && s.advanced.base_clock_hz <= 85920000,
                "Invalid base clock");
        require(valid_slot(s.selected_profile), "Invalid selected input profile");
        require(!s.inactive_profiles[s.selected_profile - 1], "Duplicate selected input profile");
        validate_profile_name(s.profile_name);
        validate_mapping(s.input);
        for (const auto& profile : s.inactive_profiles) {
            if (profile) {
                validate_profile_name(profile->name);
                validate_mapping(profile->mapping);
            }
        }
        for (const auto* path : {&s.video.palette_file, &s.folders.rom, &s.folders.state}) {
            require(path->find('\0') == std::string::npos, "NUL in path");
            require(to_wide(*path).size() < 32768, "Path exceeds Windows path limit");
        }
        require(s.cd.bios_file.find('\0') == std::string::npos &&
                    to_wide(s.cd.bios_file).size() < 32768,
                "Invalid BIOS path");
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
bool decode(const std::string& json, Settings& settings, std::string& error) {
    try {
        require(json.size() <= limit, "Settings file is too large");
        picojson::value root;
        std::string parse_error;
        const auto end = picojson::parse(root, json.begin(), json.end(), &parse_error);
        require(parse_error.empty(), "Invalid JSON");
        require(std::all_of(end, json.end(),
                            [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }),
                "Trailing data after JSON");
        const auto& r = object(root);
        int version = 2;
        read(r, "version", version);
        require(version == 2, "Unsupported settings version");
        Settings s;
        const auto& v = object(field(r, "video"));
        read(v, "adapter", s.video.adapter);
        read(v, "zoom", s.video.zoom);
        std::string aspect = "square_pixels";
        read(v, "aspect_mode", aspect);
        require(aspect == "square_pixels" || aspect == "tv_4_3", "Invalid aspect ratio");
        s.video.aspect_mode = aspect == "tv_4_3" ? rxpce_video::AspectMode::tv_4_3
                                                 : rxpce_video::AspectMode::square_pixels;
        read(v, "frame_skip", s.video.frame_skip);
        read(v, "auto_frame_skip", s.video.auto_frame_skip);
        read(v, "palette_file", s.video.palette_file);
        const auto& a = object(field(r, "audio"));
        read(a, "enabled", s.audio.enabled);
        read(a, "backend", s.audio.backend);
        read(a, "sample_rate", s.audio.sample_rate);
        read(a, "channels", s.audio.channels);
        read(a, "latency_ms", s.audio.latency_ms);
        read(a, "cooperative_level", s.audio.cooperative_level);
        const auto& i = object(field(r, "input"));
        read(i, "xinput_enabled", s.input.xinput_enabled);
        read(i, "directinput_enabled", s.input.directinput_enabled);
        const auto profiles = i.find("profiles");
        if (profiles == i.end()) {
            require(i.count("selected_profile") == 0, "Missing input profiles");
            read_mapping(i, s.input);
        } else {
            require(i.count("selected_profile") != 0, "Missing selected input profile");
            read(i, "selected_profile", s.selected_profile);
            require(valid_slot(s.selected_profile), "Invalid selected input profile");
            require(profiles->second.is<picojson::array>(), "Invalid input profiles array");
            const auto& slots = profiles->second.get<picojson::array>();
            require(slots.size() == Settings::input_profile_count, "Expected ten input profiles");
            for (int slot = 1; slot <= Settings::input_profile_count; ++slot) {
                const auto& value = slots[slot - 1];
                if (value.is<picojson::null>()) {
                    require(slot != s.selected_profile, "Selected input profile is empty");
                    continue;
                }
                const auto& profile = object(value);
                if (slot == s.selected_profile) {
                    read(profile, "name", s.profile_name);
                    read_mapping(profile, s.input);
                } else {
                    auto& inactive = s.inactive_profiles[slot - 1].emplace();
                    read(profile, "name", inactive.name);
                    read_mapping(profile, inactive.mapping);
                }
            }
        }
        const auto& f = object(field(r, "folders"));
        read(f, "rom", s.folders.rom);
        read(f, "state", s.folders.state);
        const auto& cd = object(field(r, "cd"));
        read(cd, "bios_file", s.cd.bios_file);
        read(cd, "allow_network", s.cd.allow_network);
        const auto& x = object(field(r, "advanced"));
        std::string vdc_mode = "accurate";
        read(x, "vdc_emulation", vdc_mode);
        read(x, "hardware_sprite_limit", s.advanced.hardware_sprite_limit);
        require(vdc_mode == "accurate" || vdc_mode == "fast", "Invalid VDC emulation mode");
        s.advanced.vdc_emulation =
            vdc_mode == "fast" ? rxpce_video::VdcMode::fast : rxpce_video::VdcMode::accurate;
        read(x, "base_clock_hz", s.advanced.base_clock_hz);
        read(x, "log", s.advanced.log);
        read(x, "show_fps", s.advanced.show_fps);
        if (!validate(s, error))
            return false;
        s.advanced.base_clock_hz -= s.advanced.base_clock_hz % 1000;
        settings = std::move(s);
        error.clear();
        return true;
    } catch (const std::exception& e) {
        error = e.what();
        return false;
    }
}
std::string encode(const Settings& s) {
    require(rxpce_video::valid_aspect_mode(s.video.aspect_mode), "Invalid aspect ratio");
    picojson::object v, a, i, f, x, r, cd;
    v["adapter"] = val(s.video.adapter);
    v["zoom"] = val(s.video.zoom);
    v["aspect_mode"] = val(std::string(
        s.video.aspect_mode == rxpce_video::AspectMode::tv_4_3 ? "tv_4_3" : "square_pixels"));
    v["frame_skip"] = val(s.video.frame_skip);
    v["auto_frame_skip"] = val(s.video.auto_frame_skip);
    v["palette_file"] = val(s.video.palette_file);
    a["enabled"] = val(s.audio.enabled);
    a["backend"] = val(s.audio.backend);
    a["sample_rate"] = val(s.audio.sample_rate);
    a["channels"] = val(s.audio.channels);
    a["latency_ms"] = val(s.audio.latency_ms);
    a["cooperative_level"] = val(s.audio.cooperative_level);
    picojson::array profiles;
    for (int slot = 1; slot <= Settings::input_profile_count; ++slot) {
        const auto& inactive = s.inactive_profiles[slot - 1];
        if (slot == s.selected_profile)
            profiles.push_back(encode_profile(s.profile_name, s.input));
        else if (inactive)
            profiles.push_back(encode_profile(inactive->name, inactive->mapping));
        else
            profiles.emplace_back();
    }
    i["profiles"] = picojson::value(profiles);
    i["selected_profile"] = val(s.selected_profile);
    i["xinput_enabled"] = val(s.input.xinput_enabled);
    i["directinput_enabled"] = val(s.input.directinput_enabled);
    f["rom"] = val(s.folders.rom);
    f["state"] = val(s.folders.state);
    cd["bios_file"] = val(s.cd.bios_file);
    cd["allow_network"] = val(s.cd.allow_network);
    r["cd"] = picojson::value(cd);
    x["vdc_emulation"] = val(
        std::string(s.advanced.vdc_emulation == rxpce_video::VdcMode::fast ? "fast" : "accurate"));
    x["hardware_sprite_limit"] = val(s.advanced.hardware_sprite_limit);
    x["base_clock_hz"] = val(s.advanced.base_clock_hz - s.advanced.base_clock_hz % 1000);
    x["log"] = val(s.advanced.log);
    x["show_fps"] = val(s.advanced.show_fps);
    r["version"] = val(2);
    r["video"] = picojson::value(v);
    r["audio"] = picojson::value(a);
    r["input"] = picojson::value(i);
    r["folders"] = picojson::value(f);
    r["advanced"] = picojson::value(x);
    return picojson::value(r).serialize(true) + "\n";
}
bool load(const std::filesystem::path& path, Settings& settings, std::string& error) {
    const DWORD attr = GetFileAttributesW(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES && GetLastError() == ERROR_FILE_NOT_FOUND) {
        settings = Settings{};
        error.clear();
        return true;
    }
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input || input.tellg() < 0 || input.tellg() > static_cast<std::streamoff>(limit)) {
        error = "Cannot read settings file (limit: 128 KiB)";
        return false;
    }
    std::string json(static_cast<std::size_t>(input.tellg()), '\0');
    input.seekg(0);
    if (!json.empty() && !input.read(&json[0], json.size())) {
        error = "Cannot read settings file";
        return false;
    }
    return decode(json, settings, error);
}
bool save(const std::filesystem::path& path, const Settings& settings, std::string& error) {
    if (!validate(settings, error))
        return false;
    const auto json = encode(settings);
    if (json.size() > limit) {
        error = "Settings file is too large";
        return false;
    }
    auto temporary = path;
    temporary += L".tmp";
    rxpce_host::UniqueHandle file(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        error = "Cannot create settings temporary file";
        return false;
    }
    DWORD written = 0;
    bool ok =
        WriteFile(file.get(), json.data(), static_cast<DWORD>(json.size()), &written, nullptr) &&
        written == json.size() && FlushFileBuffers(file.get());
    file.reset();
    if (ok)
        ok = MoveFileExW(temporary.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    if (!ok) {
        DeleteFileW(temporary.c_str());
        error = "Cannot replace settings file";
        return false;
    }
    error.clear();
    return true;
}
} // namespace rxpce_config
