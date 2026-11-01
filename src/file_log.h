#pragma once
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
namespace rxpce_log {
// Stream parameter permits deterministic open/write/flush fault injection.
// Rotation belongs to the session owner and must not repeat on recovery.
template <class Stream = std::ofstream> class FileWriter {
  public:
    FileWriter(std::filesystem::path path, std::string header, Stream stream = Stream{})
        : path_(std::move(path)), header_(std::move(header)), stream_(std::move(stream)) {}
    void operator()(std::string_view text) {
        try {
            if (!stream_.is_open()) {
                stream_.clear();
                stream_.open(path_, std::ios::binary | (opened_ ? std::ios::app : std::ios::trunc));
                if (!stream_)
                    throw std::runtime_error("Cannot open diagnostic log");
                opened_ = true;
            }
            if (!header_written_) {
                stream_.write(header_.data(), static_cast<std::streamsize>(header_.size()));
                stream_.flush();
                if (!stream_)
                    throw std::runtime_error("Cannot write diagnostic session");
                header_written_ = true;
            }
            stream_.write(text.data(), static_cast<std::streamsize>(text.size()));
            stream_.flush();
            if (!stream_)
                throw std::runtime_error("Cannot write diagnostic log");
        } catch (...) {
            try {
                stream_.close();
                stream_.clear();
            } catch (...) {
            }
            throw;
        }
    }

  private:
    std::filesystem::path path_;
    std::string header_;
    Stream stream_;
    bool opened_ = false, header_written_ = false;
};
} // namespace rxpce_log
