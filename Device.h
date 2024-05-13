#pragma once

#include "Types.h"
#include "DescriptorHeap.h"

#include <memory>
#include <array>

namespace D3D12MA {
	class Allocator;
}

class Queue;
class Camera;/*TEMP*/
struct Input;

constexpr uint32_t FRAMES_IN_FLIGHT = 2;
constexpr uint32_t NUM_BACK_BUFFERS = 3;

class Device {
public:
	Device();
	~Device();

	ID3D12Device5* GetDevice() { return mDevice.Get(); }
	TextureResource& GetBackbuffer(uint32_t index) { return mBackBuffers[index]; }
	TextureResource& GetCurrentBackbuffer() { return mBackBuffers[mSwapChain->GetCurrentBackBufferIndex()]; }
	ID3D12DescriptorHeap* GetSrvHeap() { return mSRVDescriptorHeap->GetHeap(); }
	void WaitForIdle();

	std::unique_ptr<BufferResource> CreateBuffer(BufferDescription& desc, void* data = nullptr);
	std::unique_ptr<TextureResource> CreateTexture(TextureDescription& desc);
	
	void BeginFrame();
	void EndFrame();

private:
	void InitializeDevice();
	void InitializeDeviceResources();


public:
	std::array<ComPtr<ID3D12CommandAllocator>, FRAMES_IN_FLIGHT> mCommandAllocators;
	ComPtr<ID3D12GraphicsCommandList5> mCommandList = nullptr;

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

};