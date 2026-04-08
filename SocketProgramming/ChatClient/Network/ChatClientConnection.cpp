#include "Network/ChatClientConnection.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace
{
    constexpr const char* kServerHost = "127.0.0.1";
    constexpr u_short kServerPort = 9000;
    constexpr int kLogLimit = 300;
    constexpr int kReceiveBufferSize = 512;
}

ChatClientConnection::ChatClientConnection() = default;

ChatClientConnection::~ChatClientConnection()
{
    Disconnect();
}

bool ChatClientConnection::Connect()
{
    // 클라이언트도 서버가 대신 연결해 주는 것이 아니라, 자기 프로세스 안에서 직접 127.0.0.1:9000으로 connect()를 수행한다.
    if (connected_ || receiveThread_.joinable())
    {
        return true;
    }

    WSADATA wsaData = {};
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        lastSocketError_ = WSAGetLastError();
        SetStatus("WSAStartup failed");
        AppendLog("WSAStartup failed.", LogKind::Error);
        return false;
    }

    initialized_ = true;
    stopRequested_ = false;
    lastSocketError_ = 0;
    receiveBuffer_.clear();

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET)
    {
        lastSocketError_ = WSAGetLastError();
        SetStatus("socket() failed");
        AppendLog("socket() failed.", LogKind::Error);
        WSACleanup();
        initialized_ = false;
        return false;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(kServerPort);
    inet_pton(AF_INET, kServerHost, &serverAddr.sin_addr);

    if (connect(clientSocket, reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
    {
        lastSocketError_ = WSAGetLastError();
        SetStatus("connect() failed");
        AppendLog(std::string("connect() failed. Is the server running on ") + kServerHost + ":" + std::to_string(kServerPort) + "?", LogKind::Error);
        closesocket(clientSocket);
        WSACleanup();
        initialized_ = false;
        return false;
    }

    socket_ = static_cast<SocketHandle>(clientSocket);
    connected_ = true;
    SetStatus(std::string("Connected to ") + kServerHost + ":" + std::to_string(kServerPort));
    AppendLog(std::string("Connected to chat server at ") + kServerHost + ":" + std::to_string(kServerPort), LogKind::Info);
    receiveThread_ = std::thread(&ChatClientConnection::ReceiveLoop, this);
    return true;
}

void ChatClientConnection::Disconnect()
{
    stopRequested_ = true;

    if (socket_ != kInvalidSocketHandle)
    {
        shutdown(static_cast<SOCKET>(socket_), SD_BOTH);
    }

    if (receiveThread_.joinable())
    {
        receiveThread_.join();
    }

    if (socket_ != kInvalidSocketHandle)
    {
        closesocket(static_cast<SOCKET>(socket_));
        socket_ = kInvalidSocketHandle;
    }

    if (initialized_)
    {
        WSACleanup();
    }

    connected_ = false;
    initialized_ = false;
    stopRequested_ = false;
    SetStatus("Disconnected");
}

bool ChatClientConnection::SendText(const std::string& message)
{
    // 서버와 마찬가지로 send-all 방식으로 전송한다.
    // 한 번의 send()만 믿지 않고 남은 바이트가 없어질 때까지 반복한다.
    if (!connected_ || socket_ == kInvalidSocketHandle || message.empty())
    {
        return false;
    }

    std::string outbound = message;
    outbound.push_back('\n');

    const char* data = outbound.data();
    int remainingBytes = static_cast<int>(outbound.size());
    while (remainingBytes > 0)
    {
        const int sentBytes = send(static_cast<SOCKET>(socket_), data, remainingBytes, 0);
        if (sentBytes == SOCKET_ERROR)
        {
            lastSocketError_ = WSAGetLastError();
            SetStatus("send() failed");
            AppendLog("send() failed.", LogKind::Error);
            shutdown(static_cast<SOCKET>(socket_), SD_BOTH);
            connected_ = false;
            return false;
        }

        data += sentBytes;
        remainingBytes -= sentBytes;
    }

    return true;
}

ChatClientSnapshot ChatClientConnection::GetSnapshot() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    ChatClientSnapshot snapshot;
    snapshot.initialized = initialized_.load();
    snapshot.connected = connected_.load();
    snapshot.lastSocketError = lastSocketError_.load();
    snapshot.status = status_;
    snapshot.logs = logs_;
    return snapshot;
}

void ChatClientConnection::ClearLogs()
{
    std::lock_guard<std::mutex> lock(mutex_);
    logs_.clear();
}

void ChatClientConnection::ReceiveLoop()
{
    // 클라이언트 수신도 줄 단위 프로토콜을 사용한다.
    // 서버가 브로드캐스트한 텍스트를 receiveBuffer에 누적한 뒤, \n이 보일 때마다 한 줄씩 꺼내서 로그에 반영한다.
    while (!stopRequested_)
    {
        char buffer[kReceiveBufferSize] = {};
        const int receivedBytes = recv(static_cast<SOCKET>(socket_), buffer, sizeof(buffer), 0);
        if (receivedBytes > 0)
        {
            receiveBuffer_.append(buffer, receivedBytes);

            size_t lineEnd = std::string::npos;
            while ((lineEnd = receiveBuffer_.find('\n')) != std::string::npos)
            {
                std::string line = receiveBuffer_.substr(0, lineEnd);
                receiveBuffer_.erase(0, lineEnd + 1);

                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }

                if (!line.empty())
                {
                    AppendLog(line, LogKind::Chat);
                }
            }

            continue;
        }

        if (receivedBytes == 0)
        {
            AppendLog("Server disconnected.", LogKind::Warning);
            SetStatus("Server disconnected");
        }
        else if (!stopRequested_)
        {
            lastSocketError_ = WSAGetLastError();
            AppendLog("recv() failed.", LogKind::Error);
            SetStatus("recv() failed");
        }

        connected_ = false;
        break;
    }
}

void ChatClientConnection::AppendLog(const std::string& message, LogKind kind)
{
    std::lock_guard<std::mutex> lock(mutex_);
    logs_.push_back(FormatLog(message, kind));
    if (logs_.size() > kLogLimit)
    {
        logs_.erase(logs_.begin());
    }
}

void ChatClientConnection::SetStatus(const std::string& status)
{
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = status;
}

std::string ChatClientConnection::FormatLog(const std::string& message, LogKind kind) const
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm localTime = {};
    localtime_s(&localTime, &time);

    std::ostringstream stream;
    stream << '[' << std::put_time(&localTime, "%H:%M:%S") << "] ";

    switch (kind)
    {
    case LogKind::Warning:
        stream << "[warn] ";
        break;
    case LogKind::Error:
        stream << "[error] ";
        break;
    case LogKind::Chat:
        break;
    case LogKind::Info:
    default:
        stream << "[info] ";
        break;
    }

    stream << message;
    return stream.str();
}
