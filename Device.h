#pragma once

#include "Types.h"

#include <memory>
#include <array>

namespace D3D12MA {
	class Allocator;
}

class DescriptorHeap;
class Queue;

constexpr uint32_t FRAMES_IN_FLIGHT = 2;
constexpr uint32_t NUM_BACK_BUFFERS = 3;

class Device {
public:
	Device();
	~Device();

private:
	void InitializeDevice();
	void InitializeDeviceResources();

	// TEMP
	void InitializePipeline();

private:
	ComPtr<IDXGIFactory2> mFactory = nullptr;
	ComPtr<ID3D12Device5> mDevice = nullptr;
	ComPtr<IDXGISwapChain3> mSwapChain = nullptr;
	ComPtr<D3D12MA::Allocator> mAllocator = nullptr;

	std::unique_ptr<DescriptorHeap> mSRVDescriptorHeap = nullptr;
	std::unique_ptr<DescriptorHeap> mRTVDescriptorHeap = nullptr;
	std::unique_ptr<Queue> mGraphicsQueue = nullptr;

	std::array<uint64_t, FRAMES_IN_FLIGHT> mFenceValues;
	std::array<TextureResource, NUM_BACK_BUFFERS> mBackBuffers;

	std::array<ComPtr<ID3D12CommandAllocator>, FRAMES_IN_FLIGHT> mCommandAllocators;
	ComPtr<ID3D12GraphicsCommandList5> mCommandList = nullptr;
};