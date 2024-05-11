#include "Application.h"
#include "Window.h"

#include <iostream>

int main()
{
	Application app{};
	Window window{1920, 1080, &app};

	app.Initialize();

    bool shouldExit = false;
    while (!shouldExit)
    {
        MSG msg{ 0 };
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        if (msg.message == WM_QUIT)
        {
            shouldExit = true;
        }

        app.Render();
    }
}
