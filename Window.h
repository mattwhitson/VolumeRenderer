#pragma once

#include <string>

class Application;

class Window {
public:
	Window(uint32_t width, uint32_t height, Application* app);
	~Window();

	static uint32_t GetWidth() { return mWidth; }
	static uint32_t GetHeight() { return mHeight; }

	static HWND GetHwnd() { return mHwnd; }
private:
	static LRESULT WINAPI WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

private:
	static uint32_t mWidth, mHeight;
	static HWND mHwnd;
	static HINSTANCE mModuleHandle;
	const std::wstring mAppName = L"D3D12 Volume Rendering";
};