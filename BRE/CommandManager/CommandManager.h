#pragma once

#include <d3d12.h>
#include <mutex>
#include <tbb/concurrent_hash_map.h>
#include <wrl.h>

// This class is responsible to create/get/erase:
// - Command queue
// - Command list
// - Command allocator
class CommandManager {
public:
	static CommandManager& Create(ID3D12Device& device) noexcept;
	static CommandManager& Get() noexcept;
		
	~CommandManager() = default;
	CommandManager(const CommandManager&) = delete;
	const CommandManager& operator=(const CommandManager&) = delete;
	CommandManager(CommandManager&&) = delete;
	CommandManager& operator=(CommandManager&&) = delete;

	std::size_t CreateCmdQueue(const D3D12_COMMAND_QUEUE_DESC& desc, ID3D12CommandQueue* &cmdQueue) noexcept;
	std::size_t CreateCmdList(const D3D12_COMMAND_LIST_TYPE& type, ID3D12CommandAllocator& cmdAlloc, ID3D12GraphicsCommandList* &cmdList) noexcept;
	std::size_t CreateCmdAlloc(const D3D12_COMMAND_LIST_TYPE& type, ID3D12CommandAllocator* &cmdListAlloc) noexcept;

	// Asserts if resource id is not present
	ID3D12CommandQueue& GetCmdQueue(const std::size_t id) noexcept;
	ID3D12GraphicsCommandList& GetCmdList(const std::size_t id) noexcept;
	ID3D12CommandAllocator& GetCmdAlloc(const std::size_t id) noexcept;
	
	void EraseCmdQueue(const std::size_t id) noexcept;
	void EraseCmdList(const std::size_t id) noexcept;
	void EraseCmdAlloc(const std::size_t id) noexcept;

	// These will invalidate all ids.
	__forceinline void ClearCmdQueues() noexcept { mCmdQueueById.clear(); }
	__forceinline void ClearCmdLists() noexcept { mCmdListById.clear(); }
	__forceinline void ClearCmdAllocs() noexcept { mCmdAllocById.clear(); }
	__forceinline void Clear() noexcept { ClearCmdQueues(); ClearCmdLists(); ClearCmdAllocs(); }

private:
	explicit CommandManager(ID3D12Device& device);

	ID3D12Device& mDevice;

	using CmdQueueById = tbb::concurrent_hash_map<std::size_t, Microsoft::WRL::ComPtr<ID3D12CommandQueue>>;
	CmdQueueById mCmdQueueById;

	using CmdListById = tbb::concurrent_hash_map<std::size_t, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>>;
	CmdListById mCmdListById;

	using CmdAllocById = tbb::concurrent_hash_map<std::size_t, Microsoft::WRL::ComPtr<ID3D12CommandAllocator>>;
	CmdAllocById mCmdAllocById;

	std::mutex mMutex;
};
