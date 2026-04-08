#include "Server/SocketServer.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    constexpr u_short kServerPort = 9000;
    constexpr int kLogLimit = 200;
    constexpr int kReceiveBufferSize = 512;
}

SocketServer::SocketServer() = default;

SocketServer::~SocketServer()
{
    Stop();
}

bool SocketServer::Start()
{
    // 이미 워커 스레드가 살아 있다면 중복으로 서버를 시작하지 않는다.
    if (workerThread_.joinable())
    {
        return false;
    }

    // Start()에서는 상태와 카운터만 초기화하고, 실제 TCP 처리 루프는 별도 워커 스레드에서 시작한다.

    stopRequested_ = false;
    initialized_ = false;
    listening_ = false;
    totalConnections_ = 0;
    totalMessages_ = 0;

    lastSocketError_ = 0;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        logs_.clear();
        clients_.clear();
        nextClientId_ = 1;
        status_ = "Starting chat server...";
    }

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
    snapshot.activeClients = static_cast<int>(clients_.size());
    snapshot.totalConnections = totalConnections_.load();
    snapshot.totalMessages = totalMessages_.load();
    snapshot.lastSocketError = lastSocketError_.load();
    snapshot.status = status_;
    snapshot.logs = logs_;
    snapshot.clients.reserve(clients_.size());

    for (const ClientConnection& client : clients_)
    {
        snapshot.clients.push_back({ client.name, client.endpoint });
    }

    return snapshot;
}

void SocketServer::ClearLogs()
{
    std::lock_guard<std::mutex> lock(mutex_);
    logs_.clear();
}

void SocketServer::Run()
{
    // 이 함수가 실제 TCP 채팅 서버 워커 루프다.
    // 순서는 WSAStartup -> socket -> bind -> listen -> nonblocking 설정 -> accept/recv/broadcast 반복이다.
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
    serverAddr.sin_port = htons(kServerPort);
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
    SetStatus("Listening for chat clients on port 9000");
    AppendLog("Chat server started. Listening on port 9000.");

    while (!stopRequested_)
    {
        // 이 안쪽 루프는 "새 연결이 더 있는지"를 확인하는 구간이다.
        // 이미 등록된 클라이언트가 다시 여기로 들어오는 것이 아니라, accept()로 아직 등록되지 않은 새 연결만 받는다.
        while (true)
        {
            sockaddr_in clientAddr = {};
            int clientAddrSize = sizeof(clientAddr);
            SOCKET clientSocket = accept(listenSocket, reinterpret_cast<sockaddr*>(&clientAddr), &clientAddrSize);
            if (clientSocket == INVALID_SOCKET)
            {
                const int socketError = WSAGetLastError();
                if (socketError != WSAEWOULDBLOCK)
                {
                    lastSocketError_ = socketError;
                    AppendLog("accept() failed with error " + std::to_string(socketError) + ".");
                }
                break;
            }

            ioctlsocket(clientSocket, FIONBIO, &nonBlockingMode);

            char clientIp[INET_ADDRSTRLEN] = {};
            inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));

            ClientConnection newClient;
            newClient.socket = static_cast<SocketHandle>(clientSocket);
            newClient.endpoint = std::string(clientIp[0] ? clientIp : "unknown") + ":" + std::to_string(ntohs(clientAddr.sin_port));

            {
                // 등록은 accept() 직후 여기서 한 번만 일어난다.
                // mutex 범위를 따로 중괄호로 묶은 이유는, 공유 자원(nextClientId_, clients_)만 짧게 보호하기 위해서다.
                std::lock_guard<std::mutex> lock(mutex_);
                newClient.name = "user" + std::to_string(nextClientId_++);
                clients_.push_back(newClient);
            }

            ++totalConnections_;

            const std::string joinedMessage = "[system] " + newClient.name + " joined from " + newClient.endpoint + "\n";
            BroadcastMessage(joinedMessage);
            AppendLog(newClient.name + " joined from " + newClient.endpoint + ".");
        }

        std::vector<std::string> outgoingMessages;
        std::vector<std::string> eventLogs;
        std::vector<std::pair<std::string, std::string>> disconnectedClients;

        {
            // 이미 등록된 클라이언트들은 accept()가 아니라 여기서 관리한다.
            // 각 소켓마다 recv()를 호출해 들어온 바이트를 읽고, 줄 단위(\n) 메시지로 조립한다.
            std::lock_guard<std::mutex> lock(mutex_);

            for (size_t index = 0; index < clients_.size();)
            {
                ClientConnection& client = clients_[index];
                char buffer[kReceiveBufferSize] = {};
                const int receivedBytes = recv(static_cast<SOCKET>(client.socket), buffer, sizeof(buffer), 0);

                if (receivedBytes > 0)
                {
                    // TCP는 메시지 경계가 없으므로, recv() 결과를 바로 한 문장으로 보면 안 된다.
                    // 그래서 클라이언트별 receiveBuffer에 누적한 뒤, \n이 나올 때마다 완성된 한 줄씩 꺼낸다.
                    client.receiveBuffer.append(buffer, receivedBytes);

                    size_t lineEnd = std::string::npos;
                    while ((lineEnd = client.receiveBuffer.find('\n')) != std::string::npos)
                    {
                        std::string line = client.receiveBuffer.substr(0, lineEnd);
                        client.receiveBuffer.erase(0, lineEnd + 1);

                        if (!line.empty() && line.back() == '\r')
                        {
                            line.pop_back();
                        }

                        if (line.empty())
                        {
                            continue;
                        }

                        outgoingMessages.push_back(client.name + ": " + line + "\n");
                        ++totalMessages_;
                    }

                    ++index;
                    continue;
                }

                if (receivedBytes == 0)
                {
                    disconnectedClients.push_back({ client.name, client.endpoint });
                    closesocket(static_cast<SOCKET>(client.socket));
                    clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(index));
                    continue;
                }

                const int socketError = WSAGetLastError();
                if (socketError == WSAEWOULDBLOCK)
                {
                    ++index;
                    continue;
                }

                lastSocketError_ = socketError;
                eventLogs.push_back("recv() failed for " + client.name + " with error " + std::to_string(socketError) + ".");
                disconnectedClients.push_back({ client.name, client.endpoint });
                closesocket(static_cast<SOCKET>(client.socket));
                clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(index));
            }
        }

        // 락 안에서는 공유 데이터 수정과 수집만 하고,
        // 실제 로그 반영/브로드캐스트는 락 밖에서 처리해 lock 점유 시간을 짧게 유지한다.
        for (const std::string& eventLog : eventLogs)
        {
            AppendLog(eventLog);
        }

        for (const std::string& message : outgoingMessages)
        {
            BroadcastMessage(message);
            AppendLog(message.substr(0, message.size() - 1));
        }

        for (const auto& disconnected : disconnectedClients)
        {
            BroadcastMessage("[system] " + disconnected.first + " left\n");
            AppendLog(disconnected.first + " left (" + disconnected.second + ").");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    std::vector<SocketHandle> remainingSockets;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const ClientConnection& client : clients_)
        {
            remainingSockets.push_back(client.socket);
        }
        clients_.clear();
    }

    for (SocketHandle socketHandle : remainingSockets)
    {
        closesocket(static_cast<SOCKET>(socketHandle));
    }

    listening_ = false;
    SetStatus("Server stopped");
    AppendLog("Chat server stopped.");

    closesocket(listenSocket);
    WSACleanup();
    initialized_ = false;
}

void SocketServer::AppendLog(const std::string& message)
{
    std::lock_guard<std::mutex> lock(mutex_);
    logs_.push_back(message);
    if (logs_.size() > kLogLimit)
    {
        logs_.erase(logs_.begin());
    }
}

void SocketServer::SetStatus(const std::string& status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = status;
}

void SocketServer::BroadcastMessage(const std::string& message)
{
    // 이미 만들어진 채팅 문자열을 현재 접속 중인 모든 클라이언트에게 전송한다.
    std::vector<std::string> eventLogs;

    {
        std::lock_guard<std::mutex> lock(mutex_);

        for (size_t index = 0; index < clients_.size();)
        {
            ClientConnection& client = clients_[index];
            if (SendText(client.socket, message))
            {
                ++index;
                continue;
            }

            eventLogs.push_back(client.name + " disconnected while broadcasting.");
            closesocket(static_cast<SOCKET>(client.socket));
            clients_.erase(clients_.begin() + static_cast<std::ptrdiff_t>(index));
        }
    }

    for (const std::string& eventLog : eventLogs)
    {
        AppendLog(eventLog);
    }
}

bool SocketServer::SendText(SocketHandle socket, const std::string& message)
{
    // send()는 한 번에 모든 바이트를 보내지 못할 수 있으므로, 남은 바이트가 없어질 때까지 반복한다.
    const char* data = message.data();
    int remainingBytes = static_cast<int>(message.size());

    while (remainingBytes > 0)
    {
        const int sentBytes = send(static_cast<SOCKET>(socket), data, remainingBytes, 0);
        if (sentBytes == SOCKET_ERROR)
        {
            const int socketError = WSAGetLastError();
            if (socketError == WSAEWOULDBLOCK)
            {
                continue;
            }

            lastSocketError_ = socketError;
            return false;
        }

        data += sentBytes;
        remainingBytes -= sentBytes;
    }

    return true;
}

