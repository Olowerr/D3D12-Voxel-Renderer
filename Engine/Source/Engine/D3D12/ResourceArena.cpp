#include "ResourceArena.h"

namespace Okay
{
	void ResourceArena::initialize(ID3D12Device* pDevice, uint64_t resourceSize, D3D12_RESOURCE_STATES initialState)
	{
		createResource(pDevice, resourceSize, initialState);
	}

	void ResourceArena::shutdown()
	{
		D3D12_RELEASE(m_pDXResource);
		m_slots.clear();
		m_slots.shrink_to_fit();
	}

	D3D12_GPU_VIRTUAL_ADDRESS ResourceArena::findAndClaimAllocationSlot(uint64_t allocationSize, ResourceSlot* pOutSlot)
	{
		uint32_t slotIdx = findAllocationSlot(allocationSize);
		D3D12_GPU_VIRTUAL_ADDRESS allocationGVA = m_pDXResource->GetGPUVirtualAddress() + m_slots[slotIdx].offset;

		if (pOutSlot)
		{
			pOutSlot->offset = m_slots[slotIdx].offset;
			pOutSlot->size = allocationSize;
		}

		if (allocationSize == m_slots[slotIdx].size)
		{
			m_slots.erase(m_slots.begin() + slotIdx);
		}
		else
		{
			m_slots[slotIdx].offset += allocationSize;
			m_slots[slotIdx].size -= allocationSize;
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

	uint32_t ResourceArena::findAllocationSlot(uint64_t allocationSize)
	{
		uint64_t bestSlotFitIdx = INVALID_UINT64;
		uint64_t smallestSlotSize = UINT64_MAX;

		// Find tightest fit
		for (uint64_t i = 0; i < m_slots.size(); i++)
		{
			ResourceSlot& slot = m_slots[i];
			if (allocationSize <= slot.size && slot.size < smallestSlotSize)
			{
				bestSlotFitIdx = i;
				smallestSlotSize = slot.size;

				// Can technically stop searching if slot.size == alocationSize but I feel like that would be a rare occurence :3
			}
		}

		OKAY_ASSERT(bestSlotFitIdx != INVALID_UINT64);
		return (uint32_t)bestSlotFitIdx;
	}

	void ResourceArena::createResource(ID3D12Device* pDevice, uint64_t resourceSize, D3D12_RESOURCE_STATES initialState)
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

		DX_CHECK(pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr, IID_PPV_ARGS(&m_pDXResource)));
		m_pDXResource->SetName(L"ResourceArena");

		m_slots.emplace_back(0, resourceSize);
	}
}
