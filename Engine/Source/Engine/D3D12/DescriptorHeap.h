#pragma once

#include "OkayD3D12.h"

#define DESCRIPTOR_HEAP_SLOT_APPEND UINT32_MAX

namespace Okay
{
	class DescriptorHeap
	{
	public:
		DescriptorHeap() = default;
		~DescriptorHeap() = default;

		void initialize(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t slots, bool shaderVisible, std::wstring_view name);
		void shutdown();

		D3D12_CPU_DESCRIPTOR_HANDLE createRTVDescriptor(uint32_t slot, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc);
		D3D12_CPU_DESCRIPTOR_HANDLE createDSVDescriptor(uint32_t slot, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc);
		D3D12_GPU_DESCRIPTOR_HANDLE createSRVDescriptor(uint32_t slot, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc);
		D3D12_GPU_DESCRIPTOR_HANDLE createUAVDescriptor(uint32_t slot, ID3D12Resource* pResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc);

		D3D12_CPU_DESCRIPTOR_HANDLE getCPUHandle(uint32_t slot) const;
		D3D12_GPU_DESCRIPTOR_HANDLE getGPUHandle(uint32_t slot) const;

		ID3D12DescriptorHeap* getD3D12DescriptorHeap() const;

		void setNextAppendSlot(uint32_t value);

	private:
		D3D12_CPU_DESCRIPTOR_HANDLE prepareDescriptorCreation(uint32_t slot);

	private:
		ID3D12Device* m_pDevice = nullptr;
		ID3D12DescriptorHeap* m_pDescriptorHeap = nullptr;
		uint32_t m_nextAppendSlot = INVALID_UINT32;
		uint32_t m_incrementSize = INVALID_UINT32;

	};
}
