#include "UI/ServerMonitorUI.h"

#include "Server/SocketServer.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

ServerMonitorUI::~ServerMonitorUI()
{
    Shutdown();
}

bool ServerMonitorUI::Initialize(HWND windowHandle, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
{
    if (initialized_)
    {
        return true;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    if (!ImGui_ImplWin32_Init(windowHandle))
    {
        return false;
    }

    if (!ImGui_ImplDX11_Init(device, deviceContext))
    {
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    initialized_ = true;
    return true;
}

void ServerMonitorUI::Shutdown()
{
    if (!initialized_)
    {
        return;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    initialized_ = false;
}

void ServerMonitorUI::BeginFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ServerMonitorUI::Render(const SocketServer& server)
{
    const ServerSnapshot snapshot = server.GetSnapshot();

    ImGui::Begin("Socket Server Monitor");
    ImGui::Text("Renderer: DirectX 11");
    ImGui::Text("ImGui backend: Win32 + DX11");
    ImGui::Separator();
    ImGui::Text("Status: %s", snapshot.status.c_str());
    ImGui::Text("Listening: %s", snapshot.listening ? "Yes" : "No");
    ImGui::Text("Initialized: %s", snapshot.initialized ? "Yes" : "No");
    ImGui::Text("Total Connections: %d", snapshot.totalConnections);
    ImGui::Text("Last Socket Error: %d", snapshot.lastSocketError);
    ImGui::TextWrapped("Open http://127.0.0.1:9000/ in a browser to test the server.");

    if (ImGui::Button("Clear Logs"))
    {
        const_cast<SocketServer&>(server).ClearLogs();
    }

    ImGui::Separator();
    ImGui::BeginChild("ServerLogs", ImVec2(0.0f, 0.0f), true);
    for (const std::string& log : snapshot.logs)
    {
        ImGui::TextWrapped("%s", log.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
    ImGui::End();
}

void ServerMonitorUI::EndFrame()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool ServerMonitorUI::HandleWndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) const
{
    return ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam);
}
