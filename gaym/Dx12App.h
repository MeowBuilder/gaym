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

	// ������
	void Render();
	void Resize(UINT width, UINT height);

	// ����ȭ
	void WaitForGPU();
	void MoveToNextFrame();

	// �� �ð�/������
	void Tick();							// �� ������ ȣ��(��Ÿ/������/FPS ����)
	double GetDeltaTime() const { return m_deltaTime; }
	double GetElapsedTime() const { return m_elapsedTime; }
	double GetFPS() const { return m_fps; }
	uint64_t GetFrameCount() const { return m_frameCount; }

private:
	void CreateDeviceAndQueue(bool enableDebug);
	void CreateSwapChain();
	void CreateRTVs();
	void CreateFenceObjects();

	// �� �ð� �ʱ�ȭ
	void InitTime();

private:
	// ������
	HWND m_hWnd = nullptr;
	UINT m_width = 0;
	UINT m_height = 0;

	// DXGI/Device/Queue
	Microsoft::WRL::ComPtr<IDXGIFactory6> m_factory;
	Microsoft::WRL::ComPtr<ID3D12Device> m_device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_cmdQueue;

	// ����ü��/RTV
	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
	UINT m_rtvDescriptorSize = 0;
	Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FrameCount];

	// Ŀ�ǵ�
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_cmdAlloc[FrameCount];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_cmdList;

	// ����ȭ
	Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
	UINT64 m_fenceValue[FrameCount] = {};
	HANDLE m_fenceEvent = nullptr;
	UINT m_frameIndex = 0;

	// ����Ʈ/����
	D3D12_VIEWPORT m_viewport{};
	D3D12_RECT m_scissorRect{};

	// �� Ÿ�̹�
	LARGE_INTEGER m_qpcFreq{};			// �ʴ� ī��Ʈ
	LARGE_INTEGER m_qpcPrev{};			// ���� ī����
	double m_deltaTime = 0.0;			// �̹� ������ ��� �ð�(��)
	double m_elapsedTime = 0.0;			// ���� �ð�(��)
	uint64_t m_frameCount = 0;			// ���� ������ ��
	double m_fps = 0.0;					// �ֱ� FPS(1�� ���� ���)
	double m_fpsAccTime = 0.0;			// FPS ����� ���� �ð�
	uint32_t m_fpsAccFrames = 0;		// FPS ����� ������ ��
	bool m_showFpsInTitle = true;		// Ÿ��Ʋ�� FPS ǥ������ ����
};