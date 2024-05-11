#pragma once

class Context;

class Queue {
public:
	Queue(ComPtr<ID3D12Device5> device, D3D12_COMMAND_LIST_TYPE type);

	ID3D12CommandQueue* GetQueue() { return mCommandQueue.Get(); }

	uint64_t Signal();
	void Submit(ID3D12CommandList* commandList);
	void WaitForQueueCpuBlocking(uint64_t fenceValue);
private:
	ComPtr<ID3D12CommandQueue> mCommandQueue = nullptr;
	ComPtr<ID3D12Fence1> mFence = nullptr;
	HANDLE mEvent = NULL;
	D3D12_COMMAND_LIST_TYPE mType;
	uint64_t mCompletedFenceValue = 0;
	uint64_t mCurrentFenceValue = 1;
};