#pragma once

#include <windows.h>
#include <d3d11.h>

class D3D11Context
{
public:
    D3D11Context() = default;
    ~D3D11Context();

    bool Initialize(HWND windowHandle);
    void Shutdown();
    void Resize(UINT width, UINT height);
    void BeginFrame(const float clearColor[4]);
    void EndFrame();

    ID3D11Device* GetDevice() const { return device_; }
    ID3D11DeviceContext* GetDeviceContext() const { return deviceContext_; }

private:
    void CreateRenderTarget();
    void CleanupRenderTarget();

private:
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* deviceContext_ = nullptr;
    IDXGISwapChain* swapChain_ = nullptr;
    ID3D11RenderTargetView* mainRenderTargetView_ = nullptr;
};
