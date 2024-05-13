#include "Camera.h"
#include "Application.h"
#include "Device.h"
#include "Window.h"

#include <iostream>
 
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

	mViewMatrix = mConstantBufferData.cameraMatrix;
}

void Camera::Update(Input& input, float deltaTime)
{
	if (mRmbIsPressed)
		UpdateViewMatrix(deltaTime);
}

void Camera::UpdateViewMatrix(float deltaTime)
{
	POINT cursorPos;
	GetCursorPos(&cursorPos);
	
	if (!mPrevDragState.inProgress)
	{
		mPrevDragState.x = cursorPos.x;
		mPrevDragState.y = cursorPos.y;
		mPrevDragState.inProgress = true;
		return;
	}

	float deltaX = (cursorPos.x - mPrevDragState.x) * deltaTime;
	float deltaY = (mPrevDragState.y - cursorPos.y) * deltaTime;

	DirectX::XMVECTOR qPitch = DirectX::XMQuaternionRotationAxis(
		DirectX::XMLoadFloat3(&mRight), DirectX::XMConvertToRadians(deltaY));
	DirectX::XMVECTOR qYaw = DirectX::XMQuaternionRotationAxis(
		DirectX::XMLoadFloat3(&mUp), DirectX::XMConvertToRadians(deltaX));

	DirectX::XMVECTOR orientation = DirectX::XMQuaternionMultiply(qPitch, qYaw);
	orientation = DirectX::XMQuaternionNormalize(orientation);

	DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationQuaternion(orientation);

	DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(mPosition.x, mPosition.y, mPosition.z);

	DirectX::XMStoreFloat4x4(&mViewMatrix, rotation * translation);
}

Camera::~Camera()
{
}