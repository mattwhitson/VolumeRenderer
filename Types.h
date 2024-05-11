#pragma once

namespace D3D12MA {
	class Allocation;
}

struct Descriptor {
	D3D12_CPU_DESCRIPTOR_HANDLE mCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mGpuHandle;
	uint32_t mHeapIndex = -1u;
};

struct Resource {
	ComPtr<ID3D12Resource> mResource = nullptr;
	ComPtr<D3D12MA::Allocation> mAllocation = nullptr;
	D3D12_RESOURCE_STATES mCurrentState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_DESC mDesc{};
	uint32_t mDescriptorIndex = 0;
};

struct BufferResource : public Resource {
	Descriptor mUavDescriptor{};
	Descriptor mSrvDescriptor{};
	Descriptor mCbvDescriptor{};
};

struct TextureResource : public Resource {
	Descriptor mUavDescriptor{};
	Descriptor mSrvDescriptor{};
	Descriptor mDsvDescriptor{};
	Descriptor mRtvDescriptor{};
	DXGI_FORMAT mTextureFormat = DXGI_FORMAT_UNKNOWN;
};