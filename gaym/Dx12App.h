#pragma once

#include "stdafx.h"
#include "Timer.h"

class Dx12App
{
public:
    Dx12App();
    ~Dx12App();

    void OnCreate(HINSTANCE hInstance, HWND hMainWnd);
    void OnDestroy();
    void FrameAdvance();
    void ToggleFullscreen();
    void OnResize(UINT nWidth, UINT nHeight);

private:
    void CreateDirect3DDevice();
    void CreateCommandQueueAndList();
    void CreateSwapChain(HINSTANCE hInstance, HWND hMainWnd);
    void CreateRtvAndDsvDescriptorHeaps();
    void CreateRenderTargetViews();
    void CreateDepthStencilView();
    void WaitForGpuComplete();
    void UpdateFrameRate();

    HINSTANCE m_hInstance;
    HWND m_hWnd;

    UINT m_nWndClientWidth;
    UINT m_nWndClientHeight;

    ComPtr<ID3D12Device> m_pd3dDevice;
    ComPtr<ID3D12CommandQueue> m_pd3dCommandQueue;
    ComPtr<IDXGIFactory4> m_pdxgiFactory;
    ComPtr<IDXGISwapChain3> m_pdxgiSwapChain;
    ComPtr<ID3D12Resource> m_pd3dRenderTargetBuffers[kFrameCount];
    ComPtr<ID3D12DescriptorHeap> m_pd3dRtvDescriptorHeap;
    UINT m_nRtvDescriptorIncrementSize;

    ComPtr<ID3D12Resource> m_pd3dDepthStencilBuffer;
    ComPtr<ID3D12DescriptorHeap> m_pd3dDsvDescriptorHeap;

    ComPtr<ID3D12CommandAllocator> m_pd3dCommandAllocator;
    ComPtr<ID3D12GraphicsCommandList> m_pd3dCommandList;

    ComPtr<ID3D12Fence> m_pd3dFence;
    UINT64 m_nFenceValue;
    HANDLE m_hFenceEvent;

    UINT m_nSwapChainBufferIndex;

    CGameTimer m_GameTimer;
    bool m_bIsFullscreen;
};