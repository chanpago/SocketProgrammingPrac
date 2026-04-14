#pragma once

#include <windows.h>

#include "Network/ChatClientConnection.h"
#include "Renderer/CircleRenderer.h"
#include "Renderer/D3D11Context.h"
#include "UI/ChatClientUI.h"

class ClientApp
{
public:
    explicit ClientApp(int clientIndex = 0);
    ~ClientApp() = default;

    int Run(HINSTANCE instanceHandle, int showCommand);

private:
    bool Initialize(HINSTANCE instanceHandle, int showCommand);
    void UpdateWindowTitle() const;
    std::wstring GetWindowTitle() const;
    void Shutdown();
    bool RegisterWindowClass(HINSTANCE instanceHandle);
    bool CreateMainWindow(HINSTANCE instanceHandle, int showCommand);
    int RunMainLoop();

    static LRESULT CALLBACK WndProcStatic(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

private:
    HWND windowHandle_ = nullptr;
    HINSTANCE instanceHandle_ = nullptr;
    int clientIndex_ = 0;
    ChatClientConnection connection_;
    D3D11Context d3d11Context_;
    CircleRenderer circleRenderer_;
    ChatClientUI chatClientUI_;
};
