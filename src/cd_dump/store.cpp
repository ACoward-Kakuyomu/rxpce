#include "store.h"
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <picojson.h>
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>
namespace rxpce_cd_dump {
namespace fs = std::filesystem;
namespace {
[[noreturn]] void io_error(const char* text) {
    throw std::runtime_error(std::string(text) + ": Win32 error " + std::to_string(GetLastError()));
}
void crypto_check(NTSTATUS status) {
    if (status < 0) throw std::runtime_error("Windows SHA-256 operation failed");
}
class Hash {
    BCRYPT_ALG_HANDLE algorithm_ = nullptr;
    BCRYPT_HASH_HANDLE hash_ = nullptr;
  public:
    Hash() {
        crypto_check(BCryptOpenAlgorithmProvider(&algorithm_,BCRYPT_SHA256_ALGORITHM,nullptr,0));
        const auto status=BCryptCreateHash(algorithm_,&hash_,nullptr,0,nullptr,0,0);
        if (status < 0) { BCryptCloseAlgorithmProvider(algorithm_,0);algorithm_=nullptr;crypto_check(status); }
    }
    ~Hash() {
        if (hash_) BCryptDestroyHash(hash_);
        if (algorithm_) BCryptCloseAlgorithmProvider(algorithm_,0);
    }
    Hash(const Hash&)=delete;
    Hash& operator=(const Hash&)=delete;
    void add(const Bytes& bytes) {
        crypto_check(BCryptHashData(hash_,const_cast<PUCHAR>(bytes.data()),static_cast<ULONG>(bytes.size()),0));
    }
    std::string finish() {
        std::array<unsigned char,32> digest{};
        crypto_check(BCryptFinishHash(hash_,digest.data(),static_cast<ULONG>(digest.size()),0));
        return hex(Bytes(digest.begin(),digest.end()));
    }
};
class OpenFile {
    HANDLE handle_ = INVALID_HANDLE_VALUE;
  public:
    explicit OpenFile(const fs::path& path) {
        handle_=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if (handle_==INVALID_HANDLE_VALUE) io_error("Create capture file");
    }
    ~OpenFile() { close(); }
    OpenFile(const OpenFile&)=delete;
    OpenFile& operator=(const OpenFile&)=delete;
    void close() { if(handle_!=INVALID_HANDLE_VALUE) { CloseHandle(handle_);handle_=INVALID_HANDLE_VALUE; } }
    void seek(std::uint64_t offset) {
        LARGE_INTEGER position{};position.QuadPart=static_cast<LONGLONG>(offset);
        if (!SetFilePointerEx(handle_,position,nullptr,FILE_BEGIN)) io_error("Seek capture file");
    }
    void append(const unsigned char* data,std::size_t bytes) {
        while (bytes) {
            const auto count=static_cast<DWORD>((std::min)(bytes,std::size_t(1024*1024)));
            DWORD done=0;
            if (!WriteFile(handle_,data,count,&done,nullptr)) io_error("Write capture file");
            if (done!=count) throw std::runtime_error("Short capture file write");
            bytes-=count;data+=count;
        }
    }
    bool matches(const Bytes& expected) {
        Bytes actual(expected.size());DWORD done=0;
        if (!ReadFile(handle_,actual.data(),static_cast<DWORD>(actual.size()),&done,nullptr)) io_error("Read capture file");
        return done==actual.size() && actual==expected;
    }
    void flush() { if (!FlushFileBuffers(handle_)) io_error("Flush capture file"); }
};
picojson::value number(std::int64_t value) { return picojson::value(static_cast<double>(value)); }
picojson::value text(const std::string& value) { return picojson::value(value); }
std::string track_name(unsigned number) {
    std::ostringstream name;name<<"track"<<std::setfill('0')<<std::setw(2)<<number<<".bin";return name.str();
}
}
struct FileStore::Impl {
    struct Entry {
        struct GapSegment { std::int32_t begin=0,end=0;std::string signature; };
        CaptureTrack range;
        std::unique_ptr<OpenFile> file;
        std::unique_ptr<Hash> hash;
        std::uint64_t written=0;
        std::vector<GapSegment> gaps;
    };
    Layout layout;
    std::string device;
    fs::path target,stage;
    std::vector<fs::path> owned;
    std::vector<Entry> entries;
    CancelCheck cancel;
    bool created=false,committed=false;
    ~Impl() {
        entries.clear();
        if (created && !committed) {
            // Never recursively delete a caller's directory or any unowned file.
            for (const auto& file:owned) DeleteFileW(file.c_str());
            RemoveDirectoryW(stage.c_str());
        }
    }
    std::unique_ptr<OpenFile> create(const fs::path& path) {
        auto file=std::make_unique<OpenFile>(path);
        try { owned.push_back(path); }
        catch (...) { file.reset();DeleteFileW(path.c_str());throw; }
        return file;
    }
    void document(const char* name,const std::string& bytes) {
        if(cancel) cancel();
        auto file=create(stage/name);
        file->append(reinterpret_cast<const unsigned char*>(bytes.data()),bytes.size());
        file->flush();
        if(cancel) cancel();
    }
    Entry& entry(unsigned track) {
        if (committed || track==0 || track>entries.size() || entries[track-1].range.toc.number!=track)
            throw std::runtime_error("Invalid capture output track/state");
        return entries[track-1];
    }
};
FileStore::FileStore(const Layout& layout,const std::wstring& destination,const std::string& device,const CancelCheck& cancel)
    : impl_(std::make_unique<Impl>()) {
    auto& s=*impl_;
    s.cancel=cancel;
    if(s.cancel) s.cancel();
    if (layout.tracks.empty() || layout.tracks.size()>99) throw std::runtime_error("Invalid output track count");
    for (std::size_t n=0;n<layout.tracks.size();++n) {
        const auto& track=layout.tracks[n];
        if (track.toc.number!=n+1 || track.begin<0 || track.end<=track.begin || track.end>449849)
            throw std::runtime_error("Invalid output track range");
    }
    s.layout=layout;s.device=device;s.target=fs::absolute(destination).lexically_normal();
    const auto parent=s.target.parent_path();
    if (s.target.filename().empty() || !fs::is_directory(parent) || fs::exists(s.target))
        throw std::runtime_error("Output folder must be new and its parent must exist");
    std::array<unsigned char,16> random{};
    crypto_check(BCryptGenRandom(nullptr,random.data(),static_cast<ULONG>(random.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG));
    s.stage=parent/(".rxpce-cd-partial-"+hex(Bytes(random.begin(),random.end())));
    if (!CreateDirectoryW(s.stage.c_str(),nullptr)) io_error("Create partial output folder");
    s.created=true;
    s.entries.reserve(layout.tracks.size());
    for (const auto& track:layout.tracks) {
        if(s.cancel) s.cancel();
        Impl::Entry entry;entry.range=track;
        entry.file=s.create(s.stage/track_name(track.toc.number));entry.hash=std::make_unique<Hash>();
        s.entries.push_back(std::move(entry));
    }
}
FileStore::~FileStore() = default;
void FileStore::write(unsigned track,std::uint64_t sector,const Bytes& bytes) {
    if(impl_->cancel) impl_->cancel();
    auto& entry=impl_->entry(track);
    const auto limit=static_cast<std::uint64_t>(entry.range.end-entry.range.begin)*2352;
    if (sector>limit/2352 || sector*2352!=entry.written || bytes.size()%2352 || bytes.size()>limit-entry.written)
        throw std::runtime_error("Invalid capture write range/order");
    entry.file->seek(entry.written);
    entry.file->append(bytes.data(),bytes.size());entry.hash->add(bytes);
    for(std::size_t n=0;n<bytes.size()/2352;++n) {
        const auto lba=entry.range.begin+static_cast<std::int32_t>(sector+n);
        if(lba>=entry.range.toc.index1) break;
        const auto* raw=bytes.data()+n*2352;
        const bool sync=raw[0]==0 && raw[11]==0 && std::all_of(raw+1,raw+11,[](auto b){return b==255;});
        const std::string signature=sync && raw[15]==1?"mode1_header":
                                    sync && raw[15]==2?"mode2_header":"no_cdrom_header";
        if(!entry.gaps.empty() && entry.gaps.back().end==lba && entry.gaps.back().signature==signature)
            entry.gaps.back().end=lba+1;
        else entry.gaps.push_back({lba,lba+1,signature});
    }
    entry.written+=bytes.size();
}
bool FileStore::matches(unsigned track,std::uint64_t sector,const Bytes& bytes) {
    if(impl_->cancel) impl_->cancel();
    auto& entry=impl_->entry(track);
    if (sector>entry.written/2352 || bytes.size()>entry.written-sector*2352)
        throw std::runtime_error("Capture comparison outside written data");
    entry.file->seek(sector*2352);return entry.file->matches(bytes);
}
void FileStore::commit(const Layout& layout,const CaptureStats& stats) {
    auto& s=*impl_;
    if(s.cancel) s.cancel();
    if (s.committed || layout.boundary_source!=s.layout.boundary_source ||
        !same_toc(layout.toc,s.layout.toc) || make_cue(layout)!=make_cue(s.layout) ||
        stats.captured!=static_cast<std::uint64_t>(layout.toc.leadout) || stats.verified!=stats.captured)
        throw std::runtime_error("Incomplete or inconsistent capture cannot be committed");
    picojson::object report;
    report["tool"]=text("rxpce_cd_dump");report["tool_version"]=text("0.1.0");
    report["status"]=text("repeated_reads_match");report["device"]=text(s.device);
    report["boundary_source"]=text(layout.boundary_source);
    report["q_boundaries_confirmed"]=picojson::value(layout.boundary_source=="positional_q");
    report["program_begin_lba"]=number(0);report["leadout_lba"]=number(layout.toc.leadout);
    report["reads"]=number(2);report["captured_sectors"]=number(static_cast<std::int64_t>(stats.captured));
    report["verified_sectors"]=number(static_cast<std::int64_t>(stats.verified));
    report["read_retries"]=number(stats.retries);report["batch_splits"]=number(stats.splits);
    report["capture_batch_sectors"]=number(stats.capture_batch);
    report["verify_batch_sectors"]=number(stats.verify_batch);
    report["audio_prime_reads"]=number(stats.audio_prime_reads);
    report["q_retries"]=number(layout.q_retries);report["nonposition_q_records"]=number(layout.nonposition_records);
    report["audio_offset_calibrated"]=picojson::value(false);
    report["drive_cache_bypass_guaranteed"]=picojson::value(false);
    report["media_generation_available"]=picojson::value(stats.media_generation_available);
    report["excluded_scope"]=text("Before program LBA 0, lead-in/out, full subchannels, offset calibration");
    picojson::array tracks;
    for (auto& entry:s.entries) {
        if(s.cancel) s.cancel();
        const auto& track=entry.range;
        if (entry.written!=static_cast<std::uint64_t>(track.end-track.begin)*2352)
            throw std::runtime_error("Incomplete output BIN");
        entry.file->flush();
        picojson::object info;
        info["number"]=number(track.toc.number);info["control"]=number(track.toc.control);
        info["file"]=text(track_name(track.toc.number));info["begin_lba"]=number(track.begin);
        info["index1_lba"]=number(track.toc.index1);info["end_lba"]=number(track.end);
        info["bytes"]=number(static_cast<std::int64_t>(entry.written));
        info["sha256"]=text(entry.hash->finish());tracks.emplace_back(info);
        picojson::array segments;
        for(const auto& gap:entry.gaps) {
            picojson::object segment;
            segment["begin_lba"]=number(gap.begin);segment["end_lba"]=number(gap.end);
            segment["observed_signature"]=text(gap.signature);segments.emplace_back(segment);
        }
        tracks.back().get<picojson::object>()["gap_segments"]=picojson::value(segments);
        entry.file->close();
    }
    report["tracks"]=picojson::value(tracks);
    picojson::array observations;
    for (const auto& item:layout.evidence) {
        picojson::object info;
        info["requested_lba"]=number(item.requested);info["q_lba"]=number(item.q.absolute);
        info["track"]=number(item.q.track);info["index"]=number(item.q.index);
        info["relative_frames"]=number(item.q.relative);info["control"]=number(item.q.control);
        observations.emplace_back(info);
    }
    report["q_observations"]=picojson::value(observations);
    s.document("disc.cue",make_cue(layout));s.document("capture.json",picojson::value(report).serialize(true)+"\n");
    if(s.cancel) s.cancel();
    if (!MoveFileExW(s.stage.c_str(),s.target.c_str(),MOVEFILE_WRITE_THROUGH)) io_error("Publish completed capture folder");
    s.committed=true;
}
}
