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
	mSamplerDescriptorHeap = std::make_unique<DescriptorHeap>(mDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 1, true);

	D3D12_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.BorderColor[0] = samplerDesc.BorderColor[1] = samplerDesc.BorderColor[2] = samplerDesc.BorderColor[3] = 0.0f;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 16;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

	mDevice->CreateSampler(&samplerDesc, mSamplerDescriptorHeap->GetDescriptor().mCpuHandle);

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

	BufferDescription bufferDesc = {
		.heapType = D3D12_HEAP_TYPE_UPLOAD,
		.size = 1024 * 1024 * 32};

	mUploadBuffer = CreateBuffer(bufferDesc);
	mUploadBuffer->mResource->Map(0, nullptr, reinterpret_cast<void**>(&mUploadBuffer->mMapped));
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
	buffer->mSize = desc.Width;

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
		//buffer->mDescriptorIndex = buffer->mSrvDescriptor.mHeapIndex;
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
		//buffer->mDescriptorIndex = buffer->mSrvDescriptor.mHeapIndex;
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
	texture->mSize = desc.Width * desc.Height * desc.DepthOrArraySize; // also should add mip map size

	if ((textureDesc.textureDescriptor & DescriptorType::Dsv) == DescriptorType::Dsv)
	{
		D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{
			.Format = textureDesc.format,
			.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D,
			.Flags = D3D12_DSV_FLAG_NONE,
			.Texture2D = {.MipSlice = 0 } };

		texture->mDsvDescriptor = mDSVDescriptorHeap->GetDescriptor();
		//texture->mDescriptorIndex = texture->mDsvDescriptor.mHeapIndex;
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
		//texture->mDescriptorIndex = texture->mSrvDescriptor.mHeapIndex;
		mDevice->CreateRenderTargetView(texture->mResource.Get(), &rtvDesc, texture->mRtvDescriptor.mCpuHandle);
	}

		// THIS WILL ONLY WORK FOR BASIC setups, need to change this for cube maps/ 3d textures etc...
	if ((textureDesc.textureDescriptor & DescriptorType::Srv) == DescriptorType::Srv)
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
		if (textureDesc.dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			srvDesc = {
				.Format = textureDesc.format,
				.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Texture2D = {
					.MostDetailedMip = 0,
					.MipLevels = textureDesc.mipLevels,
					.PlaneSlice = 0,
					.ResourceMinLODClamp = 0.0f } };
		}
		else if (textureDesc.dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
		{
			srvDesc = {
				.Format = textureDesc.format,
				.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D,
				.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING,
				.Texture3D = {
					.MostDetailedMip = 0,
					.MipLevels = textureDesc.mipLevels,
					.ResourceMinLODClamp = 0.0f } };

		}

		texture->mSrvDescriptor = mSRVDescriptorHeap->GetDescriptor();
		texture->mDescriptorIndex = texture->mSrvDescriptor.mHeapIndex;
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

void Device::ImmediateUploadToGpu(Resource* resource, void* data)
{
	uint64_t arraySize = resource->mDesc.DepthOrArraySize;
	uint64_t mipLevels = resource->mDesc.MipLevels;
	if (resource->mDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D)
	{
		arraySize = 1;
	}

	static constexpr uint32_t MAX_TEXTURE_SUBRESOURCE_COUNT = 32;
	UINT numRows[MAX_TEXTURE_SUBRESOURCE_COUNT];
	uint64_t rowSizesInBytes[MAX_TEXTURE_SUBRESOURCE_COUNT];
	std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT, MAX_TEXTURE_SUBRESOURCE_COUNT> subResourceLayouts{}; // this should be a part of TextureResource
	uint32_t numSubresources = static_cast<uint32_t>(mipLevels * arraySize); // so basically, if its a 3d texture, you don't want to multiply by depthOrArraySize, otherwise yes
	uint64_t dataSize = 0;

	mDevice->GetCopyableFootprints(&resource->mDesc, 0, numSubresources, 0, subResourceLayouts.data(), numRows, rowSizesInBytes, &dataSize);

	uint8_t* sourceSubResourceMemory = static_cast<uint8_t*>(data);
	for (uint64_t arrayIndex = 0; arrayIndex < arraySize; arrayIndex++)
	{
		for (uint64_t mipIndex = 0; mipIndex < mipLevels; mipIndex++)
		{
			const uint64_t subResourceIndex = mipIndex + (arrayIndex * mipLevels);
			const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& subResourceLayout = subResourceLayouts[subResourceIndex];
			const uint64_t subResourceHeight = numRows[subResourceIndex];
			const uint64_t subResourcePitch = utils::AlignU32(subResourceLayout.Footprint.RowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
			const uint64_t subResourceDepth = subResourceLayout.Footprint.Depth;
			uint8_t* destinationSubResourceMemory = static_cast<uint8_t*>(mUploadBuffer->mMapped) + subResourceLayout.Offset;

			for (uint64_t sliceIndex = 0; sliceIndex < subResourceDepth; sliceIndex++)
			{
				for (uint64_t height = 0; height < subResourceHeight; height++)
				{
					memcpy(destinationSubResourceMemory, sourceSubResourceMemory, subResourcePitch);
					destinationSubResourceMemory += subResourcePitch;
					sourceSubResourceMemory += (resource->mDesc.Width * sizeof(uint8_t)); // this is not good enough for generic use obviously (need a stride variable attached to resource)
				}
			}
		}
	}



	mCommandAllocators[mFrameIndex]->Reset();
	mCommandList->Reset(mCommandAllocators[mFrameIndex].Get(), nullptr);

	D3D12_TEXTURE_COPY_LOCATION destinationLocation = {};
	destinationLocation.pResource = resource->mResource.Get();
	destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	destinationLocation.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION sourceLocation = {};
	sourceLocation.pResource = mUploadBuffer->mResource.Get();
	sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	sourceLocation.PlacedFootprint = subResourceLayouts[0];

	memcpy(mUploadBuffer->mMapped, data, resource->mSize);
	mCommandList->CopyTextureRegion(&destinationLocation, 0, 0, 0, &sourceLocation, nullptr);

	mGraphicsQueue->Submit(mCommandList.Get());
	mFenceValues[mFrameIndex] = mGraphicsQueue->Signal();
	mGraphicsQueue->WaitForQueueCpuBlocking(mFenceValues[mFrameIndex]);
}