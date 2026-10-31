#pragma once
#include <windows.h>
#include <audioclient.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace rxpce_audio {
template <class Client, class Render, class Available, class Read, class Observe>
HRESULT service_wasapi(Client& client, Render& render, UINT32 capacity, UINT32 target,
                       unsigned channels, Available available, Read read, Observe observe) {
    if (!capacity || !target || target > capacity || (channels != 1 && channels != 2))
        return E_INVALIDARG;
    UINT32 padding = 0;
    auto status = client.GetCurrentPadding(&padding);
    if (FAILED(status))
        return status;
    if (padding > capacity)
        return E_INVALIDARG;
    if (padding >= target)
        return S_OK;
    const UINT32 needed = target - padding;
    UINT32 frames = needed;
    try {
        if (padding)
            frames = static_cast<UINT32>((std::min)(std::size_t(needed), available()));
        observe(padding == 0, frames < needed);
    } catch (...) {
        return E_FAIL;
    }
    if (!frames)
        return S_OK;
    BYTE* bytes = nullptr;
    status = render.GetBuffer(frames, &bytes);
    if (FAILED(status))
        return status;
    if (!bytes) {
        render.ReleaseBuffer(frames, AUDCLNT_BUFFERFLAGS_SILENT);
        return E_POINTER;
    }
    try {
        auto samples = reinterpret_cast<float*>(bytes);
        read(samples, frames);
        for (std::size_t n = 0; n < std::size_t(frames) * channels; ++n)
            samples[n] =
                std::isfinite(samples[n]) ? (std::max)(-1.0f, (std::min)(1.0f, samples[n])) : 0.0f;
    } catch (...) {
        render.ReleaseBuffer(frames, AUDCLNT_BUFFERFLAGS_SILENT);
        return E_FAIL;
    }
    return render.ReleaseBuffer(frames, 0);
}
} // namespace rxpce_audio
