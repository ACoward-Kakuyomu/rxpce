#pragma once
#include "capture.h"
#include <memory>
namespace rxpce_cd_dump {
class FileStore final : public CaptureSink {
    struct Impl;
    std::unique_ptr<Impl> impl_;
  public:
    FileStore(const Layout& layout, const std::wstring& destination, const std::string& device,
              const CancelCheck& cancel = {});
    ~FileStore() override;
    void write(unsigned track, std::uint64_t sector, const Bytes& bytes) override;
    bool matches(unsigned track, std::uint64_t sector, const Bytes& bytes) override;
    void commit(const Layout& layout, const CaptureStats& stats) override;
};
}
