#pragma once

#include <memory>

#include "Types.h"

class Device;
struct Input;

class Camera {
public: 
	Camera(Device& device, Input& input, uint32_t descriptorIndex /*TEMP*/);
	~Camera();

	std::unique_ptr<BufferResource> mConstantBuffer;

private:
	Device& mDevice;
	Input& mInput;
	PerFrameConstantBuffer mConstantBufferData;

	DirectX::XMFLOAT3 mPosition;
	DirectX::XMFLOAT3 mFront;
	DirectX::XMFLOAT3 mRight;
	DirectX::XMFLOAT3 mUp;

	bool mRmbIsPressed = false;
};