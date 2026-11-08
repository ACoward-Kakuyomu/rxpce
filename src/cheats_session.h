#pragma once
#include "memory_search_pause.h"
#include <atomic>

namespace rxpce_cheats {
// Live definitions and paths belong to the emulation worker. Only freezes() is
// read by the UI; editing crosses the paused request channel.
class Session {
  public:
    void start(rxpce_machine::Machine& machine, bool playback);
    rxpce_search::Pause::Context capture(const rxpce_machine::Machine& machine) const;
    rxpce_search::Pause::Reply process(rxpce_machine::Machine& machine,
                                     const rxpce_search::Pause::Request& request);
    bool freezes() const { return freezes_.load(); }
    void boundary(bool skip);
    const std::string& warning() const { return warning_; } // Worker only.
  private:
    struct Write {
        std::uint8_t* destination;
        unsigned bytes;
        std::uint32_t value;
    };
    static std::vector<Write> bind(rxpce_machine::Machine& machine, const Document& document);
    std::vector<Write> writes_;
    Document document_;
    std::filesystem::path path_;
    std::string warning_;
    bool disabled_ = true, overwrite_confirmation_ = false;
    std::atomic_bool freezes_{false};
};
}
