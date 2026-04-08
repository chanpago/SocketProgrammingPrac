#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct ClientSnapshot
{
    std::string name;
    std::string endpoint;
};

struct ServerSnapshot
{
    bool initialized = false;
    bool listening = false;
    int activeClients = 0;
    int totalConnections = 0;
    int totalMessages = 0;
    int lastSocketError = 0;
    std::string status;
    std::vector<ClientSnapshot> clients;
    std::vector<std::string> logs;
};

class SocketServer
{
public:
    SocketServer();
    ~SocketServer();

    bool Start();
    void Stop();

    ServerSnapshot GetSnapshot() const;
    void ClearLogs();

private:
    using SocketHandle = std::uintptr_t;
    static constexpr SocketHandle kInvalidSocketHandle = static_cast<SocketHandle>(~static_cast<std::uintptr_t>(0));

    struct ClientConnection
    {
        SocketHandle socket = kInvalidSocketHandle;
        std::string name;
        std::string endpoint;
        std::string receiveBuffer;
    };

    void Run();
    void AppendLog(const std::string& message);
    void SetStatus(const std::string& status);
    void BroadcastMessage(const std::string& message);
    bool SendText(SocketHandle socket, const std::string& message);

private:
    std::thread workerThread_;
    std::atomic<bool> stopRequested_ = false;
    std::atomic<bool> initialized_ = false;
    std::atomic<bool> listening_ = false;
    std::atomic<int> totalConnections_ = 0;
    std::atomic<int> totalMessages_ = 0;
    std::atomic<int> lastSocketError_ = 0;

    mutable std::mutex mutex_;
    std::string status_ = "Stopped";
    std::vector<std::string> logs_;
    std::vector<ClientConnection> clients_;
    int nextClientId_ = 1;
};
