#pragma once

#include <array>
#include <memory>

class Device;

struct Input {
	Input()
	{
		
	}

	std::array<bool, 26> keys{};
};

class Application {
public:
	Application();
	~Application();

	void Initialize();

	void Render();

public:
	bool mIsInitialized = false;
	Input mInput;

private:
	std::unique_ptr<Device> mDevice = nullptr;
};