#pragma once

#include "Types.h"
#include "Application.h"/*temp*/

#include <memory>
#include <array>

namespace D3D12MA {
	class Allocator;
}

class DescriptorHeap;
class Queue;
class Camera;/*TEMP*/
struct Input;

constexpr uint32_t FRAMES_IN_FLIGHT = 2;
constexpr uint32_t NUM_BACK_BUFFERS = 3;

class Device {
public:
	Device(Input& input);
	~Device();

	std::unique_ptr<BufferResource> CreateBuffer(BufferDescription& desc, void* data = nullptr);
	std::unique_ptr<TextureResource> CreateTexture(TextureDescription& desc);
	// TEMP
	void Render();
private:
	void InitializeDevice();
	void InitializeDeviceResources();

	// TEMP
	void InitializePipeline();

private:
	uint32_t mFrameIndex = 0;
	ComPtr<IDXGIFactory2> mFactory = nullptr;
	ComPtr<ID3D12Device5> mDevice = nullptr;
	ComPtr<IDXGISwapChain3> mSwapChain = nullptr;
	ComPtr<D3D12MA::Allocator> mAllocator = nullptr;

	std::unique_ptr<DescriptorHeap> mSRVDescriptorHeap = nullptr;
	std::unique_ptr<DescriptorHeap> mRTVDescriptorHeap = nullptr;
	std::unique_ptr<DescriptorHeap> mDSVDescriptorHeap = nullptr;
	std::unique_ptr<Queue> mGraphicsQueue = nullptr;

	std::array<uint64_t, FRAMES_IN_FLIGHT> mFenceValues;
	std::array<TextureResource, NUM_BACK_BUFFERS> mBackBuffers;

	std::array<ComPtr<ID3D12CommandAllocator>, FRAMES_IN_FLIGHT> mCommandAllocators;
	ComPtr<ID3D12GraphicsCommandList5> mCommandList = nullptr;

	ComPtr<ID3D12PipelineState> mGraphicsPipeline = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	std::unique_ptr<TextureResource> mDepthBuffer = nullptr;
	std::unique_ptr<BufferResource> mCube = nullptr;

	std::unique_ptr<TextureResource> mCubeFront = nullptr;
	std::unique_ptr<TextureResource> mCubeBack = nullptr;

	ComPtr<ID3D12PipelineState> mCullFrontFacePipeline = nullptr;
	std::unique_ptr<Camera> mCamera = nullptr;
	Input& mInput;
};