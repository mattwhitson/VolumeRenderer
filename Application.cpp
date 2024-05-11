#include "Application.h"
#include "Device.h"

Application::Application()
{
}

Application::~Application()
{

}

void Application::Initialize()
{
	mDevice = std::make_unique<Device>();
}