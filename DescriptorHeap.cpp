#include "DescriptorHeap.h"

#define DX_ASSERT(hr) { if FAILED(hr) assert(false);}

DescriptorHeap::DescriptorHeap(ComPtr<ID3D12Device> device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t count, bool isShaderVisible)
	: mType(type)
	, mDescriptorCount(count)
	, mIsShaderVisible(isShaderVisible)
	, mCurrentHandle(0)
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {
		.Type = mType,
		.NumDescriptors = mDescriptorCount,
		.Flags = mIsShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE };

	DX_ASSERT(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mDescriptorHeap)));

	mHeapStart.mCpuHandle = mDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	if (mIsShaderVisible)
	{
		mHeapStart.mGpuHandle = mDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	}

	mDescriptorHandleSize = device->GetDescriptorHandleIncrementSize(mType);
}

Descriptor DescriptorHeap::GetDescriptor()
{
	assert(mCurrentHandle < mDescriptorCount && "Ran out of descriptor handles in heap");

	uint32_t handleId = mCurrentHandle++;

	Descriptor descriptor{};
	descriptor.mHeapIndex = handleId;
	descriptor.mCpuHandle.ptr = mHeapStart.mCpuHandle.ptr + (static_cast<uint64_t>(handleId) * mDescriptorHandleSize);
	if (mIsShaderVisible)
	{
		descriptor.mGpuHandle.ptr = mHeapStart.mGpuHandle.ptr + (static_cast<uint64_t>(handleId) * mDescriptorHandleSize);
	}

	return descriptor;
}