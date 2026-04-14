#include "App/ClientApp.h"

namespace
{
    constexpr wchar_t kWindowClassName[] = L"SocketProgrammingChatClientWindow";
}

ClientApp::ClientApp(int clientIndex)
    : clientIndex_(clientIndex)
{
}

int ClientApp::Run(HINSTANCE instanceHandle, int showCommand)
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

bool ClientApp::Initialize(HINSTANCE instanceHandle, int showCommand)
{
    // 이 클라이언트는 서버가 실행해 준 별도 exe 프로세스지만,
    // 실행 후에는 자기 프로세스 안에서 창/DX11/UI를 초기화하고 스스로 서버에 TCP 연결한다.
    instanceHandle_ = instanceHandle;

    if (!RegisterWindowClass(instanceHandle))
    {
        return false;
    }

    if (!CreateMainWindow(instanceHandle, showCommand))
    {
        return false;
    }

    UpdateWindowTitle();

    if (!d3d11Context_.Initialize(windowHandle_))
    {
        return false;
    }

    if (!chatClientUI_.Initialize(windowHandle_, d3d11Context_.GetDevice(), d3d11Context_.GetDeviceContext()))
    {
        return false;
    }

    if (!circleRenderer_.Initialize(d3d11Context_.GetDevice()))
    {
        return false;
    }

    connection_.Connect();
    return true;
}

void ClientApp::Shutdown()
{
    connection_.Disconnect();
    circleRenderer_.Shutdown();
    chatClientUI_.Shutdown();
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

bool ClientApp::RegisterWindowClass(HINSTANCE instanceHandle)
{
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_CLASSDC;
    windowClass.lpfnWndProc = WndProcStatic;
    windowClass.hInstance = instanceHandle;
    windowClass.lpszClassName = kWindowClassName;
    return RegisterClassExW(&windowClass) != 0;
}

bool ClientApp::CreateMainWindow(HINSTANCE instanceHandle, int showCommand)
{
    windowHandle_ = CreateWindowW(
        kWindowClassName,
        GetWindowTitle().c_str(),
        WS_OVERLAPPEDWINDOW,
        140,
        140,
        1280,
        600,
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

void ClientApp::UpdateWindowTitle() const
{
    if (windowHandle_)
    {
        SetWindowTextW(windowHandle_, GetWindowTitle().c_str());
    }
}

std::wstring ClientApp::GetWindowTitle() const
{
    // clientIndex는 네트워크 식별자가 아니라, 자동 실행된 여러 GUI 창을 눈으로 구분하기 위한 값이다.
    if (clientIndex_ > 0)
    {
        return L"Chat Client " + std::to_wstring(clientIndex_) + L" - ImGui DX11";
    }

    return L"Chat Client - ImGui DX11";
}

int ClientApp::RunMainLoop()
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

        chatClientUI_.BeginFrame();
        chatClientUI_.Render(connection_, clientIndex_);

        const float clearColor[4] = { 0.08f, 0.08f, 0.10f, 1.00f };
        d3d11Context_.BeginFrame(clearColor);
        circleRenderer_.Render(d3d11Context_.GetDeviceContext(), d3d11Context_.GetViewportWidth(), d3d11Context_.GetViewportHeight());
        chatClientUI_.EndFrame();
        d3d11Context_.EndFrame();
    }

    return 0;
}

LRESULT CALLBACK ClientApp::WndProcStatic(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    ClientApp* app = nullptr;

    if (message == WM_NCCREATE)
    {
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        app = static_cast<ClientApp*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
    }
    else
    {
        app = reinterpret_cast<ClientApp*>(GetWindowLongPtrW(windowHandle, GWLP_USERDATA));
    }

    if (app)
    {
        return app->WndProc(windowHandle, message, wParam, lParam);
    }

    return DefWindowProcW(windowHandle, message, wParam, lParam);
}

LRESULT ClientApp::WndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
{
    if (chatClientUI_.HandleWndProc(windowHandle, message, wParam, lParam))
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
