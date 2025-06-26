#pragma once
#include "RingBuffer.h"

namespace Okay
{
	class Window;
	class World;

	struct FrameResources
	{
		uint64_t fenceValue = INVALID_UINT64;
		ID3D12Fence* pFence = nullptr;

		ID3D12CommandAllocator* pCommandAllocator = nullptr;
		ID3D12GraphicsCommandList* pCommandList = nullptr;
		
		ID3D12Resource* pBackBuffer = {};
		D3D12_CPU_DESCRIPTOR_HANDLE cpuBackBufferRTV = {};

		RingBuffer ringBuffer;
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

		void render(const World& world);

	private:
		void updateBuffers(const World& world);
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

		void createRootSignature(const D3D12_ROOT_SIGNATURE_DESC* pDesc, ID3D12RootSignature** ppOutRootSignature);

		void createVoxelRenderPass();

	private:
		ID3D12Device* m_pDevice = nullptr;
		ID3D12CommandQueue* m_pCommandQueue = nullptr;
		IDXGISwapChain3* m_pSwapChain = nullptr;

		ID3D12DescriptorHeap* m_pRTVDescHeap = nullptr;
		FrameResources m_frames[MAX_FRAMES_IN_FLIGHT] = {};

		ID3D12RootSignature* m_pVoxelRootSignature = nullptr;
		ID3D12PipelineState* m_pVoxelPSO = nullptr;

		D3D12_VIEWPORT m_viewport = {};
		D3D12_RECT m_scissorRect = {};

		D3D12_GPU_VIRTUAL_ADDRESS m_renderDataGVA = INVALID_UINT64;

	private:
		uint32_t m_rtvIncrementSize = INVALID_UINT32;
	};
}