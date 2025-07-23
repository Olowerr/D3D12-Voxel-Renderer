#pragma once
#include "OkayD3D12.h"

namespace Okay
{
	struct ResourceSlot
	{
		ResourceSlot() = default;
		ResourceSlot(uint64_t offset, uint64_t size)
			:offset(offset), size(size)
		{ }

		uint64_t offset = INVALID_UINT64;
		uint64_t size = INVALID_UINT64;
	};

	class ResourceArena
	{
	public:
		ResourceArena() = default;
		~ResourceArena() = default;

		void initialize(ID3D12Device* pDevice, uint64_t resourceSize, D3D12_RESOURCE_STATES initialState);
		void shutdown();

		// Assumes the release of the dxResource is handled by the caller after the function call
		void resize(ID3D12GraphicsCommandList* pCommandList, uint64_t newSize, D3D12_RESOURCE_STATES currentState);

		bool findAllocationSlot(uint64_t allocationSize, uint32_t* pOutAllocationHandle);
		D3D12_GPU_VIRTUAL_ADDRESS claimAllocationSlot(uint64_t allocationSize, uint32_t allocationHandle, ResourceSlot* pOutSlot);
		void removeAllocation(const ResourceSlot& removedSlot);

		ID3D12Resource* getDXResource() const;

	private:
		ID3D12Resource* createResource(uint64_t resourceSize, D3D12_RESOURCE_STATES initialState);

	private:
		ID3D12Device* m_pDevice = nullptr;
		ID3D12Resource* m_pDXResource = nullptr;
		std::vector<ResourceSlot> m_slots;

	};
}
