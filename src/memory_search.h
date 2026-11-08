#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>

namespace rxpce_machine { class Machine; }
namespace rxpce_search {
enum class RegionId { unknown, main_ram, backup_ram, cd_ram, super_ram, hucard_ram };
const char* region_key(RegionId region);
std::size_t writable_bytes(RegionId region);
struct Region {
    std::string name;
    std::vector<std::uint8_t> bytes;
    RegionId id = RegionId::unknown;
};
struct Snapshot {
    std::uint64_t epoch = 0;
    std::vector<Region> regions;
};
struct Candidate {
    std::size_t region;
    std::size_t offset;
};
enum class Condition { value, unchanged, changed, increased, decreased };
Snapshot capture_ram(const rxpce_machine::Machine& machine);
bool parse_value(const std::wstring& text, unsigned width, std::uint32_t& output);
std::wstring format_value(std::uint32_t value, unsigned width);
class Search {
  public:
    void capture(Snapshot snapshot);
    void refresh_current(Snapshot snapshot);
    void reset(unsigned width);
    void filter(Condition condition, std::uint32_t value = 0);
    bool has_previous() const { return previous_.has_value(); }
    unsigned width() const { return width_; }
    const std::vector<Candidate>& candidates() const { return candidates_; }
    const Snapshot& current() const { return current_; }
    std::uint32_t value(Candidate candidate) const;
    std::uint32_t previous_value(Candidate candidate) const;
  private:
    Snapshot current_;
    std::optional<Snapshot> previous_;
    std::optional<Snapshot> captured_;
    std::vector<Candidate> candidates_;
    unsigned width_ = 1;
};
} // namespace rxpce_search
