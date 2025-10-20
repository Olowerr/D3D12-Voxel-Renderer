#include "DescriptorHeap.h"

namespace Okay
{
	void DescriptorHeap::initialize(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t slots, bool shaderVisible, std::wstring_view name)
	{
		m_pDevice = pDevice;

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = type;
		desc.NumDescriptors = slots;
		desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		DX_CHECK(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDescriptorHeap)));
		m_pDescriptorHeap->SetName(name.data());

		m_nextAppendSlot = 0;
		m_incrementSize = pDevice->GetDescriptorHandleIncrementSize(type);
	}

	void DescriptorHeap::shutdown()
	{
		D3D12_RELEASE(m_pDescriptorHeap);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::createRTVDescriptor(uint32_t slot, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc)
	{
		OKAY_ASSERT(pResource || pDesc);
		OKAY_ASSERT(m_pDescriptorHeap->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = prepareDescriptorCreation(slot);
		m_pDevice->CreateRenderTargetView(pResource, pDesc, cpuHandle);

		return cpuHandle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::createDSVDescriptor(uint32_t slot, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc)
	{
		OKAY_ASSERT(pResource || pDesc);
		OKAY_ASSERT(m_pDescriptorHeap->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = prepareDescriptorCreation(slot);
		m_pDevice->CreateDepthStencilView(pResource, pDesc, cpuHandle);

		return cpuHandle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::createSRVDescriptor(uint32_t slot, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc)
	{
		OKAY_ASSERT(pResource || pDesc);
		OKAY_ASSERT(m_pDescriptorHeap->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = prepareDescriptorCreation(slot);
		m_pDevice->CreateShaderResourceView(pResource, pDesc, cpuHandle);

		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		gpuHandle.ptr += (m_nextAppendSlot - 1ull) * (uint64_t)m_incrementSize;
		return gpuHandle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::createUAVDescriptor(uint32_t slot, ID3D12Resource* pResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc)
	{
		OKAY_ASSERT(pResource || pDesc);
		OKAY_ASSERT(m_pDescriptorHeap->GetDesc().Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = prepareDescriptorCreation(slot);
		m_pDevice->CreateUnorderedAccessView(pResource, nullptr, pDesc, cpuHandle);

		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		gpuHandle.ptr += (m_nextAppendSlot - 1ull) * (uint64_t)m_incrementSize;
		return gpuHandle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::getCPUHandle(uint32_t slot) const
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += slot * (uint64_t)m_incrementSize;
		return cpuHandle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::getGPUHandle(uint32_t slot) const
	{
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		gpuHandle.ptr += slot * (uint64_t)m_incrementSize;
		return gpuHandle;
	}

	ID3D12DescriptorHeap* DescriptorHeap::getD3D12DescriptorHeap() const
	{
		return m_pDescriptorHeap;
	}

	void DescriptorHeap::setNextAppendSlot(uint32_t value)
	{
		m_nextAppendSlot = value;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::prepareDescriptorCreation(uint32_t slot)
	{
		if (slot == DESCRIPTOR_HEAP_SLOT_APPEND)
			slot = m_nextAppendSlot;
		
		m_nextAppendSlot = slot + 1;

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += slot * (uint64_t)m_incrementSize;

		return cpuHandle;
	}
}
