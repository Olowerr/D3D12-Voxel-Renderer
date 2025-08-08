#include "Renderer.h"
#include "Engine/Application/Window.h"
#include "Engine/World/World.h"
#include "Engine/Utilities/ThreadPool.h"
#include "Engine/Application/ImguiHelper.h"
#include "Engine/World/Camera.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include <shared_mutex>

namespace Okay
{
	struct GPURenderData
	{
		glm::mat4 viewProjMatrix = glm::mat4(1.f);
		uint32_t textureSheetTileSize = 0;
		uint32_t textureSheetPadding = 0;
		glm::vec2 padding0 = glm::vec2(0.f);
	};
	
	struct GPUDrawCallData
	{
		glm::vec3 chunkWorldPos = glm::vec3(0.f);
	};

	static std::shared_mutex s_loadingChunksMutis;

	void Renderer::initialize(Window& window)
	{
#if 0
		enableDebugLayer();
		enableGPUBasedValidation();
#endif

		IDXGIFactory* pFactory = nullptr;
		DX_CHECK(CreateDXGIFactory(IID_PPV_ARGS(&pFactory)));

		createDevice(pFactory);
		createCommandQueue();

		m_pRTVDescHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_FRAMES_IN_FLIGHT, false, L"BackBufferRTVDescriptorHeap");
		m_pDSVDescHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MAX_FRAMES_IN_FLIGHT, false, L"DepthTextureDSVDescriptorHeap");
		m_pTextureDescHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, true, L"TexturesSRVDescriptorHeap");

		m_rtvIncrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_dsvIncrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
		m_cbvSrvUavIncrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		createSwapChain(pFactory, window);
		D3D12_RELEASE(pFactory);

		window.registerResizeCallback(std::bind(&Renderer::onResize, this, std::placeholders::_1, std::placeholders::_2));

		for (FrameResources& frame : m_frames)
			initializeFrameResources(frame, 100'000'000);

		createVoxelRenderPass();

		m_gpuVertexData.initialize(m_pDevice, 1'000'000, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_gpuIndicesData.initialize(m_pDevice, 1'000'000, D3D12_RESOURCE_STATE_INDEX_BUFFER);

		FrameResources initFrame;
		initializeFrameResources(initFrame, 100'000);
		reset(initFrame.pCommandAllocator, initFrame.pCommandList);

		m_pTextureSheet = createTextureSheet(initFrame);
		m_textureHandle = createSRVDescriptor(m_pTextureDescHeap, 0, m_pTextureSheet, nullptr);

		flush(initFrame.pCommandList, initFrame.pCommandAllocator, initFrame.pFence, initFrame.fenceValue);
		shutdowFrameResources(initFrame);

		generateTextureSheetMipMaps(m_pTextureSheet, TEXTURE_SHEET_TILE_SIZE);

		// In this version of Imgui, only 1 SRV is needed, it's stated that future versions will need more, but I don't see a reason to switch version atm :]
		m_pImguiDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, true, L"Imgui");
		imguiInitialize(window, m_pDevice, m_pCommandQueue, m_pImguiDescriptorHeap, MAX_FRAMES_IN_FLIGHT);
	}

	void Renderer::shutdown()
	{
		for (FrameResources& frame : m_frames)
			shutdowFrameResources(frame);

		for (FrameGarbage& frameGarbage : m_frameGarbage)
			D3D12_RELEASE(frameGarbage.pDxUnknown);

		D3D12_RELEASE(m_pDevice);
		D3D12_RELEASE(m_pCommandQueue);
		D3D12_RELEASE(m_pSwapChain);

		D3D12_RELEASE(m_pVoxelRootSignature);
		D3D12_RELEASE(m_pVoxelPSO);
		D3D12_RELEASE(m_pWaterPSO);
		D3D12_RELEASE(m_pTextureSheet);

		D3D12_RELEASE(m_pRTVDescHeap);
		D3D12_RELEASE(m_pDSVDescHeap);
		D3D12_RELEASE(m_pTextureDescHeap);

		m_gpuVertexData.shutdown();
		m_gpuIndicesData.shutdown();

		D3D12_RELEASE(m_pImguiDescriptorHeap);
		imguiShutdown();
	}

	void Renderer::onResize(uint32_t width, uint32_t height)
	{
		for (FrameResources& frame : m_frames)
		{
			wait(frame.pFence, frame.fenceValue);
			reset(frame.pCommandAllocator, frame.pCommandList);
			
			// Release all in-direct backBuffer references
			frame.pCommandList->ClearState(nullptr);

			execute(frame.pCommandList);
			signal(frame.pFence, frame.fenceValue);
			wait(frame.pFence, frame.fenceValue);

			// Release all direct backBuffer references
			D3D12_RELEASE(frame.pBackBuffer);
			D3D12_RELEASE(frame.pDepthTexture);
		}

		m_pSwapChain->ResizeBuffers(MAX_FRAMES_IN_FLIGHT, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		updateBackBufferTextures();
	}

	void Renderer::unloadChunks()
	{
		// Incase frames are still reading the vertex & index data
		for (FrameResources& frame : m_frames)
			wait(frame.pFence, frame.fenceValue);

		m_dxChunks.clear();
		m_gpuVertexData.clear();
		m_gpuIndicesData.clear();

		std::unique_lock lock(s_loadingChunksMutis);
		m_loadingChunkMesh.clear();
	}

	void Renderer::render(const World& world, const Camera& camera)
	{
		FrameResources& frame = getCurrentFrameResorces();
		wait(frame.pFence, frame.fenceValue);

		clearFrameGarbage();
		reset(frame.pCommandAllocator, frame.pCommandList);

		frame.ringBuffer.jumpToStart();
		frame.ringBuffer.map();

		updateBuffers(world, camera);
		preRender();
		renderWorld();
		postRender();

		frame.ringBuffer.unmap();
	}

	void Renderer::updateBuffers(const World& world, const Camera& camera)
	{
		FrameResources& frame = getCurrentFrameResorces();

		GPURenderData renderData = {};
		renderData.viewProjMatrix = glm::transpose(camera.getProjectionMatrix() * camera.transform.getViewMatrix());
		renderData.textureSheetTileSize = TEXTURE_SHEET_TILE_SIZE;
		renderData.textureSheetPadding = TEXTURE_SHEET_PADDING;

		m_renderDataGVA = frame.ringBuffer.allocate(&renderData, sizeof(renderData));

		updateChunks(world);
	}

	void Renderer::preRender()
	{
		FrameResources& frame = getCurrentFrameResorces();
		transitionResource(frame.pCommandList, frame.pBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		static const float CLEAR_COLOUR[4] = { 0.4f, 0.3f, 0.7f, 0.f };
		frame.pCommandList->ClearRenderTargetView(frame.cpuBackBufferRTV, CLEAR_COLOUR, 0, nullptr);
		frame.pCommandList->ClearDepthStencilView(frame.cpuDepthTextureDSV, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
	}

	void Renderer::renderWorld()
	{
		FrameResources& frame = getCurrentFrameResorces();

		frame.pCommandList->SetGraphicsRootSignature(m_pVoxelRootSignature);
		frame.pCommandList->SetPipelineState(m_pVoxelPSO);

		frame.pCommandList->SetDescriptorHeaps(1, &m_pTextureDescHeap);
		frame.pCommandList->SetGraphicsRootDescriptorTable(3, m_textureHandle);

		frame.pCommandList->SetGraphicsRootConstantBufferView(0, m_renderDataGVA);

		frame.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		frame.pCommandList->RSSetViewports(1, &frame.viewport);
		frame.pCommandList->RSSetScissorRects(1, &frame.scissorRect);
		frame.pCommandList->OMSetRenderTargets(1, &frame.cpuBackBufferRTV, false, &frame.cpuDepthTextureDSV);

		for (DXChunk& dxChunk : m_dxChunks)
		{
			GPUDrawCallData drawData = {};
			drawData.chunkWorldPos = chunkCoordToWorldCoord(chunkIDToChunkCoord(dxChunk.chunkID));
			dxChunk.drawDataGVA = frame.ringBuffer.allocate(&drawData, sizeof(drawData));

			drawGPUMeshInfo(dxChunk, dxChunk.blockGPUMeshInfo);
		}

		frame.pCommandList->SetPipelineState(m_pWaterPSO);

		for (const DXChunk& dxChunk : m_dxChunks)
		{
			drawGPUMeshInfo(dxChunk, dxChunk.waterGPUMeshInfo);
		}
	}

	void Renderer::postRender()
	{
		FrameResources& frame = getCurrentFrameResorces();
		
		frame.pCommandList->SetDescriptorHeaps(1, &m_pImguiDescriptorHeap);
		imguiEndFrame(frame.pCommandList);

		transitionResource(frame.pCommandList, frame.pBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		execute(frame.pCommandList);
		m_pSwapChain->Present(0, 0);

		signal(frame.pFence, frame.fenceValue);
	}

	void Renderer::drawGPUMeshInfo(const DXChunk& dxChunk, const GPUMeshInfo& gpuMeshInfo)
	{
		if (gpuMeshInfo.indicesCount == 0)
			return;

		FrameResources& frame = getCurrentFrameResorces();

		frame.pCommandList->IASetIndexBuffer(&gpuMeshInfo.indicesView);
		frame.pCommandList->SetGraphicsRootShaderResourceView(2, gpuMeshInfo.vertexDataGVA);
		frame.pCommandList->SetGraphicsRootConstantBufferView(1, dxChunk.drawDataGVA);
		frame.pCommandList->DrawIndexedInstanced(gpuMeshInfo.indicesCount, 1, 0, 0, 0);
	}

	void Renderer::signal(ID3D12Fence* pFence, uint64_t& fenceValue)
	{
		DX_CHECK(m_pCommandQueue->Signal(pFence, ++fenceValue));
	}

	void Renderer::execute(ID3D12GraphicsCommandList* pCommandList)
	{
		DX_CHECK(pCommandList->Close());
		m_pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&pCommandList);
	}

	void Renderer::wait(ID3D12Fence* pFence, uint64_t fenceValue)
	{
		if (pFence->GetCompletedValue() >= fenceValue)
			return;

		HANDLE eventHandle = CreateEventEx(nullptr, 0, 0, EVENT_ALL_ACCESS);
		DX_CHECK(pFence->SetEventOnCompletion(fenceValue, eventHandle));

		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	void Renderer::reset(ID3D12CommandAllocator* pCommandAlloator, ID3D12GraphicsCommandList* pCommandList)
	{
		DX_CHECK(pCommandAlloator->Reset());
		DX_CHECK(pCommandList->Reset(pCommandAlloator, nullptr));
	}

	void Renderer::flush(ID3D12GraphicsCommandList* pCommandList, ID3D12CommandAllocator* pCommandAlloator, ID3D12Fence* pFence, uint64_t& fenceValue)
	{
		execute(pCommandList);
		signal(pFence, fenceValue);
		wait(pFence, fenceValue);
		reset(pCommandAlloator, pCommandList);
	}

	void Renderer::addToFrameGarbage(IUnknown* pDxUnknown)
	{
		m_frameGarbage.emplace_back(m_pSwapChain->GetCurrentBackBufferIndex(), pDxUnknown);
	}

	void Renderer::clearFrameGarbage()
	{
		uint32_t currentFrameIdx = m_pSwapChain->GetCurrentBackBufferIndex();
		for (int i = (int)m_frameGarbage.size() - 1; i >= 0; i--)
		{
			if (m_frameGarbage[i].frameIdx == currentFrameIdx)
			{
				D3D12_RELEASE(m_frameGarbage[i].pDxUnknown);
				m_frameGarbage.erase(m_frameGarbage.begin() + i);
			}
		}
	}

	void Renderer::transitionResource(ID3D12GraphicsCommandList* pCommandList, ID3D12Resource* pResource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES newState, uint32_t subResource)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = pResource;
		barrier.Transition.StateBefore = beforeState;
		barrier.Transition.StateAfter = newState;
		barrier.Transition.Subresource = subResource;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		pCommandList->ResourceBarrier(1, &barrier);
	}

	void Renderer::updateDefaultHeapResource(ID3D12Resource* pTarget, uint64_t targetOffset, const void* pData, uint64_t dataSize)
	{
		FrameResources& frame = getCurrentFrameResorces();

		uint64_t uploadBufferOffset = frame.ringBuffer.getOffset();
		frame.ringBuffer.allocate(pData, dataSize);
		frame.pCommandList->CopyBufferRegion(pTarget, targetOffset, frame.ringBuffer.getDXResource(), uploadBufferOffset, dataSize);
	}

	static void addVertex(std::vector<uint32_t>& indices, std::vector<Vertex>& verticiesData, Vertex newVertex)
	{
		uint32_t idx = INVALID_UINT32;
		int startIdx = glm::max((int)verticiesData.size() - 5, 0); // Lmao
		for (uint64_t i = startIdx; i < verticiesData.size(); i++)
		{
			if (verticiesData[i] == newVertex)
			{
				idx = (uint32_t)i;
				break;
			}
		}

		if (idx == INVALID_UINT32)
		{
			indices.emplace_back((uint32_t)verticiesData.size());
			verticiesData.emplace_back(newVertex);
		}
		else
		{
			indices.emplace_back(idx);
		}
	}

	void Renderer::generateChunkMesh(const World* pWorld, ChunkID chunkID, uint32_t chunkGenID, ChunkMeshData& outMeshData)
	{
		glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkID);
		glm::ivec3 worldCoord = chunkCoordToWorldCoord(chunkCoord);

		outMeshData.blockMesh.vertices.reserve(MAX_BLOCKS_IN_CHUNK * 36ull);
		outMeshData.blockMesh.indices.reserve(MAX_BLOCKS_IN_CHUNK * 36ull);

		outMeshData.waterMesh.vertices.reserve(MAX_BLOCKS_IN_CHUNK * 36ull / 2);
		outMeshData.waterMesh.indices.reserve(MAX_BLOCKS_IN_CHUNK * 36ull / 2);

		for (uint32_t i = 0; i < MAX_BLOCKS_IN_CHUNK; i++)
		{
			std::shared_lock lock(s_loadingChunksMutis);

			const auto& chunkIterator = m_loadingChunkMesh.find(chunkID);
			if (chunkIterator == m_loadingChunkMesh.end())
				return;

			const ThreadSafeChunkMesh& threadChunk = chunkIterator->second;
			if (threadChunk.latestChunkGenID != chunkGenID)
				return;

			lock.unlock();


			BlockType block = pWorld->tryGetBlock(chunkID, i);
			if (block == BlockType::INVALID) // Chunk is no longer loaded
				return;

			if (block == BlockType::AIR)
				continue;


			glm::ivec3 chunkBlockCoord = chunkBlockIdxToChunkBlockCoord(i);
			glm::ivec3 worldBlockCoord = chunkBlockCoord + worldCoord;

			if (block == BlockType::WATER)
			{
				addWaterMeshData(pWorld, chunkBlockCoord, worldBlockCoord, outMeshData.waterMesh);
			}
			else
			{
				addBlockMeshData(pWorld, block, chunkBlockCoord, worldBlockCoord, outMeshData.blockMesh);
			}
		}
	}

	void Renderer::addBlockMeshData(const World* pWorld, BlockType block, const glm::ivec3& chunkBlockCoord, const glm::ivec3& worldBlockCoord, MeshData& outMeshData)
	{
		// Top
		if (!pWorld->isBlockCoordSolid(worldBlockCoord + UP_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::TOP);

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId, 0));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 1), glm::vec2(1, 1), textureId, 0));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 1), textureId, 0));

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 1), textureId, 0));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(0, 0), textureId, 0));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId, 0));
		}

		// Bottom
		if (!pWorld->isBlockCoordSolid(worldBlockCoord - UP_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::BOTTOM);

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), textureId, 1));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(1, 0), textureId, 1));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 0), textureId, 1));

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 0), textureId, 1));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 0), glm::vec2(0, 1), textureId, 1));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), textureId, 1));
		}

		// Right
		if (!pWorld->isBlockCoordSolid(worldBlockCoord + RIGHT_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::SIDE);

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(0, 0), textureId, 2));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(1, 0), textureId, 2));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), textureId, 2));

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(0, 0), textureId, 2));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), textureId, 2));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 0), glm::vec2(0, 1), textureId, 2));
		}

		// Left
		if (!pWorld->isBlockCoordSolid(worldBlockCoord - RIGHT_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::SIDE);

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(0, 1), textureId, 3));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 1), glm::vec2(0, 0), textureId, 3));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId, 3));

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(1, 1), textureId, 3));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(0, 1), textureId, 3));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId, 3));
		}

		// Forward
		if (!pWorld->isBlockCoordSolid(worldBlockCoord + FORWARD_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::SIDE);

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 0), textureId, 4));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 1), glm::vec2(1, 0), textureId, 4));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(1, 1), textureId, 4));

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(1, 1), textureId, 4));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(0, 1), textureId, 4));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 0), textureId, 4));
		}

		// Backward
		if (!pWorld->isBlockCoordSolid(worldBlockCoord - FORWARD_DIR))
		{
			uint32_t textureId = getTextureID(block, BlockSide::SIDE);

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 1), textureId, 5));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(0, 0), textureId, 5));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(1, 0), textureId, 5));

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(1, 0), textureId, 5));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 0), glm::vec2(1, 1), textureId, 5));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 1), textureId, 5));
		}
	}

	void Renderer::addWaterMeshData(const World* pWorld, const glm::ivec3& chunkBlockCoord, const glm::ivec3& worldBlockCoord, MeshData& outMeshData)
	{
		// Top
		if (pWorld->getBlockAtBlockCoord(worldBlockCoord + UP_DIR) != BlockType::WATER)
		{
			uint32_t textureId = getTextureID(BlockType::WATER, BlockSide::SIDE);

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 1), glm::vec2(1, 1), textureId));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 1), textureId));

			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 1), textureId));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(0, 0), textureId));
			addVertex(outMeshData.indices, outMeshData.vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), textureId));
		}
	}

	void Renderer::updateChunks(const World& world)
	{
		for (ChunkID chunkID : world.getRemovedChunks())
		{
			findAndDeleteDXChunk(chunkID);
		}

		std::unique_lock lock(s_loadingChunksMutis);
		processAddedChunks(world);
		processLoadingChunkMeshes(world);
	}

	void Renderer::processAddedChunks(const World& world)
	{
		// Not sure if should be static or not, prolly doesn't really matter :3
		static const glm::ivec2 OFFSETS[] =
		{
			glm::ivec2( 0,  0),
			glm::ivec2(-1,  0),
			glm::ivec2( 1,  0),
			glm::ivec2( 0, -1),
			glm::ivec2( 0,  1),
		};

		for (ChunkID chunkID : world.getAddedChunks())
		{
			glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkID);
			for (const glm::ivec2& offset : OFFSETS)
			{
				ChunkID adjacentChunkID = chunkCoordToChunkID(chunkCoord + offset);
				if (!world.isChunkLoaded(adjacentChunkID))
					continue;

				uint32_t chunkGenID = INVALID_UINT32;
				auto adjacentChunkIterator = m_loadingChunkMesh.find(adjacentChunkID);

				if (adjacentChunkIterator != m_loadingChunkMesh.end())
				{
					ThreadSafeChunkMesh& chunkMesh = adjacentChunkIterator->second;
					chunkGenID = ++chunkMesh.latestChunkGenID;
					chunkMesh.meshGenerated.store(false);
				}
				else
				{
					ThreadSafeChunkMesh& chunkMesh = m_loadingChunkMesh[adjacentChunkID];
					chunkMesh.latestChunkGenID = 0;
					chunkGenID = 0;
					chunkMesh.meshGenerated.store(false);
				}	

				const World* pWorld = &world;
				ThreadPool::queueJob([=]()
					{
						ChunkMeshData outMeshData;
						generateChunkMesh(pWorld, adjacentChunkID, chunkGenID, outMeshData);

						std::shared_lock lock(s_loadingChunksMutis);
						auto chunkIterator = m_loadingChunkMesh.find(adjacentChunkID);
						if (chunkIterator == m_loadingChunkMesh.end())
							return;

						ThreadSafeChunkMesh& threadChunk = chunkIterator->second;
						if (threadChunk.latestChunkGenID == chunkGenID)
						{
							threadChunk.meshData = std::move(outMeshData);
							threadChunk.meshGenerated.store(true);
						}
					});
			}
		}
	}

	void Renderer::processLoadingChunkMeshes(const World& world)
	{
		FrameResources& frame = getCurrentFrameResorces();

		bool meshResourcesTranitioned = false;
		auto chunkIterator = m_loadingChunkMesh.begin();
		while (chunkIterator != m_loadingChunkMesh.end())
		{
			ThreadSafeChunkMesh& threadChunk = chunkIterator->second;
			if (!threadChunk.meshGenerated.load())
			{
				++chunkIterator;
				continue;
			}

			ChunkID chunkID = chunkIterator->first;
			if (!world.isChunkLoaded(chunkID))
			{
				chunkIterator = m_loadingChunkMesh.erase(chunkIterator);
				continue;
			}

			if (!meshResourcesTranitioned)
			{
				meshResourcesTranitioned = true;
				transitionResource(frame.pCommandList, m_gpuVertexData.getDXResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
				transitionResource(frame.pCommandList, m_gpuIndicesData.getDXResource(), D3D12_RESOURCE_STATE_INDEX_BUFFER, D3D12_RESOURCE_STATE_COPY_DEST);
			}

			findAndDeleteDXChunk(chunkID);

			DXChunk& dxChunk = m_dxChunks.emplace_back();
			dxChunk.chunkID = chunkID;
			writeMeshData(dxChunk.blockGPUMeshInfo, threadChunk.meshData.blockMesh);
			writeMeshData(dxChunk.waterGPUMeshInfo, threadChunk.meshData.waterMesh);

			chunkIterator = m_loadingChunkMesh.erase(chunkIterator);
		}

		if (meshResourcesTranitioned)
		{
			transitionResource(frame.pCommandList, m_gpuVertexData.getDXResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			transitionResource(frame.pCommandList, m_gpuIndicesData.getDXResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);

			// Need to re-get the GVAs incase the arenas had to resize
			D3D12_GPU_VIRTUAL_ADDRESS verticesGVA = m_gpuVertexData.getDXResource()->GetGPUVirtualAddress();
			D3D12_GPU_VIRTUAL_ADDRESS indicesGVA = m_gpuIndicesData.getDXResource()->GetGPUVirtualAddress();
			for (DXChunk& dxChunk : m_dxChunks)
			{
				dxChunk.blockGPUMeshInfo.vertexDataGVA = verticesGVA + dxChunk.blockGPUMeshInfo.vertexDataSlot.offset;
				dxChunk.blockGPUMeshInfo.indicesView.BufferLocation = indicesGVA + dxChunk.blockGPUMeshInfo.indicesDataSlot.offset;

				dxChunk.waterGPUMeshInfo.vertexDataGVA = verticesGVA + dxChunk.waterGPUMeshInfo.vertexDataSlot.offset;
				dxChunk.waterGPUMeshInfo.indicesView.BufferLocation = indicesGVA + dxChunk.waterGPUMeshInfo.indicesDataSlot.offset;
			}
		}
	}

	void Renderer::writeMeshData(GPUMeshInfo& gpuMeshInfo, const MeshData& meshData)
	{
		if (!meshData.vertices.size())
		{
			gpuMeshInfo.indicesCount = 0;
			return;
		}

		uint64_t vertexDataSize = meshData.vertices.size() * sizeof(Vertex);
		uint64_t indexDataSize = meshData.indices.size() * sizeof(uint32_t);

		gpuMeshInfo.vertexDataGVA = allocateIntoResourceArena(m_gpuVertexData, &gpuMeshInfo.vertexDataSlot, meshData.vertices.data(), vertexDataSize);
		gpuMeshInfo.indicesCount = (uint32_t)meshData.indices.size();
		gpuMeshInfo.indicesView.BufferLocation = allocateIntoResourceArena(m_gpuIndicesData, &gpuMeshInfo.indicesDataSlot, meshData.indices.data(), indexDataSize);
		gpuMeshInfo.indicesView.Format = DXGI_FORMAT_R32_UINT;
		gpuMeshInfo.indicesView.SizeInBytes = (uint32_t)indexDataSize;

		updateDefaultHeapResource(m_gpuVertexData.getDXResource(), gpuMeshInfo.vertexDataSlot.offset, meshData.vertices.data(), vertexDataSize);
		updateDefaultHeapResource(m_gpuIndicesData.getDXResource(), gpuMeshInfo.indicesDataSlot.offset, meshData.indices.data(), indexDataSize);
	}

	void Renderer::findAndDeleteDXChunk(ChunkID chunkID)
	{
		for (uint64_t i = 0; i < m_dxChunks.size(); i++)
		{
			if (m_dxChunks[i].chunkID != chunkID)
				continue;

			m_gpuVertexData.removeAllocation(m_dxChunks[i].blockGPUMeshInfo.vertexDataSlot);
			m_gpuIndicesData.removeAllocation(m_dxChunks[i].blockGPUMeshInfo.indicesDataSlot);

			m_gpuVertexData.removeAllocation(m_dxChunks[i].waterGPUMeshInfo.vertexDataSlot);
			m_gpuIndicesData.removeAllocation(m_dxChunks[i].waterGPUMeshInfo.indicesDataSlot);

			m_dxChunks.erase(m_dxChunks.begin() + i);
			break;
		}
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Renderer::createRTVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_RENDER_TARGET_VIEW_DESC* pDesc)
	{
		OKAY_ASSERT(pResource || pDesc);

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += slotIdx * (uint64_t)m_rtvIncrementSize;
		m_pDevice->CreateRenderTargetView(pResource, pDesc, cpuHandle);

		return cpuHandle;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE Renderer::createDSVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_DEPTH_STENCIL_VIEW_DESC* pDesc)
	{
		OKAY_ASSERT(pResource || pDesc);

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += slotIdx * (uint64_t)m_dsvIncrementSize;
		m_pDevice->CreateDepthStencilView(pResource, pDesc, cpuHandle);
		
		return cpuHandle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::createSRVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* pDesc)
	{
		OKAY_ASSERT(pResource || pDesc);

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += slotIdx * (uint64_t)m_cbvSrvUavIncrementSize;
		m_pDevice->CreateShaderResourceView(pResource, pDesc, cpuHandle);

		// returns for convenience
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		gpuHandle.ptr += slotIdx * (uint64_t)m_cbvSrvUavIncrementSize;
		return gpuHandle;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE Renderer::createUAVDescriptor(ID3D12DescriptorHeap* pDescriptorHeap, uint32_t slotIdx, ID3D12Resource* pResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* pDesc)
	{
		OKAY_ASSERT(pResource || pDesc);

		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
		cpuHandle.ptr += slotIdx * (uint64_t)m_cbvSrvUavIncrementSize;
		m_pDevice->CreateUnorderedAccessView(pResource, nullptr, pDesc, cpuHandle);

		// returns for convenience
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
		gpuHandle.ptr += slotIdx * (uint64_t)m_cbvSrvUavIncrementSize;
		return gpuHandle;
	}

	D3D12_GPU_VIRTUAL_ADDRESS Renderer::allocateIntoResourceArena(ResourceArena& arena, ResourceSlot* pOutSlot, const void* pData, uint64_t dataSize)
	{
		FrameResources& frame = getCurrentFrameResorces();

		uint32_t allocatinonHandle = INVALID_UINT32;
		if (arena.findAllocationSlot(dataSize, &allocatinonHandle))
			return arena.claimAllocationSlot(dataSize, allocatinonHandle, pOutSlot);

		ID3D12Resource* pOldArenaResource = arena.getDXResource();
		uint64_t oldSize = pOldArenaResource->GetDesc().Width;
		arena.resize(frame.pCommandList, uint64_t((oldSize + dataSize) * 1.25f), D3D12_RESOURCE_STATE_COPY_DEST);

		addToFrameGarbage(pOldArenaResource);

		arena.findAllocationSlot(dataSize, &allocatinonHandle);
		return arena.claimAllocationSlot(dataSize, allocatinonHandle, pOutSlot);
	}

	FrameResources& Renderer::getCurrentFrameResorces()
	{
		return m_frames[m_pSwapChain->GetCurrentBackBufferIndex()];
	}

	void Renderer::enableDebugLayer()
	{
		ID3D12Debug* pDebugController = nullptr;
		DX_CHECK(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController)));

		pDebugController->EnableDebugLayer();
		D3D12_RELEASE(pDebugController);
	}

	void Renderer::enableGPUBasedValidation()
	{
		ID3D12Debug1* pDebugController = nullptr;
		DX_CHECK(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController)));

		pDebugController->SetEnableGPUBasedValidation(true);
		D3D12_RELEASE(pDebugController);
	}

	void Renderer::createDevice(IDXGIFactory* pFactory)
	{
		IDXGIAdapter* pAdapter = nullptr;
		uint32_t adapterIndex = 0;

		while (!pAdapter)
		{
			DX_CHECK(pFactory->EnumAdapters(adapterIndex, &pAdapter));

			HRESULT hr = D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device*), nullptr);
			if (FAILED(hr))
			{
				D3D12_RELEASE(pAdapter);
			}

			adapterIndex++;
		}

		D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_pDevice));
		D3D12_RELEASE(pAdapter);
	}

	void Renderer::createCommandQueue()
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc{};
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queueDesc.NodeMask = 0;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

		DX_CHECK(m_pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_pCommandQueue)));
	}

	void Renderer::createSwapChain(IDXGIFactory* pFactory, const Window& window)
	{
		DXGI_SWAP_CHAIN_DESC swapChainDesc{};
		swapChainDesc.BufferDesc.Width = 0;
		swapChainDesc.BufferDesc.Height = 0;
		swapChainDesc.BufferDesc.RefreshRate.Numerator = 0u;
		swapChainDesc.BufferDesc.RefreshRate.Denominator = 1u;
		swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;

		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = MAX_FRAMES_IN_FLIGHT;
		swapChainDesc.OutputWindow = window.getHWND();
		swapChainDesc.Windowed = true;

		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = 0;

		IDXGISwapChain* pSwapChain = nullptr;
		DX_CHECK(pFactory->CreateSwapChain(m_pCommandQueue, &swapChainDesc, &pSwapChain));

		m_pSwapChain = (IDXGISwapChain3*)pSwapChain;

		updateBackBufferTextures();
	}

	void Renderer::initializeFrameResources(FrameResources& frame, uint64_t ringBufferSize)
	{
		DX_CHECK(m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.pCommandAllocator)));
		DX_CHECK(m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame.pCommandAllocator, nullptr, IID_PPV_ARGS(&frame.pCommandList)));
		DX_CHECK(frame.pCommandList->Close());

		DX_CHECK(m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frame.pFence)));
		frame.fenceValue = 0;

		frame.ringBuffer.initialize(m_pDevice, ringBufferSize);
	}

	void Renderer::shutdowFrameResources(FrameResources& frame)
	{
		wait(frame.pFence, frame.fenceValue);

		D3D12_RELEASE(frame.pFence);
		D3D12_RELEASE(frame.pCommandAllocator);
		D3D12_RELEASE(frame.pCommandList);
		D3D12_RELEASE(frame.pBackBuffer);
		D3D12_RELEASE(frame.pDepthTexture);

		frame.ringBuffer.shutdown();
	}

	void Renderer::updateBackBufferTextures()
	{
		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 0;
		heapProperties.VisibleNodeMask = 0;

		D3D12_CLEAR_VALUE clearValue = {};
		clearValue.DepthStencil.Depth = 1.f;
		clearValue.DepthStencil.Stencil = 0;

		for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			FrameResources& frame = m_frames[i];

			// Back Buffer
			DX_CHECK(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&frame.pBackBuffer)));
			frame.cpuBackBufferRTV = createRTVDescriptor(m_pRTVDescHeap, i, frame.pBackBuffer, nullptr);


			// Depth
			D3D12_RESOURCE_DESC textureDesc = frame.pBackBuffer->GetDesc();
			textureDesc.Format = DXGI_FORMAT_D32_FLOAT;
			textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
			clearValue.Format = textureDesc.Format;
			DX_CHECK(m_pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&frame.pDepthTexture)));

			frame.cpuDepthTextureDSV = m_pDSVDescHeap->GetCPUDescriptorHandleForHeapStart();
			frame.cpuDepthTextureDSV.ptr += (uint64_t)i * m_dsvIncrementSize;

			m_pDevice->CreateDepthStencilView(frame.pDepthTexture, nullptr, frame.cpuDepthTextureDSV);


			// Viewport & ScissorRect
			frame.viewport.TopLeftX = 0;
			frame.viewport.TopLeftY = 0;
			frame.viewport.Width = (float)textureDesc.Width;
			frame.viewport.Height = (float)textureDesc.Height;
			frame.viewport.MinDepth = 0.f;
			frame.viewport.MaxDepth = 1.f;

			frame.scissorRect.left = 0;
			frame.scissorRect.top = 0;
			frame.scissorRect.right = (LONG)textureDesc.Width;
			frame.scissorRect.bottom = (LONG)textureDesc.Height;
		}
	}

	ID3D12DescriptorHeap* Renderer::createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, bool shaderVisible, std::wstring_view name)
	{
		D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
		descHeapDesc.NodeMask = 0;
		descHeapDesc.Type = type;
		descHeapDesc.NumDescriptors = numDescriptors;
		descHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		ID3D12DescriptorHeap* pDescriptorHeap = nullptr;
		DX_CHECK(m_pDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&pDescriptorHeap)));

		pDescriptorHeap->SetName(name.data());
		return pDescriptorHeap;
	}

	ID3D12Resource* Renderer::createCommittedBuffer(uint64_t size, D3D12_RESOURCE_STATES initialState, D3D12_HEAP_TYPE heapType, std::wstring_view name)
	{
		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		desc.Width = size;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.Format = DXGI_FORMAT_UNKNOWN;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = heapType;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 0;
		heapProperties.VisibleNodeMask = 0;

		ID3D12Resource* pResource = nullptr;
		DX_CHECK(m_pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, initialState, nullptr, IID_PPV_ARGS(&pResource)));

		pResource->SetName(name.data());
		return pResource;
	}

	ID3D12Resource* Renderer::createTextureSheet(FrameResources& frame)
	{
		std::unordered_map<BlockType, BlockTextures> textureNames;
		findBlockTextures(textureNames);

		std::unordered_map<std::string, uint32_t> textureNameToId;
		uint32_t textureID = 0;
		for (const auto& blockTextures : textureNames)
		{
			const BlockTextures& textures = blockTextures.second;
			for (const std::string& texture : textures.textures)
			{
				if (textureNameToId.contains(texture))
					continue;

				textureNameToId[texture] = textureID++;
			}
		}

		// Store texture IDs for use during rendering
		for (const auto& blockTextures : textureNames)
		{
			BlockType blockType = blockTextures.first;
			const BlockTextures& textures = blockTextures.second;

			for (uint32_t i = 0; i < 3; i++)
			{
				m_textureIds[blockType].sideIDs[i] = textureNameToId[textures.textures[i]];
			}
		}

		uint32_t numXTiles = (uint32_t)glm::ceil(glm::sqrt(textureNameToId.size()));
		uint32_t numYTiles = (uint32_t)glm::ceil((float)textureNameToId.size() / (float)numXTiles);

		uint32_t textureSheetWidth = numXTiles * TEXTURE_SHEET_TILE_SIZE + (numXTiles - 1) * TEXTURE_SHEET_PADDING;
		uint32_t textureSheetHeight = numYTiles * TEXTURE_SHEET_TILE_SIZE + (numYTiles - 1) * TEXTURE_SHEET_PADDING;

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		desc.Width = (uint64_t)textureSheetWidth;
		desc.Height = (uint32_t)textureSheetHeight;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 0; // 0 means max possible mipLevels (ID3D12Resource::GetDesc().MipLevels will be set to the real value)
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
		desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		D3D12_HEAP_PROPERTIES heapProperties = {};
		heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
		heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heapProperties.CreationNodeMask = 0;
		heapProperties.VisibleNodeMask = 0;

		ID3D12Resource* pTexture = nullptr;
		DX_CHECK(m_pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pTexture)));
		pTexture->SetName(L"TextureSheet");

		uploadTextureSheetData(pTexture, frame, textureNameToId);
		transitionResource(frame.pCommandList, pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

		return pTexture;
	}

	void Renderer::uploadTextureSheetData(ID3D12Resource* pTarget, FrameResources& frame, const std::unordered_map<std::string, uint32_t>& textureIds)
	{
		D3D12_RESOURCE_DESC resourceDesc = pTarget->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		uint64_t rowSizeInBytes = INVALID_UINT64;
		uint64_t totalSizeInBytes = INVALID_UINT64;
		m_pDevice->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &footprint, nullptr, &rowSizeInBytes, &totalSizeInBytes);

		uint32_t padding = TEXTURE_SHEET_PADDING;
		uint32_t tileSize = TEXTURE_SHEET_TILE_SIZE;

		uint32_t numXTiles = (uint32_t)resourceDesc.Width / (TEXTURE_SHEET_TILE_SIZE + padding) + 1;
		uint32_t numYTiles = resourceDesc.Height / (TEXTURE_SHEET_TILE_SIZE + padding) + 1;

		uint8_t* pMappedBuffer = frame.ringBuffer.map();

		// Copy source texture data into ringBuffer, taking padding into consideration
 		for (const auto& textureInfo : textureIds)
		{
			const std::string& textureName = textureInfo.first;
			uint32_t textureId = textureInfo.second;

			uint32_t xSlot = textureId % numXTiles;
			uint32_t ySlot = textureId / numXTiles;

			uint8_t* pTarget = pMappedBuffer +
				xSlot * (tileSize + padding) * 4 +
				ySlot * (tileSize + padding) * footprint.Footprint.RowPitch;

			int sourceWidth, sourceHeight;
			std::string texturePath = (TEXTURES_PATH / (textureName + ".png")).string();
			
			uint8_t* pSource = stbi_load(texturePath.c_str(), &sourceWidth, &sourceHeight, nullptr, STBI_rgb_alpha);
			OKAY_ASSERT(pSource);

			for (uint32_t i = 0; i < tileSize; i++)
			{
				memcpy(pTarget + i * footprint.Footprint.RowPitch, pSource + i * TEXTURE_SHEET_TILE_SIZE * 4, tileSize * 4ull);
			}

			stbi_image_free(pSource);
		}


		// Fill in column padding
		for (uint32_t xTex = 0; xTex < numXTiles - 1; xTex++)
		{
			uint8_t* pTarget = pMappedBuffer + tileSize * 4 + (xTex * (tileSize + padding) * 4);
			uint8_t* pSource = pTarget - 4;

			for (uint32_t j = 0; j < padding / 2; j++)
			{
				for (uint32_t i = 0; i < resourceDesc.Height; i++)
				{
					memcpy(pTarget + i * footprint.Footprint.RowPitch + j * 4, pSource + i * footprint.Footprint.RowPitch, 4ull);
				}
			}

			pTarget += (padding / 2) * 4;
			pSource = pTarget + padding * 2;

			for (uint32_t j = 0; j < padding / 2; j++)
			{
				for (uint32_t i = 0; i < resourceDesc.Height; i++)
				{
					memcpy(pTarget + i * footprint.Footprint.RowPitch + j * 4, pSource + i * footprint.Footprint.RowPitch, 4ull);
				}
			}
		}

		// Fill in row padding
		for (uint32_t yTex = 0; yTex < numYTiles - 1; yTex++)
		{
			uint8_t* pTarget = pMappedBuffer + tileSize * footprint.Footprint.RowPitch + (yTex * (tileSize + padding) * footprint.Footprint.RowPitch);
			uint8_t* pSource = pTarget - footprint.Footprint.RowPitch;

			for (uint32_t i = 0; i < padding / 2; i++)
			{
				memcpy(pTarget + i * footprint.Footprint.RowPitch, pSource, rowSizeInBytes);
			}

			pTarget += (padding / 2) * footprint.Footprint.RowPitch;
			pSource = pTarget + (padding / 2) * footprint.Footprint.RowPitch;

			for (uint32_t i = 0; i < padding / 2; i++)
			{
				memcpy(pTarget + i * footprint.Footprint.RowPitch, pSource, rowSizeInBytes);
			}
		}


		D3D12_TEXTURE_COPY_LOCATION copySource = {};
		copySource.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		copySource.PlacedFootprint = footprint;
		copySource.PlacedFootprint.Offset = frame.ringBuffer.getOffset();
		copySource.pResource = frame.ringBuffer.getDXResource();

		D3D12_TEXTURE_COPY_LOCATION copyDest = {};
		copyDest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		copyDest.SubresourceIndex = 0;
		copyDest.pResource = pTarget;

		frame.pCommandList->CopyTextureRegion(&copyDest, 0, 0, 0, &copySource, nullptr);
		frame.ringBuffer.unmap();
	}

	void Renderer::generateTextureSheetMipMaps(ID3D12Resource* pTextureSheet, uint32_t tileSize)
	{
		D3D12_RESOURCE_DESC textureDesc = pTextureSheet->GetDesc();

		ID3D12RootSignature* pComputeSignature = nullptr;
		ID3D12PipelineState* pComputePSO = nullptr;
		{
			D3D12_DESCRIPTOR_RANGE descRange[] =
			{
				createRangeSRV(0, 0, 1, 0),
				createRangeUAV(0, 0, 1, 1),
			};
			D3D12_ROOT_PARAMETER rootParam = createRootParamTable(D3D12_SHADER_VISIBILITY_ALL, descRange, _countof(descRange));

			D3D12_STATIC_SAMPLER_DESC samplerDesc = createDefaultStaticPointSamplerDesc();
			samplerDesc.Filter = D3D12_FILTER_ANISOTROPIC;
			samplerDesc.MaxAnisotropy = 16;
			samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
			samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

			D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
			rootDesc.NumParameters = 1;
			rootDesc.pParameters = &rootParam;
			rootDesc.NumStaticSamplers = 1;
			rootDesc.pStaticSamplers = &samplerDesc;
			rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

			pComputeSignature = createRootSignature(&rootDesc, L"TextureSheetMipMap");


			ID3DBlob* pShaderBlob = nullptr;
			D3D12_COMPUTE_PIPELINE_STATE_DESC pipelineDesc = {};
			pipelineDesc.pRootSignature = pComputeSignature;
			pipelineDesc.CS = compileShader(SHADER_PATH / "TextureSheetMipMapCS.hlsl", "cs_5_1", &pShaderBlob);
			pipelineDesc.NodeMask = 0;
			pipelineDesc.CachedPSO.pCachedBlob = nullptr;
			pipelineDesc.CachedPSO.CachedBlobSizeInBytes = 0;
			pipelineDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
			DX_CHECK(m_pDevice->CreateComputePipelineState(&pipelineDesc, IID_PPV_ARGS(&pComputePSO)));
			D3D12_RELEASE(pShaderBlob);
		}


		ID3D12DescriptorHeap* pDescriptorHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, textureDesc.MipLevels * 2, true, L"MipMapTextureSheet");
		ID3D12Resource* pUAVTexture = nullptr;
		{
			D3D12_HEAP_PROPERTIES heapProperties = {};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.CreationNodeMask = 0;
			heapProperties.VisibleNodeMask = 0;
			textureDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

			DX_CHECK(m_pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&pUAVTexture)));
		}


		ID3D12GraphicsCommandList* pComputeList = nullptr;
		ID3D12CommandAllocator* pComputeAllocator = nullptr;
		DX_CHECK(m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pComputeAllocator)));
		DX_CHECK(m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pComputeAllocator, nullptr, IID_PPV_ARGS(&pComputeList)));

		ID3D12Fence* pComputeFence = nullptr;
		uint64_t fenceValue = 0;
		DX_CHECK(m_pDevice->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pComputeFence)));


		D3D12_UNORDERED_ACCESS_VIEW_DESC targetUAV = {};
		D3D12_SHADER_RESOURCE_VIEW_DESC sourceSRV = {};
		{
			targetUAV.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			targetUAV.Texture2D.PlaneSlice = 0;
			targetUAV.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

			sourceSRV.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			sourceSRV.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			sourceSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			sourceSRV.Texture2D.MipLevels = 1;
			sourceSRV.Texture2D.PlaneSlice = 0;
			sourceSRV.Texture2D.ResourceMinLODClamp = 0;
		}


		transitionResource(pComputeList, pTextureSheet, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COPY_SOURCE);
		pComputeList->CopyResource(pUAVTexture, pTextureSheet);

		transitionResource(pComputeList, pUAVTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		transitionResource(pComputeList, pTextureSheet, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		pComputeList->SetComputeRootSignature(pComputeSignature);
		pComputeList->SetPipelineState(pComputePSO);
		pComputeList->SetDescriptorHeaps(1, &pDescriptorHeap);

		for (uint32_t i = 0; i < textureDesc.MipLevels - 1u; i++)
		{
			sourceSRV.Texture2D.MostDetailedMip = i;
			targetUAV.Texture2D.MipSlice = i + 1;

			D3D12_GPU_DESCRIPTOR_HANDLE descriptor0Handle = createSRVDescriptor(pDescriptorHeap, (i * 2), pUAVTexture, &sourceSRV);
			createUAVDescriptor(pDescriptorHeap, (i * 2) + 1, pUAVTexture, &targetUAV); // Descriptor 1 Handle

			uint32_t mipWidth = glm::max(1u, (uint32_t)textureDesc.Width / (uint32_t)glm::pow(2, i));
			uint32_t mipHeight = glm::max(1u, textureDesc.Height / (uint32_t)glm::pow(2, i));
			uint32_t xGroups = (uint32_t)glm::ceil(mipWidth / 16.f);
			uint32_t yGroups = (uint32_t)glm::ceil(mipHeight / 16.f);

			transitionResource(pComputeList, pUAVTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, i);
			pComputeList->SetComputeRootDescriptorTable(0, descriptor0Handle);
			pComputeList->Dispatch(xGroups, yGroups, 1);
		}

		transitionResource(pComputeList, pUAVTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, textureDesc.MipLevels - 1);
		transitionResource(pComputeList, pUAVTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_SOURCE);

		transitionResource(pComputeList, pTextureSheet, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_COPY_DEST);
		pComputeList->CopyResource(pTextureSheet, pUAVTexture);
		transitionResource(pComputeList, pTextureSheet, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

		flush(pComputeList, pComputeAllocator, pComputeFence, fenceValue);

		D3D12_RELEASE(pComputeAllocator);
		D3D12_RELEASE(pComputeList);
		D3D12_RELEASE(pComputeSignature);
		D3D12_RELEASE(pComputePSO);
		D3D12_RELEASE(pDescriptorHeap);
		D3D12_RELEASE(pUAVTexture);
		D3D12_RELEASE(pComputeFence);
	}

	uint32_t Renderer::getTextureID(BlockType blockType, BlockSide blockSide)
	{
		return m_textureIds[blockType].sideIDs[blockSide];
	}

	ID3D12RootSignature* Renderer::createRootSignature(const D3D12_ROOT_SIGNATURE_DESC* pDesc, std::wstring_view name)
	{
		ID3DBlob* pRootBlob = nullptr;
		ID3DBlob* pErrorBlob = nullptr;

		HRESULT hr = D3D12SerializeRootSignature(pDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &pRootBlob, &pErrorBlob);
		if (FAILED(hr))
		{
			printf("Root serialization failed: %s\n", pErrorBlob ? (char*)pErrorBlob->GetBufferPointer() : "No errors produced.");
			OKAY_ASSERT(false);
		}

		ID3D12RootSignature* pRootSignature = nullptr;
		DX_CHECK(m_pDevice->CreateRootSignature(0, pRootBlob->GetBufferPointer(), pRootBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSignature)));

		pRootSignature->SetName(name.data());

		D3D12_RELEASE(pRootBlob);
		D3D12_RELEASE(pErrorBlob);

		return pRootSignature;
	}

	void Renderer::createVoxelRenderPass()
	{
		std::vector<D3D12_ROOT_PARAMETER> rootParams;
		rootParams.emplace_back(createRootParamCBV(D3D12_SHADER_VISIBILITY_ALL, 0, 0));
		rootParams.emplace_back(createRootParamCBV(D3D12_SHADER_VISIBILITY_VERTEX, 1, 0));
		rootParams.emplace_back(createRootParamSRV(D3D12_SHADER_VISIBILITY_VERTEX, 0, 0));

		D3D12_DESCRIPTOR_RANGE textureRange = createRangeSRV(1, 0, 1, 0);
		rootParams.emplace_back(createRootParamTable(D3D12_SHADER_VISIBILITY_ALL, &textureRange, 1));

		D3D12_STATIC_SAMPLER_DESC pointSampler = createDefaultStaticPointSamplerDesc();
		pointSampler.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
		pointSampler.MaxLOD = 4;

		D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
		rootDesc.NumParameters = (uint32_t)rootParams.size();
		rootDesc.pParameters = rootParams.data();
		rootDesc.NumStaticSamplers = 1;
		rootDesc.pStaticSamplers = &pointSampler;
		rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

		m_pVoxelRootSignature = createRootSignature(&rootDesc, L"VoxelRootSignature");


		ID3DBlob* pShaderBlobs[5] = {};
		uint32_t shaderBlobIdx = 0;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = createDefaultGraphicsPipelineStateDesc();
		pipelineDesc.pRootSignature = m_pVoxelRootSignature;
		pipelineDesc.VS = compileShader(SHADER_PATH / "VertexShader.hlsl", "vs_5_1", &pShaderBlobs[shaderBlobIdx++]);
		pipelineDesc.PS = compileShader(SHADER_PATH / "PixelShader.hlsl", "ps_5_1", &pShaderBlobs[shaderBlobIdx++]);
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		DX_CHECK(m_pDevice->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&m_pVoxelPSO)));

		for (ID3DBlob*& pBlob : pShaderBlobs)
			D3D12_RELEASE(pBlob);

		
		pipelineDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
		pipelineDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		pipelineDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		pipelineDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
		pipelineDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		
		pipelineDesc.VS = compileShader(SHADER_PATH / "WaterVertexShader.hlsl", "vs_5_1", &pShaderBlobs[shaderBlobIdx++]);
		pipelineDesc.PS = compileShader(SHADER_PATH / "WaterPixelShader.hlsl", "ps_5_1", &pShaderBlobs[shaderBlobIdx++]);
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

		DX_CHECK(m_pDevice->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&m_pWaterPSO)));

		for (ID3DBlob*& pBlob : pShaderBlobs)
			D3D12_RELEASE(pBlob);
	}
}
