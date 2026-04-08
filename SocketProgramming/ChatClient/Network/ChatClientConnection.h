#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct ChatClientSnapshot
{
    bool initialized = false;
    bool connected = false;
    int lastSocketError = 0;
    std::string status;
    std::vector<std::string> logs;
};

enum class LogKind
{
    Info,
    Warning,
    Error,
    Chat,
};

class ChatClientConnection
{
public:
    ChatClientConnection();
    ~ChatClientConnection();

    bool Connect();
    void Disconnect();
    bool SendText(const std::string& message);

    ChatClientSnapshot GetSnapshot() const;
    void ClearLogs();

private:
    using SocketHandle = std::uintptr_t;
    static constexpr SocketHandle kInvalidSocketHandle = static_cast<SocketHandle>(~static_cast<std::uintptr_t>(0));

    void ReceiveLoop();
    void AppendLog(const std::string& message, LogKind kind = LogKind::Info);
    void SetStatus(const std::string& status);
    std::string FormatLog(const std::string& message, LogKind kind) const;

private:
    std::atomic<bool> initialized_ = false;
    std::atomic<bool> connected_ = false;
    std::atomic<bool> stopRequested_ = false;
    std::atomic<int> lastSocketError_ = 0;
    std::thread receiveThread_;
    SocketHandle socket_ = kInvalidSocketHandle;

    mutable std::mutex mutex_;
    std::string status_ = "Disconnected";
    std::vector<std::string> logs_;
    std::string receiveBuffer_;
};
