#include "ToneMappingPass.h"

#include <d3d12.h>
#include <DirectXColors.h>

#include <CommandListExecutor/CommandListExecutor.h>
#include <CommandManager\CommandManager.h>
#include <DXUtils/d3dx12.h>
#include <ModelManager\Mesh.h>
#include <ModelManager\Model.h>
#include <ModelManager\ModelManager.h>
#include <ResourceManager\ResourceManager.h>
#include <ShaderUtils\CBuffers.h>
#include <Utils\DebugUtils.h>

namespace {
	void CreateCommandObjects(
		ID3D12CommandAllocator* cmdAllocs[Settings::sQueuedFrameCount],
		ID3D12GraphicsCommandList* &cmdList,
		ID3D12Fence* &fence) noexcept {

		ASSERT(Settings::sQueuedFrameCount > 0U);
		ASSERT(cmdList == nullptr);
		ASSERT(fence == nullptr);

		// Create command allocators and command list
		for (std::uint32_t i = 0U; i < Settings::sQueuedFrameCount; ++i) {
			ASSERT(cmdAllocs[i] == nullptr);
			CommandManager::Get().CreateCmdAlloc(D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocs[i]);
		}
		CommandManager::Get().CreateCmdList(D3D12_COMMAND_LIST_TYPE_DIRECT, *cmdAllocs[0], cmdList);
		cmdList->Close();

		ResourceManager::Get().CreateFence(0U, D3D12_FENCE_FLAG_NONE, fence);
	}

	void ExecuteCommandList(
		ID3D12CommandQueue& cmdQueue, 
		ID3D12GraphicsCommandList& cmdList, 
		ID3D12Fence& fence) noexcept {

		cmdList.Close();

		ID3D12CommandList* cmdLists[1U]{ &cmdList };
		cmdQueue.ExecuteCommandLists(_countof(cmdLists), cmdLists);

		const std::uint64_t fenceValue = fence.GetCompletedValue() + 1UL;

		CHECK_HR(cmdQueue.Signal(&fence, fenceValue));

		// Wait until the GPU has completed commands up to this fence point.
		if (fence.GetCompletedValue() < fenceValue) {
			const HANDLE eventHandle{ CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS) };
			ASSERT(eventHandle);

			// Fire event when GPU hits current fence.  
			CHECK_HR(fence.SetEventOnCompletion(fenceValue, eventHandle));

			// Wait until the GPU hits current fence event is fired.
			WaitForSingleObject(eventHandle, INFINITE);
			CloseHandle(eventHandle);
		}
	}
}

void ToneMappingPass::Init(
	ID3D12Device& device,
	CommandListExecutor& cmdListExecutor,
	ID3D12CommandQueue& cmdQueue,
	ID3D12Resource& colorBuffer,
	const D3D12_CPU_DESCRIPTOR_HANDLE& depthBufferCpuDesc) noexcept {

	ASSERT(ValidateData() == false);
	
	CreateCommandObjects(mCmdAllocs, mCmdList, mFence);
	mCmdListExecutor = &cmdListExecutor;
	mCmdQueue = &cmdQueue;
	mColorBuffer = &colorBuffer;

	CHECK_HR(mCmdList->Reset(mCmdAllocs[0U], nullptr));
	
	// Create model for a full screen quad geometry. 
	Model* model;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadVertexBuffer;
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadIndexBuffer;
	ModelManager::Get().CreateFullscreenQuad(model, *mCmdList, uploadVertexBuffer, uploadIndexBuffer);
	ASSERT(model != nullptr);

	// Get vertex and index buffers data from the only mesh this model must have.
	ASSERT(model->Meshes().size() == 1UL);
	const Mesh& mesh = model->Meshes()[0U];
	ExecuteCommandList(cmdQueue, *mCmdList, *mFence);

	// Initialize recorder's PSO
	ToneMappingCmdListRecorder::InitPSO();

	// Initialize recorder
	mRecorder.reset(new ToneMappingCmdListRecorder(device, cmdListExecutor.CmdListQueue()));
	mRecorder->Init(mesh.VertexBufferData(), mesh.IndexBufferData(), colorBuffer, depthBufferCpuDesc);

	ASSERT(ValidateData());
}

void ToneMappingPass::Execute(
	ID3D12Resource& frameBuffer,
	const D3D12_CPU_DESCRIPTOR_HANDLE& frameBufferCpuDesc) noexcept {

	ASSERT(ValidateData());
	ASSERT(frameBufferCpuDesc.ptr != 0UL);

	ExecuteBeginTask(frameBuffer, frameBufferCpuDesc);

	mCmdListExecutor->ResetExecutedCmdListCount();
	mRecorder->RecordAndPushCommandLists(frameBufferCpuDesc);

	// Wait until all previous tasks command lists are executed
	while (mCmdListExecutor->ExecutedCmdListCount() < 1) {
		Sleep(0U);
	}
}

bool ToneMappingPass::ValidateData() const noexcept {
	for (std::uint32_t i = 0U; i < Settings::sQueuedFrameCount; ++i) {
		if (mCmdAllocs[i] == nullptr) {
			return false;
		}
	}

	const bool b =
		mCmdListExecutor != nullptr &&
		mCmdQueue != nullptr &&
		mCmdList != nullptr &&
		mFence != nullptr &&
		mRecorder.get() != nullptr &&
		mColorBuffer != nullptr;

	return b;
}

void ToneMappingPass::ExecuteBeginTask(
	ID3D12Resource& frameBuffer,
	const D3D12_CPU_DESCRIPTOR_HANDLE& frameBufferCpuDesc) noexcept {

	ASSERT(ValidateData());
	ASSERT(frameBufferCpuDesc.ptr != 0UL);

	// Used to choose a different command list allocator each call.
	static std::uint32_t cmdAllocIndex{ 0U };

	ID3D12CommandAllocator* cmdAlloc{ mCmdAllocs[cmdAllocIndex] };
	cmdAllocIndex = (cmdAllocIndex + 1U) % _countof(mCmdAllocs);

	CHECK_HR(cmdAlloc->Reset());
	CHECK_HR(mCmdList->Reset(cmdAlloc, nullptr));

	// Set barriers
	CD3DX12_RESOURCE_BARRIER barriers[]{
		CD3DX12_RESOURCE_BARRIER::Transition(mColorBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE),
		CD3DX12_RESOURCE_BARRIER::Transition(&frameBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET),
	};
	mCmdList->ResourceBarrier(_countof(barriers), barriers);

	// Clear render targets
	mCmdList->ClearRenderTargetView(frameBufferCpuDesc, DirectX::Colors::Black, 0U, nullptr);
	CHECK_HR(mCmdList->Close());

	// Execute preliminary task
	mCmdListExecutor->ResetExecutedCmdListCount();
	ID3D12CommandList* cmdLists[] = { mCmdList };
	mCmdQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
}
