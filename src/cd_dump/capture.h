#pragma once
#include "layout.h"
#include <stdexcept>

namespace rxpce_cd_dump {
class Cancelled : public std::runtime_error {
  public:
    Cancelled() : std::runtime_error("Capture cancelled") {}
};
struct CaptureOptions { unsigned batch = 16, retries = 3; };
struct CaptureStats {
    std::uint64_t captured = 0, verified = 0;
    unsigned retries = 0, splits = 0;
    bool media_generation_available = false;
    unsigned capture_batch = 16, verify_batch = 15;
    unsigned audio_prime_reads = 0;
};
enum class CapturePhase { reading, verifying, retrying, finalizing };
struct CaptureProgress {
    bool verifying = false;
    unsigned track = 0;
    std::uint64_t completed = 0, total = 0;
    CapturePhase phase = CapturePhase::reading;
    std::int32_t lba = -1;
    unsigned attempt = 0;
};
class CaptureSink {
  public:
    virtual ~CaptureSink() = default;
    virtual void write(unsigned track, std::uint64_t sector, const Bytes& bytes) = 0;
    virtual bool matches(unsigned track, std::uint64_t sector, const Bytes& bytes) = 0;
    virtual void commit(const Layout& layout, const CaptureStats& stats) = 0;
};
using Progress = std::function<void(const CaptureProgress&)>;
// Fails before commit on unverified data, media changes, cancellation or I/O errors.
CaptureStats capture(Transport& drive, const Layout& layout, CaptureSink& output,
                     const CaptureOptions& options = {}, const Progress& progress = {},
                     const CancelCheck& cancel = {});
void validate_mode1(const std::uint8_t* sector, std::int32_t lba);
} // namespace rxpce_cd_dump
