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

Device::Device(Input& input)
	: mInput(input)
{
	InitializeDevice();
	InitializeDeviceResources();
	
	// this will be moved out later
	InitializePipeline();
	Render();
}

Device::~Device()
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

	TextureDescription depthBufferDesc{
		.textureDescriptor = DescriptorType::Dsv,
		.format = DXGI_FORMAT_D32_FLOAT,
		.initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE,
		.flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		.width = Window::GetWidth(),
		.height = Window::GetHeight() };

	mDepthBuffer = CreateTexture(depthBufferDesc);

	TextureDescription cubeRenderDesc{
		.textureDescriptor = DescriptorType::Rtv | DescriptorType::Srv,
		.format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.initialState = D3D12_RESOURCE_STATE_RENDER_TARGET,
		.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.width = Window::GetWidth(),
		.height = Window::GetHeight()
	};

	mCubeFront =	CreateTexture(cubeRenderDesc);
	mCubeBack =		CreateTexture(cubeRenderDesc);
}

void Device::InitializePipeline()
{
	D3D12_ROOT_PARAMETER1 params[] = {
		{.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
		 .Descriptor = {.ShaderRegister = 0, .RegisterSpace = 0},
		 .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL} };

	D3D12_ROOT_SIGNATURE_DESC1 desc = { 
		.NumParameters = static_cast<uint32_t>(std::size(params)),
		.pParameters = params, 
		.NumStaticSamplers = 0,
		.pStaticSamplers = nullptr,
		.Flags = D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED | D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED };

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC vDesc{
		.Version = D3D_ROOT_SIGNATURE_VERSION_1_1,
		.Desc_1_1 = desc };

	ComPtr<ID3DBlob> blob;
	ComPtr<ID3DBlob> error;
	D3D12SerializeVersionedRootSignature(&vDesc, blob.GetAddressOf(), error.GetAddressOf());
	DX_ASSERT(mDevice->CreateRootSignature(0, blob->GetBufferPointer(),
		blob->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));


	ComPtr<ID3DBlob> vertexBlob;
	ComPtr<ID3DBlob> pixelBlob;
	DX_ASSERT(D3DReadFileToBlob(L"VertexShader.cso", &vertexBlob));
	DX_ASSERT(D3DReadFileToBlob(L"PixelShader.cso", &pixelBlob));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{
		.pRootSignature = mRootSignature.Get(),
		.VS = {
			.pShaderBytecode = vertexBlob->GetBufferPointer(),
			.BytecodeLength = vertexBlob->GetBufferSize()},
		.PS = {
			.pShaderBytecode = pixelBlob->GetBufferPointer(),
			.BytecodeLength = pixelBlob->GetBufferSize()},
		.BlendState = {},
		.SampleMask = 0xFFFFFFFF,
		.RasterizerState = {
			.FillMode = D3D12_FILL_MODE_SOLID,
			.CullMode = D3D12_CULL_MODE_BACK,
			.FrontCounterClockwise = TRUE,
			.DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
			.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
			.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
			.DepthClipEnable = TRUE,
			.MultisampleEnable = FALSE,
			.AntialiasedLineEnable = FALSE,
			.ForcedSampleCount = 0,
			.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF},
		.DepthStencilState {
			.DepthEnable = TRUE,
			.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL,
			.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL,
			.StencilEnable = FALSE},
		.InputLayout = {
			.pInputElementDescs = nullptr,
			.NumElements = 0},
		.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
		.NumRenderTargets = NUM_BACK_BUFFERS,
		.DSVFormat = DXGI_FORMAT_D32_FLOAT,
		.SampleDesc = {.Count = 1, .Quality = 0},
		.NodeMask = 0 };


	pipelineDesc.BlendState.AlphaToCoverageEnable = FALSE;
	pipelineDesc.BlendState.IndependentBlendEnable = FALSE;
	const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
	{
		FALSE,FALSE,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};

	for (uint32_t i = 0; i < NUM_BACK_BUFFERS; i++)
	{
		pipelineDesc.BlendState.RenderTarget[i] = defaultRenderTargetBlendDesc;
		pipelineDesc.RTVFormats[i] = mBackBuffers[i].mTextureFormat;
	}

	DX_ASSERT(mDevice->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&mGraphicsPipeline)));
	
	{
		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		DX_ASSERT(mDevice->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&mCullFrontFacePipeline)));
	}

	// Cube
	{
		DirectX::XMFLOAT3 meshVertices[36] = {
			{1.0f, -1.0f, 1.0f},
			{1.0f, -1.0f, -1.0f},
			{-1.0f, -1.0f, 1.0f},
			{-1.0f, -1.0f, 1.0f},
			{1.0f, -1.0f, -1.0f},
			{-1.0f, -1.0f, -1.0f},
			{1.0f, 1.0f, -1.0f},
			{1.0f, 1.0f, 1.0f},
			{-1.0f, 1.0f, -1.0f}, 
			{-1.0f, 1.0f, -1.0f}, 
			{1.0f, 1.0f, 1.0f}, 
			{-1.0f, 1.0f, 1.0f}, 
			{-1.0f, -1.0f, -1.0f},
			{-1.0f, 1.0f, -1.0f}, 
			{-1.0f, -1.0f, 1.0f}, 
			{-1.0f, -1.0f, 1.0f}, 
			{-1.0f, 1.0f, -1.0f}, 
			{-1.0f, 1.0f, 1.0f},
			{-1.0f, -1.0f, 1.0f},
			{-1.0f, 1.0f, 1.0f},
			{1.0f, -1.0f, 1.0f},
			{1.0f, -1.0f, 1.0f},
			{-1.0f, 1.0f, 1.0f},
			{1.0f, 1.0f, 1.0f},
			{1.0f, -1.0f, 1.0f},
			{1.0f, 1.0f, 1.0f},
			{1.0f, -1.0f, -1.0f},
			{1.0f, -1.0f, -1.0f},
			{1.0f, 1.0f, 1.0f}, 
			{1.0f, 1.0f, -1.0f}, 
			{1.0f, -1.0f, -1.0f}, 
			{1.0f, 1.0f, -1.0f}, 
			{-1.0f, -1.0f, -1.0f},
			{-1.0f, -1.0f, -1.0f}, 
			{1.0f, 1.0f, -1.0f}, 
			{-1.0f, 1.0f, -1.0f},
		};

		BufferDescription desc{
			.bufferDescriptor = DescriptorType::Srv,
			.heapType = D3D12_HEAP_TYPE_UPLOAD,
			.size = sizeof(DirectX::XMFLOAT3) * 36,
			.count = 36,
			.stride = sizeof(DirectX::XMFLOAT3),
			.isRaw = true };
		mCube = CreateBuffer(desc, meshVertices);

		void* data;
		mCube->mResource->Map(0, nullptr, &data);
		for (uint32_t i = 0; i < 36; i++)
		{
			memcpy(data, &meshVertices[i], sizeof(DirectX::XMFLOAT3));
			data = static_cast<char*>(data) + sizeof(DirectX::XMFLOAT3);
		}
		mCube->mResource->Unmap(0, nullptr);
	}

	mCamera = std::make_unique<Camera>(*this, mInput, mCube->mDescriptorIndex);
}

static void AddBarrier(std::vector<D3D12_RESOURCE_BARRIER>& barriers, Resource* resource, D3D12_RESOURCE_STATES newState)
{		
	if (resource->mCurrentState == newState)
		return;
	D3D12_RESOURCE_BARRIER barrier{
		.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION,
		.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE,
		.Transition = {
			.pResource = resource->mResource.Get(),
			.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
			.StateBefore = resource->mCurrentState,
			.StateAfter = newState} };

	resource->mCurrentState = newState;

	barriers.push_back(barrier);
}

void Device::Render()
{
	mGraphicsQueue->WaitForQueueCpuBlocking(mFenceValues[mFrameIndex]);
	mCommandAllocators[mFrameIndex]->Reset();
	mCommandList->Reset(mCommandAllocators[mFrameIndex].Get(), nullptr);
	
	ID3D12DescriptorHeap* heaps[] = { mSRVDescriptorHeap->GetHeap() };
	mCommandList->SetDescriptorHeaps(static_cast<uint32_t>(std::size(heaps)), heaps);

	// render cube front
	{
		/*std::vector<D3D12_RESOURCE_BARRIER> barriers;
		AddBarrier(barriers, mCubeFront.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		mCommandList->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());

		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		mCommandList->ClearRenderTargetView(mCubeFront->mRtvDescriptor.mCpuHandle, clearColor, 0, nullptr);
		mCommandList->ClearDepthStencilView(mDepthBuffer->mDsvDescriptor.mCpuHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);*/
	}

	TextureResource& currentBackbuffer = mBackBuffers[mSwapChain->GetCurrentBackBufferIndex()];

	std::vector<D3D12_RESOURCE_BARRIER> barriers;
	AddBarrier(barriers, &currentBackbuffer, D3D12_RESOURCE_STATE_RENDER_TARGET);
	AddBarrier(barriers, mDepthBuffer.get(), D3D12_RESOURCE_STATE_DEPTH_WRITE);
	mCommandList->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());

	float clearColor[4] = { 0.02f, 0.02f, 0.02f, 1.0f };
	mCommandList->ClearRenderTargetView(currentBackbuffer.mRtvDescriptor.mCpuHandle, clearColor, 0, nullptr);
	mCommandList->ClearDepthStencilView(mDepthBuffer->mDsvDescriptor.mCpuHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 0.0f, 0, 0, nullptr);

	D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[] = { currentBackbuffer.mRtvDescriptor.mCpuHandle };
	mCommandList->OMSetRenderTargets(static_cast<uint32_t>(std::size(renderTargets)), renderTargets, false, &mDepthBuffer->mDsvDescriptor.mCpuHandle);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
	mCommandList->SetPipelineState(mGraphicsPipeline.Get());
	mCommandList->SetGraphicsRootConstantBufferView(0, mCamera->mConstantBuffer->mResource->GetGPUVirtualAddress());

	D3D12_VIEWPORT viewPort{
		.TopLeftX = 0,
		.TopLeftY = 0,
		.Width = static_cast<float>(Window::GetWidth()),
		.Height = static_cast<float>(Window::GetHeight()),
		.MinDepth = 0.0f,
		.MaxDepth = 1.0f };

	D3D12_RECT scissor{
		.left = 0,
		.top = 0,
		.right = static_cast<LONG>(viewPort.Width),
		.bottom = static_cast<LONG>(viewPort.Height) };

	mCommandList->RSSetViewports(1, &viewPort);
	mCommandList->RSSetScissorRects(1, &scissor);
	mCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	mCommandList->DrawInstanced(36, 1, 0, 0);

	barriers.resize(0);
	AddBarrier(barriers, &currentBackbuffer, D3D12_RESOURCE_STATE_PRESENT);
	mCommandList->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());

	mGraphicsQueue->Submit(mCommandList.Get());
	
	mSwapChain->Present(0, 0);

	mFenceValues[mFrameIndex] = mGraphicsQueue->Signal();
	mFrameIndex = (mFrameIndex + 1) % FRAMES_IN_FLIGHT;
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