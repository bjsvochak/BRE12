#include "EnvironmentLightCmdListRecorder.h"

#include <DirectXMath.h>

#include <CommandManager/CommandManager.h>
#include <DescriptorManager\DescriptorManager.h>
#include <PSOCreator/PSOCreator.h>
#include <ResourceManager/ResourceManager.h>
#include <ShaderUtils\CBuffers.h>
#include <Utils/DebugUtils.h>

// Root Signature:
// "CBV(b0, visibility = SHADER_VISIBILITY_VERTEX), " \ 0 -> Frame CBuffer
// "CBV(b0, visibility = SHADER_VISIBILITY_PIXEL), " \ 1 -> Frame CBuffer
// "DescriptorTable(SRV(t0), SRV(t1), SRV(t2), SRV(t3), SRV(t4), visibility = SHADER_VISIBILITY_PIXEL), " \ 2 -> Textures 

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

EnvironmentLightCmdListRecorder::EnvironmentLightCmdListRecorder(ID3D12Device& device, tbb::concurrent_queue<ID3D12CommandList*>& cmdListQueue)
	: mDevice(device)
	, mCmdListQueue(cmdListQueue)
{
	BuildCommandObjects(mCmdList, mCmdAlloc, _countof(mCmdAlloc));
}

void EnvironmentLightCmdListRecorder::InitPSO() noexcept {
	ASSERT(sPSO == nullptr);
	ASSERT(sRootSign == nullptr);

	// Build pso and root signature
	PSOCreator::PSOParams psoParams{};
	const std::size_t rtCount{ _countof(psoParams.mRtFormats) };
	psoParams.mBlendDesc = D3DFactory::AlwaysBlendDesc();
	psoParams.mDepthStencilDesc = D3DFactory::DisableDepthStencilDesc();
	psoParams.mInputLayout = D3DFactory::PosNormalTangentTexCoordInputLayout();
	psoParams.mPSFilename = "EnvironmentLightPass/Shaders/PS.cso";
	psoParams.mRootSignFilename = "EnvironmentLightPass/Shaders/RS.cso";
	psoParams.mVSFilename = "EnvironmentLightPass/Shaders/VS.cso";
	psoParams.mNumRenderTargets = 1U;
	psoParams.mRtFormats[0U] = Settings::sColorBufferFormat;
	for (std::size_t i = psoParams.mNumRenderTargets; i < rtCount; ++i) {
		psoParams.mRtFormats[i] = DXGI_FORMAT_UNKNOWN;
	}
	psoParams.mTopology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	PSOCreator::CreatePSO(psoParams, sPSO, sRootSign);

	ASSERT(sPSO != nullptr);
	ASSERT(sRootSign != nullptr);
}

void EnvironmentLightCmdListRecorder::Init(
	const BufferCreator::VertexBufferData& vertexBufferData,
	const BufferCreator::IndexBufferData indexBufferData,
	Microsoft::WRL::ComPtr<ID3D12Resource>* geometryBuffers,
	const std::uint32_t geometryBuffersCount,
	ID3D12Resource& depthBuffer,
	const D3D12_CPU_DESCRIPTOR_HANDLE& colorBufferCpuDesc,
	const D3D12_CPU_DESCRIPTOR_HANDLE& depthBufferCpuDesc,
	ID3D12Resource& diffuseIrradianceCubeMap,
	ID3D12Resource& specularPreConvolvedCubeMap) noexcept
{
	ASSERT(ValidateData() == false);
	ASSERT(geometryBuffers != nullptr);
	ASSERT(geometryBuffersCount > 0U);

	mVertexBufferData = std::move(vertexBufferData);
	mIndexBufferData = std::move(indexBufferData);
	mColorBufferCpuDesc = colorBufferCpuDesc;
	mDepthBufferCpuDesc = depthBufferCpuDesc;

	BuildBuffers(geometryBuffers, geometryBuffersCount, depthBuffer, diffuseIrradianceCubeMap, specularPreConvolvedCubeMap);

	ASSERT(ValidateData());
}

void EnvironmentLightCmdListRecorder::RecordAndPushCommandLists(const FrameCBuffer& frameCBuffer) noexcept {
	ASSERT(ValidateData());
	ASSERT(sPSO != nullptr);
	ASSERT(sRootSign != nullptr);

	ID3D12CommandAllocator* cmdAlloc{ mCmdAlloc[mCurrFrameIndex] };
	ASSERT(cmdAlloc != nullptr);

	// Update frame constants
	UploadBuffer& uploadFrameCBuffer(*mFrameCBuffer[mCurrFrameIndex]);
	uploadFrameCBuffer.CopyData(0U, &frameCBuffer, sizeof(frameCBuffer));
	
	CHECK_HR(cmdAlloc->Reset());
	CHECK_HR(mCmdList->Reset(cmdAlloc, sPSO));

	mCmdList->RSSetViewports(1U, &Settings::sScreenViewport);
	mCmdList->RSSetScissorRects(1U, &Settings::sScissorRect);
	mCmdList->OMSetRenderTargets(1U, &mColorBufferCpuDesc, false, &mDepthBufferCpuDesc);

	ID3D12DescriptorHeap* heaps[] = { &DescriptorManager::Get().GetCbvSrcUavDescriptorHeap() };
	mCmdList->SetDescriptorHeaps(_countof(heaps), heaps);
	mCmdList->SetGraphicsRootSignature(sRootSign);
	
	mCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Set root parameters
	const D3D12_GPU_VIRTUAL_ADDRESS frameCBufferGpuVAddress(uploadFrameCBuffer.Resource()->GetGPUVirtualAddress());
	mCmdList->SetGraphicsRootConstantBufferView(0U, frameCBufferGpuVAddress);
	mCmdList->SetGraphicsRootConstantBufferView(1U, frameCBufferGpuVAddress);
	mCmdList->SetGraphicsRootDescriptorTable(2U, mTexturesGpuDescHandle);

	// Draw object
	mCmdList->IASetVertexBuffers(0U, 1U, &mVertexBufferData.mBufferView);
	mCmdList->IASetIndexBuffer(&mIndexBufferData.mBufferView);
	mCmdList->DrawIndexedInstanced(mIndexBufferData.mCount, 1U, 0U, 0U, 0U);

	mCmdList->Close();

	mCmdListQueue.push(mCmdList);

	// Next frame
	mCurrFrameIndex = (mCurrFrameIndex + 1) % Settings::sQueuedFrameCount;
}

bool EnvironmentLightCmdListRecorder::ValidateData() const noexcept {

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

	const bool result =
		mCmdList != nullptr &&
		mColorBufferCpuDesc.ptr != 0UL &&
		mDepthBufferCpuDesc.ptr != 0UL && 
		mTexturesGpuDescHandle.ptr != 0UL;

	return result;
}

void EnvironmentLightCmdListRecorder::BuildBuffers(
	Microsoft::WRL::ComPtr<ID3D12Resource>* geometryBuffers, 
	const std::uint32_t geometryBuffersCount,
	ID3D12Resource& depthBuffer,
	ID3D12Resource& diffuseIrradianceCubeMap,
	ID3D12Resource& specularPreConvolvedCubeMap) noexcept
{
	ASSERT(geometryBuffers != nullptr);
	ASSERT(geometryBuffersCount > 0U);

	// Used to create SRV descriptors
	std::vector<D3D12_SHADER_RESOURCE_VIEW_DESC> srvDesc;
	srvDesc.resize(geometryBuffersCount + 3U); // 3 = depth buffer + 2 cube maps
	std::vector<ID3D12Resource*> res;
	res.resize(geometryBuffersCount + 3U);

	// Fill geometry buffers SRV descriptors
	for (std::uint32_t i = 0U; i < geometryBuffersCount; ++i) {
		ASSERT(geometryBuffers[i].Get() != nullptr);
		res[i] = geometryBuffers[i].Get();
		srvDesc[i].Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srvDesc[i].ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srvDesc[i].Texture2D.MostDetailedMip = 0;
		srvDesc[i].Texture2D.ResourceMinLODClamp = 0.0f;
		srvDesc[i].Format = res[i]->GetDesc().Format;
		srvDesc[i].Texture2D.MipLevels = res[i]->GetDesc().MipLevels;
	}

	// Fill depth buffer descriptor
	std::uint32_t descIndex = geometryBuffersCount;
	srvDesc[descIndex].Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc[descIndex].ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc[descIndex].Texture2D.MostDetailedMip = 0;
	srvDesc[descIndex].Texture2D.ResourceMinLODClamp = 0.0f;
	srvDesc[descIndex].Format = Settings::sDepthStencilSRVFormat;
	srvDesc[descIndex].Texture2D.MipLevels = depthBuffer.GetDesc().MipLevels;
	res[descIndex] = &depthBuffer;
	++descIndex;

	// Fill cube map texture descriptors	
	srvDesc[descIndex].Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc[descIndex].ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc[descIndex].TextureCube.MostDetailedMip = 0;
	srvDesc[descIndex].TextureCube.MipLevels = diffuseIrradianceCubeMap.GetDesc().MipLevels;
	srvDesc[descIndex].TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc[descIndex].Format = diffuseIrradianceCubeMap.GetDesc().Format;
	res[descIndex] = &diffuseIrradianceCubeMap;
	++descIndex;

	srvDesc[descIndex].Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc[descIndex].ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc[descIndex].TextureCube.MostDetailedMip = 0;
	srvDesc[descIndex].TextureCube.MipLevels = specularPreConvolvedCubeMap.GetDesc().MipLevels;
	srvDesc[descIndex].TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc[descIndex].Format = specularPreConvolvedCubeMap.GetDesc().Format;
	res[descIndex] = &specularPreConvolvedCubeMap;
	++descIndex;

	mTexturesGpuDescHandle = DescriptorManager::Get().CreateShaderResourceView(res.data(), srvDesc.data(), static_cast<std::uint32_t>(srvDesc.size()));

	// Create frame cbuffers
	const std::size_t frameCBufferElemSize{ UploadBuffer::CalcConstantBufferByteSize(sizeof(FrameCBuffer)) };
	for (std::uint32_t i = 0U; i < Settings::sQueuedFrameCount; ++i) {
		ResourceManager::Get().CreateUploadBuffer(frameCBufferElemSize, 1U, mFrameCBuffer[i]);
	}
}