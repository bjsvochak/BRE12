#pragma once

#include <GeometryPass/GeometryPassCmdListRecorder.h>

struct Material;

// Recorder that does texture mapping + normal mapping
class NormalCmdListRecorder : public GeometryPassCmdListRecorder {
public:
	explicit NormalCmdListRecorder(ID3D12Device& device);

	~NormalCmdListRecorder() = default;
	NormalCmdListRecorder(const NormalCmdListRecorder&) = delete;
	const NormalCmdListRecorder& operator=(const NormalCmdListRecorder&) = delete;
	NormalCmdListRecorder(NormalCmdListRecorder&&) = default;
	NormalCmdListRecorder& operator=(NormalCmdListRecorder&&) = default;

	// This method is to initialize PSO that is a shared between all this kind
	// of recorders.
	// This method is initialized by its corresponding pass.
	static void InitPSO(const DXGI_FORMAT* geometryBufferFormats, const std::uint32_t geometryBufferCount) noexcept;

	// This method must be called before calling RecordAndPushCommandLists()
	void Init(
		const GeometryData* geometryDataVec,
		const std::uint32_t numGeomData,
		const Material* materials,
		ID3D12Resource** textures,
		ID3D12Resource** normals,
		const std::uint32_t numResources) noexcept;

	void RecordAndPushCommandLists(const FrameCBuffer& frameCBuffer) noexcept final override;

	bool ValidateData() const noexcept final override;

private:
	void BuildBuffers(
		const Material* materials, 
		ID3D12Resource** textures, 
		ID3D12Resource** normals,
		const std::uint32_t dataCount) noexcept;

	D3D12_GPU_DESCRIPTOR_HANDLE mTexturesBufferGpuDescHandleBegin;
	D3D12_GPU_DESCRIPTOR_HANDLE mNormalsBufferGpuDescHandleBegin;
};