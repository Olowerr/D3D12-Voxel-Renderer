#pragma once
#include "RingBuffer.h"
#include "ResourceArena.h"
#include "Engine/World/Chunk.h"

namespace Okay
{
	constexpr uint32_t TEXTURE_SHEET_TILE_SIZE = 16;
	constexpr uint32_t TEXTURE_SHEET_PADDING = 8;

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

		// do these need to be frame specific ? I don't think soooo :thonk:
		D3D12_VIEWPORT viewport = {};
		D3D12_RECT scissorRect = {};
	};

	struct DXChunk
	{
		ChunkID chunkID = INVALID_CHUNK_ID;

		D3D12_GPU_VIRTUAL_ADDRESS vertexDataGVA = {};
		D3D12_INDEX_BUFFER_VIEW indicesView = {};
		uint32_t indicesCount = INVALID_UINT32;

		ResourceSlot vertexDataSlot;
		ResourceSlot indicesDataSlot;
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

	struct Vertex
	{
		Vertex() = default;
		Vertex(const glm::ivec3& position, const glm::vec2& globalUV, uint32_t textureID)
		{
			data = 0;
			writeBits(position.x, 0, 5);
			writeBits(position.y, 5, 9);
			writeBits(position.z, 14, 5);

			writeBits((uint32_t)globalUV.x, 19, 1);
			writeBits((uint32_t)globalUV.y, 20, 1);
			writeBits(textureID, 21, 8);
		}

		void writeBits(uint32_t value, uint32_t bitPos, uint32_t numBits)
		{
			data |= value << (32 - (bitPos + numBits));
		}

		bool operator==(Vertex other) const
		{
			return data == other.data;
		}

		uint32_t data = INVALID_UINT32;
	};

	struct ChunkMeshData
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
	};

	struct ThreadSafeChunkMesh
	{
		std::atomic<bool> meshGenerated;
		std::atomic<uint32_t> latestChunkGenID;
		// Might be neccessay to add a 'finishedID' int here to confirm that lastID == finishedID
		// incase threadFinished is set to true right before lastID is incremented & threadFinished set to false.
		// But Seems oki for now :3

		ChunkMeshData meshData;
	};

	struct SideTextureIDs
	{
		SideTextureIDs()
		{
			for (uint8_t& id : sideIDs)
				id = INVALID_UINT8;
		}
		uint8_t sideIDs[3] = {};
	};

	class Renderer
	{
	public:
		static const uint32_t MAX_FRAMES_IN_FLIGHT = 3;

	public:
		Renderer() = default;
		~Renderer() = default;

		void initialize(Window& window);
		void shutdown();

		void onResize(uint32_t width, uint32_t height);

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
		void updateDefaultHeapResource(ID3D12Resource* pTarget, uint64_t targetOffset, const void* pData, uint64_t dataSize);

		void updateChunks(const World& world);
		void processAddedChunks(const World& world);
		void processLoadingChunkMeshes(const World& world);
		void writeChunkDataToGPU(ChunkID chunkID, const ChunkMeshData& chunkMesh);
		void findAndDeleteDXChunk(ChunkID chunkID);
		void generateChunkMesh(const World* pWorld, ChunkID chunkID, ThreadSafeChunkMesh* pChunkMesh, uint32_t chunkGenID, ChunkMeshData& outMeshData);


		D3D12_CPU_DESCRIPTOR_HANDLE createRTVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc);
		D3D12_CPU_DESCRIPTOR_HANDLE createDSVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc);
		D3D12_GPU_DESCRIPTOR_HANDLE createSRVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc);
		D3D12_GPU_DESCRIPTOR_HANDLE createUAVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc);

		D3D12_GPU_VIRTUAL_ADDRESS allocateIntoResourceArena(ResourceArena& arena, ResourceSlot* pOutSlot, const void* pData, uint64_t dataSize);

		FrameResources& getCurrentFrameResorces();

	private: // Creation
		void enableDebugLayer();
		void enableGPUBasedValidation();

		void createDevice(IDXGIFactory* pFactory);
		void createCommandQueue();
		void createSwapChain(IDXGIFactory* pFactory, const Window& window);
		
		void initializeFrameResources(FrameResources& frame, uint64_t ringBufferSize);
		void shutdowFrameResources(FrameResources& frame);
		void updateBackBufferTextures();


		ID3D12RootSignature* createRootSignature(const D3D12_ROOT_SIGNATURE_DESC* pDesc, std::wstring_view name);
		ID3D12DescriptorHeap* createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, bool shaderVisible, std::wstring_view name);
		ID3D12Resource* createCommittedBuffer(uint64_t size, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType, std::wstring_view name);

		ID3D12Resource* createTextureSheet(FrameResources& frame);
		void uploadTextureSheetData(ID3D12Resource* pTarget, FrameResources& frame, const std::unordered_map<std::string, uint32_t>& textureIds);
		void generateTextureSheetMipMaps(ID3D12Resource* pTextureSheet, uint32_t tileSize);
		uint32_t getTextureID(BlockType blockType, BlockSide blockSide);

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

		D3D12_GPU_VIRTUAL_ADDRESS m_renderDataGVA = INVALID_UINT64;

		std::vector<DXChunk> m_dxChunks;
		std::unordered_map<ChunkID, ThreadSafeChunkMesh> m_loadingChunkMesh;

		ResourceArena m_gpuVertexData;
		ResourceArena m_gpuIndicesData;

		ID3D12Resource* m_pTextureSheet = nullptr;
		D3D12_GPU_DESCRIPTOR_HANDLE m_textureHandle = {};

		// can maybe be vector instead? idx 0 is air tho but 3-6 extra bytes don't really matter
		std::unordered_map<BlockType, SideTextureIDs> m_textureIds;

	private:
		uint32_t m_rtvIncrementSize = INVALID_UINT32;
		uint32_t m_dsvIncrementSize = INVALID_UINT32;
		uint32_t m_cbvSrvUavIncrementSize = INVALID_UINT32;

	};
}