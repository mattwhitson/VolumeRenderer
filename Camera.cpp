#include "Camera.h"
#include "Application.h"
#include "Device.h"
#include "Window.h"

#include "D3D12MemAlloc.h"
 
Camera::Camera(Device& device, Input& input, uint32_t descriptorIndex)
	: mInput(input)
	, mDevice(device)
	, mPosition(0, 0, -10)
	, mFront(0, 0, 1)
	, mRight(1, 0, 0)
	, mUp(0, 1, 0)
{
	BufferDescription desc{
			.bufferDescriptor = DescriptorType::Cbv,
			.heapType = D3D12_HEAP_TYPE_UPLOAD,
			.size = sizeof(PerFrameConstantBuffer),
			.count = 1 };

	mConstantBuffer = device.CreateBuffer(desc);

	void* data;
	mConstantBuffer->mResource->Map(0, nullptr, &data);

	float aspectRatio = static_cast<float>(Window::GetWidth()) / Window::GetHeight();

	DirectX::XMStoreFloat4x4(&mConstantBufferData.projectionMatrix,
		DirectX::XMMatrixTranspose(
			DirectX::XMMatrixPerspectiveFovLH(3.14159f / 4.0f, aspectRatio, 0.01f, 100.f)));
	DirectX::XMStoreFloat4x4(&mConstantBufferData.cameraMatrix,	
		DirectX::XMMatrixTranspose(
			DirectX::XMMatrixLookAtLH(DirectX::XMLoadFloat3(&mPosition), { 0, 0, 0 }, DirectX::XMLoadFloat3(&mUp))));
	DirectX::XMStoreFloat4x4(&mConstantBufferData.worldMatrix, 
		DirectX::XMMatrixTranspose(DirectX::XMMatrixIdentity()));
	mConstantBufferData.mCubeDescriptorIndex = descriptorIndex;

	memcpy(data, &mConstantBufferData, sizeof(PerFrameConstantBuffer));
}

Camera::~Camera()
{
}