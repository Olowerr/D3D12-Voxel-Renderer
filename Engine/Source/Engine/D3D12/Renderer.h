#pragma once
#include "RingBuffer.h"

namespace Okay
{
	class Window;
	class World;
	struct Chunk;

	struct FrameResources
	{
		uint64_t fenceValue = INVALID_UINT64;
		ID3D12Fence* pFence = nullptr;

		ID3D12CommandAllocator* pCommandAllocator = nullptr;
		ID3D12GraphicsCommandList* pCommandList = nullptr;

		ID3D12Resource* pBackBuffer = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuBackBufferRTV = {};

		ID3D12Resource* pDepthTexture = nullptr;
		D3D12_CPU_DESCRIPTOR_HANDLE cpuDepthTextureDSV = {};

		RingBuffer ringBuffer;
	};

	struct DXChunk
	{
		ChunkID chunkID = INVALID_CHUNK_ID;
		uint32_t vertexCount = INVALID_UINT32;

		// Temp, should be 1 big buffer for all chunks but then we need to handle memory fragmentation
		ID3D12Resource* pMeshResource = nullptr;
	};

	struct FrameGarbage
	{
		FrameGarbage(uint32_t frameIdx, IUnknown* pDxUnknown)
			:frameIdx(frameIdx), pDxUnknown(pDxUnknown)
		{
		}

		uint32_t frameIdx = INVALID_UINT32;
		IUnknown* pDxUnknown = nullptr; // Base class containing Release()
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
		void flush(ID3D12GraphicsCommandList* pCommandList, ID3D12CommandAllocator* pCommandAlloator, ID3D12Fence* pFence, uint64_t& fenceValue);

		void addToFrameGarbage(IUnknown* pDxUnknown);
		void clearFrameGarbage();

		void transitionResource(ID3D12GraphicsCommandList* pCommandList, ID3D12Resource* pResource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES newState, uint32_t subResource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
		void updateDefaultHeapResource(ID3D12Resource* pTarget, uint64_t targetOffset, FrameResources& frame, void* pData, uint64_t dataSize);

		void updateChunks(FrameResources& frame, const World& world);
		void writeChunkDataToGPU(const World& world, ChunkID chunkId, FrameResources& frame);

		D3D12_CPU_DESCRIPTOR_HANDLE createRTVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc);
		D3D12_CPU_DESCRIPTOR_HANDLE createDSVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc);
		D3D12_GPU_DESCRIPTOR_HANDLE createSRVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc);
		D3D12_GPU_DESCRIPTOR_HANDLE createUAVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc);

	private: // Creation
		void enableDebugLayer();
		void enableGPUBasedValidation();

		void createDevice(IDXGIFactory* pFactory);
		void createCommandQueue();
		void createSwapChain(IDXGIFactory* pFactory, const Window& window);
		void initializeFrameResources(FrameResources& frame, uint64_t ringBufferSize);
		void shutdowFrameResources(FrameResources& frame);

		ID3D12RootSignature* createRootSignature(const D3D12_ROOT_SIGNATURE_DESC* pDesc, std::wstring_view name);
		ID3D12DescriptorHeap* createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, bool shaderVisible, std::wstring_view name);
		ID3D12Resource* createCommittedBuffer(uint64_t size, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType, std::wstring_view name);

		ID3D12Resource* createTextureSheet(const FilePath& filePath, FrameResources& frame, uint32_t padding, uint32_t tileSize, std::wstring_view name);
		void uploadTextureSheetData(ID3D12Resource* pTarget, FrameResources& frame, uint8_t* pTextureData, uint32_t origTextureWidth, uint32_t origTextureHeight, uint32_t padding, uint32_t tileSize);
		void generateTextureSheetMipMaps(ID3D12Resource* pTextureSheet, uint32_t tileSize);

		void createVoxelRenderPass();

	private:
		ID3D12Device* m_pDevice = nullptr;
		ID3D12CommandQueue* m_pCommandQueue = nullptr;
		IDXGISwapChain3* m_pSwapChain = nullptr;

		FrameResources m_frames[MAX_FRAMES_IN_FLIGHT] = {};
		std::vector<FrameGarbage> m_frameGarbage;

		ID3D12DescriptorHeap* m_pRTVDescHeap = nullptr;
		ID3D12DescriptorHeap* m_pDSVDescHeap = nullptr;
		ID3D12DescriptorHeap* m_pTextureDescHeap = nullptr;

		ID3D12RootSignature* m_pVoxelRootSignature = nullptr;
		ID3D12PipelineState* m_pVoxelPSO = nullptr;

		D3D12_VIEWPORT m_viewport = {}; // Move to FrameResources? :spinthink:
		D3D12_RECT m_scissorRect = {}; // Move to FrameResources? :spinthink:

		D3D12_GPU_VIRTUAL_ADDRESS m_renderDataGVA = INVALID_UINT64;

		std::vector<DXChunk> m_dxChunks;

		ID3D12Resource* m_pTextureSheet = nullptr;
		D3D12_GPU_DESCRIPTOR_HANDLE m_textureHandle = {};

	private:
		uint32_t m_rtvIncrementSize = INVALID_UINT32;
		uint32_t m_dsvIncrementSize = INVALID_UINT32;
		uint32_t m_cbvSrvUavIncrementSize = INVALID_UINT32;

	};
}