#include "GeometryPassCmdListRecorder.h"

#include <CommandManager/CommandManager.h>
#include <ShaderUtils\CBuffers.h>
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

GeometryPassCmdListRecorder::GeometryPassCmdListRecorder(ID3D12Device& device)
	: mDevice(device)
{
	BuildCommandObjects(mCmdList, mCmdAlloc, _countof(mCmdAlloc));
}

bool GeometryPassCmdListRecorder::ValidateData() const noexcept {
	for (std::uint32_t i = 0UL; i < Settings::sQueuedFrameCount; ++i) {
		if (mCmdAlloc[i] == nullptr) {
			return false;
		}
	}

	const std::size_t numGeomData{ mGeometryDataVec.size() };
	for (std::size_t i = 0UL; i < numGeomData; ++i) {
		const std::size_t numMatrices{ mGeometryDataVec[i].mWorldMatrices.size() };
		if (numMatrices == 0UL) {
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
		mObjectCBuffer != nullptr &&
		mObjectCBufferGpuDescHandleBegin.ptr != 0UL &&
		numGeomData != 0UL &&
		mMaterialsCBuffer != nullptr &&
		mMaterialsCBufferGpuDescHandleBegin.ptr != 0UL;
}

void GeometryPassCmdListRecorder::InitInternal(
	tbb::concurrent_queue<ID3D12CommandList*>& cmdListQueue,
	const D3D12_CPU_DESCRIPTOR_HANDLE* geometryBuffersCpuDescs,
	const std::uint32_t geometryBuffersCpuDescCount,
	const D3D12_CPU_DESCRIPTOR_HANDLE& depthBufferCpuDesc) noexcept
{
	ASSERT(geometryBuffersCpuDescs != nullptr);
	ASSERT(geometryBuffersCpuDescCount != 0U);
	ASSERT(depthBufferCpuDesc.ptr != 0UL);

	mCmdListQueue = &cmdListQueue;
	mGeometryBuffersCpuDescs = geometryBuffersCpuDescs;
	mGeometryBuffersCpuDescCount = geometryBuffersCpuDescCount;
	mDepthBufferCpuDesc = depthBufferCpuDesc;
}
