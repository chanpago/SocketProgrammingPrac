#include "UI/ChatClientUI.h"

#include "Network/ChatClientConnection.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <filesystem>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam);

namespace
{
    std::string GetProjectFontPath()
    {
        wchar_t modulePath[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, modulePath, MAX_PATH) == 0)
        {
            return {};
        }

        const std::filesystem::path fontPath = std::filesystem::path(modulePath).parent_path() / L".." / L".." / L"Fonts" / L"malgun.ttf";
        return fontPath.lexically_normal().string();
    }
}

ChatClientUI::~ChatClientUI()
{
    Shutdown();
}

bool ChatClientUI::Initialize(HWND windowHandle, ID3D11Device* device, ID3D11DeviceContext* deviceContext)
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

    const std::string fontPath = GetProjectFontPath();
    if (!fontPath.empty())
    {
        ImFontConfig fontConfig;
        fontConfig.OversampleH = 1;
        fontConfig.OversampleV = 1;
        fontConfig.PixelSnapH = true;
        static const ImWchar koreanRanges[] = { 0x0020, 0x00FF, 0x3131, 0x3163, 0xAC00, 0xD7A3, 0 };
        io.Fonts->AddFontFromFileTTF(fontPath.c_str(), 18.0f, &fontConfig, koreanRanges);
    }

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

void ChatClientUI::Shutdown()
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

void ChatClientUI::BeginFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ChatClientUI::Render(ChatClientConnection& connection, int clientIndex)
{
    ChatClientSnapshot snapshot = connection.GetSnapshot();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 12.0f, viewport->WorkPos.y + 12.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSize(ImVec2(226.0f, viewport->WorkSize.y - 24.0f), ImGuiCond_Appearing);
    ImGui::Begin("TCP Chat Client");
    ImGui::Text("Renderer: DirectX 11");
    ImGui::Text("ImGui backend: Win32 + DX11");
    if (clientIndex > 0)
    {
        ImGui::Text("Client Instance: %d", clientIndex);
    }

    ImGui::Separator();
    const ImVec4 statusColor = snapshot.connected ? ImVec4(0.35f, 0.85f, 0.45f, 1.0f) : ImVec4(0.95f, 0.35f, 0.35f, 1.0f);
    ImGui::TextColored(statusColor, "Status: %s", snapshot.status.c_str());
    ImGui::Text("Connected: %s", snapshot.connected ? "Yes" : "No");
    ImGui::Text("Initialized: %s", snapshot.initialized ? "Yes" : "No");
    ImGui::Text("Server: 127.0.0.1:9000");
    ImGui::Text("Last Socket Error: %d", snapshot.lastSocketError);

    if (snapshot.connected)
    {
        if (ImGui::Button("Disconnect"))
        {
            connection.Disconnect();
            snapshot = connection.GetSnapshot();
        }
    }
    else
    {
        if (ImGui::Button("Connect"))
        {
            connection.Connect();
            snapshot = connection.GetSnapshot();
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Logs"))
    {
        connection.ClearLogs();
        snapshot = connection.GetSnapshot();
    }

    const bool canSend = snapshot.connected;
    const bool hasInput = inputBuffer_[0] != '\0';

    if (!canSend)
    {
        ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.25f, 1.0f), "Send controls are disabled until connected.");
    }

    ImGui::Separator();
    ImGui::Text("Chat Logs");
    ImGui::BeginChild("ClientLogs", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing() * 4.5f), true);
    for (const std::string& log : snapshot.logs)
    {
        ImGui::TextWrapped("%s", log.c_str());
    }
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    {
        ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();

    ImGui::BeginDisabled(!canSend);
    ImGui::PushItemWidth(-90.0f);
    const bool sendWithEnter = ImGui::InputText("##MessageInput", inputBuffer_.data(), inputBuffer_.size(), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    const bool sendWithButton = ImGui::Button("Send", ImVec2(80.0f, 0.0f));

    if ((sendWithEnter || sendWithButton) && hasInput)
    {
        if (connection.SendText(inputBuffer_.data()))
        {
            inputBuffer_[0] = '\0';
        }

        snapshot = connection.GetSnapshot();
    }
    ImGui::EndDisabled();

    ImGui::End();
}

void ChatClientUI::EndFrame()
{
    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool ChatClientUI::HandleWndProc(HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam) const
{
    return ImGui_ImplWin32_WndProcHandler(windowHandle, message, wParam, lParam);
}
