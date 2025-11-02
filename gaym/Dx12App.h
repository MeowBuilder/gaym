#include "stdafx.h"
#include "Timer.h"
#include "Scene.h"
#include "InputSystem.h" // Added InputSystem include
#include <memory>

class Dx12App
{
public:
    Dx12App();
    ~Dx12App();

    static Dx12App* GetInstance() { return s_pInstance; }

    void OnCreate(HINSTANCE hInstance, HWND hMainWnd);
    void OnDestroy();
    void FrameAdvance();
    void ToggleFullscreen();
    void OnResize(UINT nWidth, UINT nHeight);

    InputSystem& GetInputSystem() { return m_inputSystem; } // Added getter for InputSystem

    static ComPtr<ID3D12Resource> CreateBufferResource(const void* pData, UINT nBytes, D3D12_HEAP_TYPE d3dHeapType = D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATES d3dResourceStates = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, ComPtr<ID3D12Resource>* ppd3dUploadBuffer = NULL);

private:
    static Dx12App* s_pInstance;

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

    std::unique_ptr<Scene> m_pScene;
    InputSystem m_inputSystem; // Added InputSystem member
};