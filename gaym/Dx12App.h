// Dx12App.h
#pragma once
#include "stdafx.h"
#include <stdexcept>
#include <cstdint> // uint64_t

#ifndef CHECK_HR
#define CHECK_HR(x)													\
	do																\
	{																\
		HRESULT _hr__ = (x);										\
		if (FAILED(_hr__))											\
		{															\
			throw std::runtime_error("HRESULT failed: " #x);		\
		}															\
	} while (0)
#endif

class Dx12App
{
public:
	static constexpr UINT FrameCount = 2;

public:
	Dx12App();
	~Dx12App();

	void Initialize(HWND hwnd, UINT width, UINT height, bool enableDebug);
	void Cleanup();

	// 루프용
	void Render();
	void Resize(UINT width, UINT height);

	// 동기화
	void WaitForGPU();
	void MoveToNextFrame();

	// ★ 시간/프레임
	void Tick();							// 매 프레임 호출(델타/프레임/FPS 갱신)
	double GetDeltaTime() const { return m_deltaTime; }
	double GetElapsedTime() const { return m_elapsedTime; }
	double GetFPS() const { return m_fps; }
	uint64_t GetFrameCount() const { return m_frameCount; }

private:
	void CreateDeviceAndQueue(bool enableDebug);
	void CreateSwapChain();
	void CreateRTVs();
	void CreateFenceObjects();

	// ★ 시간 초기화
	void InitTime();

private:
	// 윈도우
	HWND m_hWnd = nullptr;
	UINT m_width = 0;
	UINT m_height = 0;

	// DXGI/Device/Queue
	Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory;
	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_cmdQueue;

	// 스왑체인/RTV
	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	UINT m_rtvDescriptorSize = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameCount];

	// 커맨드
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_cmdAlloc[FrameCount];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmdList;

	// 동기화
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue[FrameCount] = {};
	HANDLE m_fenceEvent = nullptr;
	UINT m_frameIndex = 0;

	// 뷰포트/시저
	D3D12_VIEWPORT m_viewport{};
	D3D12_RECT m_scissorRect{};

	// ★ 타이밍
	LARGE_INTEGER m_qpcFreq{};			// 초당 카운트
	LARGE_INTEGER m_qpcPrev{};			// 이전 카운터
	double m_deltaTime = 0.0;			// 이번 프레임 경과 시간(초)
	double m_elapsedTime = 0.0;			// 누적 시간(초)
	uint64_t m_frameCount = 0;			// 누적 프레임 수
	double m_fps = 0.0;					// 최근 FPS(1초 누적 평균)
	double m_fpsAccTime = 0.0;			// FPS 집계용 누적 시간
	uint32_t m_fpsAccFrames = 0;		// FPS 집계용 프레임 수
	bool m_showFpsInTitle = true;		// 타이틀에 FPS 표시할지 여부
};