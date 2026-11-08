#include "cheats.h"
#include <algorithm>
#include <tuple>

namespace rxpce_cheats {
void Editor::reset(Document document) {
    auto saved = document;
    working_ = std::move(document);
    saved_ = std::move(saved);
}
bool Editor::dirty() const {
    return working_.cheats.size() != saved_.cheats.size() ||
        !std::equal(working_.cheats.begin(), working_.cheats.end(), saved_.cheats.begin(),
            [](const Entry& a, const Entry& b) {
                return std::tie(a.name, a.enabled, a.region, a.offset, a.bytes, a.value, a.always_apply) ==
                       std::tie(b.name, b.enabled, b.region, b.offset, b.bytes, b.value, b.always_apply);
            });
}
bool Editor::store(const Entry& entry, std::optional<std::size_t> index,
                   const rxpce_search::Snapshot& ram, std::string& error) {
    if (index && *index >= working_.cheats.size()) {
        error = "Select a cheat to update";
        return false;
    }
    if (!index && working_.cheats.size() >= max_entries) {
        error = "Cheat list exceeds 1024 entries";
        return false;
    }
    if (!validate(Document{working_.media, {entry}}, ram, error)) return false;
    if (index) working_.cheats[*index] = entry;
    else working_.cheats.push_back(entry);
    error.clear();
    return true;
}
void Editor::erase(std::size_t index) {
    if (index >= working_.cheats.size()) throw std::out_of_range("Select a cheat to delete");
    working_.cheats.erase(working_.cheats.begin() + index);
}
std::size_t Editor::from_search(const rxpce_search::Search& search, rxpce_search::Candidate candidate) {
    const auto& region = search.current().regions.at(candidate.region);
    if (!rxpce_search::writable_bytes(region.id))
        throw std::invalid_argument("This RAM region supports search only");
    for (std::size_t i = 0; i < working_.cheats.size(); ++i) {
        const auto& entry = working_.cheats[i];
        if (entry.region == region.id && entry.offset == candidate.offset && entry.bytes == search.width())
            return i;
    }
    Entry entry;
    entry.name = region.name + " + " + std::to_string(candidate.offset);
    entry.region = region.id;
    entry.offset = static_cast<std::uint32_t>(candidate.offset);
    entry.bytes = search.width();
    entry.value = search.value(candidate);
    std::string error;
    if (!store(entry, {}, search.current(), error)) throw std::invalid_argument(error);
    return working_.cheats.size() - 1;
}
void Editor::discard() {
    working_ = saved_;
}
} // namespace rxpce_cheats
