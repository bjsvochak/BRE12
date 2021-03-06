#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <tbb/task.h>

#include <Camera/Camera.h>
#include <GeometryPass\GeometryPass.h>
#include <GlobalData\Settings.h>
#include <LightingPass\LightingPass.h>
#include <SkyBoxPass\SkyBoxPass.h>
#include <ShaderUtils\CBuffers.h>
#include <ToneMappingPass\ToneMappingPass.h>
#include <Timer/Timer.h>

class CommandListExecutor;
class Scene;

// Initializes passes (geometry, light, skybox, etc) based on a Scene.
// Steps:
// - Use MasterRender::Create() to create and spawn and instance. 
// - When you want to terminate this task, you should call MasterRender::Terminate()
class MasterRender : public tbb::task {
public:
	static MasterRender* Create(const HWND hwnd, ID3D12Device& device, Scene* scene) noexcept;

	~MasterRender() = default;
	MasterRender(const MasterRender&) = delete;
	const MasterRender& operator=(const MasterRender&) = delete;
	MasterRender(MasterRender&&) = delete;
	MasterRender& operator=(MasterRender&&) = delete;

	void Terminate() noexcept;

private:
	explicit MasterRender(const HWND hwnd, ID3D12Device& device, Scene* scene);

	// Called when tbb::task is spawned
	tbb::task* execute() final override;

	void InitPasses(Scene* scene) noexcept;

	void CreateRtvAndDsv() noexcept;
	void CreateColorBuffer() noexcept;
	void CreateMergePassCommandObjects() noexcept;
	
	ID3D12Resource* CurrentFrameBuffer() const noexcept;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentFrameBufferCpuDesc() const noexcept;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilCpuDesc() const noexcept;

	void ExecuteMergePass();

	void FlushCommandQueue() noexcept;
	void SignalFenceAndPresent() noexcept;

	HWND mHwnd{ nullptr };
	ID3D12Device& mDevice;
	Microsoft::WRL::ComPtr<IDXGISwapChain3> mSwapChain{ nullptr };
	ID3D12CommandQueue* mCmdQueue{ nullptr };
			
	CommandListExecutor* mCmdListExecutor{ nullptr };
	
	// Fences data for syncrhonization purposes.
	ID3D12Fence* mFence{ nullptr };
	std::uint32_t mCurrQueuedFrameIndex{ 0U };
	std::uint64_t mFenceValueByQueuedFrameIndex[Settings::sQueuedFrameCount]{ 0UL };
	std::uint64_t mCurrFenceValue{ 0UL };

	// Passes
	GeometryPass mGeometryPass;
	LightingPass mLightingPass;
	SkyBoxPass mSkyBoxPass;
	ToneMappingPass mToneMappingPass;

	// Command allocarts and list needed for merge pass
	ID3D12CommandAllocator* mMergePassCmdAllocs[Settings::sQueuedFrameCount]{ nullptr };
	ID3D12GraphicsCommandList* mMergePassCmdList{ nullptr };
	
	// Frame buffers
	Microsoft::WRL::ComPtr<ID3D12Resource> mFrameBuffers[Settings::sSwapChainBufferCount];
	D3D12_CPU_DESCRIPTOR_HANDLE mFrameBufferRTVs[Settings::sSwapChainBufferCount]{ 0UL };

	// Depth stencil buffer
	ID3D12Resource* mDepthStencilBuffer{ nullptr };
	D3D12_CPU_DESCRIPTOR_HANDLE mDepthStencilBufferRTV{ 0UL };

	// Color buffer is a buffer used for intermediate computations.
	// It is used as render target (light pass) or pixel shader resource (post processing passes)
	Microsoft::WRL::ComPtr<ID3D12Resource> mColorBuffer;
	D3D12_CPU_DESCRIPTOR_HANDLE mColorBufferRTVCpuDescHandle;

	// Per frame constant buffer.
	// We cache it here, as is is used by most passes.
	FrameCBuffer mFrameCBuffer;

	Camera mCamera;
	Timer mTimer;
	
	// When it is true, master render thread is destroyed.
	bool mTerminate{ false };
};
