#include "LightingPassCmdListRecorder.h"

#include <CommandManager/CommandManager.h>
#include <Utils/DebugUtils.h>

namespace {
	void BuildCommandObjects(ID3D12GraphicsCommandList* &cmdList, ID3D12CommandAllocator* cmdAlloc[], const std::size_t cmdAllocCount) noexcept {
		ASSERT(cmdList == nullptr);

#ifdef _DEBUG
		for (std::uint32_t i = 0U; i < cmdAllocCount; ++i) {
			ASSERT(cmdAlloc[i] == nullptr);
		}
#endif

		for (std::uint32_t i = 0U; i < cmdAllocCount; ++i) {
			CommandManager::Get().CreateCmdAlloc(D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAlloc[i]);
		}

		CommandManager::Get().CreateCmdList(D3D12_COMMAND_LIST_TYPE_DIRECT, *cmdAlloc[0], cmdList);

		cmdList->Close();
	}
}

LightingPassCmdListRecorder::LightingPassCmdListRecorder(ID3D12Device& device)
	: mDevice(device)
{
	BuildCommandObjects(mCmdList, mCmdAlloc, _countof(mCmdAlloc));
}

bool LightingPassCmdListRecorder::ValidateData() const noexcept {
	for (std::uint32_t i = 0UL; i < Settings::sQueuedFrameCount; ++i) {
		if (mCmdAlloc[i] == nullptr) {
			return false;
		}
	}

	for (std::uint32_t i = 0UL; i < Settings::sQueuedFrameCount; ++i) {
		if (mFrameCBuffer[i] == nullptr) {
			return false;
		}
	}

	return
		mCmdList != nullptr &&
		mNumLights != 0UL &&
		mImmutableCBuffer != nullptr &&
		mLightsBuffer != nullptr &&
		mLightsBufferGpuDescHandleBegin.ptr != 0UL;
}

void LightingPassCmdListRecorder::InitInternal(
	tbb::concurrent_queue<ID3D12CommandList*>& cmdListQueue,
	const D3D12_CPU_DESCRIPTOR_HANDLE colorBufferCpuDesc,
	const D3D12_CPU_DESCRIPTOR_HANDLE& depthBufferCpuDesc) noexcept
{
	ASSERT(colorBufferCpuDesc.ptr != 0UL);
	ASSERT(depthBufferCpuDesc.ptr != 0UL);

	mCmdListQueue = &cmdListQueue;
	mColorBufferCpuDesc = colorBufferCpuDesc;
	mDepthBufferCpuDesc = depthBufferCpuDesc;
}