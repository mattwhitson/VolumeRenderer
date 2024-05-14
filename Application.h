#pragma once

#include "Types.h"

#include <array>
#include <memory>
#include <vector>

class Device;
class Camera;
struct TextureResource;
struct BufferResource;

struct Input {
	Input()
	{
		
	}

	std::array<bool, 26> keys{};
};

class Application {
public:
	Application();
	~Application();

	void Initialize();

	Camera& GetCamera() { return *mCamera.get(); }

	void Render();
	void Update();
private:
	void InitializePipelines();
	void LoadVolumeData();

public:
	bool mIsInitialized = false;
	Input mInput;

private:
	std::unique_ptr<Device> mDevice = nullptr;

	ComPtr<ID3D12PipelineState> mGraphicsPipeline = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	std::unique_ptr<TextureResource> mDepthBuffer = nullptr;
	std::unique_ptr<BufferResource> mCube = nullptr;

	std::unique_ptr<TextureResource> mCubeFront = nullptr;
	std::unique_ptr<TextureResource> mCubeBack = nullptr;

	ComPtr<ID3D12PipelineState> mCullFrontFacePipeline = nullptr;
	ComPtr<ID3D12PipelineState> mCullBackFacePipeline = nullptr;
	std::unique_ptr<Camera> mCamera = nullptr;

	std::vector<float> mVolumeData;
	std::unique_ptr<TextureResource> mVolumeTexture = nullptr;

	PerFrameConstantBuffer mPerFrameConstantBufferData{};
	std::unique_ptr<BufferResource> mPerFrameConstantBuffer = nullptr;
};