#include "App/MainApp.h"

int WINAPI WinMain(HINSTANCE instanceHandle, HINSTANCE, LPSTR, int showCommand)
{
    MainApp app;
    return app.Run(instanceHandle, showCommand);
}
