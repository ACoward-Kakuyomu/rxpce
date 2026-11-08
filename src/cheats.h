#pragma once
#include "memory_search.h"
#include "replay_media.h"
#include <filesystem>

namespace rxpce_cheats {
inline constexpr std::size_t max_file_bytes = 1024 * 1024, max_entries = 1024;
struct Entry {
    std::string name;
    bool enabled = false;
    rxpce_search::RegionId region = rxpce_search::RegionId::main_ram;
    std::uint32_t offset = 0;
    unsigned bytes = 1;
    std::uint32_t value = 0;
    bool always_apply = false;
};
struct Document {
    rxpce_replay::MediaIdentity media{rxpce_replay::MediaKind::hucard, {}, {}};
    std::vector<Entry> cheats;
};
bool apply_once(rxpce_machine::Machine& machine, const Entry& entry, std::string& error);
bool validate(const Document& document, const rxpce_search::Snapshot& ram, std::string& error);
std::string encode(const Document& document);
bool decode(const std::string& json, const rxpce_replay::MediaIdentity& expected,
            const rxpce_search::Snapshot& ram, Document& output, std::string& error);
std::filesystem::path sidecar_path(const std::filesystem::path& image);
bool load(const std::filesystem::path& path, const rxpce_replay::MediaIdentity& expected,
          const rxpce_search::Snapshot& ram, Document& output, std::string& error);
bool save(const std::filesystem::path& path, const Document& document,
          const rxpce_search::Snapshot& ram, std::string& error);
class Editor {
  public:
    void reset(Document document);
    const Document& document() const { return working_; }
    bool dirty() const;
    bool store(const Entry& entry, std::optional<std::size_t> index,
               const rxpce_search::Snapshot& ram, std::string& error);
    void erase(std::size_t index);
    std::size_t from_search(const rxpce_search::Search& search, rxpce_search::Candidate candidate);
    void discard();
  private:
    Document saved_, working_;
};
} // namespace rxpce_cheats
