#include "win32_support.h"
#include "wasapi.h"
#include "wasapi_stream.h"
#include "Sound.h"
#include "log.h"
#include "unique_handle.h"
#include <mmdeviceapi.h>
#include <avrt.h>
#include <wrl/client.h>
#include <process.h>
#include <atomic>
namespace {
using Microsoft::WRL::ComPtr;
rxpce_host::UniqueHandle stop_event, ready_event, playback_thread;
std::atomic<HRESULT> initial_status{E_PENDING};
unsigned sample_rate = 0, channel_count = 0, latency = 0;
class ThreadApartment {
  public:
    ThreadApartment() : status_(CoInitializeEx(nullptr, COINIT_MULTITHREADED)) {}
    ThreadApartment(const ThreadApartment&) = delete;
    ThreadApartment& operator=(const ThreadApartment&) = delete;
    ~ThreadApartment() {
        if (SUCCEEDED(status_))
            CoUninitialize();
    }
    HRESULT status() const {
        return status_;
    }

  private:
    HRESULT status_;
};
class AudioTask {
  public:
    AudioTask() {
        DWORD index = 0;
        value_ = AvSetMmThreadCharacteristicsW(L"Audio", &index);
    }
    AudioTask(const AudioTask&) = delete;
    AudioTask& operator=(const AudioTask&) = delete;
    ~AudioTask() {
        if (value_)
            AvRevertMmThreadCharacteristics(value_);
    }

  private:
    HANDLE value_ = nullptr;
};
struct Session {
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IAudioClient> client;
    ComPtr<IAudioRenderClient> render;
    rxpce_host::UniqueHandle event;
    UINT32 capacity = 0, target = 0;
    bool started = false;
    ~Session() {
        if (started)
            client->Stop();
        render.Reset();
        client.Reset();
        device.Reset();
        enumerator.Reset();
    }
    HRESULT service() {
        return rxpce_audio::service_wasapi(*client.Get(), *render.Get(), capacity, target,
                                           channel_count, ReadMixedAudio);
    }
    HRESULT start() {
        auto status = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                       IID_PPV_ARGS(&enumerator));
        if (FAILED(status))
            return status;
        status = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        if (FAILED(status))
            return status;
        status = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                                  reinterpret_cast<void**>(client.GetAddressOf()));
        if (FAILED(status))
            return status;
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
        format.nChannels = static_cast<WORD>(channel_count);
        format.nSamplesPerSec = sample_rate;
        format.wBitsPerSample = 32;
        format.nBlockAlign = static_cast<WORD>(channel_count * sizeof(float));
        format.nAvgBytesPerSec = sample_rate * format.nBlockAlign;
        status = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                    AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                                        AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                                        AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
                                    REFERENCE_TIME(latency) * 10000, 0, &format, nullptr);
        if (FAILED(status))
            return status;
        event.reset(CreateEvent(nullptr, FALSE, FALSE, nullptr));
        if (!event)
            return HRESULT_FROM_WIN32(GetLastError());
        status = client->SetEventHandle(event.get());
        if (FAILED(status))
            return status;
        status = client->GetBufferSize(&capacity);
        if (FAILED(status))
            return status;
        target =
            (std::min)(capacity, (std::max)(1u, static_cast<UINT32>(std::uint64_t(sample_rate) *
                                                                    latency / 1000)));
        status = client->GetService(IID_PPV_ARGS(&render));
        if (FAILED(status))
            return status;
        status = service();
        if (FAILED(status))
            return status;
        status = client->Start();
        if (FAILED(status))
            return status;
        started = true;
        LogDumpDetail(
            "WASAPI started: rate=%u channels=%u latency=%u ms buffer=%u target=%u frames\n",
            sample_rate, channel_count, latency, capacity, target);
        return S_OK;
    }
    HRESULT run() {
        const HANDLE events[] = {stop_event.get(), event.get()};
        for (;;) {
            const auto wake = WaitForMultipleObjects(2, events, FALSE, 1000);
            if (wake == WAIT_OBJECT_0)
                return S_OK;
            if (wake != WAIT_OBJECT_0 + 1 && wake != WAIT_TIMEOUT)
                return HRESULT_FROM_WIN32(GetLastError());
            const auto status = service();
            if (FAILED(status))
                return status;
        }
    }
};
unsigned __stdcall play(void*) {
    ThreadApartment apartment;
    const auto com = apartment.status();
    if (FAILED(com)) {
        LogDump("WASAPI COM initialization failed: %08lx\n", com);
        initial_status = com;
        SetEvent(ready_event.get());
        return 1;
    }
    AudioTask task;
    bool first = true;
    try {
        for (;;) {
            HRESULT status;
            {
                Session session;
                status = session.start();
                if (first) {
                    initial_status = status;
                    SetEvent(ready_event.get());
                }
                if (SUCCEEDED(status))
                    status = session.run();
            }
            if (FAILED(status))
                LogDump("WASAPI error: %08lx\n", status);
            if (first && FAILED(initial_status.load()))
                break;
            first = false;
            if (SUCCEEDED(status) || WaitForSingleObject(stop_event.get(), 1000) == WAIT_OBJECT_0)
                break;
        }
    } catch (...) {
        if (first && initial_status == E_PENDING) {
            initial_status = E_FAIL;
            SetEvent(ready_event.get());
        }
        LogDump("WASAPI playback exception\n");
    }
    return 0;
}
} // namespace
BOOL InitWasapi(unsigned rate, unsigned channels, unsigned latency_ms) {
    ReleaseWasapi();
    if (rate < 8000 || rate > 48000 || (channels != 1 && channels != 2) || latency_ms < 20 ||
        latency_ms > 160)
        return FALSE;
    sample_rate = rate;
    channel_count = channels;
    latency = latency_ms;
    initial_status = E_PENDING;
    stop_event.reset(CreateEvent(nullptr, TRUE, FALSE, nullptr));
    ready_event.reset(CreateEvent(nullptr, TRUE, FALSE, nullptr));
    if (!stop_event || !ready_event) {
        ReleaseWasapi();
        return FALSE;
    }
    playback_thread.reset(
        reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, play, nullptr, 0, nullptr)));
    if (!playback_thread) {
        ReleaseWasapi();
        return FALSE;
    }
    if (WaitForSingleObject(ready_event.get(), 5000) != WAIT_OBJECT_0 ||
        FAILED(initial_status.load())) {
        ReleaseWasapi();
        return FALSE;
    }
    return TRUE;
}
void ReleaseWasapi() {
    if (stop_event)
        SetEvent(stop_event.get());
    if (playback_thread) {
        WaitForSingleObject(playback_thread.get(), INFINITE);
        playback_thread.reset();
    }
    ready_event.reset();
    stop_event.reset();
}
