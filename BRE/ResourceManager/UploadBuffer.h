#pragma once

#include <cstddef>
#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class UploadBuffer {
public:
	explicit UploadBuffer(ID3D12Device& device, const std::size_t elemSize, const std::uint32_t elemCount);

	~UploadBuffer();
	UploadBuffer(const UploadBuffer&) = delete;
	const UploadBuffer& operator=(const UploadBuffer&) = delete;
	UploadBuffer(UploadBuffer&&) = delete;
	UploadBuffer& operator=(UploadBuffer&&) = delete;

	__forceinline ID3D12Resource* Resource() const noexcept { return mBuffer.Get(); }

	void CopyData(const std::uint32_t elemIndex, const void* srcData, const std::size_t srcDataSize) const noexcept;

	static std::size_t CalcConstantBufferByteSize(const std::size_t byteSize);

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> mBuffer;
	std ::uint8_t* mMappedData{ nullptr };
	std::size_t mElemSize{ 0U };
};