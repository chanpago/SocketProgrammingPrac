#include "App/ClientApp.h"

#include <cstdlib>
#include <string>

int WINAPI WinMain(HINSTANCE instanceHandle, HINSTANCE, LPSTR commandLine, int showCommand)
{
    // 서버가 LaunchChatClient()에서 넘긴 --client-index 값을 읽어,
    // 어떤 클라이언트 창인지 제목과 UI에서 구분할 수 있게 한다.
    int clientIndex = 0;
    const std::string commandLineText = commandLine ? commandLine : "";
    const std::string prefix = "--client-index=";
    const size_t prefixPosition = commandLineText.find(prefix);
    if (prefixPosition != std::string::npos)
    {
        clientIndex = std::atoi(commandLineText.c_str() + prefixPosition + prefix.size());
    }

    ClientApp app(clientIndex);
    return app.Run(instanceHandle, showCommand);
}
