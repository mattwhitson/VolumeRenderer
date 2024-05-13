#pragma once

#include "D3D12MemAlloc.h"

struct Descriptor {
	D3D12_CPU_DESCRIPTOR_HANDLE mCpuHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE mGpuHandle;
	uint32_t mHeapIndex = UINT_MAX;
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

	uint32_t mStride = 0;
	void* mMapped = nullptr;
};

struct TextureResource : public Resource {
	Descriptor mUavDescriptor{};
	Descriptor mSrvDescriptor{};
	Descriptor mDsvDescriptor{};
	Descriptor mRtvDescriptor{};
	DXGI_FORMAT mTextureFormat = DXGI_FORMAT_UNKNOWN;
};

enum DescriptorType : uint8_t {
	None = 0,
	Cbv = 1,
	Srv = 2,
	Uav = 4,
	Rtv = 8,
	Dsv = 16
};

inline DescriptorType operator|(DescriptorType a, DescriptorType b)
{
	return static_cast<DescriptorType>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

struct BufferDescription {
	DescriptorType bufferDescriptor = None;
	D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
	uint32_t size = 0;
	uint32_t count = 0;
	uint32_t stride = 0;
	bool isShaderVisible = false;
	bool isRaw = false;
};

struct TextureDescription {
	DescriptorType textureDescriptor = DescriptorType::None;
	D3D12_RESOURCE_DIMENSION dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	DXGI_FORMAT format = DXGI_FORMAT_R32_TYPELESS;
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
	D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE;
	uint32_t width = 1;
	uint32_t height = 1;
	uint16_t depthOrArraySize = 1;
	uint16_t mipLevels = 1;
};

namespace utils {
	static uint32_t AlignU32(uint32_t valueToAlign, uint32_t alignment)
	{
		alignment -= 1;
		return (uint32_t)((valueToAlign + alignment) & ~alignment);
	}
}

struct PerFrameConstantBuffer {
	DirectX::XMFLOAT4X4 projectionMatrix{};
	DirectX::XMFLOAT4X4 cameraMatrix{};
	DirectX::XMFLOAT4X4 worldMatrix{};
	uint32_t mCubeDescriptorIndex = UINT_MAX;
};