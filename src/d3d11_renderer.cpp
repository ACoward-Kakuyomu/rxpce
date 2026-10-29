#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "d3d11_renderer.h"
#include "video_frame.h"
#include "presentation_state.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>
#include <wrl/client.h>
#include <cstring>

namespace rxpce_video {
using Microsoft::WRL::ComPtr;
struct Renderer::Impl {
    HWND window = nullptr;
    UINT width = 0, height = 0, source_width = 0, source_height = 0;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    ComPtr<IDXGISwapChain> swap;
    ComPtr<ID3D11RenderTargetView> target;
    ComPtr<ID3D11Texture2D> texture;
    ComPtr<ID3D11ShaderResourceView> view;
    ComPtr<ID3D11VertexShader> vertex;
    ComPtr<ID3D11PixelShader> pixel;
    ComPtr<ID3D11SamplerState> sampler;
    HANDLE frame_ready = nullptr;
    PresentationState presentation;
    ~Impl() {
        if (frame_ready)
            CloseHandle(frame_ready);
    }

    HRESULT submit() {
        const auto status = swap->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
        if (status == DXGI_ERROR_WAS_STILL_DRAWING) {
            presentation.submitted(PresentResult::busy);
            return S_FALSE;
        }
        if (SUCCEEDED(status))
            presentation.submitted(status == DXGI_STATUS_OCCLUDED ? PresentResult::occluded
                                                                  : PresentResult::presented);
        return status;
    }

    HRESULT resize(UINT w, UINT h) {
        if (width == w && height == h && target)
            return S_OK;
        context->OMSetRenderTargets(0, nullptr, nullptr);
        target.Reset();
        HRESULT hr = swap->ResizeBuffers(0, w, h, DXGI_FORMAT_UNKNOWN,
                                         DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
        if (FAILED(hr))
            return hr;
        ComPtr<ID3D11Texture2D> buffer;
        hr = swap->GetBuffer(0, IID_PPV_ARGS(&buffer));
        if (FAILED(hr))
            return hr;
        hr = device->CreateRenderTargetView(buffer.Get(), nullptr, &target);
        if (SUCCEEDED(hr)) {
            width = w;
            height = h;
        }
        return hr;
    }
};
Renderer::Renderer() = default;
Renderer::~Renderer() = default;
void Renderer::shutdown() {
    impl.reset();
}

HRESULT Renderer::initialize(HWND window, int adapter_index) {
    shutdown();
    auto next = std::make_unique<Impl>();
    next->window = window;
    ComPtr<IDXGIFactory1> factory;
    HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr))
        return hr;
    ComPtr<IDXGIAdapter1> adapter;
    if (adapter_index >= 0) {
        hr = factory->EnumAdapters1(adapter_index, &adapter);
        if (FAILED(hr))
            return hr;
    } else if (adapter_index != -1 && adapter_index != -2)
        return E_INVALIDARG;
    DXGI_SWAP_CHAIN_DESC desc = {};
    desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.OutputWindow = window;
    desc.Windowed = TRUE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
    const D3D_FEATURE_LEVEL feature = D3D_FEATURE_LEVEL_11_0;
    auto create = [&](D3D_DRIVER_TYPE type) {
        return D3D11CreateDeviceAndSwapChain(
            adapter.Get(), type, nullptr, 0, &feature, 1, D3D11_SDK_VERSION, &desc,
            next->swap.ReleaseAndGetAddressOf(), next->device.ReleaseAndGetAddressOf(), nullptr,
            next->context.ReleaseAndGetAddressOf());
    };
    hr = create(adapter               ? D3D_DRIVER_TYPE_UNKNOWN
                : adapter_index == -2 ? D3D_DRIVER_TYPE_WARP
                                      : D3D_DRIVER_TYPE_HARDWARE);
    if (FAILED(hr) && adapter_index == -1)
        hr = create(D3D_DRIVER_TYPE_WARP);
    if (FAILED(hr))
        return hr;
    ComPtr<IDXGISwapChain2> swap2;
    hr = next->swap.As(&swap2);
    if (FAILED(hr))
        return hr;
    hr = swap2->SetMaximumFrameLatency(1);
    if (FAILED(hr))
        return hr;
    next->frame_ready = swap2->GetFrameLatencyWaitableObject();
    if (!next->frame_ready)
        return E_FAIL;
    // Use the device's factory for window association, including the WARP path.
    ComPtr<IDXGIDevice> dxgi_device;
    ComPtr<IDXGIAdapter> actual_adapter;
    ComPtr<IDXGIFactory> actual_factory;
    hr = next->device.As(&dxgi_device);
    if (FAILED(hr))
        return hr;
    hr = dxgi_device->GetAdapter(&actual_adapter);
    if (FAILED(hr))
        return hr;
    hr = actual_adapter->GetParent(IID_PPV_ARGS(&actual_factory));
    if (FAILED(hr))
        return hr;
    hr = actual_factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
    if (FAILED(hr))
        return hr;

    const char shader[] = "struct V { float4 p:SV_POSITION; float2 uv:TEXCOORD0; };"
                          "V vs(uint id:SV_VertexID) { V o; o.uv=float2((id<<1)&2,id&2);"
                          "o.p=float4(o.uv*float2(2,-2)+float2(-1,1),0,1); return o; }"
                          "Texture2D tex:register(t0); SamplerState samp:register(s0);"
                          "float4 ps(V i):SV_TARGET { return tex.Sample(samp,i.uv); }";
    ComPtr<ID3DBlob> code;
    hr = D3DCompile(shader, sizeof(shader) - 1, nullptr, nullptr, nullptr, "vs", "vs_4_0",
                    D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, nullptr);
    if (FAILED(hr))
        return hr;
    hr = next->device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
                                          &next->vertex);
    if (FAILED(hr))
        return hr;
    code.Reset();
    hr = D3DCompile(shader, sizeof(shader) - 1, nullptr, nullptr, nullptr, "ps", "ps_4_0",
                    D3DCOMPILE_ENABLE_STRICTNESS, 0, &code, nullptr);
    if (FAILED(hr))
        return hr;
    hr = next->device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr,
                                         &next->pixel);
    if (FAILED(hr))
        return hr;
    D3D11_SAMPLER_DESC sampling = {};
    sampling.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampling.AddressU = sampling.AddressV = sampling.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampling.MaxLOD = D3D11_FLOAT32_MAX;
    hr = next->device->CreateSamplerState(&sampling, &next->sampler);
    if (FAILED(hr))
        return hr;
    impl = std::move(next);
    return S_OK;
}

HRESULT Renderer::render(const std::uint32_t* rgba, int width, int height, int scale, bool present,
                         bool nonblocking, AspectMode aspect_mode) {
    if (!impl || !rgba || width <= 0 || height <= 0 || width > 512 || height > 256 || scale < 0 ||
        scale > 4 || !valid_aspect_mode(aspect_mode))
        return E_INVALIDARG;
    auto& d = *impl;
    RECT client;
    if (!GetClientRect(d.window, &client))
        return HRESULT_FROM_WIN32(GetLastError());
    if (IsIconic(d.window) || client.right <= 0 || client.bottom <= 0)
        return S_FALSE;
    const auto r =
        display_placement(width, height, client.right, client.bottom, scale, aspect_mode);
    if (r.width == 0 || r.height == 0)
        return S_FALSE;
    if (present && d.presentation.next() == PresentAction::visibility) {
        const auto status = d.swap->Present(0, DXGI_PRESENT_TEST);
        if (status != S_OK)
            return status;
        d.presentation.visible(true);
    }
    if (present && d.presentation.next() == PresentAction::retry)
        return d.submit();
    if (present && d.presentation.next() == PresentAction::acquire) {
        const auto ready = WaitForSingleObject(d.frame_ready, nonblocking ? 0 : 1);
        if (ready == WAIT_FAILED)
            return HRESULT_FROM_WIN32(GetLastError());
        if (!d.presentation.acquire(ready == WAIT_OBJECT_0))
            return S_FALSE;
    }
    HRESULT hr = d.resize(client.right, client.bottom);
    if (FAILED(hr))
        return hr;
    if (d.source_width != static_cast<unsigned>(width) ||
        d.source_height != static_cast<unsigned>(height) || !d.texture) {
        ID3D11ShaderResourceView* empty = nullptr;
        d.context->PSSetShaderResources(0, 1, &empty);
        d.view.Reset();
        d.texture.Reset();
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = d.device->CreateTexture2D(&desc, nullptr, &d.texture);
        if (FAILED(hr))
            return hr;
        hr = d.device->CreateShaderResourceView(d.texture.Get(), nullptr, &d.view);
        if (FAILED(hr))
            return hr;
        d.source_width = width;
        d.source_height = height;
    }
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = d.context->Map(d.texture.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr))
        return hr;
    for (int row = 0; row < height; ++row)
        std::memcpy(static_cast<unsigned char*>(mapped.pData) + row * mapped.RowPitch,
                    rgba + row * width, width * 4);
    d.context->Unmap(d.texture.Get(), 0);
    ID3D11RenderTargetView* target = d.target.Get();
    d.context->OMSetRenderTargets(1, &target, nullptr);
    const float black[] = {0, 0, 0, 1};
    d.context->ClearRenderTargetView(target, black);
    const D3D11_VIEWPORT viewport = {static_cast<float>(r.x),
                                     static_cast<float>(r.y),
                                     static_cast<float>(r.width),
                                     static_cast<float>(r.height),
                                     0,
                                     1};
    d.context->RSSetViewports(1, &viewport);
    d.context->IASetInputLayout(nullptr);
    d.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    d.context->VSSetShader(d.vertex.Get(), nullptr, 0);
    d.context->PSSetShader(d.pixel.Get(), nullptr, 0);
    ID3D11ShaderResourceView* view = d.view.Get();
    ID3D11SamplerState* sampler = d.sampler.Get();
    d.context->PSSetShaderResources(0, 1, &view);
    d.context->PSSetSamplers(0, 1, &sampler);
    d.context->Draw(3, 0);
    if (!present)
        return S_OK;
    return d.submit();
}

HRESULT Renderer::readback(std::vector<std::uint32_t>& rgba) {
    if (!impl || !impl->target)
        return E_UNEXPECTED;
    auto& d = *impl;
    ComPtr<ID3D11Texture2D> buffer, staging;
    HRESULT hr = d.swap->GetBuffer(0, IID_PPV_ARGS(&buffer));
    if (FAILED(hr))
        return hr;
    D3D11_TEXTURE2D_DESC desc;
    buffer->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = desc.MiscFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    hr = d.device->CreateTexture2D(&desc, nullptr, &staging);
    if (FAILED(hr))
        return hr;
    d.context->CopyResource(staging.Get(), buffer.Get());
    rgba.resize(static_cast<std::size_t>(d.width) * d.height);
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = d.context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
        return hr;
    for (UINT row = 0; row < d.height; ++row)
        std::memcpy(rgba.data() + row * d.width,
                    static_cast<unsigned char*>(mapped.pData) + row * mapped.RowPitch, d.width * 4);
    d.context->Unmap(staging.Get(), 0);
    return S_OK;
}
} // namespace rxpce_video
