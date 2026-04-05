#include "Renderer/D3D11Context.h"

#include <array>

D3D11Context::~D3D11Context()
{
    Shutdown();
}

bool D3D11Context::Initialize(HWND windowHandle)
{
    DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
    swapChainDesc.BufferCount = 2;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = windowHandle;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    const std::array<D3D_FEATURE_LEVEL, 2> featureLevels =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_0,
    };

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    const HRESULT result = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        createDeviceFlags,
        featureLevels.data(),
        static_cast<UINT>(featureLevels.size()),
        D3D11_SDK_VERSION,
        &swapChainDesc,
        &swapChain_,
        &device_,
        &featureLevel,
        &deviceContext_);

    if (FAILED(result))
    {
        return false;
    }

    CreateRenderTarget();
    return true;
}

void D3D11Context::Shutdown()
{
    CleanupRenderTarget();

    if (swapChain_)
    {
        swapChain_->Release();
        swapChain_ = nullptr;
    }
    if (deviceContext_)
    {
        deviceContext_->Release();
        deviceContext_ = nullptr;
    }
    if (device_)
    {
        device_->Release();
        device_ = nullptr;
    }
}

void D3D11Context::Resize(UINT width, UINT height)
{
    if (!swapChain_ || width == 0 || height == 0)
    {
        return;
    }

    CleanupRenderTarget();
    swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    CreateRenderTarget();
}

void D3D11Context::BeginFrame(const float clearColor[4])
{
    deviceContext_->OMSetRenderTargets(1, &mainRenderTargetView_, nullptr);
    deviceContext_->ClearRenderTargetView(mainRenderTargetView_, clearColor);
}

void D3D11Context::EndFrame()
{
    swapChain_->Present(1, 0);
}

void D3D11Context::CreateRenderTarget()
{
    ID3D11Texture2D* backBuffer = nullptr;
    swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (backBuffer)
    {
        device_->CreateRenderTargetView(backBuffer, nullptr, &mainRenderTargetView_);
        backBuffer->Release();
    }
}

void D3D11Context::CleanupRenderTarget()
{
    if (mainRenderTargetView_)
    {
        mainRenderTargetView_->Release();
        mainRenderTargetView_ = nullptr;
    }
}
