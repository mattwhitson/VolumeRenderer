#include "Window.h"
#include "Application.h"

LRESULT WINAPI Window::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
    Application* app = reinterpret_cast<Application*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

    switch (msg)
    {
    case WM_CLOSE:
    case WM_DESTROY: PostQuitMessage(0); [[fallthrough]];
    case WM_SIZING: [[fallthrough]];
    case WM_SIZE: [[fallthrough]];
    default: return DefWindowProcW(hwnd, msg, wparam, lparam);
    }
}

HWND Window::mHwnd = NULL;
HINSTANCE Window::mModuleHandle = NULL;
uint32_t Window::mWidth = 0;
uint32_t Window::mHeight = 0;

Window::Window(uint32_t width, uint32_t height, Application* app)
{
    mWidth = width;
    mHeight = height;

    mModuleHandle = GetModuleHandle(nullptr);

    WNDCLASSW wcw = { 
        .lpfnWndProc = &WndProc,
        .hInstance = mModuleHandle,
        .hCursor = LoadCursor(nullptr, IDC_ARROW),
        .hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH),
        .lpszClassName = mAppName.c_str() };

    RegisterClassW(&wcw);

    RECT windowRect = { 0, 0, static_cast<LONG>(width), static_cast<LONG>(height) };

    mHwnd = CreateWindowExW(
        0, 
        mAppName.c_str(), 
        L"Volume Rendering",
        WS_VISIBLE | WS_OVERLAPPEDWINDOW, 
        CW_USEDEFAULT,
        CW_USEDEFAULT, 
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        nullptr, nullptr, nullptr, app);

    ShowWindow(mHwnd, SW_SHOW);
    SetForegroundWindow(mHwnd);
    SetFocus(mHwnd);
    ShowCursor(true);
}

Window::~Window()
{
    DestroyWindow(mHwnd);
    mHwnd = nullptr;

    UnregisterClassW(mAppName.c_str(), mModuleHandle);
    mModuleHandle = nullptr;
}