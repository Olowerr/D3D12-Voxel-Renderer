#include "Renderer.h"
#include "Engine/Application/Window.h"
#include "Engine/World/World.h"

namespace Okay
{
	struct RenderData
	{
		glm::mat4 viewProjMatrix = glm::mat4(1.f);
	};

	void Renderer::initialize(const Window& window)
	{
#ifdef _DEBUG
		enableDebugLayer();
		enableGPUBasedValidation();
#endif

		IDXGIFactory* pFactory = nullptr;
		DX_CHECK(CreateDXGIFactory(IID_PPV_ARGS(&pFactory)));

		createDevice(pFactory);
		createCommandQueue();

		m_pRTVDescHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, MAX_FRAMES_IN_FLIGHT, false, L"BackBufferRTVDescriptorHeap");
		m_pDSVDescHeap = createDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, MAX_FRAMES_IN_FLIGHT, false, L"DepthTextureDSVDescriptorHeap");
		m_rtvIncrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		m_dsvIncrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		createSwapChain(pFactory, window);
		D3D12_RELEASE(pFactory);

		for (FrameResources& frame : m_frames)
		{
			DX_CHECK(m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&frame.pCommandAllocator)));
			DX_CHECK(m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frame.pCommandAllocator, nullptr, IID_PPV_ARGS(&frame.pCommandList)));
			DX_CHECK(frame.pCommandList->Close());

			DX_CHECK(m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&frame.pFence)));
			frame.fenceValue = 0;

			frame.ringBuffer.initialize(m_pDevice, 10'000'000);
		}
	

		createVoxelRenderPass();
		m_pMeshResource = createCommittedBuffer(10'000'000, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_HEAP_TYPE_DEFAULT, L"MeshBuffer");
	}

	void Renderer::shutdown()
	{
		for (FrameResources& frame : m_frames)
		{
			wait(frame.pFence, frame.fenceValue);

			D3D12_RELEASE(frame.pFence);
			D3D12_RELEASE(frame.pCommandAllocator);
			D3D12_RELEASE(frame.pCommandList);
			D3D12_RELEASE(frame.pBackBuffer);
			D3D12_RELEASE(frame.pDepthTexture);

			frame.ringBuffer.shutdown();
		}

		D3D12_RELEASE(m_pDevice);
		D3D12_RELEASE(m_pCommandQueue);
		D3D12_RELEASE(m_pSwapChain);

		D3D12_RELEASE(m_pVoxelRootSignature);
		D3D12_RELEASE(m_pVoxelPSO);
		D3D12_RELEASE(m_pMeshResource);

		D3D12_RELEASE(m_pRTVDescHeap);
		D3D12_RELEASE(m_pDSVDescHeap);
	}

	void Renderer::render(const World& world)
	{
		FrameResources& frame = m_frames[m_pSwapChain->GetCurrentBackBufferIndex()];
		wait(frame.pFence, frame.fenceValue);
		reset(frame.pCommandAllocator, frame.pCommandList);

		frame.ringBuffer.jumpToStart();
		frame.ringBuffer.map();

		updateBuffers(world);
		preRender();
		renderWorld();
		postRender();

		frame.ringBuffer.unmap();
	}

	void Renderer::updateChunkData(const Chunk& chunk)
	{
		// Temp

		reset(m_frames[0].pCommandAllocator, m_frames[0].pCommandList);
		writeChunkDataToGPU(chunk, m_frames[0]);

		execute(m_frames[0].pCommandList);
		signal(m_frames[0].pFence, m_frames[0].fenceValue);
	}

	void Renderer::updateBuffers(const World& world)
	{
		const Camera& camera = world.getCameraConst();

		RenderData renderData = {};
		renderData.viewProjMatrix = glm::transpose(camera.getProjectionMatrix(m_viewport.Width, m_viewport.Height) * camera.transform.getViewMatrix());

		FrameResources& frame = m_frames[m_pSwapChain->GetCurrentBackBufferIndex()];
		m_renderDataGVA = frame.ringBuffer.allocate(&renderData, sizeof(renderData));
	}

	void Renderer::preRender()
	{
		FrameResources& frame = m_frames[m_pSwapChain->GetCurrentBackBufferIndex()];
		transitionResource(frame.pCommandList, frame.pBackBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		static const float CLEAR_COLOUR[4] = {0.4f, 0.3f, 0.7f, 0.f};
		frame.pCommandList->ClearRenderTargetView(frame.cpuBackBufferRTV, CLEAR_COLOUR, 0, nullptr);
		frame.pCommandList->ClearDepthStencilView(frame.cpuDepthTextureDSV, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);
	}

	void Renderer::renderWorld()
	{
		FrameResources& frame = m_frames[m_pSwapChain->GetCurrentBackBufferIndex()];

		frame.pCommandList->SetGraphicsRootSignature(m_pVoxelRootSignature);
		frame.pCommandList->SetPipelineState(m_pVoxelPSO);

		frame.pCommandList->SetGraphicsRootConstantBufferView(0, m_renderDataGVA);
		frame.pCommandList->SetGraphicsRootShaderResourceView(1, m_pMeshResource->GetGPUVirtualAddress());

		frame.pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		frame.pCommandList->RSSetViewports(1, &m_viewport);
		frame.pCommandList->RSSetScissorRects(1, &m_scissorRect);
		frame.pCommandList->OMSetRenderTargets(1, &frame.cpuBackBufferRTV, false, &frame.cpuDepthTextureDSV);

		// 36 temp
		frame.pCommandList->DrawInstanced(MAX_BLOCKS_IN_CHUNK * 36, 1, 0, 0);
	}

	void Renderer::postRender()
	{
		FrameResources& frame = m_frames[m_pSwapChain->GetCurrentBackBufferIndex()];

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

	void Renderer::transitionResource(ID3D12GraphicsCommandList* pCommandList, ID3D12Resource* pResource, D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES newState)
	{
		D3D12_RESOURCE_BARRIER barrier = {};
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = pResource;
		barrier.Transition.StateBefore = beforeState;
		barrier.Transition.StateAfter = newState;
		barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

		pCommandList->ResourceBarrier(1, &barrier);
	}

	void Renderer::updateDefaultHeapResource(ID3D12Resource* pTarget, uint64_t targetOffset, FrameResources& frame, void* pData, uint64_t dataSize)
	{
		bool mapped = frame.ringBuffer.getMappedPtr();
		if (!mapped)
			frame.ringBuffer.map();

		uint64_t uploadBufferOffset = frame.ringBuffer.getOffset();
		frame.ringBuffer.allocate(pData, dataSize);
		frame.pCommandList->CopyBufferRegion(pTarget, targetOffset, frame.ringBuffer.getDXResource(), uploadBufferOffset, dataSize);

		if (!mapped)
			frame.ringBuffer.unmap();
	}

	void Renderer::writeChunkDataToGPU(const Chunk& chunk, FrameResources& frame)
	{
		std::vector<Vertex> meshData;
		meshData.reserve(MAX_BLOCKS_IN_CHUNK * 36ull); // 36 verticies in a cube (without indices)

		for (uint32_t i = 0; i < MAX_BLOCKS_IN_CHUNK; i++)
		{
			if (chunk.blocks[i] == 0)
				continue;
			
			glm::vec3 chunkCoord = Chunk::blockIdxToChunkCoord(i);

			// Top
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 1.f, 0.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 1.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 1.f, 1.f));

			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 1.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 1.f, 0.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 1.f, 0.f));

			// Bottom
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 0.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 0.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 0.f, 0.f));

			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 0.f, 0.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 0.f, 0.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 0.f, 1.f));

			// Right
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 1.f, 0.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 1.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 0.f, 1.f));

			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 1.f, 0.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 0.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 0.f, 0.f));

			// Left
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 0.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 1.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 1.f, 0.f));

			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 0.f, 0.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 0.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 1.f, 0.f));

			// Forward
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 1.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 1.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 0.f, 1.f));

			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 0.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 0.f, 1.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 1.f, 1.f));

			// Backward
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 0.f, 0.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 1.f, 0.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 1.f, 0.f));

			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 1.f, 0.f));
			meshData.emplace_back(chunkCoord + glm::vec3(1.f, 0.f, 0.f));
			meshData.emplace_back(chunkCoord + glm::vec3(0.f, 0.f, 0.f));

		}

		// DBG
		for (Vertex& vertex : meshData)
			vertex.colour = glm::vec3(rand() / (float)RAND_MAX, rand() / (float)RAND_MAX, rand() / (float)RAND_MAX);

		updateDefaultHeapResource(m_pMeshResource, 0, frame, meshData.data(), meshData.size() * sizeof(Vertex));
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

			frame.cpuBackBufferRTV = m_pRTVDescHeap->GetCPUDescriptorHandleForHeapStart();
			frame.cpuBackBufferRTV.ptr += (uint64_t)i * m_rtvIncrementSize;

			m_pDevice->CreateRenderTargetView(frame.pBackBuffer, nullptr, frame.cpuBackBufferRTV);


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
		rootParams.emplace_back(createRootParamSRV(D3D12_SHADER_VISIBILITY_VERTEX, 0, 0));

		D3D12_ROOT_SIGNATURE_DESC rootDesc = {};
		rootDesc.NumParameters = (uint32_t)rootParams.size();
		rootDesc.pParameters = rootParams.data();
		rootDesc.NumStaticSamplers = 0;
		rootDesc.pStaticSamplers = nullptr;
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
