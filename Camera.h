#pragma once

#include <memory>

#include "Types.h"

class Device;
struct Input;

class Camera {

	struct PrevDragState {
		float x = -1;
		float y = -1;
		bool inProgress = false;
	};
public: 
	Camera(Device& device, Input& input, uint32_t descriptorIndex /*TEMP*/);
	~Camera();

	void Update(Input& input, float deltaTime);

private:
	void UpdateViewMatrix(float deltaTime);

public:
	std::unique_ptr<BufferResource> mConstantBuffer;
	PrevDragState mPrevDragState{};
	bool mRmbIsPressed = false;

private:
	Device& mDevice;
	Input& mInput;
	PerFrameConstantBuffer mConstantBufferData;

	DirectX::XMFLOAT4X4 mViewMatrix{};
	DirectX::XMFLOAT3 mPosition;
	DirectX::XMFLOAT3 mFront;
	DirectX::XMFLOAT3 mRight;
	DirectX::XMFLOAT3 mUp;

};