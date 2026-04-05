#include "Server/SocketServer.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <sstream>

SocketServer::SocketServer() = default;

SocketServer::~SocketServer()
{
    Stop();
}

bool SocketServer::Start()
{
    if (workerThread_.joinable())
    {
        return false;
    }

    stopRequested_ = false;
    totalConnections_ = 0;
    lastSocketError_ = 0;
    ClearLogs();
    SetStatus("Starting server...");

    workerThread_ = std::thread(&SocketServer::Run, this);
    return true;
}

void SocketServer::Stop()
{
    stopRequested_ = true;
    if (workerThread_.joinable())
    {
        workerThread_.join();
    }
}

ServerSnapshot SocketServer::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    ServerSnapshot snapshot;
    snapshot.initialized = initialized_.load();
    snapshot.listening = listening_.load();
    snapshot.totalConnections = totalConnections_.load();
    snapshot.lastSocketError = lastSocketError_.load();
    snapshot.status = status_;
    snapshot.logs = logs_;
    return snapshot;
}

void SocketServer::ClearLogs()
{
    std::lock_guard<std::mutex> lock(mutex_);
    logs_.clear();
}

void SocketServer::Run()
{
    WSADATA wsaData = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        lastSocketError_ = WSAGetLastError();
        SetStatus("WSAStartup failed");
        AppendLog("WSAStartup failed.");
        return;
    }

    initialized_ = true;

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        lastSocketError_ = WSAGetLastError();
        SetStatus("socket() failed");
        AppendLog("socket() failed.");
        WSACleanup();
        initialized_ = false;
        return;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000);
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        lastSocketError_ = WSAGetLastError();
        SetStatus("bind() failed - check port 9000");
        AppendLog("bind() failed. Port 9000 may already be in use.");
        closesocket(listenSocket);
        WSACleanup();
        initialized_ = false;
        return;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        lastSocketError_ = WSAGetLastError();
        SetStatus("listen() failed");
        AppendLog("listen() failed.");
        closesocket(listenSocket);
        WSACleanup();
        initialized_ = false;
        return;
    }

    u_long nonBlockingMode = 1;
    ioctlsocket(listenSocket, FIONBIO, &nonBlockingMode);

    listening_ = true;
    SetStatus("Listening on port 9000");
    AppendLog("Server started. Listening on port 9000.");

    while (!stopRequested_)
    {
        sockaddr_in clientAddr = {};
        int clientAddrSize = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize);

        if (clientSocket == INVALID_SOCKET)
        {
            const int socketError = WSAGetLastError();
            if (socketError == WSAEWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
                continue;
            }

            lastSocketError_ = socketError;
            AppendLog("accept() failed with error " + std::to_string(socketError) + ".");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        char clientIp[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));

        const int receiveTimeoutMs = 1000;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&receiveTimeoutMs), sizeof(receiveTimeoutMs));

        char buffer[1024] = {};
        const int receivedBytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (receivedBytes > 0)
        {
            buffer[receivedBytes] = '\0';
        }
        else
        {
            buffer[0] = '\0';
        }

        const std::string body = MakeHttpResponseBody();
        const std::string response =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html; charset=utf-8\r\n"
            "Connection: close\r\n"
            "Content-Length: " + std::to_string(body.size()) + "\r\n"
            "\r\n" +
            body;

        send(clientSocket, response.c_str(), static_cast<int>(response.size()), 0);
        closesocket(clientSocket);

        ++totalConnections_;

        std::ostringstream logStream;
        logStream << "Client connected from " << (clientIp[0] ? clientIp : "unknown") << ":" << ntohs(clientAddr.sin_port);
        if (receivedBytes > 0)
        {
            std::string requestLine(buffer);
            const size_t lineEnd = requestLine.find("\r\n");
            if (lineEnd != std::string::npos)
            {
                requestLine = requestLine.substr(0, lineEnd);
            }
            logStream << " | " << requestLine;
        }
        AppendLog(logStream.str());
    }

    listening_ = false;
    SetStatus("Server stopped");
    AppendLog("Server stopped.");

    closesocket(listenSocket);
    WSACleanup();
    initialized_ = false;
}

void SocketServer::AppendLog(const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    logs_.push_back(message);
    if (logs_.size() > 200)
    {
        logs_.erase(logs_.begin());
    }
}

void SocketServer::SetStatus(const std::string& status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = status;
}

std::string SocketServer::MakeHttpResponseBody() const
{
    return "Hello, Client! ImGui + DirectX11 server is running.";
}
