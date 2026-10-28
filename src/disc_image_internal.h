#pragma once
#include "disc_image.h"

namespace rxpce_cd {
// Private format-specific owners; read() borrows a cached sector until the next
// backend read. A candidate replaces the mounted owner only after validation.
struct DiscImage::Impl {
    enum class Format { cue, chd };
    explicit Impl(Format value) : format(value) {}
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    virtual ~Impl() = default;
    const Format format;
    Toc toc;
    std::vector<ImageFileInfo> files;
    std::string identity;
    virtual const unsigned char* read(std::size_t track, std::uint64_t sector) = 0;
    struct Cue;
    struct Chd;
    static std::unique_ptr<Impl> open_cue(const std::wstring&, const ImageProgress&);
    static std::unique_ptr<Impl> open_chd(const std::wstring&, const ImageProgress&);
};
} // namespace rxpce_cd
