#pragma once

#include "OkayD3D12.h"

namespace Okay
{
	class RingBuffer
	{
	public:
		RingBuffer() = default;
		virtual ~RingBuffer() = default;

		void initialize(ID3D12Device* pDevice, uint64_t size);
		void shutdown();

		D3D12_GPU_VIRTUAL_ADDRESS allocate(const void* pData, uint64_t byteWidth);

		uint8_t* getMappedPtr();
		D3D12_GPU_VIRTUAL_ADDRESS getGPUAddress() const;

		uint8_t* map();
		void unmap();

		void offsetMappedPtr(uint64_t offset);
		uint64_t getOffset() const;

		void alignOffset(uint32_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		ID3D12Resource* getDXResource() const;
		void jumpToStart();

		void resize(uint64_t size);

	private:
		void createBuffer(uint64_t size);

	private:
		ID3D12Device* m_pDevice = nullptr;
		ID3D12Resource* m_pRingBuffer = nullptr;
		uint8_t* m_pMappedPtr = nullptr;

		uint64_t m_bufferOffset = INVALID_UINT64;
		uint64_t m_maxSize = INVALID_UINT64;

	};
}
