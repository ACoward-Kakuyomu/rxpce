#include "disc_image_internal.h"
#include "disc_file_internal.h"
#include "disc_sector_internal.h"
extern "C" {
#include <libchdr/cdrom.h>
}
#include <algorithm>
#include <limits>
#include <sstream>
#include <cstdio>
#include <utility>

namespace rxpce_cd {
using namespace detail;
namespace {
std::wstring text_decode(const std::vector<unsigned char>& bytes) {
    if (bytes.size() >= 2 &&
        ((bytes[0] == 0xff && bytes[1] == 0xfe) || (bytes[0] == 0xfe && bytes[1] == 0xff)))
        throw std::runtime_error("Unsupported UTF-16 CUE encoding");
    const bool bom = bytes.size() >= 3 && bytes[0] == 0xef && bytes[1] == 0xbb && bytes[2] == 0xbf;
    const auto offset = bom ? 3u : 0u;
    if (bytes.size() == offset)
        return {};
    const auto input = reinterpret_cast<const char*>(bytes.data() + offset);
    const auto count = static_cast<int>(bytes.size() - offset);
    unsigned page = CP_UTF8;
    auto size = MultiByteToWideChar(page, MB_ERR_INVALID_CHARS, input, count, nullptr, 0);
    if (!size && !bom) {
        page = 932;
        size = MultiByteToWideChar(page, MB_ERR_INVALID_CHARS, input, count, nullptr, 0);
    }
    if (!size)
        throw std::runtime_error("Cannot decode CUE text");
    std::wstring result(size, L'\0');
    if (MultiByteToWideChar(page, MB_ERR_INVALID_CHARS, input, count, &result[0], size) != size ||
        result.find(L'\0') != std::wstring::npos)
        throw std::runtime_error("Invalid CUE text");
    return result;
}
std::wstring upper(std::wstring text) {
    for (auto& character : text)
        if (character >= L'a' && character <= L'z')
            character -= L'a' - L'A';
    return text;
}
bool space(wchar_t ch) {
    return ch == L' ' || ch == L'\t' || ch == L'\r' || ch == L'\v' || ch == L'\f';
}
std::vector<std::wstring> tokens(const std::wstring& line) {
    std::vector<std::wstring> out;
    std::size_t at = 0;
    while (at < line.size()) {
        while (at < line.size() && space(line[at]))
            ++at;
        if (at == line.size())
            break;
        if (line[at] == L'"') {
            const auto end = line.find(L'"', ++at);
            if (end == std::wstring::npos)
                throw std::runtime_error("Unterminated CUE quote");
            out.push_back(line.substr(at, end - at));
            at = end + 1;
            if (at < line.size() && !space(line[at]))
                throw std::runtime_error("Missing whitespace after CUE quote");
        } else {
            const auto start = at;
            while (at < line.size() && !space(line[at])) {
                if (line[at] == L'"')
                    throw std::runtime_error("Unexpected CUE quote");
                ++at;
            }
            out.push_back(line.substr(start, at - start));
        }
    }
    return out;
}
unsigned number(const std::wstring& text, unsigned limit) {
    if (text.empty())
        throw std::runtime_error("Missing CUE number");
    unsigned result = 0;
    for (auto ch : text) {
        if (ch < L'0' || ch > L'9' || result > limit / 10 || result * 10 + ch - L'0' > limit)
            throw std::runtime_error("Invalid CUE number");
        result = result * 10 + ch - L'0';
    }
    return result;
}
std::int64_t frames(const std::wstring& text) {
    const auto first = text.find(L':'), last = text.rfind(L':');
    if (first == std::wstring::npos || first == last || text.find(L':', first + 1) != last)
        throw std::runtime_error("Invalid CUE MSF");
    return (number(text.substr(0, first), 99) * 60 +
            number(text.substr(first + 1, last - first - 1), 59)) *
               75 +
           number(text.substr(last + 1), 74);
}
struct CueFile {
    std::wstring name;
    unsigned line = 0, sector_bytes = 0;
    std::size_t source = 0, first_track = 0;
};
struct CueTrack {
    TrackMode mode = TrackMode::audio;
    std::size_t file = 0;
    unsigned line = 0;
    std::int64_t index0 = -1, index1 = -1, pregap = 0;
    bool has_pregap = false;
};
struct ParsedCue {
    std::vector<CueFile> files;
    std::vector<CueTrack> tracks;
};
std::string line_error(unsigned line, const std::string& message) {
    return "CUE line " + std::to_string(line) + ": " + message;
}
ParsedCue parse(const std::wstring& text) {
    ParsedCue result;
    std::wistringstream input(text);
    std::wstring line;
    unsigned line_no = 0;
    bool in_track = false;
    while (std::getline(input, line)) {
        ++line_no;
        try {
            if (line.size() > 4096)
                throw std::runtime_error("CUE line is too long");
            auto first = line.find_first_not_of(L" \t\r\v\f");
            if (first == std::wstring::npos)
                continue;
            const auto key =
                upper(line.substr(first, line.find_first_of(L" \t\r\v\f", first) - first));
            if (key == L"REM" || key == L"TITLE" || key == L"PERFORMER" || key == L"SONGWRITER" ||
                key == L"CATALOG" || key == L"ISRC")
                continue;
            const auto values = tokens(line);
            if (key == L"FILE") {
                if (values.size() != 3 || values[1].empty() || upper(values[2]) != L"BINARY")
                    throw std::runtime_error("Expected FILE filename BINARY");
                if (result.files.size() >= 99)
                    throw std::runtime_error("Too many FILE declarations");
                result.files.push_back({values[1], line_no, 0, 0, result.tracks.size()});
                in_track = false;
            } else if (key == L"TRACK") {
                if (values.size() != 3 || result.files.empty() || result.tracks.size() >= 99 ||
                    number(values[1], 99) != result.tracks.size() + 1)
                    throw std::runtime_error("Expected sequential TRACK after FILE");
                const auto type = upper(values[2]);
                TrackMode mode;
                if (type == L"AUDIO")
                    mode = TrackMode::audio;
                else if (type == L"MODE1/2352")
                    mode = TrackMode::mode1_2352;
                else if (type == L"MODE1/2048")
                    mode = TrackMode::mode1_2048;
                else
                    throw std::runtime_error("Unsupported TRACK mode");
                const unsigned size = mode == TrackMode::mode1_2048 ? 2048 : 2352;
                auto& file = result.files.back();
                if (file.sector_bytes && file.sector_bytes != size)
                    throw std::runtime_error("Mixed sector sizes in one FILE");
                file.sector_bytes = size;
                result.tracks.push_back({mode, result.files.size() - 1, line_no});
                in_track = true;
            } else if (key == L"INDEX") {
                if (values.size() != 3 || !in_track)
                    throw std::runtime_error("INDEX without TRACK");
                auto& track = result.tracks.back();
                const auto index = number(values[1], 1);
                const auto frame = frames(values[2]);
                if (index == 0) {
                    if (track.index0 >= 0 || track.index1 >= 0)
                        throw std::runtime_error("Duplicate or misplaced INDEX 00");
                    track.index0 = frame;
                } else {
                    if (track.index1 >= 0 || (track.index0 >= 0 && track.index0 > frame))
                        throw std::runtime_error("Duplicate or descending INDEX 01");
                    track.index1 = frame;
                }
            } else if (key == L"PREGAP") {
                if (values.size() != 2 || !in_track)
                    throw std::runtime_error("PREGAP without TRACK");
                auto& track = result.tracks.back();
                if (track.has_pregap || track.index0 >= 0 || track.index1 >= 0)
                    throw std::runtime_error("Duplicate or misplaced PREGAP");
                track.pregap = frames(values[1]);
                track.has_pregap = true;
            } else {
                std::string keyword;
                for (auto ch : key)
                    keyword += ch >= 32 && ch < 127 ? static_cast<char>(ch) : '?';
                throw std::runtime_error("Unsupported CUE directive: " + keyword);
            }
        } catch (const std::exception& ex) {
            throw std::runtime_error(line_error(line_no, ex.what()));
        }
    }
    if (result.tracks.empty())
        throw std::runtime_error(line_error(1, "No tracks"));
    for (const auto& file : result.files)
        if (!file.sector_bytes)
            throw std::runtime_error(line_error(file.line, "FILE has no tracks"));
    bool data = false;
    for (const auto& track : result.tracks) {
        if (track.index1 < 0)
            throw std::runtime_error(line_error(track.line, "Missing INDEX 01"));
        data |= track.mode != TrackMode::audio;
    }
    if (!data)
        throw std::runtime_error(line_error(1, "No game data track"));
    return result;
}
} // namespace

struct DiscImage::Impl::Cue final : Impl {
    Cue() : Impl(Format::cue) {}
    std::vector<std::shared_ptr<File>> sources;
    std::array<unsigned char, 2352> cache{};
    bool cached = false;
    std::size_t cached_file = 0;
    std::uint64_t cached_offset = 0;
    unsigned cached_bytes = 0;
    const unsigned char* read(std::size_t track, std::uint64_t sector) override {
        return read(toc.tracks[track], sector);
    }
    const unsigned char* read(const Track& track, std::uint64_t sector) {
        const auto bytes = track.sector_bytes();
        if (sector > std::numeric_limits<std::uint64_t>::max() / bytes)
            throw std::runtime_error("File offset overflow");
        const auto offset = sector * bytes;
        if (!cached || cached_file != track.file || cached_offset != offset ||
            cached_bytes != bytes) {
            cached = false;
            sources[track.file]->read(offset, cache.data(), bytes);
            cached = true;
            cached_file = track.file;
            cached_offset = offset;
            cached_bytes = bytes;
        }
        return cache.data();
    }
};

std::unique_ptr<DiscImage::Impl> DiscImage::Impl::open_cue(const std::wstring& path,
                                                           const ImageProgress& progress) {
    const auto cue_path = full_path(path);
    File cue(cue_path);
    if (cue.bytes > 1024 * 1024)
        throw std::runtime_error("CUE exceeds 1 MiB");
    std::vector<unsigned char> text(static_cast<std::size_t>(cue.bytes));
    cue.read(0, text.data(), static_cast<unsigned>(text.size()));
    auto parsed = parse(text_decode(text));
    auto next = std::make_unique<Cue>();
    const auto folder = cue_path.substr(0, cue_path.find_last_of(L"/\\") + 1);
    for (auto& file : parsed.files) {
        try {
            const auto slash = [](wchar_t ch) { return ch == L'\\' || ch == L'/'; };
            const bool drive = file.name.size() >= 2 && file.name[1] == L':';
            const bool unc = file.name.size() >= 2 && slash(file.name[0]) && slash(file.name[1]);
            const bool absolute = unc || (drive && file.name.size() >= 3 && slash(file.name[2]));
            if (!absolute && (drive || slash(file.name[0])))
                throw std::runtime_error(
                    "Drive-relative or root-relative FILE path is unsupported");
            auto source =
                std::make_shared<File>(full_path(absolute ? file.name : folder + file.name));
            if (!source->bytes || source->bytes % file.sector_bytes ||
                source->bytes / file.sector_bytes > 900000)
                throw std::runtime_error("Invalid BIN sector count or size remainder");
            const auto found = std::find_if(next->sources.begin(), next->sources.end(),
                                            [&](const std::shared_ptr<File>& previous) {
                                                return source->same_file(*previous);
                                            });
            file.source = static_cast<std::size_t>(found - next->sources.begin());
            if (found == next->sources.end()) {
                next->files.push_back({source->bytes, {}});
                next->sources.push_back(std::move(source));
            }
        } catch (const std::exception& ex) {
            throw std::runtime_error(line_error(file.line, ex.what()));
        }
    }
    std::int64_t cursor = 0;
    for (std::size_t n = 0; n < parsed.tracks.size(); ++n) {
        const auto& raw = parsed.tracks[n];
        const auto& file = parsed.files[raw.file];
        const auto begin = raw.index0 >= 0 ? raw.index0 : raw.index1;
        const auto file_frames =
            static_cast<std::int64_t>(next->sources[file.source]->bytes / file.sector_bytes);
        auto end = file_frames;
        if (n + 1 < parsed.tracks.size() && parsed.tracks[n + 1].file == raw.file) {
            const auto& after = parsed.tracks[n + 1];
            end = after.index0 >= 0 ? after.index0 : after.index1;
        }
        if ((file.first_track == n && begin != 0) || begin < 0 || raw.index1 < begin ||
            end <= raw.index1 || end > file_frames)
            throw std::runtime_error(line_error(raw.line, "Invalid INDEX order or file boundary"));
        Track track;
        track.number = static_cast<unsigned>(n + 1);
        track.mode = raw.mode;
        track.file = file.source;
        track.file_sector = static_cast<std::uint64_t>(begin);
        track.pregap_lba = cursor;
        track.file_lba = cursor + raw.pregap;
        track.start_lba = track.file_lba + raw.index1 - begin;
        track.end_lba = track.file_lba + end - begin;
        cursor = track.end_lba;
        next->toc.tracks.push_back(track);
    }
    const auto origin = next->toc.tracks.front().start_lba;
    for (auto& track : next->toc.tracks) {
        track.pregap_lba -= origin;
        track.file_lba -= origin;
        track.start_lba -= origin;
        track.end_lba -= origin;
    }
    next->toc.leadout = cursor - origin;
    std::array<unsigned char, 3> end_time;
    if (!lba_to_msf(next->toc.leadout, end_time))
        throw std::runtime_error("Disc leadout exceeds BCD MSF range");
    std::uint64_t total = 0, done = 0;
    for (const auto& source : next->sources)
        total += source->bytes;
    const auto report = [&] {
        if (progress && !progress(done, total))
            throw std::runtime_error("Disc image loading cancelled");
    };
    report();
    std::vector<unsigned char> buffer(1024 * 1024);
    for (std::size_t n = 0; n < next->sources.size(); ++n) {
        Sha256 hash;
        const auto& source = next->sources[n];
        for (std::uint64_t offset = 0; offset < source->bytes;) {
            const auto count = static_cast<unsigned>(
                (std::min)(source->bytes - offset, static_cast<std::uint64_t>(buffer.size())));
            source->read(offset, buffer.data(), count);
            hash.add(buffer.data(), count);
            offset += count;
            done += count;
            report();
        }
        next->files[n].sha256 = hash.finish();
    }
    Sha256 identity;
    const char tag[] = "RXPCE-DISC-01";
    identity.add(tag, sizeof(tag) - 1);
    identity.integer(next->toc.tracks.size());
    for (const auto& track : next->toc.tracks) {
        identity.integer(track.number);
        identity.integer(static_cast<unsigned>(track.mode));
        identity.integer(track.file);
        identity.integer(static_cast<std::uint64_t>(track.pregap_lba));
        identity.integer(static_cast<std::uint64_t>(track.file_lba));
        identity.integer(static_cast<std::uint64_t>(track.start_lba));
        identity.integer(static_cast<std::uint64_t>(track.end_lba));
        identity.integer(track.file_sector);
    }
    identity.integer(next->files.size());
    for (const auto& file : next->files) {
        identity.integer(file.bytes);
        identity.add(file.sha256.data(), file.sha256.size());
    }
    next->identity = identity.finish();
    return next;
}

bool DiscImage::write_materialized_cue(const std::wstring& folder, std::string& error,
                                       const ImageProgress& progress) {
    struct OutputFolder {
        std::wstring path;
        std::vector<std::wstring> files;
        bool created = false, complete = false;
        ~OutputFolder() {
            if (created && !complete) {
                for (const auto& file : files)
                    DeleteFileW(file.c_str());
                RemoveDirectoryW(path.c_str());
            }
        }
    } output;
    try {
        if (!impl_ || impl_->format != Impl::Format::cue)
            throw std::runtime_error("Open a source CUE before materializing gaps");
        output.path = full_path(folder);
        if (!CreateDirectoryW(output.path.c_str(), nullptr))
            throw std::runtime_error("Output folder must be new and its parent must exist");
        output.created = true;
        const auto create = [&](const std::wstring& path, Handle& file) {
            file.value = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                     FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file.value == INVALID_HANDLE_VALUE)
                throw std::runtime_error("Cannot create materialized CUE/BIN");
            output.files.push_back(path);
        };
        const auto write = [](HANDLE file, const void* data, unsigned size) {
            DWORD done = 0;
            if (size && (!WriteFile(file, data, size, &done, nullptr) || done != size))
                throw std::runtime_error("Cannot write materialized CUE/BIN");
        };
        const auto msf = [](std::uint64_t frames) {
            if (frames >= 450000)
                throw std::runtime_error("Materialized INDEX exceeds CUE MSF range");
            char text[16];
            std::snprintf(text, sizeof(text), "%02u:%02u:%02u", unsigned(frames / 4500),
                          unsigned(frames / 75 % 60), unsigned(frames % 75));
            return std::string(text);
        };
        std::uint64_t total = 0, done = 0;
        for (const auto& track : impl_->toc.tracks)
            total +=
                static_cast<std::uint64_t>(track.end_lba - track.pregap_lba) * track.sector_bytes();
        const auto report = [&] {
            if (progress && !progress(done, total))
                throw std::runtime_error("CUE materialization cancelled");
        };
        report();
        std::string cue;
        for (std::size_t n = 0; n < impl_->toc.tracks.size(); ++n) {
            const auto& track = impl_->toc.tracks[n];
            char name[32];
            std::snprintf(name, sizeof(name), "track%02u.bin", track.number);
            const std::string name_text = name;
            const std::wstring wide_name(name_text.begin(), name_text.end());
            Handle file;
            create(output.path + L"\\" + wide_name, file);
            std::vector<unsigned char> buffer;
            buffer.reserve(1024 * 1024 + 2352);
            for (auto lba = track.pregap_lba; lba < track.end_lba; ++lba) {
                std::array<unsigned char, 2352> generated{};
                const unsigned char* sector = generated.data();
                if (lba >= track.file_lba) {
                    // Verify stored RAW data, then preserve all 2352 original
                    // bytes, including its header, EDC and parity.
                    if (track.mode == TrackMode::mode1_2352) {
                        std::array<unsigned char, 2048> data;
                        if (!read_data(lba, data, error))
                            throw std::runtime_error(error);
                    }
                    sector = impl_->read(n, track.file_sector +
                                                static_cast<std::uint64_t>(lba - track.file_lba));
                } else if (track.mode == TrackMode::mode1_2352) {
                    std::fill(generated.begin() + 1, generated.begin() + 11, 255);
                    std::array<unsigned char, 3> time{};
                    if (!lba_to_msf(lba, time))
                        throw std::runtime_error("Generated RAW pregap has no representable MSF");
                    std::copy(time.begin(), time.end(), generated.begin() + 12);
                    generated[15] = 1;
                    const auto checksum = edc(generated.data(), 2064);
                    for (unsigned i = 0; i < 4; ++i)
                        generated[2064 + i] = static_cast<unsigned char>(checksum >> (i * 8));
                    ecc_generate(generated.data());
                }
                buffer.insert(buffer.end(), sector, sector + track.sector_bytes());
                done += track.sector_bytes();
                if (buffer.size() >= 1024 * 1024) {
                    write(file.value, buffer.data(), static_cast<unsigned>(buffer.size()));
                    buffer.clear();
                    report();
                }
            }
            write(file.value, buffer.data(), static_cast<unsigned>(buffer.size()));
            if (!FlushFileBuffers(file.value))
                throw std::runtime_error("Cannot flush materialized BIN");
            report();
            const auto mode = track.mode == TrackMode::audio        ? "AUDIO"
                              : track.mode == TrackMode::mode1_2352 ? "MODE1/2352"
                                                                    : "MODE1/2048";
            char number_text[4];
            std::snprintf(number_text, sizeof(number_text), "%02u", track.number);
            cue += "FILE \"" + name_text + "\" BINARY\n  TRACK " + number_text + " " + mode + "\n";
            const auto gap = static_cast<std::uint64_t>(track.start_lba - track.pregap_lba);
            if (gap)
                cue += "    INDEX 00 00:00:00\n";
            cue += "    INDEX 01 " + msf(gap) + "\n";
        }
        {
            Handle file;
            create(output.path + L"\\disc.cue", file);
            write(file.value, cue.data(), static_cast<unsigned>(cue.size()));
            if (!FlushFileBuffers(file.value))
                throw std::runtime_error("Cannot flush materialized CUE");
        }
        output.complete = true;
        error.clear();
        return true;
    } catch (const std::exception& ex) {
        error = ex.what();
        return false;
    }
}
} // namespace rxpce_cd
