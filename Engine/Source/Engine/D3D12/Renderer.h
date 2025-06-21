#pragma once
#include "OkayD3D12.h"
#include "Engine/Application/Window.h"

namespace Okay
{
	class Renderer
	{
	public:
		Renderer() = default;
		~Renderer() = default;

		void initialize(const Window& window);
		void shutdown();

		void render();

	private:
		void preRender();
		void renderWorld();
		void postRender();

		void signal(uint64_t& fenceValue);
		void execute(ID3D12GraphicsCommandList* pCommandList);
		void wait(ID3D12Fence* pFence, uint64_t fenceValue);
		void reset(ID3D12CommandAllocator* pCommandAlloator, ID3D12GraphicsCommandList* pCommandList);

		void transitionResource(ID3D12GraphicsCommandList* pCommandList, ID3D12Resource* pResource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES newState);

	private:
		void enableDebugLayer();
		void enableGPUBasedValidation();

		void createDevice(IDXGIFactory* pFactory);
		void createCommandQueue();
		void createSwapChain(IDXGIFactory* pFactory, const Window& window);

		void createDescriptorHeap(ID3D12DescriptorHeap** ppDescriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, bool shaderVisible);

	private:
		ID3D12Device* m_pDevice = nullptr;
		ID3D12CommandQueue* m_pCommandQueue = nullptr;
		IDXGISwapChain3* m_pSwapChain = nullptr;

		ID3D12DescriptorHeap* m_pRTVDescHeap = nullptr;

		// Temp
		uint64_t m_fenceValue = INVALID_UINT64;
		ID3D12Fence* m_pFence = nullptr;
		ID3D12CommandAllocator* m_pCommandAllocator = nullptr;
		ID3D12GraphicsCommandList* m_pCommandList = nullptr;
		
		ID3D12Resource* m_pBackBuffers[2] = {};
		D3D12_CPU_DESCRIPTOR_HANDLE m_cpuBackBufferRTVs[2] = {};

	private:
		uint32_t m_rtvIncrementSize = INVALID_UINT32;
	};
}