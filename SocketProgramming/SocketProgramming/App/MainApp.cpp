#include "App/MainApp.h"

#include <chrono>
#include <filesystem>
#include <thread>

namespace
{
    constexpr wchar_t kWindowClassName[] = L"SocketProgrammingImGuiWindow";
    constexpr int kServerStartWaitIterations = 100;
    constexpr auto kServerStartWaitInterval = std::chrono::milliseconds(20);
    constexpr size_t kAutoLaunchClientCount = 2;
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
    // 앱 인스턴스 핸들을 저장한 뒤, 창 등록 -> 창 생성 -> DX11 초기화 -> ImGui UI 초기화 순서로 진행한다.
    // 여기까지는 네트워크 처리가 아니라 "서버 모니터 창을 띄우기 위한 준비 단계"다.
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

    // 실제 TCP 서버 루프는 UI 스레드가 아니라 별도의 워커 스레드에서 돈다.
    server_.Start();

    // Start()는 스레드만 시작하므로, 바로 클라이언트를 띄우면 아직 listen 상태가 아닐 수 있다.
    // 그래서 listening=true가 될 때까지 잠깐 폴링하면서 기다린다.
    for (int iteration = 0; iteration < kServerStartWaitIterations; ++iteration)
    {
        if (server_.GetSnapshot().listening)
        {
            break;
        }

        std::this_thread::sleep_for(kServerStartWaitInterval);
    }

    // 여기서 ChatClient 소스의 main/WinMain을 직접 호출하는 것이 아니라,
    // 이미 빌드되어 있는 ChatClient.exe를 별도 프로세스로 실행한다.
    LaunchChatClient();
    return true;
}

void MainApp::Shutdown()
{
    CloseChatClientHandles();
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

bool MainApp::LaunchChatClient()
{
    // 이미 실행한 클라이언트 프로세스 핸들이 남아 있으면 중복 실행하지 않는다.
    for (const PROCESS_INFORMATION& processInfo : chatClientProcessInfos_)
    {
        if (processInfo.hProcess || processInfo.hThread)
        {
            return true;
        }
    }

    wchar_t modulePath[MAX_PATH] = {};
    const DWORD modulePathLength = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
    if (modulePathLength == 0 || modulePathLength == MAX_PATH)
    {
        return false;
    }

    const std::filesystem::path executableDirectory = std::filesystem::path(modulePath).parent_path();
    // 서버 exe와 같은 출력 폴더에 빌드된 ChatClient.exe를 찾아서 실행한다.
    const std::filesystem::path chatClientPath = executableDirectory / L"ChatClient.exe";
    if (!std::filesystem::exists(chatClientPath))
    {
        return false;
    }

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof(startupInfo);

    for (size_t index = 0; index < kAutoLaunchClientCount; ++index)
    {
        // 각 클라이언트는 별도 프로세스로 뜨고, 구분을 위해 client index 인자를 함께 넘긴다.
        std::wstring commandLine = L"\"" + chatClientPath.wstring() + L"\" --client-index=" + std::to_wstring(index + 1);
        PROCESS_INFORMATION processInfo = {};
        if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, FALSE, 0, nullptr, executableDirectory.c_str(), &startupInfo, &processInfo))
        {
            CloseChatClientHandles();
            return false;
        }

        chatClientProcessInfos_[index] = processInfo;
    }

    return true;
}

void MainApp::CloseChatClientHandles()
{
    // 여기서는 클라이언트 프로그램 자체를 강제 종료하는 것이 아니라,
    // 서버가 들고 있던 프로세스/스레드 핸들만 정리한다.
    for (PROCESS_INFORMATION& processInfo : chatClientProcessInfos_)
    {
        if (processInfo.hThread)
        {
            CloseHandle(processInfo.hThread);
            processInfo.hThread = nullptr;
        }

        if (processInfo.hProcess)
        {
            CloseHandle(processInfo.hProcess);
            processInfo.hProcess = nullptr;
        }

        processInfo.dwProcessId = 0;
        processInfo.dwThreadId = 0;
    }
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
