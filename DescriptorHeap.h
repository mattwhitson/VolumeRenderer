#pragma once

#include "Types.h"

class DescriptorHeap {
public:
	DescriptorHeap(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool isShaderVisible);

	ID3D12DescriptorHeap* GetHeap() { return mDescriptorHeap.Get(); }

	Descriptor GetDescriptor();

private:
	ComPtr<ID3D12DescriptorHeap> mDescriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_TYPE mType;
	Descriptor mHeapStart;
	uint32_t mDescriptorCount;
	uint32_t mDescriptorHandleSize;
	uint32_t mCurrentHandle;
	bool mIsShaderVisible;
};