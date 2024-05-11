#include "Queue.h"

#define DX_ASSERT(hr) { if FAILED(hr) assert(false);}

Queue::Queue(ComPtr<ID3D12Device5> device, D3D12_COMMAND_LIST_TYPE type)
	: mType(type)
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {
		.Type = mType };

	DX_ASSERT(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&mCommandQueue)));
	DX_ASSERT(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&mFence)));

	mEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
	if (mEvent == nullptr)
	{
		DX_ASSERT(HRESULT_FROM_WIN32(GetLastError()));
	}
}

uint64_t Queue::Signal()
{
	mCommandQueue->Signal(mFence.Get(), mCurrentFenceValue);
	return mCurrentFenceValue++;
}

void Queue::WaitForQueueCpuBlocking(uint64_t fenceValue)
{
	while (mFence->GetCompletedValue() < fenceValue)
	{
		DX_ASSERT(mFence->SetEventOnCompletion(fenceValue, mEvent));
		WaitForSingleObject(mEvent, INFINITE); // probably shouldn't be infinite, but it's whatever for now
	}
	mCompletedFenceValue = mFence->GetCompletedValue();
}

void Queue::Submit(ID3D12CommandList* commandList)
{
	static_cast<ID3D12GraphicsCommandList*>(commandList)->Close();
	mCommandQueue->ExecuteCommandLists(1, &commandList);

}