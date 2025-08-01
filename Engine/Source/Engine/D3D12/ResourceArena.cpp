#include "ResourceArena.h"

namespace Okay
{
	void ResourceArena::initialize(ID3D12Device* pDevice, uint64_t resourceSize, D3D12_RESOURCE_STATES initialState)
	{
		m_pDevice = pDevice;
		m_pDXResource = createResource(resourceSize, initialState);
		m_slots.emplace_back(0, m_pDXResource->GetDesc().Width);
	}

	void ResourceArena::shutdown()
	{
		D3D12_RELEASE(m_pDXResource);
		m_slots.clear();
		m_slots.shrink_to_fit();
	}

	void ResourceArena::resize(ID3D12GraphicsCommandList* pCommandList, uint64_t newSize, D3D12_RESOURCE_STATES currentState)
	{
		uint64_t oldSize = m_pDXResource->GetDesc().Width;

		ID3D12Resource* newResource = createResource(newSize, D3D12_RESOURCE_STATE_COPY_DEST);

		D3D12_RESOURCE_BARRIER barriers[2] = {};
		barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[0].Transition.pResource = m_pDXResource;
		barriers[0].Transition.StateBefore = currentState;
		barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barriers[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		pCommandList->ResourceBarrier(1, barriers);
		
		pCommandList->CopyBufferRegion(newResource, 0, m_pDXResource, 0, oldSize);

		uint32_t numBarriers = 1;
		barriers[0].Transition.pResource = m_pDXResource;
		barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
		barriers[0].Transition.StateAfter = currentState;
		if (currentState != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			barriers[1].Transition.pResource = newResource;
			barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			barriers[1].Transition.StateAfter = currentState;
			numBarriers++;
		}
		pCommandList->ResourceBarrier(numBarriers, barriers);


		m_pDXResource = newResource;

		// Handles potential slot merging
		removeAllocation(ResourceSlot(oldSize, newSize - oldSize));
	}

	void ResourceArena::clear()
	{
		m_slots.clear();
		m_slots.emplace_back(0, m_pDXResource->GetDesc().Width);
	}

	D3D12_GPU_VIRTUAL_ADDRESS ResourceArena::claimAllocationSlot(uint64_t allocationSize, uint32_t allocationHandle, ResourceSlot* pOutSlot)
	{
		OKAY_ASSERT(allocationHandle < m_slots.size());
		D3D12_GPU_VIRTUAL_ADDRESS allocationGVA = m_pDXResource->GetGPUVirtualAddress() + m_slots[allocationHandle].offset;

		if (pOutSlot)
		{
			pOutSlot->offset = m_slots[allocationHandle].offset;
			pOutSlot->size = allocationSize;
		}

		if (allocationSize == m_slots[allocationHandle].size)
		{
			m_slots.erase(m_slots.begin() + allocationHandle);
		}
		else
		{
			m_slots[allocationHandle].offset += allocationSize;
			m_slots[allocationHandle].size -= allocationSize;
		}

		return allocationGVA;
	}

	void ResourceArena::removeAllocation(const ResourceSlot& removedSlot)
	{
		// Search for slot we can merge with
		uint64_t removedSlotEnd = removedSlot.offset + removedSlot.size;
		for (uint64_t i = 0; i < m_slots.size(); i++)
		{
			ResourceSlot& slot = m_slots[i];
			if (removedSlotEnd == slot.offset)
			{
				slot.offset = removedSlot.offset;
				slot.size += removedSlot.size;

				// Check if we expanded the slot into another slot
				for (uint64_t j = 0; j < m_slots.size(); j++)
				{
					ResourceSlot& otherSlot = m_slots[j];
					if (slot.offset != otherSlot.offset + otherSlot.size)
						continue;

					slot.offset = otherSlot.offset;
					slot.size += otherSlot.size;
					m_slots.erase(m_slots.begin() + j);
					break;
				}

				return;
			}

			if (slot.offset + slot.size == removedSlot.offset)
			{
				slot.size += removedSlot.size;
				
				// Check if we expanded the slot into another slot
				uint64_t slotEnd = slot.offset + slot.size;
				for (uint64_t j = 0; j < m_slots.size(); j++)
				{
					ResourceSlot& otherSlot = m_slots[j];
					if (slotEnd != otherSlot.offset)
						continue;

					slot.size += otherSlot.size;
					m_slots.erase(m_slots.begin() + j);
					break;
				}

				return;
			}
		}

		// Can't merge into any slots, add new
		m_slots.emplace_back(removedSlot);
	}

	ID3D12Resource* ResourceArena::getDXResource() const
	{
		return m_pDXResource;
	}

	bool ResourceArena::findAllocationSlot(uint64_t allocationSize, uint32_t* pOutAllocationHandle)
	{
		*pOutAllocationHandle = INVALID_UINT32;
		uint64_t smallestSlotSize = UINT64_MAX;

		// Find tightest fit
		for (uint64_t i = 0; i < m_slots.size(); i++)
		{
			ResourceSlot& slot = m_slots[i];
			if (allocationSize <= slot.size && slot.size < smallestSlotSize)
			{
				*pOutAllocationHandle = (uint32_t)i;
				smallestSlotSize = slot.size;

				// Can technically stop searching if slot.size == alocationSize but I feel like that would be a rare occurence :3
			}
		}

		return *pOutAllocationHandle != INVALID_UINT32;
	}

	ID3D12Resource* ResourceArena::createResource(uint64_t resourceSize, D3D12_RESOURCE_STATES initialState)
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		desc.Width = alignUint64(resourceSize, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 0;
		heapProperties.VisibleNodeMask = 0;

		ID3D12Resource* pResource = nullptr;
		DX_CHECK(m_pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr, IID_PPV_ARGS(&pResource)));
		pResource->SetName(L"ResourceArena");

		return pResource;
	}
}
