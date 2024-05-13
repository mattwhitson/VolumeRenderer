#include "Device.h"
#include "DescriptorHeap.h"
#include "Queue.h"
#include "Window.h"
#include "Camera.h" /*temp*/
#include "Application.h"/*temp*/

#include "D3D12MemAlloc.h"

#ifdef _DEBUG
#include <dxgidebug.h>
#include <iostream>
#endif
#include <vector>

#define DX_ASSERT(hr) { if FAILED(hr) assert(false);}

Device::Device()
{
	InitializeDevice();
	InitializeDeviceResources();
}

Device::~Device()
{
	WaitForIdle();
}

void Device::WaitForIdle()
{
	for (uint32_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		mGraphicsQueue->WaitForQueueCpuBlocking(mFenceValues[i]);
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

			DX_ASSERT(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_2, IID_PPV_ARGS(&mDevice)) );

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
	mRTVDescriptorHeap =	std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, NUM_BACK_BUFFERS + 2, false);
	mDSVDescriptorHeap =	std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

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
		backbuffer.mTextureFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	}

	for (uint32_t frameIndex = 0; frameIndex < FRAMES_IN_FLIGHT; frameIndex++)
	{
		DX_ASSERT(mDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mCommandAllocators[frameIndex])));
		mFenceValues[frameIndex] = 0;
	}

	DX_ASSERT(mDevice->CreateCommandList1(0, D3D12_COMMAND_LIST_TYPE_DIRECT, D3D12_COMMAND_LIST_FLAG_NONE, IID_PPV_ARGS(&mCommandList)));
}

std::unique_ptr<BufferResource> Device::CreateBuffer(BufferDescription& bufferDesc, void* data)
{
	auto buffer = std::make_unique<BufferResource>();

	D3D12MA::ALLOCATION_DESC allocDesc{
	.HeapType = bufferDesc.heapType };

	D3D12_RESOURCE_DESC desc{
		.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
		.Width = utils::AlignU32(bufferDesc.size, 256),
		.Height = 1,
		.DepthOrArraySize = 1,
		.MipLevels = 1,
		.Format = bufferDesc.format,
		.SampleDesc = {.Count = 1, .Quality = 0},
		.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR };

	DX_ASSERT(mAllocator->CreateResource(
		&allocDesc,
		&desc,
		bufferDesc.initialState,
		nullptr,
		&buffer->mAllocation,
		IID_PPV_ARGS(&buffer->mResource)));

	buffer->mDesc = desc;
	buffer->mStride = bufferDesc.stride;
	buffer->mCurrentState = bufferDesc.initialState;

	uint32_t numElements = bufferDesc.stride > 0 ? (bufferDesc.size / bufferDesc.stride) : 1;

	if ((bufferDesc.bufferDescriptor & DescriptorType::Srv) == DescriptorType::Srv)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{
			.Format = bufferDesc.isRaw ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_SRV_DIMENSION_BUFFER,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Buffer = {
				.FirstElement = 0,
				.NumElements = bufferDesc.isRaw ? (bufferDesc.size / 4u) : numElements,
				.StructureByteStride = bufferDesc.isRaw ? 0 : bufferDesc.stride,
				.Flags = bufferDesc.isRaw ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE } };

		buffer->mSrvDescriptor = mSRVDescriptorHeap->GetDescriptor();
		buffer->mDescriptorIndex = buffer->mSrvDescriptor.mHeapIndex;
		mDevice->CreateShaderResourceView(buffer->mResource.Get(), &srvDesc, buffer->mSrvDescriptor.mCpuHandle);
	}
	if ((bufferDesc.bufferDescriptor & DescriptorType::Cbv) == DescriptorType::Cbv)
	{
		D3D12_CONSTANT_BUFFER_VIEW_DESC constantBufferViewDesc = {
			.BufferLocation = buffer->mResource->GetGPUVirtualAddress(),
			.SizeInBytes = static_cast<uint32_t>(buffer->mDesc.Width) };

		buffer->mCbvDescriptor = mSRVDescriptorHeap->GetDescriptor();
		buffer->mDescriptorIndex = buffer->mSrvDescriptor.mHeapIndex;
		mDevice->CreateConstantBufferView(&constantBufferViewDesc, buffer->mCbvDescriptor.mCpuHandle);
	}
	if ((bufferDesc.bufferDescriptor & DescriptorType::Uav) == DescriptorType::Uav)
	{
		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {
			.Format = bufferDesc.isRaw ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN,
			.ViewDimension = D3D12_UAV_DIMENSION_BUFFER,
			.Buffer = {
				.FirstElement = 0,
				.NumElements = bufferDesc.isRaw ? (bufferDesc.size / 4u) : numElements,
				.StructureByteStride = bufferDesc.isRaw ? 0 : buffer->mStride,
				.CounterOffsetInBytes = 0,
				.Flags = bufferDesc.isRaw ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE } };

		buffer->mUavDescriptor = mSRVDescriptorHeap->GetDescriptor();
		buffer->mDescriptorIndex = buffer->mSrvDescriptor.mHeapIndex;
		mDevice->CreateUnorderedAccessView(buffer->mResource.Get(), nullptr, &uavDesc, buffer->mUavDescriptor.mCpuHandle);
	}

	return buffer;
}

std::unique_ptr<TextureResource> Device::CreateTexture(TextureDescription& textureDesc)
{
	auto texture = std::make_unique<TextureResource>();

	D3D12MA::ALLOCATION_DESC allocDesc{
		.HeapType = D3D12_HEAP_TYPE_DEFAULT };

	bool hasRtv = ((textureDesc.textureDescriptor & DescriptorType::Rtv) == DescriptorType::Rtv);
	bool hasDsv = ((textureDesc.textureDescriptor & DescriptorType::Dsv) == DescriptorType::Dsv);
	bool hasSrv = ((textureDesc.textureDescriptor & DescriptorType::Srv) == DescriptorType::Srv);
	bool hasUav = ((textureDesc.textureDescriptor & DescriptorType::Uav) == DescriptorType::Uav);

	DXGI_FORMAT resourceFormat = textureDesc.format;
	if (hasRtv && resourceFormat == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)
	{
		resourceFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	D3D12_RESOURCE_DESC desc{
		.Dimension = textureDesc.dimension,
		.Width = textureDesc.width,
		.Height = textureDesc.height,
		.DepthOrArraySize = textureDesc.depthOrArraySize,
		.MipLevels = textureDesc.mipLevels,
		.Format = resourceFormat,
		.SampleDesc = {.Count = 1, .Quality = 0},
		.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
		.Flags = textureDesc.flags };


	D3D12_CLEAR_VALUE clearValue{
		.Format = textureDesc.format };

	if (hasRtv)
	{
		FLOAT color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		clearValue.Color[0] = clearValue.Color[1] = clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 1.0f;
	}
	else if (hasDsv)
	{
		clearValue.DepthStencil.Depth = 0.0f;
	}

	DX_ASSERT(mAllocator->CreateResource(
		&allocDesc,
		&desc,
		textureDesc.initialState,
		(hasRtv || hasDsv) ? &clearValue : nullptr,
		&texture->mAllocation,
		IID_PPV_ARGS(&texture->mResource)));

	texture->mDesc = desc;
	texture->mCurrentState = textureDesc.initialState;

	if ((textureDesc.textureDescriptor & DescriptorType::Dsv) == DescriptorType::Dsv)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
			.Format = textureDesc.format,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_NONE,
			.Texture2D = {.MipSlice = 0 } };

		texture->mDsvDescriptor = mDSVDescriptorHeap->GetDescriptor();
		texture->mDescriptorIndex = texture->mDsvDescriptor.mHeapIndex;
		mDevice->CreateDepthStencilView(texture->mResource.Get(), &dsvDesc, texture->mDsvDescriptor.mCpuHandle);
	}
	if ((textureDesc.textureDescriptor & DescriptorType::Rtv) == DescriptorType::Rtv)
	{
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{
			.Format = textureDesc.format,
			.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
			.Texture2D = {
				.MipSlice = 0,
				.PlaneSlice = 0} };

		texture->mRtvDescriptor = mRTVDescriptorHeap->GetDescriptor();
		texture->mDescriptorIndex = texture->mDsvDescriptor.mHeapIndex;
		mDevice->CreateRenderTargetView(texture->mResource.Get(), &rtvDesc, texture->mRtvDescriptor.mCpuHandle);
	}
	if ((textureDesc.textureDescriptor & DescriptorType::Srv) == DescriptorType::Srv)
	{
		// THIS WILL ONLY WORK FOR BASIC setups, need to change this for cube maps/ 3d textures etc...
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{ .Format = textureDesc.format,
			.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
			.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
			.Texture2D = {
				.MostDetailedMip = 0,
				.MipLevels = textureDesc.mipLevels,
				.PlaneSlice = 0,
				.ResourceMinLODClamp = 0.0f } };

		texture->mSrvDescriptor = mSRVDescriptorHeap->GetDescriptor();
		texture->mDescriptorIndex = texture->mDsvDescriptor.mHeapIndex;
		mDevice->CreateShaderResourceView(texture->mResource.Get(), &srvDesc, texture->mSrvDescriptor.mCpuHandle);
	}

	return texture;
}

void Device::BeginFrame()
{
	mGraphicsQueue->WaitForQueueCpuBlocking(mFenceValues[mFrameIndex]);
	mCommandAllocators[mFrameIndex]->Reset();
	mCommandList->Reset(mCommandAllocators[mFrameIndex].Get(), nullptr);
}

void Device::EndFrame()
{
	mGraphicsQueue->Submit(mCommandList.Get());

	mSwapChain->Present(0, 0);

	mFenceValues[mFrameIndex] = mGraphicsQueue->Signal();
	mFrameIndex = (mFrameIndex + 1) % FRAMES_IN_FLIGHT;
}