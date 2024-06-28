#include "Application.h"
#include "Device.h"
#include "Window.h"
#include "Camera.h"

#include "D3D12MemAlloc.h"

#include <vector>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <array>

#define DX_ASSERT(hr) { if FAILED(hr) assert(false);}

Application::Application()
	: mInput(Input())
{
}

Application::~Application()
{
	mDevice->WaitForIdle();
}

void Application::Initialize()
{
	mDevice = std::make_unique<Device>();

	TextureDescription depthBufferDesc{
		.textureDescriptor = DescriptorType::Dsv,
		.format = DXGI_FORMAT_D32_FLOAT,
		.initialState = D3D12_RESOURCE_STATE_DEPTH_WRITE,
		.flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
		.width = Window::GetWidth(),
		.height = Window::GetHeight() };

	mDepthBuffer = mDevice->CreateTexture(depthBufferDesc);

	TextureDescription cubeRenderDesc{
		.textureDescriptor = DescriptorType::Rtv | DescriptorType::Srv,
		.format = DXGI_FORMAT_R8G8B8A8_UNORM,
		.initialState = D3D12_RESOURCE_STATE_RENDER_TARGET,
		.flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
		.width = Window::GetWidth(),
		.height = Window::GetHeight()
	};

	mCubeFront = mDevice->CreateTexture(cubeRenderDesc);
	mCubeBack = mDevice->CreateTexture(cubeRenderDesc);


	InitializePipelines();
	LoadVolumeData();

	BufferDescription bufferDesc{
		.bufferDescriptor = DescriptorType::Cbv,
		.heapType = D3D12_HEAP_TYPE_UPLOAD,
		.size = sizeof(PerFrameConstantBuffer),
		.count = 1 };
	mPerFrameConstantBuffer = mDevice->CreateBuffer(bufferDesc);
	mPerFrameConstantBuffer->mResource->Map(0, nullptr, &mPerFrameConstantBuffer->mMapped);

	mPerFrameConstantBufferData = {
		.cameraDimensions = DirectX::XMFLOAT2(Window::GetWidth(), Window::GetHeight()),
		.frontDescriptorIndex = mCubeFront->mDescriptorIndex,
		.backDescriptorIndex = mCubeBack->mDescriptorIndex,
		.cubeDescriptorIndex = mCube->mDescriptorIndex,
		.volumeDataDescriptor = mVolumeTexture->mDescriptorIndex
		};
	DirectX::XMStoreFloat4x4(&mPerFrameConstantBufferData.modelMatrix,
		DirectX::XMMatrixIdentity());

	memcpy(mPerFrameConstantBuffer->mMapped, &mPerFrameConstantBufferData, sizeof(PerFrameConstantBuffer));

	mCamera = std::make_unique<Camera>(*mDevice.get(), mInput);

	mIsInitialized = true;
}

void Application::LoadVolumeData()
{
	std::vector<uint8_t> data = utils::LoadFileIntoVector<uint8_t>(std::filesystem::path(RESOURCE_DIR "/foot_256x256x256_uint8.raw"));

	//mVolumeData.resize(d.size(), 0.0f);
	//for (uint32_t i = 0; i < mVolumeData.size(); i++)
	//{
	//	mVolumeData[i] = (static_cast<float>(d[i]) / UINT8_MAX);
	//}

	TextureDescription desc{
		.textureDescriptor = DescriptorType::Srv,
		.dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D,
		.format = DXGI_FORMAT_R8_UNORM,
		.initialState = D3D12_RESOURCE_STATE_COPY_DEST,
		.width = 256,
		.height = 256,
		.depthOrArraySize = 256};
	mVolumeTexture = mDevice->CreateTexture(desc);

	mDevice->ImmediateUploadToGpu(mVolumeTexture.get(), data.data());
}

void Application::InitializePipelines()
{
	D3D12_ROOT_PARAMETER1 params[] = {
		{.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
		 .Descriptor = {.ShaderRegister = 0, .RegisterSpace = 0},
		 .ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL},
		{.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV,
		 .Descriptor = {.ShaderRegister = 0, .RegisterSpace = 1},
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
	DX_ASSERT(mDevice->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(),
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

	const D3D12_RENDER_TARGET_BLEND_DESC modifiedRenderTargetBlendDesc =
	{
		TRUE,FALSE,
		D3D12_BLEND_SRC_ALPHA, D3D12_BLEND_ONE, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};

	for (uint32_t i = 0; i < NUM_BACK_BUFFERS; i++)
	{
		pipelineDesc.BlendState.RenderTarget[i] = modifiedRenderTargetBlendDesc;
		pipelineDesc.RTVFormats[i] = mDevice->GetBackbuffer(i).mTextureFormat;
	}

	DX_ASSERT(mDevice->GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&mGraphicsPipeline)));

	{
		ComPtr<ID3DBlob> vertBlob;
		ComPtr<ID3DBlob> pixBlob;
		DX_ASSERT(D3DReadFileToBlob(L"VolumeBoundsVertex.cso", &vertBlob));
		DX_ASSERT(D3DReadFileToBlob(L"VolumeBoundsPixel.cso", &pixBlob));

		pipelineDesc.VS = {
			.pShaderBytecode = vertBlob->GetBufferPointer(),
			.BytecodeLength = vertBlob->GetBufferSize() };
		pipelineDesc.PS = {
				.pShaderBytecode = pixBlob->GetBufferPointer(),
				.BytecodeLength = pixBlob->GetBufferSize() };
		pipelineDesc.DepthStencilState = {
			.DepthEnable = FALSE,
			.StencilEnable = FALSE };

		for (uint32_t i = 0; i < NUM_BACK_BUFFERS; i++)
		{
			pipelineDesc.RTVFormats[i] = DXGI_FORMAT_R8G8B8A8_UNORM;
		}

		DX_ASSERT(mDevice->GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&mCullBackFacePipeline)));

		pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		DX_ASSERT(mDevice->GetDevice()->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&mCullFrontFacePipeline)));
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
		mCube = mDevice->CreateBuffer(desc, meshVertices);

		void* data;
		mCube->mResource->Map(0, nullptr, &data);
		for (uint32_t i = 0; i < 36; i++)
		{
			memcpy(data, &meshVertices[i], sizeof(DirectX::XMFLOAT3));
			data = static_cast<char*>(data) + sizeof(DirectX::XMFLOAT3);
		}
		mCube->mResource->Unmap(0, nullptr);
	}
}

// will be put into Context class when I get around to making it !!!
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

static std::chrono::high_resolution_clock::time_point prev = std::chrono::steady_clock::now();

void Application::Update()
{
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	float deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(now - prev).count() / 1000000.0f;
	prev = now;

	mCamera->Update(mInput, deltaTime);
}

void Application::Render()
{
	mDevice->BeginFrame();
	ComPtr<ID3D12GraphicsCommandList5> mCommandList = mDevice->mCommandList;
	ID3D12DescriptorHeap* heaps[] = { mDevice->GetSrvHeap(), mDevice->GetSamplerHeap() };
	mCommandList->SetDescriptorHeaps(static_cast<uint32_t>(std::size(heaps)), heaps);

	{
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		AddBarrier(barriers, mCubeFront.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		AddBarrier(barriers, mCubeBack.get(), D3D12_RESOURCE_STATE_RENDER_TARGET);
		if (!barriers.empty()) 
			mCommandList->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());
	}

	 //render cube front
	{
		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		mCommandList->ClearRenderTargetView(mCubeFront->mRtvDescriptor.mCpuHandle, clearColor, 0, nullptr);

		D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[] = { mCubeFront->mRtvDescriptor.mCpuHandle };
		mCommandList->OMSetRenderTargets(static_cast<uint32_t>(std::size(renderTargets)), renderTargets, false, nullptr);

		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
		mCommandList->SetPipelineState(mCullBackFacePipeline.Get());
		mCommandList->SetGraphicsRootConstantBufferView(0, mCamera->mConstantBuffer->mResource->GetGPUVirtualAddress());
		mCommandList->SetGraphicsRootConstantBufferView(1, mPerFrameConstantBuffer->mResource->GetGPUVirtualAddress());

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
	}
	// render cube back
	{

		float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		mCommandList->ClearRenderTargetView(mCubeBack->mRtvDescriptor.mCpuHandle, clearColor, 0, nullptr);

		D3D12_CPU_DESCRIPTOR_HANDLE renderTargets[] = { mCubeBack->mRtvDescriptor.mCpuHandle };
		mCommandList->OMSetRenderTargets(static_cast<uint32_t>(std::size(renderTargets)), renderTargets, false, nullptr);

		mCommandList->SetGraphicsRootSignature(mRootSignature.Get());
		mCommandList->SetPipelineState(mCullFrontFacePipeline.Get());
		mCommandList->SetGraphicsRootConstantBufferView(0, mCamera->mConstantBuffer->mResource->GetGPUVirtualAddress());
		mCommandList->SetGraphicsRootConstantBufferView(1, mPerFrameConstantBuffer->mResource->GetGPUVirtualAddress());

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
	}
	// barriers
	{
		std::vector<D3D12_RESOURCE_BARRIER> barriers;
		AddBarrier(barriers, mCubeFront.get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		AddBarrier(barriers, mCubeBack.get(), D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		if (!barriers.empty())
			mCommandList->ResourceBarrier(static_cast<uint32_t>(barriers.size()), barriers.data());
	}

	TextureResource& currentBackbuffer = mDevice->GetCurrentBackbuffer();

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
	mCommandList->SetGraphicsRootConstantBufferView(1, mPerFrameConstantBuffer->mResource->GetGPUVirtualAddress());

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

	mDevice->EndFrame();
}