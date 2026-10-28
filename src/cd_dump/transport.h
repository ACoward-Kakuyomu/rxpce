#pragma once
#include "model.h"
#include <memory>
#include <optional>

namespace rxpce_cd_dump {
class Transport {
  public:
    virtual ~Transport() = default;
    // Must throw on OS/SCSI failure. Returned bytes are the actual transfer.
    virtual Bytes execute(const Bytes& command, unsigned expected) = 0;
    virtual std::optional<std::uint32_t> media_generation() { return std::nullopt; }
};
struct DriveInfo {
    std::wstring path;
    std::string name;
};
std::vector<DriveInfo> list_drives();
std::string utf8(const std::wstring& text);
class WindowsDrive final : public Transport {
    struct Impl;
    std::unique_ptr<Impl> impl_;
  public:
    explicit WindowsDrive(const std::wstring& path);
    ~WindowsDrive() override;
    Bytes execute(const Bytes& command, unsigned expected) override;
    std::string inquiry();
    bool lock_media();
    std::optional<std::uint32_t> media_generation() override;
};
Toc read_toc(Transport& drive);
void read_session(Transport& drive);
unsigned read_profile(Transport& drive);
Bytes read_cd(Transport& drive, std::int32_t lba, unsigned count, unsigned subchannel,
              unsigned expected_type);
Bytes read_q_frame(Transport& drive,std::int32_t lba);
} // namespace rxpce_cd_dump
