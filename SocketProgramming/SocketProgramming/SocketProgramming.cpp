// SocketProgramming.cpp : 이 파일에는 'main' 함수가 포함됩니다. 거기서 프로그램 실행이 시작되고 종료됩니다.
//

#include <iostream>
#include <winsock2.h>
#include <string>

#pragma comment(lib, "ws2_32.lib") // 라이브러리 링크

int main()
{
    // 0단계: 윈도우 소켓 초기화 (전화국에 개통 신청)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 1단계: Socket() - 전화기 구입
    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, 0);

    // 2단계: Bind() - 전화번호(IP, Port) 할당
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9000); // 9000번 포트 사용
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY); // 아무 IP나 다 받음
    bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    // 3단계: Listen() - 전화 오길 대기
    listen(listenSocket, SOMAXCONN);
    std::cout << "서버가 시작되었습니다. 대기 중..." << std::endl;

    while (true) {
        sockaddr_in clientAddr;
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(listenSocket, (sockaddr*)&clientAddr, &addrLen);

        if (clientSocket != INVALID_SOCKET) {
            std::cout << "클라이언트 접속!" << std::endl;

            // 1. 읽기 (Receive)
            char buffer[1024] = { 0, };
            recv(clientSocket, buffer, sizeof(buffer), 0);
            std::cout << "--- 받은 메시지 ---\n" << buffer << "\n----------------" << std::endl;

            // 2. 답장 준비 (Response 조립)
            std::string body = "Hello, Client! Your server info: ";
            body += wsaData.szDescription;

            // ★ 중요: 전체 응답을 std::string 변수에 확실히 담아둡니다.
            std::string response =
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: " + std::to_string(body.length()) + "\r\n"
                "\r\n" +
                body;

            // 3. 보내기 (Send)
            // response가 살아있는 상태에서 .c_str()을 호출해야 안전합니다.
            send(clientSocket, response.c_str(), (int)response.length(), 0);

            // 4. 끊기
            closesocket(clientSocket);
        }
    }
    closesocket(listenSocket);
    WSACleanup();
}

