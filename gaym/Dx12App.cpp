#include "stdafx.h"
#include "Dx12App.h"
using Microsoft::WRL::ComPtr;

Dx12App::Dx12App()
{
}

Dx12App::~Dx12App()
{
	Cleanup();
}

void Dx12App::Initialize(HWND hwnd, UINT width, UINT height, bool enableDebug)
{
	m_hWnd = hwnd;
	m_width = width;
	m_height = height;

	CreateDeviceAndQueue(enableDebug);
	CreateSwapChain();
	CreateRTVs();

	// 커맨드리스트/할로케이터
	for (UINT i = 0; i < FrameCount; ++i)
	{
		CHECK_HR(m_device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			IID_PPV_ARGS(m_cmdAlloc[i].ReleaseAndGetAddressOf())));
	}
	CHECK_HR(m_device->CreateCommandList(
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_cmdAlloc[m_frameIndex].Get(),
		nullptr,
		IID_PPV_ARGS(m_cmdList.ReleaseAndGetAddressOf())));
	CHECK_HR(m_cmdList->Close());

	// 뷰포트/시저
	m_viewport.TopLeftX = 0.0f;
	m_viewport.TopLeftY = 0.0f;
	m_viewport.Width = static_cast<float>(m_width);
	m_viewport.Height = static_cast<float>(m_height);
	m_viewport.MinDepth = 0.0f;
	m_viewport.MaxDepth = 1.0f;

	m_scissorRect.left = 0;
	m_scissorRect.top = 0;
	m_scissorRect.right = static_cast<LONG>(m_width);
	m_scissorRect.bottom = static_cast<LONG>(m_height);

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
	CreateFenceObjects();

	//  시간 시스템 초기화
	InitTime();

	// 초기 동기화(옵션)
	WaitForGPU();
}

void Dx12App::Cleanup()
{
	if (m_cmdQueue && m_fence)
	{
		WaitForGPU();
	}
	if (m_fenceEvent)
	{
		CloseHandle(m_fenceEvent);
		m_fenceEvent = nullptr;
	}
}

void Dx12App::CreateDeviceAndQueue(bool enableDebug)
{
#if _DEBUG
	if (enableDebug)
	{
		ComPtr<ID3D12Debug> debug;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
		{
			debug->EnableDebugLayer();
		}
	}
#endif

	UINT factoryFlags = 0;
#if _DEBUG
	if (enableDebug) factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	CHECK_HR(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(m_factory.ReleaseAndGetAddressOf())));

	// 어댑터 → 디바이스
	ComPtr<IDXGIAdapter1> adapter;
	for (UINT i = 0;
		m_factory->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND;
		++i)
	{
		DXGI_ADAPTER_DESC1 desc{};
		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;

		if (SUCCEEDED(D3D12CreateDevice(
			adapter.Get(), D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(m_device.ReleaseAndGetAddressOf()))))
		{
			break;
		}
	}
	if (!m_device)
	{
		CHECK_HR(m_factory->EnumWarpAdapter(IID_PPV_ARGS(adapter.ReleaseAndGetAddressOf())));
		CHECK_HR(D3D12CreateDevice(
			adapter.Get(), D3D_FEATURE_LEVEL_12_0,
			IID_PPV_ARGS(m_device.ReleaseAndGetAddressOf())));
	}

	// 큐
	D3D12_COMMAND_QUEUE_DESC qdesc{};
	qdesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	qdesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	CHECK_HR(m_device->CreateCommandQueue(&qdesc, IID_PPV_ARGS(m_cmdQueue.ReleaseAndGetAddressOf())));
}

void Dx12App::CreateSwapChain()
{
	if (m_swapChain)
	{
		CHECK_HR(m_swapChain->SetFullscreenState(FALSE, nullptr));
		m_swapChain.Reset();
	}

	DXGI_SWAP_CHAIN_DESC1 scDesc{};
	scDesc.Width = m_width;
	scDesc.Height = m_height;
	scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.SampleDesc.Count = 1;
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.BufferCount = FrameCount;
	scDesc.Scaling = DXGI_SCALING_STRETCH;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

	ComPtr<IDXGISwapChain1> sc1;
	CHECK_HR(m_factory->CreateSwapChainForHwnd(
		m_cmdQueue.Get(), m_hWnd, &scDesc, nullptr, nullptr,
		sc1.ReleaseAndGetAddressOf()));
	CHECK_HR(sc1.As(&m_swapChain));

	CHECK_HR(m_factory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER));
	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

void Dx12App::CreateRTVs()
{
	// RTV 힙
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.NumDescriptors = FrameCount;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	CHECK_HR(m_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(m_rtvHeap.ReleaseAndGetAddressOf())));

	m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// RTV 생성 (헬퍼 없이 수동 오프셋)
	D3D12_CPU_DESCRIPTOR_HANDLE handle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < FrameCount; ++i)
	{
		CHECK_HR(m_swapChain->GetBuffer(i, IID_PPV_ARGS(m_renderTargets[i].ReleaseAndGetAddressOf())));
		m_device->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, handle);
		handle.ptr += static_cast<SIZE_T>(m_rtvDescriptorSize);
	}
}

void Dx12App::CreateFenceObjects()
{
	CHECK_HR(m_device->CreateFence(
		0, D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS(m_fence.ReleaseAndGetAddressOf())));

	for (UINT i = 0; i < FrameCount; ++i)
	{
		m_fenceValue[i] = 0;
	}

	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (!m_fenceEvent)
	{
		CHECK_HR(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void Dx12App::WaitForGPU()
{
	if (!m_cmdQueue || !m_fence || !m_fenceEvent)
	{
		throw std::runtime_error("WaitForGPU called before queue/fence/event are ready.");
	}

	const UINT64 fenceToWait = ++m_fenceValue[m_frameIndex];
	CHECK_HR(m_cmdQueue->Signal(m_fence.Get(), fenceToWait));
	CHECK_HR(m_fence->SetEventOnCompletion(fenceToWait, m_fenceEvent));
	WaitForSingleObject(m_fenceEvent, INFINITE);
}

void Dx12App::MoveToNextFrame()
{
	const UINT64 fenceToSignal = ++m_fenceValue[m_frameIndex];
	CHECK_HR(m_cmdQueue->Signal(m_fence.Get(), fenceToSignal));

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	if (m_fence->GetCompletedValue() < m_fenceValue[m_frameIndex])
	{
		CHECK_HR(m_fence->SetEventOnCompletion(m_fenceValue[m_frameIndex], m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
}

void Dx12App::Render()
{
	Tick();
	// 리셋
	CHECK_HR(m_cmdAlloc[m_frameIndex]->Reset());
	CHECK_HR(m_cmdList->Reset(m_cmdAlloc[m_frameIndex].Get(), nullptr));

	// 리소스 배리어: PRESENT -> RENDER_TARGET
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = m_renderTargets[m_frameIndex].Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	m_cmdList->ResourceBarrier(1, &barrier);

	// RTV 핸들 계산
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += static_cast<SIZE_T>(m_frameIndex) * m_rtvDescriptorSize;

	// 렌더 타겟 바인딩/클리어
	m_cmdList->RSSetViewports(1, &m_viewport);
	m_cmdList->RSSetScissorRects(1, &m_scissorRect);
	m_cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	const FLOAT clearColor[4] = { 0.10f, 0.10f, 0.30f, 1.0f };
	m_cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

	// 리소스 배리어: RENDER_TARGET -> PRESENT
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	m_cmdList->ResourceBarrier(1, &barrier);

	// 제출
	CHECK_HR(m_cmdList->Close());
	ID3D12CommandList* lists[] = { m_cmdList.Get() };
	m_cmdQueue->ExecuteCommandLists(1, lists);

	// 표시
	CHECK_HR(m_swapChain->Present(1, 0));

	// 다음 프레임
	MoveToNextFrame();
}

void Dx12App::Resize(UINT width, UINT height)
{
	if (width == 0 || height == 0 || !m_swapChain)
		return;

	m_width = width;
	m_height = height;

	// GPU 멈춤
	WaitForGPU();

	// 기존 RT 해제
	for (UINT i = 0; i < FrameCount; ++i)
	{
		m_renderTargets[i].Reset();
	}
	m_rtvHeap.Reset();

	// 버퍼 리사이즈
	CHECK_HR(m_swapChain->ResizeBuffers(
		FrameCount,
		m_width, m_height,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		0));

	m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

	// RTV 재생성
	CreateRTVs();

	// 뷰포트/시저 갱신
	m_viewport.Width = static_cast<float>(m_width);
	m_viewport.Height = static_cast<float>(m_height);
	m_scissorRect.right = static_cast<LONG>(m_width);
	m_scissorRect.bottom = static_cast<LONG>(m_height);
}

void Dx12App::InitTime()
{
	// 고해상도 타이머 초기화
	QueryPerformanceFrequency(&m_qpcFreq);
	QueryPerformanceCounter(&m_qpcPrev);

	m_deltaTime = 0.0;
	m_elapsedTime = 0.0;
	m_frameCount = 0;
	m_fps = 0.0;
	m_fpsAccTime = 0.0;
	m_fpsAccFrames = 0;
}

void Dx12App::Tick()
{
	// 최소화 등으로 인해 비정상적으로 큰 dt를 방지하기 위한 상한(예: 100ms)
	const double MaxDelta = 0.1;

	LARGE_INTEGER now{};
	QueryPerformanceCounter(&now);

	const double dt = static_cast<double>(now.QuadPart - m_qpcPrev.QuadPart)
		/ static_cast<double>(m_qpcFreq.QuadPart);

	m_qpcPrev = now;

	// 너무 큰 dt는 클램프 (일시정지/중단 후 급격한 튐 방지)
	m_deltaTime = (dt > MaxDelta) ? MaxDelta : dt;

	m_elapsedTime += m_deltaTime;
	++m_frameCount;

	// FPS 집계 (1초 주기 평균)
	m_fpsAccTime += m_deltaTime;
	++m_fpsAccFrames;
	if (m_fpsAccTime >= 1.0)
	{
		m_fps = static_cast<double>(m_fpsAccFrames) / m_fpsAccTime;
		m_fpsAccTime = 0.0;
		m_fpsAccFrames = 0;

		if (m_showFpsInTitle && m_hWnd)
		{
			wchar_t title[256];
			swprintf_s(title, L"gaym - %.1f FPS (dt=%.3f ms)", m_fps, m_deltaTime * 1000.0);
			SetWindowTextW(m_hWnd, title);
		}
	}
}