#include "win32_support.h"
#ifdef SOUND
#include "ds_internal.h"
#include "Sound.h"
#include "log.h"
#include "audio_mixer.h"
#include "audio_stream.h"
#include "audio_error_log.h"
#include "unique_handle.h"
#include "multimedia_task.h"
#include <process.h>
#include <vector>
#include <utility>
#ifdef RXPCE_REVIEW_V160_TEST
#include "review_v160_hooks.h"
#endif

using Microsoft::WRL::ComPtr;
ComPtr<IDirectSound> lpDSound;
ComPtr<IDirectSoundBuffer> lpSoundBuffer;
int ds_nBits = 16, ds_nChannels = 2, ds_nSampleRate = 44100;
int ds_nLockLatency = 40, ds_CoopMode = 2;
DWORD ds_dwLockSize = 0, ds_dwBufferSize = 0;

namespace {
ComPtr<IDirectSoundBuffer> primary_buffer;
rxpce_host::UniqueHandle stop_event, playback_thread, poll_timer;
class TimerResolution {
  public:
    TimerResolution() = default;
    TimerResolution(const TimerResolution&) = delete;
    TimerResolution& operator=(const TimerResolution&) = delete;
    ~TimerResolution() {
        reset();
    }
    bool acquire() {
        reset();
        active_ = timeBeginPeriod(1) == TIMERR_NOERROR;
        return active_;
    }
    void reset() {
        if (active_) {
            timeEndPeriod(1);
            active_ = false;
        }
    }

  private:
    bool active_ = false;
};
TimerResolution timer_resolution;
struct Recovery {
    bool pending = false, primary = false, output = false, clear = false;
} recovery;
DWORD next_write = 0;
std::vector<float> mixed;
rxpce_audio::PlaybackCursor stream_cursor;
DWORD reported_lead = 0;

class BufferLock {
    IDirectSoundBuffer* buffer_;
    bool locked_ = false;

  public:
    void* first = nullptr;
    void* second = nullptr;
    DWORD first_bytes = 0, second_bytes = 0;
    explicit BufferLock(IDirectSoundBuffer* buffer) : buffer_(buffer) {}
    HRESULT lock(DWORD offset, DWORD bytes, DWORD flags = 0) {
        const auto result =
            buffer_->Lock(offset, bytes, &first, &first_bytes, &second, &second_bytes, flags);
        locked_ = SUCCEEDED(result);
        return result;
    }
    HRESULT unlock() {
        locked_ = false;
        return buffer_->Unlock(first, first_bytes, second, second_bytes);
    }
    ~BufferLock() {
        if (locked_)
            unlock();
    }
};

HRESULT clear_buffer() {
    BufferLock region(lpSoundBuffer.Get());
    const auto result = region.lock(0, 0, DSBLOCK_ENTIREBUFFER);
    if (FAILED(result))
        return result;
    if (!region.first || region.first_bytes > ds_dwBufferSize ||
        region.second_bytes != ds_dwBufferSize - region.first_bytes ||
        (region.second_bytes && !region.second))
        return E_INVALIDARG;
    if (region.first)
        std::memset(region.first, 0, region.first_bytes);
    if (region.second)
        std::memset(region.second, 0, region.second_bytes);
    return region.unlock();
}

bool retryable(HRESULT status) {
    return status == DSERR_BUFFERLOST || status == DSERR_PRIOLEVELNEEDED;
}
void mark_lost(bool primary) {
    if (!recovery.pending) {
        lpSoundBuffer->Stop();
        if (primary_buffer)
            primary_buffer->Stop();
    }
    recovery.pending = recovery.clear = true;
    (primary ? recovery.primary : recovery.output) = true;
    stream_cursor.reset(0);
    reported_lead = 0;
}
void report_error(rxpce_audio::ErrorLog::Event event) noexcept {
    using Kind = rxpce_audio::ErrorLog::Kind;
    if (event.kind == Kind::failure)
        LogDump("DirectSound error: %08lx\n", static_cast<unsigned long>(event.code));
    else if (event.kind == Kind::repeated)
        LogDump("DirectSound repeated error: %08lx count=%llu\n",
                static_cast<unsigned long>(event.code),
                static_cast<unsigned long long>(event.count));
    else
        LogDump("DirectSound recovered after error: %08lx\n",
                static_cast<unsigned long>(event.code));
}
HRESULT service_buffer();
HRESULT write_buffer(DWORD bytes);

unsigned __stdcall play_proc(void*) {
    rxpce_host::MultimediaTask task(L"Audio");
    task.set_enabled(true);
    if (!task.active())
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    LogDumpDetail("DirectSound multimedia scheduling: mmcss=%d\n", task.active() ? 1 : 0);
    // Stop the looping device buffer even if the worker exits on an error.
    struct StopPlayback {
        ~StopPlayback() {
            lpSoundBuffer->Stop();
            if (primary_buffer)
                primary_buffer->Stop();
        }
    } stop_playback;
    const HANDLE events[] = {stop_event.get(), poll_timer.get()};
    rxpce_audio::ErrorLog errors;
    HRESULT terminal = S_OK;
    try {
        for (;;) {
            const DWORD result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
            if (result == WAIT_OBJECT_0)
                break;
            if (result != WAIT_OBJECT_0 + 1) {
                terminal = E_FAIL;
                break;
            }
            const auto status = service_buffer();
            if (SUCCEEDED(status)) {
                errors.success(GetTickCount64(), report_error);
                continue;
            }
            if (!retryable(status)) {
                terminal = status;
                break;
            }
            errors.error(static_cast<std::uint32_t>(status), GetTickCount64(), report_error);
            const auto stopped = WaitForSingleObject(stop_event.get(), 100);
            if (stopped == WAIT_OBJECT_0)
                break;
            if (stopped != WAIT_TIMEOUT) {
                terminal = E_FAIL;
                break;
            }
        }
    } catch (...) {
        terminal = E_FAIL;
    }
    if (FAILED(terminal)) {
        errors.error(static_cast<std::uint32_t>(terminal), GetTickCount64(), report_error);
        ReportSoundOutputFailure(rxpce_audio::OutputBackend::directsound);
    }
    errors.finish(report_error);
    return FAILED(terminal) ? 1 : 0;
}
} // namespace

namespace {
HRESULT service_buffer() {
    if (!lpSoundBuffer || (ds_nChannels != 1 && ds_nChannels != 2))
        return E_INVALIDARG;
    const DWORD frame_bytes = ds_nChannels * sizeof(std::int16_t);
    if (ds_dwBufferSize < frame_bytes * 2 || ds_dwBufferSize % frame_bytes || !ds_dwLockSize ||
        ds_dwLockSize % frame_bytes)
        return E_INVALIDARG;
    for (const auto primary : {true, false}) {
        auto* buffer = primary ? primary_buffer.Get() : lpSoundBuffer.Get();
        if (!buffer)
            continue;
        DWORD state = 0;
        const auto status = buffer->GetStatus(&state);
        if (status == DSERR_BUFFERLOST || (SUCCEEDED(status) && (state & DSBSTATUS_BUFFERLOST)))
            mark_lost(primary);
        else if (FAILED(status))
            return status;
    }
    if (recovery.primary) {
        const auto status = primary_buffer->Restore();
        if (FAILED(status))
            return status;
        recovery.primary = false;
    }
    if (recovery.output) {
        const auto status = lpSoundBuffer->Restore();
        if (FAILED(status))
            return status;
        recovery.output = false;
    }
    if (recovery.clear) {
        const auto status = clear_buffer();
        if (status == DSERR_BUFFERLOST)
            mark_lost(false);
        if (FAILED(status))
            return status;
        recovery.clear = false;
    }
    DWORD play, safe_write;
    auto result = lpSoundBuffer->GetCurrentPosition(&play, &safe_write);
    if (result == DSERR_BUFFERLOST) {
        mark_lost(false);
        return result;
    }
    if (FAILED(result))
        return result;
    if (play >= ds_dwBufferSize || safe_write >= ds_dwBufferSize)
        return E_INVALIDARG;
    const DWORD capacity = ds_dwBufferSize / frame_bytes;
    if (stream_cursor.capacity() != capacity) {
        const DWORD target =
            static_cast<DWORD>(static_cast<std::uint64_t>(ds_nSampleRate) * ds_nLockLatency / 1000);
        stream_cursor.reset(capacity, target);
    }
    const auto safe_frame = ((safe_write + frame_bytes - 1) / frame_bytes) % capacity;
    const bool underrun = stream_cursor.poll(play / frame_bytes, safe_frame);
    ReportAudioOutput(underrun, false);
    if (underrun)
        LogDumpDetail("Audio buffer underrun: play=%lu safe=%lu\n", play, safe_write);
    const auto lead = stream_cursor.effective_lead(ds_dwLockSize / frame_bytes);
    if (lead != reported_lead) {
        LogDumpDetail("Audio lead: requested=%d ms effective=%.2f ms protected=%.2f ms\n",
                      ds_nLockLatency, lead * 1000.0 / ds_nSampleRate,
                      ((safe_write + ds_dwBufferSize - play) % ds_dwBufferSize) * 1000.0 /
                          (ds_nSampleRate * frame_bytes));
        reported_lead = lead;
    }
    while (const auto needed = stream_cursor.needed(ds_dwLockSize / frame_bytes)) {
        const auto frames =
            stream_cursor.write_ready(ds_dwLockSize / frame_bytes, AvailableMixedAudioFrames());
        if (frames < needed)
            ReportAudioOutput(false, true);
        if (!frames)
            break;
        next_write = stream_cursor.offset() * frame_bytes;
        result = write_buffer(frames * frame_bytes);
        if (FAILED(result))
            return result;
        stream_cursor.commit(frames);
    }
    if (recovery.pending) {
        for (const auto primary : {true, false}) {
            auto* buffer = primary ? primary_buffer.Get() : lpSoundBuffer.Get();
            if (!buffer)
                continue;
            result = buffer->Play(0, 0, DSBPLAY_LOOPING);
            if (result == DSERR_BUFFERLOST)
                mark_lost(primary);
            if (FAILED(result))
                return result;
        }
        recovery = {};
    }
    return S_OK;
}
} // namespace
BOOL ServiceDSoundBuffer() {
    return SUCCEEDED(service_buffer());
}

BOOL InitDSound(HWND hwnd, const rxpce_audio::OutputSettings& settings) {
    ReleaseDSound();
    if (!settings.sample_rate || settings.sample_rate > 192000 ||
        (settings.channels != 1 && settings.channels != 2) || !settings.latency_ms ||
        settings.latency_ms > 1000 || settings.cooperative_level < 0 ||
        settings.cooperative_level > 2)
        return FALSE;
    ds_nSampleRate = static_cast<int>(settings.sample_rate);
    ds_nChannels = static_cast<int>(settings.channels);
    ds_nLockLatency = static_cast<int>(settings.latency_ms);
    ds_CoopMode = settings.cooperative_level;
    if (ds_nSampleRate <= 0 || (ds_nChannels != 1 && ds_nChannels != 2) || ds_nBits != 16 ||
        ds_nLockLatency <= 0 || ds_nLockLatency > 1000)
        return FALSE;
    // Balance the audio timer resolution request during teardown.
    if (!timer_resolution.acquire())
        return FALSE;
    HRESULT result;
#ifdef RXPCE_REVIEW_V160_TEST
    if (rxpce_review_v160::create_dsound)
        result = rxpce_review_v160::create_dsound(lpDSound.ReleaseAndGetAddressOf());
    else
#endif
        result = DirectSoundCreate(nullptr, lpDSound.ReleaseAndGetAddressOf(), nullptr);
    if (FAILED(result)) {
        ReleaseDSound();
        return FALSE;
    }
    const DWORD modes[] = {DSSCL_WRITEPRIMARY, DSSCL_EXCLUSIVE, DSSCL_PRIORITY};
    const int mode = ds_CoopMode >= 0 && ds_CoopMode <= 2 ? ds_CoopMode : 2;
    result = lpDSound->SetCooperativeLevel(hwnd, modes[mode]);
    if (FAILED(result)) {
        ReleaseDSound();
        return FALSE;
    }

    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = static_cast<WORD>(ds_nChannels);
    format.nSamplesPerSec = ds_nSampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = format.nChannels * sizeof(std::int16_t);
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    DSBUFFERDESC primary{};
    primary.dwSize = sizeof(primary);
    primary.dwFlags = DSBCAPS_PRIMARYBUFFER;
    ComPtr<IDirectSoundBuffer> device_primary;
    result = lpDSound->CreateSoundBuffer(&primary, device_primary.GetAddressOf(), nullptr);
    if (FAILED(result)) {
        ReleaseDSound();
        return FALSE;
    }
    auto* primary_view = device_primary.Get();
    if (mode == 0)
        lpSoundBuffer = std::move(device_primary);
    else
        primary_buffer = std::move(device_primary);
    result = primary_view->SetFormat(&format);
    if (FAILED(result)) {
        ReleaseDSound();
        return FALSE;
    }

    if (mode == 0) {
        DSBCAPS caps{};
        caps.dwSize = sizeof(caps);
        result = lpSoundBuffer->GetCaps(&caps);
        if (FAILED(result)) {
            ReleaseDSound();
            return FALSE;
        }
        ds_dwBufferSize = caps.dwBufferBytes;
        WAVEFORMATEX actual{};
        result = lpSoundBuffer->GetFormat(&actual, sizeof(actual), nullptr);
        if (FAILED(result) || actual.wFormatTag != WAVE_FORMAT_PCM || actual.wBitsPerSample != 16 ||
            actual.nChannels != format.nChannels ||
            actual.nSamplesPerSec != format.nSamplesPerSec) {
            ReleaseDSound();
            return FALSE;
        }
    } else {
        const DWORD frames = rxpce_audio::stream_capacity_frames(ds_nSampleRate, ds_nLockLatency);
        ds_dwBufferSize = frames * format.nBlockAlign;
        DSBUFFERDESC secondary{};
        secondary.dwSize = sizeof(secondary);
        secondary.dwFlags = DSBCAPS_STICKYFOCUS | DSBCAPS_GETCURRENTPOSITION2;
        secondary.dwBufferBytes = ds_dwBufferSize;
        secondary.lpwfxFormat = &format;
        result = lpDSound->CreateSoundBuffer(&secondary, lpSoundBuffer.ReleaseAndGetAddressOf(),
                                             nullptr);
        if (FAILED(result)) {
            ReleaseDSound();
            return FALSE;
        }
        DSBCAPS actual{};
        actual.dwSize = sizeof(actual);
        result = lpSoundBuffer->GetCaps(&actual);
        if (FAILED(result)) {
            ReleaseDSound();
            return FALSE;
        }
        ds_dwBufferSize = actual.dwBufferBytes;
    }
    if (ds_dwBufferSize % format.nBlockAlign || ds_dwBufferSize / format.nBlockAlign < 4) {
        ReleaseDSound();
        return FALSE;
    }
    ds_dwLockSize =
        (std::max)(1ul, (std::min)(static_cast<DWORD>(static_cast<std::uint64_t>(ds_nSampleRate) *
                                                      ds_nLockLatency / 4000),
                                   ds_dwBufferSize / format.nBlockAlign / 4)) *
        format.nBlockAlign;
    try {
        mixed.resize(ds_dwLockSize / sizeof(std::int16_t));
    } catch (const std::exception&) {
        ReleaseDSound();
        return FALSE;
    }
    stop_event.reset(CreateEvent(nullptr, TRUE, FALSE, nullptr));
    poll_timer.reset(CreateWaitableTimerEx(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION,
                                           TIMER_ALL_ACCESS));
    if (!poll_timer)
        poll_timer.reset(CreateWaitableTimer(nullptr, FALSE, nullptr));
    if (!stop_event || !poll_timer || !StartSound()) {
        ReleaseDSound();
        return FALSE;
    }
    return TRUE;
}

namespace {
HRESULT write_buffer(DWORD bytes) {
    if (ds_nChannels != 1 && ds_nChannels != 2)
        return E_INVALIDARG;
    if (!bytes)
        bytes = ds_dwLockSize;
    const DWORD frame_bytes = ds_nChannels * sizeof(std::int16_t);
    if (!lpSoundBuffer || !ds_dwBufferSize || !ds_dwLockSize || bytes > ds_dwLockSize ||
        bytes > ds_dwBufferSize || bytes % frame_bytes || ds_dwBufferSize % frame_bytes)
        return E_INVALIDARG;
    // Allocate before locking device memory; an allocation failure never leaves it locked.
    try {
        mixed.resize(bytes / sizeof(std::int16_t));
    } catch (const std::exception&) {
        return E_OUTOFMEMORY;
    }
    BufferLock region(lpSoundBuffer.Get());
    HRESULT result = region.lock(next_write, bytes);
    if (result == DSERR_BUFFERLOST) {
        mark_lost(false);
        return result;
    }
    if (FAILED(result)) {
        return result;
    }
    if (!region.first || region.first_bytes % frame_bytes || region.second_bytes % frame_bytes ||
        region.first_bytes > bytes || region.second_bytes != bytes - region.first_bytes ||
        (region.second_bytes && !region.second))
        return E_INVALIDARG;
    ReadMixedAudio(mixed.data(), bytes / frame_bytes);
    rxpce_audio::write_pcm16(mixed.data(), static_cast<std::int16_t*>(region.first),
                             region.first_bytes / 2, static_cast<std::int16_t*>(region.second),
                             region.second_bytes / 2);
    result = region.unlock();
    if (FAILED(result))
        return result;
    next_write = (next_write + bytes) % ds_dwBufferSize;
    return S_OK;
}
} // namespace
BOOL WriteDSoundBuffer(DWORD bytes) {
    return SUCCEEDED(write_buffer(bytes));
}

BOOL StartSound() {
    if (!lpSoundBuffer || !stop_event || !poll_timer || playback_thread)
        return FALSE;
    ResetEvent(stop_event.get());
    next_write = 0;
    stream_cursor.reset(0);
    reported_lead = 0;
    recovery = {};
    if (FAILED(clear_buffer()) || !ServiceDSoundBuffer())
        return FALSE;
    if (primary_buffer && FAILED(primary_buffer->Play(0, 0, DSBPLAY_LOOPING)))
        return FALSE;
    if (FAILED(lpSoundBuffer->Play(0, 0, DSBPLAY_LOOPING)))
        return FALSE;
    const DWORD frame_bytes = ds_nChannels * sizeof(std::int16_t);
    const LONG interval =
        (std::max)(1l, static_cast<LONG>(ds_dwLockSize / frame_bytes * 1000 / ds_nSampleRate / 4));
    LARGE_INTEGER due;
    due.QuadPart = -static_cast<LONGLONG>(interval) * 10000;
    if (!SetWaitableTimer(poll_timer.get(), &due, interval, nullptr, nullptr, FALSE))
        return FALSE;
    LogDumpDetail("Audio: buffer=%lu bytes block=%lu bytes poll=%ld ms\n", ds_dwBufferSize,
                  ds_dwLockSize, interval);
    playback_thread.reset(
        reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, play_proc, nullptr, 0, nullptr)));
    if (!playback_thread)
        return FALSE;
    return TRUE;
}

BOOL StopSound() {
    if (stop_event)
        SetEvent(stop_event.get());
    if (poll_timer)
        CancelWaitableTimer(poll_timer.get());
    if (playback_thread) {
        WaitForSingleObject(playback_thread.get(), INFINITE);
        playback_thread.reset();
    }
    BOOL ok = TRUE;
    if (lpSoundBuffer && FAILED(lpSoundBuffer->Stop()))
        ok = FALSE;
    if (primary_buffer && FAILED(primary_buffer->Stop()))
        ok = FALSE;
    return ok;
}

BOOL ReleaseSoundBuffer() {
    const BOOL ok = StopSound();
    lpSoundBuffer.Reset();
    primary_buffer.Reset();
    mixed.clear();
    next_write = 0;
    stream_cursor.reset(0);
    recovery = {};
    return ok;
}

BOOL ReleaseDSound() {
    const BOOL ok = ReleaseSoundBuffer();
    lpDSound.Reset();
    stop_event.reset();
    poll_timer.reset();
    timer_resolution.reset();
    return ok;
}
#endif // SOUND
