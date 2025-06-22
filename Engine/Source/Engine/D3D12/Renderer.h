#pragma once
#include "OkayD3D12.h"
#include "Engine/Application/Window.h"

namespace Okay
{
	struct FrameResources
	{
		uint64_t fenceValue = INVALID_UINT64;
		ID3D12Fence* pFence = nullptr;

		ID3D12CommandAllocator* pCommandAllocator = nullptr;
		ID3D12GraphicsCommandList* pCommandList = nullptr;
		
		ID3D12Resource* pBackBuffer = {};
		D3D12_CPU_DESCRIPTOR_HANDLE cpuBackBufferRTV = {};
	};

	class Renderer
	{
	public:
		static const uint32_t MAX_FRAMES_IN_FLIGHT = 3;

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

		void signal(ID3D12Fence* pFence, uint64_t& fenceValue);
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
		FrameResources m_frames[MAX_FRAMES_IN_FLIGHT] = {};

	private:
		uint32_t m_rtvIncrementSize = INVALID_UINT32;
	};
}