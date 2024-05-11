#pragma once

#include <memory>

class Device;

class Application {
public:
	Application();
	~Application();

	void Initialize();

	void Render();

private:
	std::unique_ptr<Device> mDevice = nullptr;
};