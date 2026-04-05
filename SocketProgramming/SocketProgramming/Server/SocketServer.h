#pragma once

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct ServerSnapshot
{
    bool initialized = false;
    bool listening = false;
    int totalConnections = 0;
    int lastSocketError = 0;
    std::string status;
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
    void Run();
    void AppendLog(const std::string& message);
    void SetStatus(const std::string& status);
    std::string MakeHttpResponseBody() const;

private:
    std::thread workerThread_;
    std::atomic<bool> stopRequested_ = false;
    std::atomic<bool> initialized_ = false;
    std::atomic<bool> listening_ = false;
    std::atomic<int> totalConnections_ = 0;
    std::atomic<int> lastSocketError_ = 0;

    mutable std::mutex mutex_;
    std::string status_ = "Stopped";
    std::vector<std::string> logs_;
};
