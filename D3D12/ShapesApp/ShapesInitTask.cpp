#include "ShapesInitTask.h"

#include <ResourceManager/ResourceManager.h>
#include <ResourceManager/UploadBuffer.h>
#include <Utils/DebugUtils.h>

void ShapesInitTask::InitCmdBuilders(tbb::concurrent_queue<ID3D12CommandList*>& cmdLists, CmdBuilderTaskInput& output) noexcept {
	InitTask::InitCmdBuilders(cmdLists, output);

	ASSERT(output.mGeomDataVec.empty());
	ASSERT(output.mCmdList != nullptr);
	ASSERT(output.mCmdAlloc[0] != nullptr);
	ASSERT(output.mPSO != nullptr);

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	CHECK_HR(output.mCmdAlloc[0]->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	CHECK_HR(output.mCmdList->Reset(output.mCmdAlloc[0], output.mPSO));

	const std::size_t numMeshes{ mInput.mGeomBuffersCreatorInputVec.size() };
	output.mGeomDataVec.reserve(numMeshes);

	const float baseOffset{ 10.0f };
	for (std::size_t i = 0UL; i < numMeshes; ++i) {
		const GeomBuffersCreator::Input& geomBuffers{ mInput.mGeomBuffersCreatorInputVec[i] };
		ASSERT(geomBuffers.ValidateData());
		GeometryData geomData;
		GeomBuffersCreator::Execute(*output.mCmdList, geomBuffers, geomData.mBuffersInfo);
		geomData.mWorld = mInput.mWorldVec[i];
		output.mGeomDataVec.push_back(geomData);
	}

	output.mCmdList->Close();
	cmdLists.push(output.mCmdList);

	BuildConstantBuffers(output);
}

void ShapesInitTask::BuildConstantBuffers(CmdBuilderTaskInput& output) noexcept {
	ASSERT(output.mCBVHeap == nullptr);
	ASSERT(!output.mGeomDataVec.empty());
	ASSERT(output.mFrameConstants == nullptr);
	ASSERT(output.mObjectConstants == nullptr);

	const std::uint32_t geomCount{ (std::uint32_t)output.mGeomDataVec.size() };

	// Create constant buffers descriptor heap
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc{};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0U;
	descHeapDesc.NumDescriptors = geomCount + 1U; // +1 for frame constants
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	ResourceManager::gManager->CreateDescriptorHeap(descHeapDesc, output.mCBVHeap);
	output.mCbvBaseGpuDescHandle = output.mCBVHeap->GetGPUDescriptorHandleForHeapStart();

	const std::size_t descHandleIncSize{ ResourceManager::gManager->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) };

	const std::size_t elemSize{ UploadBuffer::CalcConstantBufferByteSize(sizeof(DirectX::XMFLOAT4X4)) };

	// Fill constant buffers descriptor heap with per objects constants buffer views
	ResourceManager::gManager->CreateUploadBuffer(elemSize, geomCount, output.mObjectConstants);	
	D3D12_GPU_VIRTUAL_ADDRESS cbObjGPUBaseAddress{ output.mObjectConstants->Resource()->GetGPUVirtualAddress() };	
	for (std::size_t i = 0UL; i < geomCount; ++i) {
		D3D12_CPU_DESCRIPTOR_HANDLE descHandle = output.mCBVHeap->GetCPUDescriptorHandleForHeapStart();
		descHandle.ptr += i * descHandleIncSize;

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
		cbvDesc.BufferLocation = cbObjGPUBaseAddress + i * elemSize;
		cbvDesc.SizeInBytes = (std::uint32_t)elemSize;

		ResourceManager::gManager->CreateConstantBufferView(cbvDesc, descHandle);
	}

	// Fill constant buffers descriptor heap with per frame constant buffer view
	ResourceManager::gManager->CreateUploadBuffer(elemSize, 1U, output.mFrameConstants);
	D3D12_GPU_VIRTUAL_ADDRESS cbFrameGPUBaseAddress{ output.mFrameConstants->Resource()->GetGPUVirtualAddress() };
	D3D12_CPU_DESCRIPTOR_HANDLE descHandle = output.mCBVHeap->GetCPUDescriptorHandleForHeapStart();
	descHandle.ptr += geomCount * descHandleIncSize;

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
	cbvDesc.BufferLocation = cbFrameGPUBaseAddress;
	cbvDesc.SizeInBytes = (std::uint32_t)elemSize;
	ResourceManager::gManager->CreateConstantBufferView(cbvDesc, descHandle);
}