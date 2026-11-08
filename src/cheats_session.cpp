#include "cheats_session.h"
#include "machine.h"
#include "cheats_memory.h"

namespace rxpce_cheats {
std::vector<Session::Write> Session::bind(rxpce_machine::Machine& machine, const Document& document) {
    std::vector<Write> writes;
    for (const auto& entry : document.cheats) {
        if (!entry.enabled || !entry.always_apply) continue;
        const auto ram = detail::resolve_ram(machine, entry.region);
        if (!ram.data || entry.offset > ram.size || entry.bytes > ram.size - entry.offset)
            throw std::invalid_argument("Cheat target changed while resolving the RAM region");
        writes.push_back({ram.data + entry.offset, entry.bytes, entry.value});
    }
    return writes;
}
void Session::boundary(bool skip) {
    if (skip || disabled_) return;
    for (const auto& write : writes_)
        for (unsigned i = 0; i < write.bytes; ++i)
            write.destination[i] = static_cast<std::uint8_t>(write.value >> (i * 8));
}
void Session::start(rxpce_machine::Machine& machine, bool playback) {
    document_ = {};
    warning_.clear();
    path_.clear();
    disabled_ = playback;
    overwrite_confirmation_ = false;
    freezes_ = false;
    writes_.clear();
    if (playback) return;
    document_.media = rxpce_replay::current_media(machine);
    path_ = sidecar_path(machine.cd_session ? machine.cd_session->source : machine.source);
    overwrite_confirmation_ = !load(path_, document_.media, rxpce_search::capture_ram(machine), document_, warning_);
    writes_ = bind(machine, document_);
    freezes_ = !writes_.empty();
}
rxpce_search::Pause::Context Session::capture(const rxpce_machine::Machine& machine) const {
    return {rxpce_search::capture_ram(machine), document_, overwrite_confirmation_, warning_};
}
rxpce_search::Pause::Reply Session::process(rxpce_machine::Machine& machine,
                                         const rxpce_search::Pause::Request& request) {
    rxpce_search::Pause::Reply reply;
    if (disabled_) {
        reply.error = "Cheats are disabled during replay playback";
        return reply;
    }
    auto snapshot = rxpce_search::capture_ram(machine);
    if (request.kind == rxpce_search::Pause::Request::Kind::save) {
        const auto& media = request.document.media;
        if (media.kind != document_.media.kind || media.media != document_.media.media ||
            media.bios != document_.media.bios) {
            reply.error = "Cheat definitions belong to a different game or BIOS";
            return reply;
        }
        // Allocate both copies before publishing the file. Commit by moves only.
        auto committed = request.document;
        if (!validate(committed, snapshot, reply.error)) return reply;
        auto writes = bind(machine, committed);
        reply.saved = committed;
        if (!save(path_, committed, snapshot, reply.error)) {
            reply.saved.reset();
            return reply;
        }
        document_ = std::move(committed);
        writes_ = std::move(writes);
        freezes_ = !writes_.empty();
        warning_.clear();
        overwrite_confirmation_ = false;
    } else {
        if (!validate(Document{document_.media, {request.entry}}, snapshot, reply.error) ||
            !apply_once(machine, request.entry, reply.error)) return reply;
        // The immutable image was allocated before writing live RAM.
        for (auto& region : snapshot.regions) {
            if (region.id != request.entry.region) continue;
            for (unsigned i = 0; i < request.entry.bytes; ++i)
                region.bytes[request.entry.offset + i] = static_cast<std::uint8_t>(request.entry.value >> (i * 8));
        }
    }
    reply.snapshot = std::move(snapshot);
    return reply;
}
}
