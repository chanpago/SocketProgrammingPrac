#include "App/MainApp.h"

namespace
{
    constexpr wchar_t kWindowClassName[] = L"SocketProgrammingImGuiWindow";
}

int MainApp::Run(HINSTANCE instanceHandle, int showCommand)
{
    if (!Initialize(instanceHandle, showCommand))
    {
        Shutdown();
        return 1;
    }

    const int exitCode = RunMainLoop();
    Shutdown();
    return exitCode;
}

bool MainApp::Initialize(HINSTANCE instanceHandle, int showCommand)
{
    instanceHandle_ = instanceHandle;

    if (!RegisterWindowClass(instanceHandle))
    {
        return false;
    }

    if (!CreateMainWindow(instanceHandle, showCommand))
    {
        return false;
    }

    if (!d3d11Context_.Initialize(windowHandle_))
    {
        return false;
    }

    if (!serverMonitorUI_.Initialize(windowHandle_, d3d11Context_.GetDevice(), d3d11Context_.GetDeviceContext()))
    {
        return false;
    }

    server_.Start();
    return true;
}

void MainApp::Shutdown()
{
    server_.Stop();
    serverMonitorUI_.Shutdown();
    d3d11Context_.Shutdown();

    if (windowHandle_)
    {
        DestroyWindow(windowHandle_);
        windowHandle_ = nullptr;
    }

    if (instanceHandle_)
    {
        UnregisterClassW(kWindowClassName, instanceHandle_);
        instanceHandle_ = nullptr;
    }
}

bool MainApp::RegisterWindowClass(HINSTANCE instanceHandle)
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WndProcStatic;
    windowClass.hInstance = instanceHandle;
    windowClass.lpszClassName = kWindowClassName;
    return RegisterClassExW(&windowClass) != 0;
}

bool MainApp::CreateMainWindow(HINSTANCE instanceHandle, int showCommand)
{
    windowHandle_ = CreateWindowW(
        kWindowClassName,
        L"SocketProgramming - ImGui DX11",
        WS_OVERLAPPEDWINDOW,
        100,
        100,
        1280,
        720,
        nullptr,
        nullptr,
        instanceHandle,
        this);

    if (!windowHandle_)
    {
        return false;
    }

    ShowWindow(windowHandle_, showCommand);
    UpdateWindow(windowHandle_);
    return true;
}

int MainApp::RunMainLoop()
{
    MSG message = {};
    bool done = false;

    while (!done)
    {
        while (PeekMessage(&message, nullptr, 0U, 0U, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
            if (message.message == WM_QUIT)
            {
                done = true;
            }
        }

        if (done)
        {
            break;
        }

        serverMonitorUI_.BeginFrame();
        serverMonitorUI_.Render(server_);

        const float clearColor[4] = { 0.10f, 0.10f, 0.12f, 1.00f };
        d3d11Context_.BeginFrame(clearColor);
        serverMonitorUI_.EndFrame();
        d3d11Context_.EndFrame();
    }

    return 0;
}

LRESULT CALLBACK MainApp::WndProcStatic(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    MainApp* app = nullptr;

    if (message == WM_NCCREATE)
    {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<MainApp*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    else
    {
        app = reinterpret_cast<MainApp*>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA));
    }

    if (app)
    {
        return app->WndProc(windowHandle, message, wParam, lParam);
    }

    return DefWindowProcW(windowHandle, message, wParam, lParam);
}

LRESULT MainApp::WndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (serverMonitorUI_.HandleWndProc(windowHandle, message, wParam, lParam))
    {
        return true;
    }

    switch (message)
    {
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED)
        {
            d3d11Context_.Resize(static_cast<UINT>(LOWORD(lParam)), static_cast<UINT>(HIWORD(lParam)));
        }
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
        {
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(windowHandle, message, wParam, lParam);
}
