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

    // IPv4의 타입의 스트림형식의 TCP소켓을 설정한다.
    // socket함수를 통해서 OS가 내부적으로 새 소켓 객체를만든다. 그걸 핸들할 수 있는 것은 listenSocket이다. 
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

    // 서버가 어디 주소/포트에서 대기할지 설정하는 코드 bind에 넘길 주소 구조체를 채우는 과정
    // 이 서버 소켓을 IPv4의 0.0.0.0:9000에 바인드하겠다 는 설정
    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000); // 포트 9000 사용 htons로 네트워크 바이트 순서로 변환
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 특정 ip하나가 아니라 모든 로컬 인터페이스(0.0.0.0)에서 접속 허용 

    // os커널의 소켓을 핸들할 수 있는 listenSocket인스턴스에 sockaddr_in(주소)을 실제로 붙이는 단계. 
    // 서버가 어느 포트에서 받을 지 확정하는 핵심 호출 단계. 이계 성공해야 listen으로 넘어갈 수 있다.
    // IP주소 : 건물주소
    // 포트 : 그 건물에서 몇 번 창구인지
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


    // 내가 설정한 listensocket을 들어오는 연결 요청을 받을 수 있게 설정한다.
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
        // clientAddr은 “깡통”이 아니라 accept가 클라이언트 주소(IP/포트)를 써 넣는 출력 버퍼
        // clinetSocket핸들을 생성해서 누가 연결했는지 + 대화할 소켓 확보 기능을 함
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

        // 클라이언트 ip를 뽑는다.
        char clientIp[INET_ADDRSTRLEN] = {};
        inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp));

        // 클라이언트가 연결만 하고 데이터를 안 보내도 서버 스레드가 오래 묶이지 않게 하려는 설정
        const int receiveTimeoutMs = 1000;
        setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&receiveTimeoutMs), sizeof(receiveTimeoutMs));


        // 실제로 클라이언트가 보낸 데이터를 받아서 c문자열로 만든다
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
