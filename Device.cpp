#include "Device.h"
#include "DescriptorHeap.h"
#include "Queue.h"
#include "Window.h"

#include "D3D12MemAlloc.h"

#ifdef _DEBUG
#include <dxgidebug.h>
#include <iostream>
#endif

#define DX_ASSERT(hr) { if FAILED(hr) assert(false);}

Device::Device()
{
	InitializeDevice();
	InitializeDeviceResources();
	
	// this will be moved out later
	InitializePipeline();
}

Device::~Device()
{
}

void Device::InitializeDevice()
{
#ifdef _DEBUG
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
		std::cout << "Debug layer enabled." << std::endl;
	}

	ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiInfoQueue))))
	{
		DX_ASSERT(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&mFactory)));

		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
		dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
	}
	else
	{
		DX_ASSERT(CreateDXGIFactory2(0, IID_PPV_ARGS(&mFactory)));
	}

#else
	DX_ASSERT(CreateDXGIFactory2(0, IID_PPV_ARGS(&mFactory)));
#endif

	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<IDXGIFactory6> factory6;
	DX_ASSERT(mFactory.As(&factory6));
	for (UINT adapterID = 0;
		DXGI_ERROR_NOT_FOUND != factory6->EnumAdapterByGpuPreference(adapterID, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter));
		++adapterID)
	{
		DXGI_ADAPTER_DESC1 desc;
		DX_ASSERT(adapter->GetDesc1(&desc));

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// Don't select the Basic Render Driver adapter.
			continue;
		}

		// Check to see if the adapter supports Direct3D 12, but don't create the actual device yet.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, _uuidof(ID3D12Device), nullptr)))
		{
#ifdef _DEBUG
			wchar_t buff[256] = {};
			swprintf_s(buff, L"Direct3D Adapter (%u): VID:%04X, PID:%04X - %ls\n", adapterID, desc.VendorId, desc.DeviceId, desc.Description);
			OutputDebugStringW(buff);
			std::wcout << "Direct3D Adapter " << adapterID << ": " << desc.Description << std::endl;
#endif

			DX_ASSERT(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&mDevice)));

			D3D12MA::ALLOCATOR_DESC allocatorDesc = {};
			allocatorDesc.pDevice = mDevice.Get();
			allocatorDesc.pAdapter = adapter.Get();
			// These flags are optional but recommended.
			allocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_MSAA_TEXTURES_ALWAYS_COMMITTED |
				D3D12MA::ALLOCATOR_FLAG_DEFAULT_POOLS_NOT_ZEROED;

			DX_ASSERT(D3D12MA::CreateAllocator(&allocatorDesc, &mAllocator));
			return;
		}
	}

	assert(false && "Can't find device that supports D3D_FEATURE_LEAVEL_12_2");
}

void Device::InitializeDeviceResources()
{
	mGraphicsQueue =		std::make_unique<Queue>(mDevice.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT);
	mSRVDescriptorHeap =	std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true);
	mRTVDescriptorHeap =	std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_BACK_BUFFERS, false);

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{
		.Width = Window::GetWidth(),
		.Height = Window::GetHeight(),
		.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.Stereo = false,
		.SampleDesc = {
			.Count = 1,
			.Quality = 0 },
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = NUM_BACK_BUFFERS,
		.Scaling = DXGI_SCALING_NONE,
		.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
		.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED,
		.Flags = 0 };

	ComPtr<IDXGISwapChain1> swapChain = nullptr;
	DX_ASSERT(mFactory->CreateSwapChainForHwnd(mGraphicsQueue->GetQueue(), Window::GetHwnd(), &swapChainDesc, nullptr, nullptr, &swapChain));
	DX_ASSERT(swapChain->QueryInterface(__uuidof(IDXGISwapChain3), static_cast<void**>(&mSwapChain)));

	for (uint32_t bufferIndex = 0; bufferIndex < NUM_BACK_BUFFERS; bufferIndex++)
	{
		TextureResource& backbuffer = mBackBuffers[bufferIndex];

		Descriptor newDescriptor = mRTVDescriptorHeap->GetDescriptor();

		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {
			.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MipSlice = 0,
				.PlaneSlice = 0}};

		DX_ASSERT(mSwapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&backbuffer.mResource)));
		mDevice->CreateRenderTargetView(backbuffer.mResource.Get(), &rtvDesc, newDescriptor.mCpuHandle);

		backbuffer.mRtvDescriptor = newDescriptor;
		backbuffer.mDescriptorIndex = newDescriptor.mHeapIndex;
		backbuffer.mCurrentState = D3D12_RESOURCE_STATE_PRESENT;
	}

	for (uint32_t frameIndex = 0; frameIndex < FRAMES_IN_FLIGHT; frameIndex++)
	{
		DX_ASSERT(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocators[frameIndex])));
	}

	DX_ASSERT(mDevice->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&mCommandList)));
}

void Device::InitializePipeline()
{

}