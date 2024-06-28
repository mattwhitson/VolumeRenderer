#include "Camera.h"
#include "Application.h"
#include "Device.h"
#include "Window.h"

#include <iostream>
 
Camera::Camera(Device& device, Input& input)
	: mInput(input)
	, mDevice(device)
	, mPosition(0, 0, -5)
	, mPitch(0)
	, mYaw(0)
	, mForward(0, 0, 1)
	, mRight(1, 0, 0)
	, mUp(0, 1, 0)
{
	BufferDescription desc{
			.bufferDescriptor = DescriptorType::Cbv,
			.heapType = D3D12_HEAP_TYPE_UPLOAD,
			.size = sizeof(CameraConstantBuffer),
			.count = 1 };

	mConstantBuffer = device.CreateBuffer(desc);

	mConstantBuffer->mResource->Map(0, nullptr, &mConstantBuffer->mMapped);

	float aspectRatio = static_cast<float>(Window::GetWidth()) / Window::GetHeight();

	DirectX::XMStoreFloat4x4(&mConstantBufferData.projectionMatrix,
		DirectX::XMMatrixTranspose(
			DirectX::XMMatrixPerspectiveFovLH(3.14159f / 4.0f, aspectRatio, 0.01f, 100.f)));

	memcpy(mConstantBuffer->mMapped, &mConstantBufferData, sizeof(CameraConstantBuffer));
	UpdateViewMatrix();
}

void Camera::Update(Input& input, float deltaTime)
{
	if (mRmbIsPressed)
		CalculateMouseDelta(deltaTime);
	UpdatePosition(input, deltaTime);
	UpdateViewMatrix();
}

void Camera::UpdatePosition(Input& input, float deltaTime)
{
	using namespace DirectX;
	if (input.keys['w' - 'a'])
	{
		XMStoreFloat3(&mPosition, XMLoadFloat3(&mPosition) + (XMLoadFloat3(&mForward) * deltaTime * 10.0f));
	}
	if (input.keys['s' - 'a'])
	{
		XMStoreFloat3(&mPosition, XMLoadFloat3(&mPosition) - (XMLoadFloat3(&mForward) * deltaTime * 10.0f));
	}
	if (input.keys['d' - 'a'])
	{
		XMStoreFloat3(&mPosition, XMLoadFloat3(&mPosition) + (XMLoadFloat3(&mRight) * deltaTime * 10.0f));
	}
	if (input.keys['a' - 'a'])
	{
		XMStoreFloat3(&mPosition, XMLoadFloat3(&mPosition) - (XMLoadFloat3(&mRight) * deltaTime * 10.0f));
	}
}

void Camera::CalculateMouseDelta(float deltaTime)
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

	float deltaX = (cursorPos.x - mPrevDragState.x) * deltaTime * 100.0f;
	float deltaY = (mPrevDragState.y - cursorPos.y) * deltaTime * 100.0f;

	mYaw += deltaX;
	mPitch += deltaY;

	mPitch = std::min(90.0f - 0.5f, std::max(mPitch, -90.0f + 0.5f));

	mPrevDragState.x = cursorPos.x;
	mPrevDragState.y = cursorPos.y;
	mPrevDragState.deltaX = deltaX;
	mPrevDragState.deltaY = deltaY;
}

void Camera::UpdateViewMatrix()
{
	DirectX::XMVECTOR qPitch = DirectX::XMQuaternionRotationAxis(DirectX::XMVECTOR{ 1, 0, 0 }, DirectX::XMConvertToRadians(mPitch));
	DirectX::XMVECTOR qYaw = DirectX::XMQuaternionRotationAxis(DirectX::XMVECTOR{ 0, 1, 0 }, DirectX::XMConvertToRadians(-mYaw));

	DirectX::XMVECTOR orientation = DirectX::XMQuaternionNormalize(DirectX::XMQuaternionMultiply(qYaw, qPitch));
	DirectX::XMMATRIX rotation = DirectX::XMMatrixRotationQuaternion(orientation);

	DirectX::XMMATRIX translation = DirectX::XMMatrixTranslation(-mPosition.x, -mPosition.y, -mPosition.z);

	// can easily be changed to an orbit camera with mPosition.xy being the focal point
	// and mPosition.z being the distanceToFocalPoint by reversing translation * rotation
	// A^T * B^T = (BA)^T
	DirectX::XMStoreFloat4x4(&mViewMatrix, DirectX::XMMatrixTranspose(translation * rotation));
	mConstantBufferData.cameraMatrix = mViewMatrix;
	rotation = DirectX::XMMatrixTranspose(rotation);

	DirectX::XMStoreFloat3(&mRight, rotation.r[0]);
	DirectX::XMStoreFloat3(&mUp, rotation.r[1]);
	DirectX::XMStoreFloat3(&mForward, rotation.r[2]);

	memcpy(static_cast<char*>(mConstantBuffer->mMapped) + offsetof(CameraConstantBuffer, cameraMatrix),
		&mConstantBufferData.cameraMatrix, sizeof(DirectX::XMFLOAT4X4));
}

Camera::~Camera()
{
}