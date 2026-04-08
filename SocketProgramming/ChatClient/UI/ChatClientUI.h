#pragma once

#include <windows.h>
#include <d3d11.h>

#include <array>

class ChatClientConnection;

class ChatClientUI
{
public:
    ChatClientUI() = default;
    ~ChatClientUI();

    bool Initialize(HWND windowHandle, ID3D11Device* device, ID3D11DeviceContext* deviceContext);
    void Shutdown();
    void BeginFrame();
    void Render(ChatClientConnection& connection, int clientIndex);
    void EndFrame();
    bool HandleWndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) const;

private:
    bool initialized_ = false;
    std::array<char, 512> inputBuffer_ = {};
};
