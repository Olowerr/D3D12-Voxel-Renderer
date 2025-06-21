#include "Renderer.h"

namespace Okay
{
	void Renderer::initialize(const Window& window)
	{
		enableDebugLayer();
		enableGPUBasedValidation();

		IDXGIFactory* pFactory = nullptr;
		DX_CHECK(CreateDXGIFactory(IID_PPV_ARGS(&pFactory)));

		createDevice(pFactory);
		m_rtvIncrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		createCommandQueue();
		createDescriptorHeap(&m_pRTVDescHeap, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
		createSwapChain(pFactory, window);

		D3D12_RELEASE(pFactory);

		DX_CHECK(m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator)));
		DX_CHECK(m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator, nullptr, IID_PPV_ARGS(&m_pCommandList)));
		DX_CHECK(m_pCommandList->Close());

		DX_CHECK(m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)));
		m_fenceValue = 0;
	}

	void Renderer::shutdown()
	{
		wait(m_pFence, m_fenceValue);

		D3D12_RELEASE(m_pDevice);
		D3D12_RELEASE(m_pCommandQueue);
		D3D12_RELEASE(m_pSwapChain);

		D3D12_RELEASE(m_pFence);
		D3D12_RELEASE(m_pCommandAllocator);
		D3D12_RELEASE(m_pCommandList);

		D3D12_RELEASE(m_pRTVDescHeap);
		for (ID3D12Resource*& pBackBuffer : m_pBackBuffers)
		{
			D3D12_RELEASE(pBackBuffer);
		}
	}

	void Renderer::render()
	{
		preRender();
		renderWorld();
		postRender();
	}

	void Renderer::preRender()
	{
		wait(m_pFence, m_fenceValue);
		reset(m_pCommandAllocator, m_pCommandList);

		uint32_t currentBackBufferIdx = m_pSwapChain->GetCurrentBackBufferIndex();
		transitionResource(m_pCommandList, m_pBackBuffers[currentBackBufferIdx], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

		const static float CLEAR_COLOUR[4] = {0.4f, 0.3f, 0.7f, 0.f};
		m_pCommandList->ClearRenderTargetView(m_cpuBackBufferRTVs[currentBackBufferIdx], CLEAR_COLOUR, 0, nullptr);
	}

	void Renderer::renderWorld()
	{

	}

	void Renderer::postRender()
	{
		uint32_t currentBackBufferIdx = m_pSwapChain->GetCurrentBackBufferIndex();
		transitionResource(m_pCommandList, m_pBackBuffers[currentBackBufferIdx], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

		execute(m_pCommandList);
		m_pSwapChain->Present(0, 0);

		signal(m_fenceValue);
	}

	void Renderer::signal(uint64_t& fenceValue)
	{
		DX_CHECK(m_pCommandQueue->Signal(m_pFence, ++fenceValue));
	}

	void Renderer::execute(ID3D12GraphicsCommandList* pCommandList)
	{
		DX_CHECK(pCommandList->Close());
		m_pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&pCommandList);
	}

	void Renderer::wait(ID3D12Fence* pFence, uint64_t fenceValue)
	{
		if (pFence->GetCompletedValue() == fenceValue)
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
		swapChainDesc.BufferCount = 2; // Temp
		swapChainDesc.OutputWindow = window.getHWND();
		swapChainDesc.Windowed = true;

		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapChainDesc.Flags = 0;

		IDXGISwapChain* pSwapChain = nullptr;
		DX_CHECK(pFactory->CreateSwapChain(m_pCommandQueue, &swapChainDesc, &pSwapChain));

		m_pSwapChain = (IDXGISwapChain3*)pSwapChain;


		for (uint32_t i = 0; i < 2; i++)
		{
			DX_CHECK(m_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&m_pBackBuffers[i])));

			m_cpuBackBufferRTVs[i] = m_pRTVDescHeap->GetCPUDescriptorHandleForHeapStart();
			m_cpuBackBufferRTVs[i].ptr += (uint64_t)i * m_rtvIncrementSize;

			m_pDevice->CreateRenderTargetView(m_pBackBuffers[i], nullptr, m_cpuBackBufferRTVs[i]);
		}
	}

	void Renderer::createDescriptorHeap(ID3D12DescriptorHeap** ppDescriptorHeap, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t numDescriptors, bool shaderVisible)
	{
		D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
		descHeapDesc.NodeMask = 0;
		descHeapDesc.Type = type;
		descHeapDesc.NumDescriptors = numDescriptors;
		descHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

		DX_CHECK(m_pDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(ppDescriptorHeap)));
	}
}
