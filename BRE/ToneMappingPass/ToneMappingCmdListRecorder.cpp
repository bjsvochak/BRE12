#include "ToneMappingCmdListRecorder.h"

#include <DirectXMath.h>

#include <CommandManager/CommandManager.h>
#include <DescriptorManager\DescriptorManager.h>
#include <PSOCreator/PSOCreator.h>
#include <Utils/DebugUtils.h>

// Root Signature:
// "DescriptorTable(SRV(t0), visibility = SHADER_VISIBILITY_PIXEL)" 0 -> Color Buffer Texture

namespace {
	ID3D12PipelineState* sPSO{ nullptr };
	ID3D12RootSignature* sRootSign{ nullptr };

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

		// Start off in a closed state.  This is because the first time we refer 
		// to the command list we will Reset it, and it needs to be closed before
		// calling Reset.
		cmdList->Close();
	}
}

ToneMappingCmdListRecorder::ToneMappingCmdListRecorder(ID3D12Device& device, tbb::concurrent_queue<ID3D12CommandList*>& cmdListQueue)
	: mDevice(device)
	, mCmdListQueue(cmdListQueue)
{
	BuildCommandObjects(mCmdList, mCmdAlloc, _countof(mCmdAlloc));
}

void ToneMappingCmdListRecorder::InitPSO() noexcept {
	ASSERT(sPSO == nullptr);
	ASSERT(sRootSign == nullptr);

	// Build pso and root signature
	PSOCreator::PSOParams psoParams{};
	const std::size_t rtCount{ _countof(psoParams.mRtFormats) };
	psoParams.mDepthStencilDesc = D3DFactory::DisableDepthStencilDesc();
	psoParams.mInputLayout = D3DFactory::PosNormalTangentTexCoordInputLayout();
	psoParams.mPSFilename = "ToneMappingPass/Shaders/PS.cso";
	psoParams.mRootSignFilename = "ToneMappingPass/Shaders/RS.cso";
	psoParams.mVSFilename = "ToneMappingPass/Shaders/VS.cso";
	psoParams.mNumRenderTargets = 1U;
	psoParams.mRtFormats[0U] = Settings::sFrameBufferRTFormat;
	for (std::size_t i = psoParams.mNumRenderTargets; i < rtCount; ++i) {
		psoParams.mRtFormats[i] = DXGI_FORMAT_UNKNOWN;
	}
	psoParams.mTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	PSOCreator::CreatePSO(psoParams, sPSO, sRootSign);

	ASSERT(sPSO != nullptr);
	ASSERT(sRootSign != nullptr);
}

void ToneMappingCmdListRecorder::Init(
	const BufferCreator::VertexBufferData& vertexBufferData,
	const BufferCreator::IndexBufferData indexBufferData,
	ID3D12Resource& colorBuffer,
	const D3D12_CPU_DESCRIPTOR_HANDLE& depthBufferCpuDesc) noexcept
{
	ASSERT(ValidateData() == false);

	mVertexBufferData = vertexBufferData;
	mIndexBufferData = indexBufferData;
	mDepthBufferCpuDesc = depthBufferCpuDesc;

	BuildBuffers(colorBuffer);

	ASSERT(ValidateData());
}

void ToneMappingCmdListRecorder::RecordAndPushCommandLists(const D3D12_CPU_DESCRIPTOR_HANDLE& frameBufferCpuDesc) noexcept {

	ASSERT(ValidateData());
	ASSERT(sPSO != nullptr);
	ASSERT(sRootSign != nullptr);

	ID3D12CommandAllocator* cmdAlloc{ mCmdAlloc[mCurrFrameIndex] };
	ASSERT(cmdAlloc != nullptr);
	
	CHECK_HR(cmdAlloc->Reset());
	CHECK_HR(mCmdList->Reset(cmdAlloc, sPSO));

	mCmdList->RSSetViewports(1U, &Settings::sScreenViewport);
	mCmdList->RSSetScissorRects(1U, &Settings::sScissorRect);
	mCmdList->OMSetRenderTargets(1U, &frameBufferCpuDesc, false, &mDepthBufferCpuDesc);

	ID3D12DescriptorHeap* heaps[] = { &DescriptorManager::Get().GetCbvSrcUavDescriptorHeap() };
	mCmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	mCmdList->SetGraphicsRootSignature(sRootSign);
	
	mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Draw object
	mCmdList->IASetVertexBuffers(0U, 1U, &mVertexBufferData.mBufferView);
	mCmdList->IASetIndexBuffer(&mIndexBufferData.mBufferView);
	mCmdList->SetGraphicsRootDescriptorTable(0U, mColorBufferGpuDescHandle);
	mCmdList->DrawIndexedInstanced(mIndexBufferData.mCount, 1U, 0U, 0U, 0U);

	mCmdList->Close();

	mCmdListQueue.push(mCmdList);

	// Next frame
	mCurrFrameIndex = (mCurrFrameIndex + 1) % Settings::sQueuedFrameCount;
}

bool ToneMappingCmdListRecorder::ValidateData() const noexcept {

	for (std::uint32_t i = 0UL; i < Settings::sQueuedFrameCount; ++i) {
		if (mCmdAlloc[i] == nullptr) {
			return false;
		}
	}

	const bool result =
		mCmdList != nullptr &&
		mDepthBufferCpuDesc.ptr != 0UL &&
		mColorBufferGpuDescHandle.ptr != 0UL;

	return result;
}

void ToneMappingCmdListRecorder::BuildBuffers(ID3D12Resource& colorBuffer) noexcept {
	// Create color buffer texture descriptor
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = colorBuffer.GetDesc().Format;
	srvDesc.Texture2D.MipLevels = colorBuffer.GetDesc().MipLevels;
	mColorBufferGpuDescHandle = DescriptorManager::Get().CreateShaderResourceView(colorBuffer, srvDesc);
}