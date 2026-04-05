#pragma once

#include <windows.h>
#include <d3d11.h>

class SocketServer;

class ServerMonitorUI
{
public:
    ServerMonitorUI() = default;
    ~ServerMonitorUI();

    bool Initialize(HWND windowHandle, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
    void Shutdown();
    void BeginFrame();
    void Render(const SocketServer& server);
    void EndFrame();
    bool HandleWndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) const;

private:
    bool initialized_ = false;
};
