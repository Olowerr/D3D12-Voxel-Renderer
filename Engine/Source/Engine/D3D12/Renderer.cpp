#include "Renderer.h"
#include "Engine/Application/Window.h"
#include "Engine/World/World.h"
#include "Engine/Utilities/ThreadPool.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

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

	void Renderer::initialize(const Window& window)
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

		for (FrameResources& frame : m_frames)
			initializeFrameResources(frame, 100'000'000);

		createVoxelRenderPass();

		// See if you can reduce the size of these and expand when necessary (use FrameGarbage and replace the DXResource in ResourceArena)
		m_gpuVertexData.initialize(m_pDevice, 1'000'000, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
		m_gpuIndicesData.initialize(m_pDevice, 1'000'000, D3D12_RESOURCE_STATE_INDEX_BUFFER);

		FrameResources initFrame;
		initializeFrameResources(initFrame, 100'000);
		reset(initFrame.pCommandAllocator, initFrame.pCommandList);

		m_pTextureSheet = createTextureSheet(RESOURCES_PATH / "Textures" / "TextureSheet.png", initFrame, TEXTURE_SHEET_PADDING, TEXTURE_SHEET_TILE_SIZE, L"TextureSheet");
		m_textureHandle = createSRVDescriptor(m_pTextureDescHeap, 0, m_pTextureSheet, nullptr);

		flush(initFrame.pCommandList, initFrame.pCommandAllocator, initFrame.pFence, initFrame.fenceValue);
		shutdowFrameResources(initFrame);

		generateTextureSheetMipMaps(m_pTextureSheet, TEXTURE_SHEET_TILE_SIZE);
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
		D3D12_RELEASE(m_pTextureSheet);

		D3D12_RELEASE(m_pRTVDescHeap);
		D3D12_RELEASE(m_pDSVDescHeap);
		D3D12_RELEASE(m_pTextureDescHeap);

		m_gpuVertexData.shutdown();
		m_gpuIndicesData.shutdown();
	}

	void Renderer::render(const World& world)
	{
		FrameResources& frame = getCurrentFrameResorces();
		wait(frame.pFence, frame.fenceValue);

		clearFrameGarbage();
		reset(frame.pCommandAllocator, frame.pCommandList);

		//printf("frame: %u\n", m_pSwapChain->GetCurrentBackBufferIndex());
		frame.ringBuffer.jumpToStart();
		frame.ringBuffer.map();

		updateBuffers(world);
		preRender();
		renderWorld();
		postRender();

		frame.ringBuffer.unmap();
	}

	void Renderer::updateBuffers(const World& world)
	{
		const Camera& camera = world.getCameraConst();

		GPURenderData renderData = {};
		renderData.viewProjMatrix = glm::transpose(camera.getProjectionMatrix(m_viewport.Width, m_viewport.Height) * camera.transform.getViewMatrix());
		renderData.textureSheetTileSize = TEXTURE_SHEET_TILE_SIZE;
		renderData.textureSheetPadding = TEXTURE_SHEET_PADDING;

		FrameResources& frame = getCurrentFrameResorces();
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
		frame.pCommandList->RSSetViewports(1, &m_viewport);
		frame.pCommandList->RSSetScissorRects(1, &m_scissorRect);
		frame.pCommandList->OMSetRenderTargets(1, &frame.cpuBackBufferRTV, false, &frame.cpuDepthTextureDSV);

		for (const DXChunk& dxChunk : m_dxChunks)
		{
			frame.pCommandList->IASetIndexBuffer(&dxChunk.indicesView);
			frame.pCommandList->SetGraphicsRootShaderResourceView(2, dxChunk.vertexDataGVA);

			GPUDrawCallData drawData = {};
			drawData.chunkWorldPos = chunkCoordToWorldCoord(chunkIDToChunkCoord(dxChunk.chunkID));
			D3D12_GPU_VIRTUAL_ADDRESS drawDataGVA = frame.ringBuffer.allocate(&drawData, sizeof(drawData));
			frame.pCommandList->SetGraphicsRootConstantBufferView(1, drawDataGVA);

			frame.pCommandList->DrawIndexedInstanced(dxChunk.indicesCount, 1, 0, 0, 0);
		}
	}

	void Renderer::postRender()
	{
		FrameResources& frame = getCurrentFrameResorces();
		transitionResource(frame.pCommandList, frame.pBackBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		execute(frame.pCommandList);
		m_pSwapChain->Present(0, 0);

		signal(frame.pFence, frame.fenceValue);
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

	static void generateChunkMesh(const World* pWorld, ChunkID chunkID, ThreadSafeChunkMesh* pChunkMesh, uint32_t chunkGenID, ChunkMeshData& outMeshData)
	{
		glm::ivec2 chunkCoord = chunkIDToChunkCoord(chunkID);
		glm::ivec3 worldCoord = chunkCoordToWorldCoord(chunkCoord);

		std::vector<uint32_t>& indices = outMeshData.indices;
		std::vector<Vertex>& vertices = outMeshData.vertices;

		vertices.reserve(MAX_BLOCKS_IN_CHUNK * 36ull);
		indices.reserve(MAX_BLOCKS_IN_CHUNK * 36ull);

		for (uint32_t i = 0; i < MAX_BLOCKS_IN_CHUNK; i++)
		{
			if (pChunkMesh->latestChunkGenID != chunkGenID)
				return;

			uint8_t block = pWorld->tryGetBlock(chunkID, i);
			if (block == INVALID_UINT8) // Chunk is no longer loaded
				return;

			if (block == 0)
				continue;

			glm::ivec3 chunkBlockCoord = chunkBlockIdxToChunkBlockCoord(i);
			glm::ivec3 worldBlockCoord = chunkBlockCoord + worldCoord;

			uint32_t sideTextureIdx = 0; // should not be here but is oki for now :]

			/*
				I think this can be improved, addVertex doesn't need to be called for EVERY vertex right?
				tbh it might not even be neccessary at all...
				since the vertex can only be shared within the quad (since uv coords on other blocks still vary even for vertices with the same pos and textureID)
				so we KNOW(?) that 2 specific verticies can be shared (where the triangles meet), and the others are unique, RIGHT??
			*/

			// Top
			if (pWorld->getBlockAtBlockCoord(worldBlockCoord + UP_DIR) == 0)
			{
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), 2));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 1), glm::vec2(1, 1), 2));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 1), 2));

				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 1), 2));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(0, 0), 2));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), 2));

				sideTextureIdx = 1;
			}

			// Bottom
			if (pWorld->getBlockAtBlockCoord(worldBlockCoord - UP_DIR) == 0)
			{
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), 0));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(1, 0), 0));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 0), 0));

				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 0), 0));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 0), glm::vec2(0, 1), 0));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), 0));
			}

			// Right
			if (pWorld->getBlockAtBlockCoord(worldBlockCoord + RIGHT_DIR) == 0)
			{
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(0, 0), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(1, 0), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), sideTextureIdx));

				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(0, 0), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(1, 1), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 0), glm::vec2(0, 1), sideTextureIdx));
			}

			// Left
			if (pWorld->getBlockAtBlockCoord(worldBlockCoord - RIGHT_DIR) == 0)
			{
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(0, 1), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 1), glm::vec2(0, 0), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), sideTextureIdx));

				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(1, 1), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(0, 1), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(1, 0), sideTextureIdx));
			}

			// Forward
			if (pWorld->getBlockAtBlockCoord(worldBlockCoord + FORWARD_DIR) == 0)
			{
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 0), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 1), glm::vec2(1, 0), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(1, 1), sideTextureIdx));

				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 1), glm::vec2(1, 1), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 1), glm::vec2(0, 1), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 1), glm::vec2(0, 0), sideTextureIdx));
			}

			// Backward
			if (pWorld->getBlockAtBlockCoord(worldBlockCoord - FORWARD_DIR) == 0)
			{
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 1), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 1, 0), glm::vec2(0, 0), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(1, 0), sideTextureIdx));

				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 1, 0), glm::vec2(1, 0), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(1, 0, 0), glm::vec2(1, 1), sideTextureIdx));
				addVertex(indices, vertices, Vertex(chunkBlockCoord + glm::ivec3(0, 0, 0), glm::vec2(0, 1), sideTextureIdx));
			}
		}
	}

	void Renderer::updateChunks(const World& world)
	{
		for (ChunkID chunkID : world.getRemovedChunks())
		{
			findAndDeleteDXChunk(chunkID);
		}

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
				ThreadSafeChunkMesh* pThreadChunk = nullptr;

				if (adjacentChunkIterator != m_loadingChunkMesh.end())
				{
					pThreadChunk = &adjacentChunkIterator->second;
					chunkGenID = ++pThreadChunk->latestChunkGenID;
				}
				else
				{
					pThreadChunk = &m_loadingChunkMesh[adjacentChunkID];
					pThreadChunk->latestChunkGenID = 0;
					chunkGenID = 0;
				}

				pThreadChunk->meshGenerated.store(false);

				const World* pWorld = &world;
				ThreadPool::queueJob([=]()
					{
						ChunkMeshData outMeshData;
						generateChunkMesh(pWorld, adjacentChunkID, pThreadChunk, chunkGenID, outMeshData);

						if (pThreadChunk->latestChunkGenID == chunkGenID)
						{
							pThreadChunk->meshData = std::move(outMeshData);
							pThreadChunk->meshGenerated.store(true);
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
			writeChunkDataToGPU(chunkID, threadChunk.meshData);

			chunkIterator = m_loadingChunkMesh.erase(chunkIterator);
		}

		if (meshResourcesTranitioned)
		{
			transitionResource(frame.pCommandList, m_gpuVertexData.getDXResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			transitionResource(frame.pCommandList, m_gpuIndicesData.getDXResource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER);

			// Need to re-get the GVAs incase the arenas had to resize
			for (DXChunk& dxChunk : m_dxChunks)
			{
				dxChunk.vertexDataGVA = m_gpuVertexData.getDXResource()->GetGPUVirtualAddress() + dxChunk.vertexDataSlot.offset;
				dxChunk.indicesView.BufferLocation = m_gpuIndicesData.getDXResource()->GetGPUVirtualAddress() + dxChunk.indicesDataSlot.offset;
			}
		}
	}

	void Renderer::writeChunkDataToGPU(ChunkID chunkID, const ChunkMeshData& chunkMesh)
	{
		FrameResources& frame = getCurrentFrameResorces();

		uint64_t vertexDataSize = chunkMesh.vertices.size() * sizeof(Vertex);
		uint64_t indexDataSize = chunkMesh.indices.size() * sizeof(uint32_t);

		DXChunk& dxChunk = m_dxChunks.emplace_back();
		dxChunk.chunkID = chunkID;
		dxChunk.vertexDataGVA = allocateIntoResourceArena(m_gpuVertexData, &dxChunk.vertexDataSlot, chunkMesh.vertices.data(), vertexDataSize);
		dxChunk.indicesCount = (uint32_t)chunkMesh.indices.size();
		dxChunk.indicesView.BufferLocation = allocateIntoResourceArena(m_gpuIndicesData, &dxChunk.indicesDataSlot, chunkMesh.indices.data(), indexDataSize);
		dxChunk.indicesView.Format = DXGI_FORMAT_R32_UINT;
		dxChunk.indicesView.SizeInBytes = (uint32_t)indexDataSize;

		updateDefaultHeapResource(m_gpuVertexData.getDXResource(), dxChunk.vertexDataSlot.offset, chunkMesh.vertices.data(), vertexDataSize);
		updateDefaultHeapResource(m_gpuIndicesData.getDXResource(), dxChunk.indicesDataSlot.offset, chunkMesh.indices.data(), indexDataSize);
	}

	void Renderer::findAndDeleteDXChunk(ChunkID chunkID)
	{
		for (uint64_t i = 0; i < m_dxChunks.size(); i++)
		{
			if (m_dxChunks[i].chunkID != chunkID)
				continue;

			m_gpuVertexData.removeAllocation(m_dxChunks[i].vertexDataSlot);
			m_gpuIndicesData.removeAllocation(m_dxChunks[i].indicesDataSlot);
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

			D3D12_HEAP_PROPERTIES heapProperties = {};
			heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
			heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			heapProperties.CreationNodeMask = 0;
			heapProperties.VisibleNodeMask = 0;

			D3D12_CLEAR_VALUE clearValue = {};
			clearValue.Format = textureDesc.Format;
			clearValue.DepthStencil.Depth = 1.f;
			clearValue.DepthStencil.Stencil = 0;

			DX_CHECK(m_pDevice->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&frame.pDepthTexture)));

			frame.cpuDepthTextureDSV = m_pDSVDescHeap->GetCPUDescriptorHandleForHeapStart();
			frame.cpuDepthTextureDSV.ptr += (uint64_t)i * m_dsvIncrementSize;

			m_pDevice->CreateDepthStencilView(frame.pDepthTexture, nullptr, frame.cpuDepthTextureDSV);


			// Viewport & ScissorRect
			if (i == 0)
			{
				m_viewport.TopLeftX = 0;
				m_viewport.TopLeftY = 0;
				m_viewport.Width = (float)textureDesc.Width;
				m_viewport.Height = (float)textureDesc.Height;
				m_viewport.MinDepth = 0.f;
				m_viewport.MaxDepth = 1.f;

				m_scissorRect.left = 0;
				m_scissorRect.top = 0;
				m_scissorRect.right = (LONG)textureDesc.Width;
				m_scissorRect.bottom = (LONG)textureDesc.Height;
			}
		}
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

	ID3D12Resource* Renderer::createTextureSheet(const FilePath& filePath, FrameResources& frame, uint32_t padding, uint32_t tileSize, std::wstring_view name)
	{
		int textureWidth = 0;
		int textureHeight = 0;
		uint8_t* pTextureData = stbi_load(filePath.string().c_str(), &textureWidth, &textureHeight, nullptr, STBI_rgb_alpha);
		OKAY_ASSERT(pTextureData);

		uint32_t numXTiles = textureWidth / tileSize;
		uint32_t numYTiles = textureHeight / tileSize;

		D3D12_RESOURCE_DESC desc = {};
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
		desc.Width = (uint64_t)textureWidth + (numXTiles - 1ull) * padding;
		desc.Height = (uint32_t)textureHeight + (numYTiles - 1ull) * padding;
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

		pTexture->SetName(name.data());
		uploadTextureSheetData(pTexture, frame, pTextureData, (uint32_t)textureWidth, (uint32_t)textureHeight, padding, tileSize);
		transitionResource(frame.pCommandList, pTexture, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);

		return pTexture;
	}

	void Renderer::uploadTextureSheetData(ID3D12Resource* pTarget, FrameResources& frame, uint8_t* pTextureData, uint32_t origTextureWidth, uint32_t origTextureHeight, uint32_t padding, uint32_t tileSize)
	{
		D3D12_RESOURCE_DESC resourceDesc = pTarget->GetDesc();
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
		uint64_t rowSizeInBytes = INVALID_UINT64;
		uint64_t totalSizeInBytes = INVALID_UINT64;
		m_pDevice->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &footprint, nullptr, &rowSizeInBytes, &totalSizeInBytes);

		uint32_t numXTiles = origTextureWidth / tileSize;
		uint32_t numYTiles = origTextureHeight / tileSize;
		uint32_t sourceTextureRowPitch = tileSize * numXTiles * 4;

		uint8_t* pMappedBuffer = frame.ringBuffer.map();


		// Copy source texture data into ringBuffer, taking padding into consideration
		for (uint32_t yTex = 0; yTex < numYTiles; yTex++)
		{
			for (uint32_t xTex = 0; xTex < numXTiles; xTex++)
			{
				uint8_t* pTarget = pMappedBuffer +
					xTex * (tileSize + padding) * 4 +
					yTex * (tileSize + padding) * footprint.Footprint.RowPitch;

				uint8_t* pSource = pTextureData +
					xTex * tileSize * 4 +
					yTex * tileSize * sourceTextureRowPitch;

				for (uint32_t i = 0; i < tileSize; i++)
				{
					memcpy(pTarget + i * footprint.Footprint.RowPitch, pSource + i * sourceTextureRowPitch, tileSize * 4ull);
				}
			}
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
	}
}
