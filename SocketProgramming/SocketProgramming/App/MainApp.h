#pragma once

#include <windows.h>

#include "Renderer/D3D11Context.h"
#include "Server/SocketServer.h"
#include "UI/ServerMonitorUI.h"

class MainApp
{
public:
    MainApp() = default;
    ~MainApp() = default;

    int Run(HINSTANCE instanceHandle, int showCommand);

private:
    bool Initialize(HINSTANCE instanceHandle, int showCommand);
    void Shutdown();
    bool RegisterWindowClass(HINSTANCE instanceHandle);
    bool CreateMainWindow(HINSTANCE instanceHandle, int showCommand);
    int RunMainLoop();

    static LRESULT CALLBACK WndProcStatic(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT WndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

private:
    HWND windowHandle_ = nullptr;
    HINSTANCE instanceHandle_ = nullptr;
    SocketServer server_;
    D3D11Context d3d11Context_;
    ServerMonitorUI serverMonitorUI_;
};
