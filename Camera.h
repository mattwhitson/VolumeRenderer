#pragma once

#include <memory>

#include "Types.h"

class Device;
struct Input;

class Camera {

	struct PrevDragState {
		float x = 0.0f;
		float y = 0.0f;
		float deltaX = 0.0f;
		float deltaY = 0.0f;
		bool inProgress = false;
	};
public: 
	Camera(Device& device, Input& input, uint32_t descriptorIndex /*TEMP*/);
	~Camera();

	void Update(Input& input, float deltaTime);

private:
	void UpdateViewMatrix();
	void UpdatePosition(Input& input, float deltaTime);
	void CalculateMouseDelta(float deltaTime);

public:
	std::unique_ptr<BufferResource> mConstantBuffer;
	PrevDragState mPrevDragState{};
	bool mRmbIsPressed = false;

private:
	Device& mDevice;
	Input& mInput;
	CameraConstantBuffer mConstantBufferData;

	DirectX::XMFLOAT4X4 mViewMatrix{};
	DirectX::XMFLOAT3 mPosition{};
	DirectX::XMFLOAT3 mRight{};
	DirectX::XMFLOAT3 mUp{};
	DirectX::XMFLOAT3 mForward{};

	float mPitch;
	float mYaw;
};