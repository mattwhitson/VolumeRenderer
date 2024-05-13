#include "Application.h"
#include "Device.h"

Application::Application()
	: mInput(Input())
{
}

Application::~Application()
{

}

void Application::Initialize()
{
	mDevice = std::make_unique<Device>(mInput);
	mIsInitialized = true;
}

void Application::Render()
{
	mDevice->Render();
}