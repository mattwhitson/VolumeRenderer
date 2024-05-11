#pragma once

#include <memory>

class Device;

class Application {
public:
	Application();
	~Application();

	void Initialize();

private:
	std::unique_ptr<Device> mDevice = nullptr;
};