#pragma once

struct Descriptor {
	D3D12_CPU_DESCRIPTOR_HANDLE mCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mGpuHandle;
	uint32_t mHeapIndex = -1u;
};

struct Resource {
	ComPtr<ID3D12Resource> mResource = nullptr;
	D3D12_RESOURCE_STATES mCurrentState = D3D12_RESOURCE_STATE_COMMON;
	uint32_t mDescriptorIndex = 0;
};

struct TextureResource : public Resource {
	Descriptor mUavDescriptor{};
	Descriptor mSrvDescriptor{};
	Descriptor mDsvDescriptor{};
	Descriptor mRtvDescriptor{};
};