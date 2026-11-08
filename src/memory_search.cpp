#include "memory_search.h"
#include <algorithm>
#include <limits>
#include <utility>
#include <cwchar>

namespace rxpce_search {
const char* region_key(RegionId region) {
    switch (region) {
    case RegionId::main_ram: return "main_ram";
    case RegionId::cd_ram: return "cd_ram";
    case RegionId::super_ram: return "super_ram";
    default: return "";
    }
}
std::size_t writable_bytes(RegionId region) {
    switch (region) {
    case RegionId::main_ram: return 8192;
    case RegionId::cd_ram: return 65536;
    case RegionId::super_ram: return 196608;
    default: return 0;
    }
}
namespace {
std::uint32_t read_value(const Snapshot& snapshot, Candidate candidate, unsigned width) {
    const auto& bytes = snapshot.regions.at(candidate.region).bytes;
    if (candidate.offset > bytes.size() || width > bytes.size() - candidate.offset)
        throw std::out_of_range("Search value crosses the RAM region boundary");
    std::uint32_t value = 0;
    for (unsigned byte = 0; byte < width; ++byte)
        value |= std::uint32_t(bytes[candidate.offset + byte]) << (byte * 8);
    return value;
}
bool same_layout(const Snapshot& first, const Snapshot& second) {
    if (first.epoch != second.epoch || first.regions.size() != second.regions.size())
        return false;
    for (std::size_t i = 0; i < first.regions.size(); ++i)
        if (first.regions[i].id != second.regions[i].id || first.regions[i].name != second.regions[i].name ||
            first.regions[i].bytes.size() != second.regions[i].bytes.size())
            return false;
    return true;
}
}
bool parse_value(const std::wstring& text, unsigned width, std::uint32_t& output) {
    if ((width != 1 && width != 2 && width != 4) || text.empty())
        return false;
    const bool hex = text.size() >= 2 && text[0] == L'0' && (text[1] == L'x' || text[1] == L'X');
    const unsigned base = hex ? 16 : 10;
    const std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max() >> ((4 - width) * 8);
    std::uint32_t result = 0;
    const std::size_t start = hex ? 2 : 0;
    if (start == text.size())
        return false;
    for (std::size_t i = start; i < text.size(); ++i) {
        const auto ch = text[i];
        unsigned digit = 16;
        if (ch >= L'0' && ch <= L'9') digit = ch - L'0';
        else if (ch >= L'a' && ch <= L'f') digit = ch - L'a' + 10;
        else if (ch >= L'A' && ch <= L'F') digit = ch - L'A' + 10;
        if (digit >= base || digit > maximum || result > (maximum - digit) / base)
            return false;
        result = result * base + digit;
    }
    output = result;
    return true;
}
std::wstring format_value(std::uint32_t value, unsigned width) {
    if ((width != 1 && width != 2 && width != 4) ||
        value > (UINT32_MAX >> ((4 - width) * 8)))
        throw std::invalid_argument("Value does not fit the selected size");
    wchar_t text[40]{};
    std::swprintf(text, 40, L"%u (0x%0*X)", static_cast<unsigned>(value),
                  static_cast<int>(width * 2), static_cast<unsigned>(value));
    return text;
}
void Search::capture(Snapshot snapshot) {
    const bool compatible = same_layout(current_, snapshot);
    if (!compatible)
        candidates_.clear();
    previous_.reset();
    if (compatible) {
        if (captured_) previous_ = std::move(*captured_);
        else previous_ = std::move(current_);
    }
    captured_.reset();
    current_ = std::move(snapshot);
    if (!compatible)
        reset(width_);
}
void Search::refresh_current(Snapshot snapshot) {
    if (!same_layout(current_, snapshot))
        throw std::invalid_argument("Memory capture belongs to a different paused session");
    if (!captured_) captured_ = current_;
    current_ = std::move(snapshot);
}
void Search::reset(unsigned width) {
    if (width != 1 && width != 2 && width != 4)
        throw std::invalid_argument("Search size must be 1, 2 or 4 bytes");
    std::vector<Candidate> candidates;
    for (std::size_t region = 0; region < current_.regions.size(); ++region) {
        const auto size = current_.regions[region].bytes.size();
        if (size >= width)
            for (std::size_t offset = 0; offset <= size - width; ++offset)
                candidates.push_back({region, offset});
    }
    candidates_ = std::move(candidates);
    width_ = width;
}
std::uint32_t Search::value(Candidate candidate) const {
    return read_value(current_, candidate, width_);
}
std::uint32_t Search::previous_value(Candidate candidate) const {
    if (!previous_)
        throw std::invalid_argument("Capture RAM again with F9 before comparing");
    return read_value(*previous_, candidate, width_);
}
void Search::filter(Condition condition, std::uint32_t wanted) {
    if (condition != Condition::value && !previous_)
        throw std::invalid_argument("Capture RAM again with F9 before comparing");
    candidates_.erase(std::remove_if(candidates_.begin(), candidates_.end(), [&](Candidate candidate) {
        const auto now = value(candidate);
        if (condition == Condition::value)
            return now != wanted;
        const auto before = previous_value(candidate);
        switch (condition) {
        case Condition::unchanged: return now != before;
        case Condition::changed: return now == before;
        case Condition::increased: return now <= before;
        case Condition::decreased: return now >= before;
        default: throw std::invalid_argument("Unknown search condition");
        }
    }), candidates_.end());
}
} // namespace rxpce_search
