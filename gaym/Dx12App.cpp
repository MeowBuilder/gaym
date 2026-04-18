#include "stdafx.h"
#include "Dx12App.h"
#include "DamageNumberManager.h"
#include "Camera.h"
#include "d3dx12.h"
#include "SkillComponent.h"
#include "ISkillBehavior.h"
#include "SkillData.h"
#include "DropItemComponent.h"
#include "PlayerComponent.h"
#include "TransformComponent.h"
#include "Room.h"
#include "EnemyComponent.h"
#include <DescriptorHeap.h>  // DirectXTK12
#include <sstream>
#include <iomanip>

Dx12App* Dx12App::s_pInstance = nullptr;

Dx12App::Dx12App()
{
    s_pInstance = this;
    m_nWndClientWidth = kWindowWidth;
    m_nWndClientHeight = kWindowHeight;
    m_nSwapChainBufferIndex = 0;
    m_nFenceValue = 0;
    m_bIsFullscreen = false;

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
}

Dx12App::~Dx12App()
{
    CoUninitialize();
}

void Dx12App::OnCreate(HINSTANCE hInstance, HWND hMainWnd)
{
    m_hInstance = hInstance;
    m_hWnd = hMainWnd;

    CreateDirect3DDevice();
    CreateCommandQueueAndList();
    CreateSwapChain(hInstance, hMainWnd);
    CreateRtvAndDsvDescriptorHeaps();
    CreateRenderTargetViews();
    CreateDepthStencilView();
    CreateShadowMap();

    // CommandList를 열고 리소스 생성을 기록합니다.
    CHECK_HR(m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), NULL));

    m_pScene = std::make_unique<Scene>();
    m_pScene->Init(m_pd3dDevice.Get(), m_pd3dCommandList.Get());

    // Create Shadow Map SRV in Scene's descriptor heap
    CreateShadowMapSRV();

    // Update persistent descriptor watermark to include Shadow Map SRV
    // This prevents it from being overwritten during map transitions
    m_pScene->UpdatePersistentDescriptorEnd();

    // CommandList를 닫고 실행하여 리소스 업로드를 완료합니다.
    CHECK_HR(m_pd3dCommandList->Close());
    ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
    m_pd3dCommandQueue->ExecuteCommandLists(_countof(ppd3dCommandLists), ppd3dCommandLists);
    WaitForGpuComplete();

    // 텍스트 렌더링 초기화
    InitializeText();

    // 네트워크 초기화
    InitializeNetwork();

    m_GameTimer.Reset();
}

void Dx12App::OnDestroy()
{
    // 네트워크 정리
    if (m_pNetworkManager)
    {
        m_pNetworkManager->Shutdown();
        delete m_pNetworkManager;
        m_pNetworkManager = nullptr;
    }

    WaitForGpuComplete();
    if (m_pdxgiSwapChain)
    {
        m_pdxgiSwapChain->SetFullscreenState(FALSE, NULL);
    }
    CloseHandle(m_hFenceEvent);
}

void Dx12App::CreateDirect3DDevice()
{
    UINT nDXGIFactoryFlags = 0;
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> pd3dDebugController;
    D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&pd3dDebugController);
    pd3dDebugController->EnableDebugLayer();
    nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    CHECK_HR(CreateDXGIFactory2(nDXGIFactoryFlags, __uuidof(IDXGIFactory4), (void**)&m_pdxgiFactory));
    CHECK_HR(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), (void**)&m_pd3dDevice));
}

void Dx12App::CreateCommandQueueAndList()
{
    D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
    ::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
    d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    CHECK_HR(m_pd3dDevice->CreateCommandQueue(&d3dCommandQueueDesc, __uuidof(ID3D12CommandQueue), (void**)&m_pd3dCommandQueue));

    CHECK_HR(m_pd3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, __uuidof(ID3D12CommandAllocator), (void**)&m_pd3dCommandAllocator));
    CHECK_HR(m_pd3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pd3dCommandAllocator.Get(), NULL, __uuidof(ID3D12GraphicsCommandList), (void**)&m_pd3dCommandList));
    CHECK_HR(m_pd3dCommandList->Close());
}

void Dx12App::CreateSwapChain(HINSTANCE hInstance, HWND hMainWnd)
{
    DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc;
    ::ZeroMemory(&dxgiSwapChainDesc, sizeof(dxgiSwapChainDesc));
    dxgiSwapChainDesc.Width = m_nWndClientWidth;
    dxgiSwapChainDesc.Height = m_nWndClientHeight;
    dxgiSwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    dxgiSwapChainDesc.SampleDesc.Count = 1;
    dxgiSwapChainDesc.SampleDesc.Quality = 0;
    dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    dxgiSwapChainDesc.BufferCount = kFrameCount;
    dxgiSwapChainDesc.Scaling = DXGI_SCALING_NONE;
    dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    dxgiSwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    dxgiSwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    ComPtr<IDXGISwapChain1> pdxgiSwapChain1;
    CHECK_HR(m_pdxgiFactory->CreateSwapChainForHwnd(m_pd3dCommandQueue.Get(), hMainWnd, &dxgiSwapChainDesc, NULL, NULL, &pdxgiSwapChain1));
    CHECK_HR(pdxgiSwapChain1.As(&m_pdxgiSwapChain));
    m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

    CHECK_HR(m_pdxgiFactory->MakeWindowAssociation(hMainWnd, DXGI_MWA_NO_ALT_ENTER));

    CHECK_HR(m_pd3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, __uuidof(ID3D12Fence), (void**)&m_pd3dFence));
    m_hFenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
}

void Dx12App::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
    ::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
    d3dDescriptorHeapDesc.NumDescriptors = kFrameCount;
    d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    d3dDescriptorHeapDesc.NodeMask = 0;
    CHECK_HR(m_pd3dDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_pd3dRtvDescriptorHeap));
    m_nRtvDescriptorIncrementSize = m_pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    d3dDescriptorHeapDesc.NumDescriptors = 1;
    d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    CHECK_HR(m_pd3dDevice->CreateDescriptorHeap(&d3dDescriptorHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_pd3dDsvDescriptorHeap));
}

void Dx12App::CreateRenderTargetViews()
{
    D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    for (UINT i = 0; i < kFrameCount; i++)
    {
        CHECK_HR(m_pdxgiSwapChain->GetBuffer(i, __uuidof(ID3D12Resource), (void**)&m_pd3dRenderTargetBuffers[i]));
        m_pd3dDevice->CreateRenderTargetView(m_pd3dRenderTargetBuffers[i].Get(), NULL, d3dRtvCPUDescriptorHandle);
        d3dRtvCPUDescriptorHandle.ptr += m_nRtvDescriptorIncrementSize;
    }
}

void Dx12App::CreateDepthStencilView()
{
    D3D12_RESOURCE_DESC d3dResourceDesc;
    d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    d3dResourceDesc.Alignment = 0;
    d3dResourceDesc.Width = m_nWndClientWidth;
    d3dResourceDesc.Height = m_nWndClientHeight;
    d3dResourceDesc.DepthOrArraySize = 1;
    d3dResourceDesc.MipLevels = 1;
    d3dResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    d3dResourceDesc.SampleDesc.Count = 1;
    d3dResourceDesc.SampleDesc.Quality = 0;
    d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES d3dHeapProperties;
    ::ZeroMemory(&d3dHeapProperties, sizeof(D3D12_HEAP_PROPERTIES));
    d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    d3dHeapProperties.CreationNodeMask = 1;
    d3dHeapProperties.VisibleNodeMask = 1;

    D3D12_CLEAR_VALUE d3dClearValue;
    d3dClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    d3dClearValue.DepthStencil.Depth = 1.0f;
    d3dClearValue.DepthStencil.Stencil = 0;

    CHECK_HR(m_pd3dDevice->CreateCommittedResource(&d3dHeapProperties, D3D12_HEAP_FLAG_NONE, &d3dResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &d3dClearValue, __uuidof(ID3D12Resource), (void**)&m_pd3dDepthStencilBuffer));

    D3D12_DEPTH_STENCIL_VIEW_DESC d3dDepthStencilViewDesc;
    ::ZeroMemory(&d3dDepthStencilViewDesc, sizeof(D3D12_DEPTH_STENCIL_VIEW_DESC));
    d3dDepthStencilViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    d3dDepthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    d3dDepthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;

    D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = m_pd3dDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_pd3dDevice->CreateDepthStencilView(m_pd3dDepthStencilBuffer.Get(), &d3dDepthStencilViewDesc, d3dDsvCPUDescriptorHandle);
}

void Dx12App::CreateShadowMap()
{
    // Create Shadow Map texture resource
    D3D12_RESOURCE_DESC shadowMapDesc;
    shadowMapDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    shadowMapDesc.Alignment = 0;
    shadowMapDesc.Width = kShadowMapSize;
    shadowMapDesc.Height = kShadowMapSize;
    shadowMapDesc.DepthOrArraySize = 1;
    shadowMapDesc.MipLevels = 1;
    shadowMapDesc.Format = DXGI_FORMAT_R32_TYPELESS;  // Typeless for DSV/SRV compatibility
    shadowMapDesc.SampleDesc.Count = 1;
    shadowMapDesc.SampleDesc.Quality = 0;
    shadowMapDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    shadowMapDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_HEAP_PROPERTIES heapProperties;
    ::ZeroMemory(&heapProperties, sizeof(D3D12_HEAP_PROPERTIES));
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProperties.CreationNodeMask = 1;
    heapProperties.VisibleNodeMask = 1;

    D3D12_CLEAR_VALUE clearValue;
    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1.0f;
    clearValue.DepthStencil.Stencil = 0;

    CHECK_HR(m_pd3dDevice->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &shadowMapDesc,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &clearValue,
        __uuidof(ID3D12Resource),
        (void**)&m_pd3dShadowMap));

    // Create Shadow DSV Heap
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    ::ZeroMemory(&dsvHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    CHECK_HR(m_pd3dDevice->CreateDescriptorHeap(&dsvHeapDesc, __uuidof(ID3D12DescriptorHeap), (void**)&m_pd3dShadowDsvHeap));

    // Create Shadow DSV
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    ::ZeroMemory(&dsvDesc, sizeof(D3D12_DEPTH_STENCIL_VIEW_DESC));
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

    m_shadowDsvHandle = m_pd3dShadowDsvHeap->GetCPUDescriptorHandleForHeapStart();
    m_pd3dDevice->CreateDepthStencilView(m_pd3dShadowMap.Get(), &dsvDesc, m_shadowDsvHandle);
}

void Dx12App::CreateShadowMapSRV()
{
    // Allocate descriptor from Scene's heap
    D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle;
    m_pScene->AllocateDescriptor(&srvCpuHandle, &m_shadowSrvGpuHandle);

    // Create Shadow SRV in Scene's descriptor heap
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    ::ZeroMemory(&srvDesc, sizeof(D3D12_SHADER_RESOURCE_VIEW_DESC));
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    srvDesc.Texture2D.PlaneSlice = 0;

    m_pd3dDevice->CreateShaderResourceView(m_pd3dShadowMap.Get(), &srvDesc, srvCpuHandle);
}



void Dx12App::WaitForGpuComplete()
{
    const UINT64 nFence = ++m_nFenceValue;
    CHECK_HR(m_pd3dCommandQueue->Signal(m_pd3dFence.Get(), nFence));

    if (m_pd3dFence->GetCompletedValue() < nFence)
    {
        CHECK_HR(m_pd3dFence->SetEventOnCompletion(nFence, m_hFenceEvent));
        WaitForSingleObject(m_hFenceEvent, INFINITE);
    }
}

void Dx12App::ToggleFullscreen()
{
    WaitForGpuComplete();

    m_bIsFullscreen = !m_bIsFullscreen;

    CHECK_HR(m_pdxgiSwapChain->SetFullscreenState(m_bIsFullscreen, NULL));

    // Release the old buffers
    for (int i = 0; i < kFrameCount; ++i)
        m_pd3dRenderTargetBuffers[i].Reset();
    m_pd3dDepthStencilBuffer.Reset();

    DXGI_SWAP_CHAIN_DESC1 dxgiSwapChainDesc;
    CHECK_HR(m_pdxgiSwapChain->GetDesc1(&dxgiSwapChainDesc));

    CHECK_HR(m_pdxgiSwapChain->ResizeBuffers(kFrameCount, 0, 0, dxgiSwapChainDesc.Format, dxgiSwapChainDesc.Flags));

    m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

    CreateRenderTargetViews();
    CreateDepthStencilView();

    DXGI_SWAP_CHAIN_DESC1 newDesc;
    CHECK_HR(m_pdxgiSwapChain->GetDesc1(&newDesc));
    m_nWndClientWidth = newDesc.Width;
    m_nWndClientHeight = newDesc.Height;

    // Screen-Space Fluid 텍스처 리사이즈
    if (m_pScene)
        m_pScene->OnResizeSSF(m_nWndClientWidth, m_nWndClientHeight);
}

void Dx12App::UpdateFrameRate()
{
    WCHAR text[256];
    m_GameTimer.GetFrameRate(text, 256);
    ::SetWindowText(m_hWnd, text);
}

void Dx12App::FrameAdvance()
{
    m_GameTimer.Tick();

    // 네트워크 업데이트 (GPU 대기 전에 수행)
    float deltaTime = m_GameTimer.GetTimeElapsed();
    UpdateNetwork(deltaTime);

    WaitForGpuComplete();

    CHECK_HR(m_pd3dCommandAllocator->Reset());
    CHECK_HR(m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), NULL));

    // 네트워크 명령 처리 (메인 스레드에서 GameObject 생성/삭제)
    if (m_pNetworkManager)
    {
        m_pNetworkManager->Update(m_pScene.get(), m_pd3dDevice.Get(), m_pd3dCommandList.Get());
    }

    // Update scene first (calculates light matrices)
    m_pScene->Update(m_GameTimer.GetTimeElapsed(), &m_inputSystem);

    // Update damage number animations
    DamageNumberManager::Get().Update(m_GameTimer.GetTimeElapsed());

    // ========================================================================
    // Shadow Pass: Render depth from light's perspective
    // ========================================================================
    {
        // Set shadow map viewport
        D3D12_VIEWPORT shadowViewport = { 0, 0, (FLOAT)kShadowMapSize, (FLOAT)kShadowMapSize, 0.0f, 1.0f };
        m_pd3dCommandList->RSSetViewports(1, &shadowViewport);
        D3D12_RECT shadowScissorRect = { 0, 0, (LONG)kShadowMapSize, (LONG)kShadowMapSize };
        m_pd3dCommandList->RSSetScissorRects(1, &shadowScissorRect);

        // Clear shadow map
        m_pd3dCommandList->ClearDepthStencilView(m_shadowDsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

        // Set shadow map as render target (no color target)
        m_pd3dCommandList->OMSetRenderTargets(0, nullptr, FALSE, &m_shadowDsvHandle);

        // Render shadow casters
        m_pScene->RenderShadowPass(m_pd3dCommandList.Get());

        // Transition shadow map from depth write to shader resource
        D3D12_RESOURCE_BARRIER shadowBarrier;
        shadowBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        shadowBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        shadowBarrier.Transition.pResource = m_pd3dShadowMap.Get();
        shadowBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_DEPTH_WRITE;
        shadowBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        shadowBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        m_pd3dCommandList->ResourceBarrier(1, &shadowBarrier);
    }

    // ========================================================================
    // Main Pass: Render scene with shadows
    // ========================================================================
    D3D12_VIEWPORT viewport = { 0, 0, (FLOAT)m_nWndClientWidth, (FLOAT)m_nWndClientHeight, 0.0f, 1.0f };
    m_pd3dCommandList->RSSetViewports(1, &viewport);
    D3D12_RECT scissorRect = { 0, 0, (LONG)m_nWndClientWidth, (LONG)m_nWndClientHeight };
    m_pd3dCommandList->RSSetScissorRects(1, &scissorRect);

    D3D12_RESOURCE_BARRIER d3dResourceBarrier;
    d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    d3dResourceBarrier.Transition.pResource = m_pd3dRenderTargetBuffers[m_nSwapChainBufferIndex].Get();
    d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

    D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = m_pd3dRtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    d3dRtvCPUDescriptorHandle.ptr += (m_nSwapChainBufferIndex * m_nRtvDescriptorIncrementSize);

    float pfClearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
    m_pd3dCommandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle, pfClearColor, 0, NULL);

    D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = m_pd3dDsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    m_pd3dCommandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

    m_pd3dCommandList->OMSetRenderTargets(1, &d3dRtvCPUDescriptorHandle, FALSE, &d3dDsvCPUDescriptorHandle);

    // Handle drop interaction state
    DropInteractionState dropState = m_pScene->GetDropInteractionState();

    // Block regular rune input when in any drop interaction state
    GameObject* pPlayer = m_pScene->GetPlayer();
    if (pPlayer)
    {
        SkillComponent* pSkill = pPlayer->GetComponent<SkillComponent>();
        if (pSkill)
        {
            pSkill->SetRuneInputBlocked(dropState == DropInteractionState::SelectingRune ||
                                        dropState == DropInteractionState::SelectingSkill);
        }
    }

    if (dropState == DropInteractionState::SelectingRune)
    {
        // In rune selection mode - handle mouse clicks on rune options
        float screenCenterX = (float)m_nWndClientWidth / 2.0f;
        float screenCenterY = (float)m_nWndClientHeight / 2.0f;

        if (m_inputSystem.IsMouseButtonPressed(0))  // Left click
        {
            XMFLOAT2 mousePos = m_inputSystem.GetMousePosition();

            // Check if clicking on one of the 3 rune options
            float optionLineHeight = 55.0f;  // 렌더링과 동일하게
            for (int i = 0; i < 3; ++i)
            {
                float optionY = screenCenterY + i * optionLineHeight;
                float optionHeight = 45.0f;
                float optionWidth = 500.0f;

                if (mousePos.x >= screenCenterX - optionWidth / 2.0f &&
                    mousePos.x <= screenCenterX + optionWidth / 2.0f &&
                    mousePos.y >= optionY - 4.0f &&
                    mousePos.y <= optionY + optionHeight)
                {
                    m_pScene->SelectRuneByClick(i);
                    break;
                }
            }
        }

        // Also keep keyboard support
        if (m_inputSystem.IsKeyDown('1'))
        {
            m_pScene->SelectRuneByClick(0);
        }
        else if (m_inputSystem.IsKeyDown('2'))
        {
            m_pScene->SelectRuneByClick(1);
        }
        else if (m_inputSystem.IsKeyDown('3'))
        {
            m_pScene->SelectRuneByClick(2);
        }
        else if (m_inputSystem.IsKeyDown(VK_ESCAPE))
        {
            m_pScene->CancelDropInteraction();
        }
    }
    else if (dropState == DropInteractionState::SelectingSkill)
    {
        // In skill slot selection mode - handle mouse clicks on skill rune slots
        if (m_inputSystem.IsMouseButtonPressed(0))  // Left click
        {
            XMFLOAT2 mousePos = m_inputSystem.GetMousePosition();

            // Must match the UI rendering coordinates exactly!
            float screenCenterX = (float)m_nWndClientWidth / 2.0f;
            float screenCenterY = (float)m_nWndClientHeight / 2.0f;
            float slotStartY = screenCenterY - 20.0f;
            float lineHeight = 50.0f;  // 렌더링과 동일하게

            for (int skillIdx = 0; skillIdx < static_cast<int>(SkillSlot::Count); ++skillIdx)
            {
                float slotY = slotStartY + skillIdx * lineHeight;

                for (int runeIdx = 0; runeIdx < RUNES_PER_SKILL; ++runeIdx)
                {
                    float runeX = screenCenterX - 140.0f + runeIdx * 140.0f;
                    float runeWidth = 120.0f;
                    float runeHeight = 35.0f;

                    if (mousePos.x >= runeX && mousePos.x <= runeX + runeWidth &&
                        mousePos.y >= slotY && mousePos.y <= slotY + runeHeight)
                    {
                        m_pScene->SelectSkillSlot(static_cast<SkillSlot>(skillIdx), runeIdx);
                        break;
                    }
                }
            }
        }

        if (m_inputSystem.IsKeyDown(VK_ESCAPE))
        {
            m_pScene->CancelDropInteraction();
        }
    }
    else
    {
        // Normal mode - check for F key interactions
        // Priority: Drop item > Interaction cube
        if (m_inputSystem.IsKeyDown('F'))
        {
            if (m_pScene->IsNearDropItem())
            {
                m_pScene->StartDropInteraction();
            }
            else if (m_pScene->IsNearPortalCube())
            {
                m_pScene->TriggerPortalInteraction();
            }
            else if (m_pScene->IsNearInteractionCube())
            {
                m_pScene->TriggerInteraction();
            }
        }

        // DEBUG: T key = take 10 damage, Y key = heal 10
        GameObject* pPlayer = m_pScene->GetPlayer();
        if (pPlayer)
        {
            PlayerComponent* pPlayerComp = pPlayer->GetComponent<PlayerComponent>();
            if (pPlayerComp)
            {
                if (m_inputSystem.IsKeyPressed('T'))
                {
                    pPlayerComp->TakeDamage(10.0f);
                }
                if (m_inputSystem.IsKeyPressed('Y'))
                {
                    pPlayerComp->Heal(10.0f);
                }
            }
        }

        // DEBUG: K key = 현재 방 적 전원 즉사 (포탈/드랍 테스트용)
        if (m_inputSystem.IsKeyPressed('K'))
        {
            CRoom* pRoom = m_pScene->GetCurrentRoom();
            if (pRoom)
            {
                int killed = 0;
                for (EnemyComponent* pEnemy : pRoom->GetEnemies())
                {
                    if (pEnemy && !pEnemy->IsDead())
                    {
                        pEnemy->TakeDamage(99999.0f, false);
                        ++killed;
                    }
                }
                wchar_t buf[64];
                swprintf_s(buf, L"[Debug] K: killed %d enemies\n", killed);
                OutputDebugString(buf);
            }
        }
    }

    // Render scene with shadow map (mainRTV + mainDSV 전달)
    m_pScene->Render(m_pd3dCommandList.Get(), m_shadowSrvGpuHandle,
                     d3dRtvCPUDescriptorHandle, d3dDsvCPUDescriptorHandle,
                     m_pd3dRenderTargetBuffers[m_nSwapChainBufferIndex].Get());

    // Transition shadow map back to depth write for next frame
    D3D12_RESOURCE_BARRIER shadowBarrierBack;
    shadowBarrierBack.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    shadowBarrierBack.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    shadowBarrierBack.Transition.pResource = m_pd3dShadowMap.Get();
    shadowBarrierBack.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    shadowBarrierBack.Transition.StateAfter = D3D12_RESOURCE_STATE_DEPTH_WRITE;
    shadowBarrierBack.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    m_pd3dCommandList->ResourceBarrier(1, &shadowBarrierBack);

    // Text rendering (2D overlay on top of 3D scene)
    RenderText();

    d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);

    CHECK_HR(m_pd3dCommandList->Close());
    ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
    m_pd3dCommandQueue->ExecuteCommandLists(_countof(ppd3dCommandLists), ppd3dCommandLists);

    // DirectXTK12 GPU 메모리 커밋
    m_graphicsMemory->Commit(m_pd3dCommandQueue.Get());

    CHECK_HR(m_pdxgiSwapChain->Present(1, 0));

    m_nSwapChainBufferIndex = m_pdxgiSwapChain->GetCurrentBackBufferIndex();

    UpdateFrameRate();

    m_inputSystem.Reset(); // Reset input deltas for the next frame
}

void Dx12App::OnResize(UINT nWidth, UINT nHeight)
{
    if ((m_nWndClientWidth == nWidth && m_nWndClientHeight == nHeight) || nWidth == 0 || nHeight == 0)
    {
        return;
    }

    WaitForGpuComplete();

    m_nWndClientWidth = nWidth;
    m_nWndClientHeight = nHeight;

    for (int i = 0; i < kFrameCount; ++i)
        m_pd3dRenderTargetBuffers[i].Reset();
    m_pd3dDepthStencilBuffer.Reset();

    CHECK_HR(m_pdxgiSwapChain->ResizeBuffers(kFrameCount, m_nWndClientWidth, m_nWndClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    m_nSwapChainBufferIndex = 0;

    CreateRenderTargetViews();
    CreateDepthStencilView();

    // Screen-Space Fluid 텍스처 리사이즈
    if (m_pScene)
        m_pScene->OnResizeSSF(m_nWndClientWidth, m_nWndClientHeight);
}

ComPtr<ID3D12Resource> Dx12App::CreateBufferResource(const void* pData, UINT nBytes, D3D12_HEAP_TYPE d3dHeapType, D3D12_RESOURCE_STATES d3dResourceStates, ComPtr<ID3D12Resource>* ppd3dUploadBuffer)
{
    ComPtr<ID3D12Resource> pd3dBuffer;

    D3D12_HEAP_PROPERTIES d3dHeapProperties;
    ::ZeroMemory(&d3dHeapProperties, sizeof(D3D12_HEAP_PROPERTIES));
    d3dHeapProperties.Type = d3dHeapType;
    d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    d3dHeapProperties.CreationNodeMask = 1;
    d3dHeapProperties.VisibleNodeMask = 1;

    D3D12_RESOURCE_DESC d3dResourceDesc;
    ::ZeroMemory(&d3dResourceDesc, sizeof(D3D12_RESOURCE_DESC));
    d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    d3dResourceDesc.Alignment = 0;
    d3dResourceDesc.Width = nBytes;
    d3dResourceDesc.Height = 1;
    d3dResourceDesc.DepthOrArraySize = 1;
    d3dResourceDesc.MipLevels = 1;
    d3dResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    d3dResourceDesc.SampleDesc.Count = 1;
    d3dResourceDesc.SampleDesc.Quality = 0;
    d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    D3D12_RESOURCE_STATES d3dResourceInitialStates = D3D12_RESOURCE_STATE_COPY_DEST;
    if (d3dHeapType == D3D12_HEAP_TYPE_UPLOAD) d3dResourceInitialStates = D3D12_RESOURCE_STATE_GENERIC_READ;

    CHECK_HR(s_pInstance->m_pd3dDevice->CreateCommittedResource(&d3dHeapProperties, D3D12_HEAP_FLAG_NONE, &d3dResourceDesc, d3dResourceInitialStates, NULL, __uuidof(ID3D12Resource), (void**)&pd3dBuffer));

    if (pData)
    {
        if (d3dHeapType == D3D12_HEAP_TYPE_UPLOAD)
        {
            D3D12_RANGE d3dRange = { 0, 0 };
            UINT8* pBufferData = NULL;
            CHECK_HR(pd3dBuffer->Map(0, &d3dRange, (void**)&pBufferData));
            memcpy(pBufferData, pData, nBytes);
            pd3dBuffer->Unmap(0, NULL);
        }
        else
        {
            D3D12_HEAP_PROPERTIES d3dUploadHeapProperties;
            ::ZeroMemory(&d3dUploadHeapProperties, sizeof(D3D12_HEAP_PROPERTIES));
            d3dUploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
            d3dUploadHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            d3dUploadHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
            d3dUploadHeapProperties.CreationNodeMask = 1;
            d3dUploadHeapProperties.VisibleNodeMask = 1;

            CHECK_HR(s_pInstance->m_pd3dDevice->CreateCommittedResource(&d3dUploadHeapProperties, D3D12_HEAP_FLAG_NONE, &d3dResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, __uuidof(ID3D12Resource), (void**)ppd3dUploadBuffer));

            D3D12_RANGE d3dRange = { 0, 0 };
            UINT8* pBufferData = NULL;
            CHECK_HR((*ppd3dUploadBuffer)->Map(0, &d3dRange, (void**)&pBufferData));
            memcpy(pBufferData, pData, nBytes);
            (*ppd3dUploadBuffer)->Unmap(0, NULL);

            s_pInstance->m_pd3dCommandList->CopyResource(pd3dBuffer.Get(), (*ppd3dUploadBuffer).Get());

            D3D12_RESOURCE_BARRIER d3dResourceBarrier;
            ::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
            d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            d3dResourceBarrier.Transition.pResource = pd3dBuffer.Get();
            d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            d3dResourceBarrier.Transition.StateAfter = d3dResourceStates;
            d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            s_pInstance->m_pd3dCommandList->ResourceBarrier(1, &d3dResourceBarrier);
        }
    }
    return pd3dBuffer;
}

void Dx12App::InitializeText()
{
    // GraphicsMemory 초기화
    m_graphicsMemory = std::make_unique<DirectX::GraphicsMemory>(m_pd3dDevice.Get());

    // 폰트용 디스크립터 힙 생성 (폰트 1개 + 체력바 텍스처 2개 = 3개)
    m_fontDescriptorHeap = std::make_unique<DirectX::DescriptorHeap>(
        m_pd3dDevice.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        3  // 폰트 1개 + 체력바 base 1개 + 체력바 fill 1개
    );

    // 리소스 업로드 배치
    DirectX::ResourceUploadBatch resourceUpload(m_pd3dDevice.Get());
    resourceUpload.Begin();

    // SpriteBatch 생성 (알파 블렌딩 활성화)
    DirectX::RenderTargetState rtState(
        DXGI_FORMAT_R8G8B8A8_UNORM,      // 백버퍼 포맷
        DXGI_FORMAT_D24_UNORM_S8_UINT    // 깊이버퍼 포맷
    );

    // Non-premultiplied alpha blend state for PNG transparency
    CD3DX12_BLEND_DESC blendDesc(D3D12_DEFAULT);
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

    DirectX::SpriteBatchPipelineStateDescription pd(rtState, &blendDesc);
    m_spriteBatch = std::make_unique<DirectX::SpriteBatch>(m_pd3dDevice.Get(), resourceUpload, pd);

    // SpriteFont 로드
    m_spriteFont = std::make_unique<DirectX::SpriteFont>(
        m_pd3dDevice.Get(),
        resourceUpload,
        L"Fonts/myFont.spritefont",
        m_fontDescriptorHeap->GetCpuHandle(0),
        m_fontDescriptorHeap->GetGpuHandle(0)
    );

    // 업로드 완료 대기
    auto uploadFinished = resourceUpload.End(m_pd3dCommandQueue.Get());
    uploadFinished.wait();

    // 뷰포트 설정
    D3D12_VIEWPORT viewport = { 0, 0, (float)m_nWndClientWidth, (float)m_nWndClientHeight, 0, 1 };
    m_spriteBatch->SetViewport(viewport);

    // HealthBarUI 초기화 (디스크립터 인덱스 1, 2 사용)
    // CommandList를 열어서 텍스처 업로드
    CHECK_HR(m_pd3dCommandAllocator->Reset());
    CHECK_HR(m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), NULL));

    m_pHealthBarUI = std::make_unique<HealthBarUI>();
    m_pHealthBarUI->Initialize(m_pd3dDevice.Get(), m_pd3dCommandList.Get(),
                                m_fontDescriptorHeap.get(), 1);

    // CommandList를 닫고 실행
    CHECK_HR(m_pd3dCommandList->Close());
    ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
    m_pd3dCommandQueue->ExecuteCommandLists(_countof(ppd3dCommandLists), ppd3dCommandLists);
    WaitForGpuComplete();
}

void Dx12App::RenderText()
{
    // Bind descriptor heap
    ID3D12DescriptorHeap* heaps[] = { m_fontDescriptorHeap->Heap() };
    m_pd3dCommandList->SetDescriptorHeaps(1, heaps);

    m_spriteBatch->Begin(m_pd3dCommandList.Get());

    // ========== Player Health Bar ==========
    if (m_pScene && m_pHealthBarUI)
    {
        GameObject* pPlayer = m_pScene->GetPlayer();
        if (pPlayer)
        {
            PlayerComponent* pPlayerComp = pPlayer->GetComponent<PlayerComponent>();
            if (pPlayerComp)
            {
                m_pHealthBarUI->Render(m_spriteBatch.get(), pPlayerComp->GetHPRatio(),
                                        (float)m_nWndClientWidth, (float)m_nWndClientHeight);
            }
        }
    }

    // Show interaction prompt when near the cube
    if (m_pScene && m_pScene->IsInteractionCubeActive() && m_pScene->IsNearInteractionCube())
    {
        const wchar_t* interactionText = L"[F] Interact";
        float screenCenterX = (float)m_nWndClientWidth / 2.0f;
        float screenCenterY = (float)m_nWndClientHeight / 2.0f + 100.0f;
        XMVECTOR textSize = m_spriteFont->MeasureString(interactionText);
        float textWidth = XMVectorGetX(textSize);

        m_spriteFont->DrawString(
            m_spriteBatch.get(),
            interactionText,
            XMFLOAT2(screenCenterX - textWidth / 2.0f, screenCenterY),
            DirectX::Colors::Yellow
        );
    }

    // ========== Drop Interaction UI ==========
    if (m_pScene)
    {
        DropInteractionState dropState = m_pScene->GetDropInteractionState();
        float screenCenterX = (float)m_nWndClientWidth / 2.0f;
        float screenCenterY = (float)m_nWndClientHeight / 2.0f;

        if (dropState == DropInteractionState::SelectingRune)
        {
            // Show rune selection UI with clickable buttons
            CRoom* pRoom = m_pScene->GetCurrentRoom();
            if (pRoom)
            {
                GameObject* pDropItem = pRoom->GetDropItem();
                if (pDropItem)
                {
                    DropItemComponent* pDropComp = pDropItem->GetComponent<DropItemComponent>();
                    if (pDropComp)
                    {
                        const wchar_t* typeNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance", L"Split" };
                        const wchar_t* typeDescs[] = {
                            L"Empty",
                            L"1x damage",
                            L"Hold: 1x~3x damage",
                            L"Hold: 0.3x per tick",
                            L"1.5x trap damage",
                            L"Buff: 2x next attack",
                            L"Fire 2 projectiles"
                        };

                        // Title
                        const wchar_t* titleText = L"=== Click to Select a Rune ===";
                        XMVECTOR titleSize = m_spriteFont->MeasureString(titleText);
                        m_spriteFont->DrawString(m_spriteBatch.get(), titleText,
                            XMFLOAT2(screenCenterX - XMVectorGetX(titleSize) / 2.0f, screenCenterY - 60.0f),
                            DirectX::Colors::Gold);

                        // Rune options (clickable)
                        XMFLOAT2 mousePos = m_inputSystem.GetMousePosition();
                        float optionLineHeight = 55.0f;  // 간격 넓힘 (40 -> 55)
                        for (int i = 0; i < 3; ++i)
                        {
                            ActivationType runeType = pDropComp->GetRuneOption(i);
                            int typeIndex = static_cast<int>(runeType);

                            std::wstringstream optionText;
                            optionText << L"> " << typeNames[typeIndex] << L" - " << typeDescs[typeIndex];

                            float optionY = screenCenterY + i * optionLineHeight;
                            XMVECTOR optionSize = m_spriteFont->MeasureString(optionText.str().c_str());
                            float optionWidth = XMVectorGetX(optionSize);

                            // Check if mouse is hovering
                            bool isHovered = (mousePos.x >= screenCenterX - optionWidth / 2.0f &&
                                              mousePos.x <= screenCenterX + optionWidth / 2.0f &&
                                              mousePos.y >= optionY - 4.0f &&
                                              mousePos.y <= optionY + 32.0f);

                            m_spriteFont->DrawString(m_spriteBatch.get(), optionText.str().c_str(),
                                XMFLOAT2(screenCenterX - optionWidth / 2.0f, optionY),
                                isHovered ? DirectX::Colors::Yellow : DirectX::Colors::White);
                        }

                        // Cancel hint
                        const wchar_t* cancelText = L"[ESC] Cancel";
                        XMVECTOR cancelSize = m_spriteFont->MeasureString(cancelText);
                        m_spriteFont->DrawString(m_spriteBatch.get(), cancelText,
                            XMFLOAT2(screenCenterX - XMVectorGetX(cancelSize) / 2.0f, screenCenterY + 180.0f),
                            DirectX::Colors::Gray);
                    }
                }
            }
        }
        else if (dropState == DropInteractionState::SelectingSkill)
        {
            // Show skill slot selection UI
            const wchar_t* titleText = L"=== Click a Rune Slot to Assign ===";
            XMVECTOR titleSize = m_spriteFont->MeasureString(titleText);
            m_spriteFont->DrawString(m_spriteBatch.get(), titleText,
                XMFLOAT2(screenCenterX - XMVectorGetX(titleSize) / 2.0f, screenCenterY - 100.0f),
                DirectX::Colors::Gold);

            // Show selected rune info
            const wchar_t* typeNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance", L"Split" };
            ActivationType selectedRune = m_pScene->GetSelectedRune();
            std::wstringstream selectedText;
            selectedText << L"Selected Rune: " << typeNames[static_cast<int>(selectedRune)];
            XMVECTOR selectedSize = m_spriteFont->MeasureString(selectedText.str().c_str());
            m_spriteFont->DrawString(m_spriteBatch.get(), selectedText.str().c_str(),
                XMFLOAT2(screenCenterX - XMVectorGetX(selectedSize) / 2.0f, screenCenterY - 60.0f),
                DirectX::Colors::Cyan);

            // Show skill slots with rune slots (click on empty slot to assign)
            const wchar_t* slotNames[] = { L"Q", L"E", L"R", L"RMB" };
            GameObject* pPlayer = m_pScene->GetPlayer();
            SkillComponent* pSkill = pPlayer ? pPlayer->GetComponent<SkillComponent>() : nullptr;

            XMFLOAT2 mousePos = m_inputSystem.GetMousePosition();
            float slotStartY = screenCenterY - 20.0f;
            float lineHeight = 50.0f;  // 간격 넓힘 (40 -> 50)

            for (int skillIdx = 0; skillIdx < static_cast<int>(SkillSlot::Count); ++skillIdx)
            {
                float slotY = slotStartY + skillIdx * lineHeight;

                // Skill name
                std::wstringstream skillText;
                skillText << L"[" << slotNames[skillIdx] << L"] ";
                m_spriteFont->DrawString(m_spriteBatch.get(), skillText.str().c_str(),
                    XMFLOAT2(screenCenterX - 250.0f, slotY), DirectX::Colors::White);

                // Rune slots (3 boxes)
                for (int runeIdx = 0; runeIdx < RUNES_PER_SKILL; ++runeIdx)
                {
                    float runeX = screenCenterX - 140.0f + runeIdx * 140.0f;
                    float runeWidth = 120.0f;
                    float runeHeight = 35.0f;

                    ActivationType runeType = pSkill ? pSkill->GetRuneSlot(static_cast<SkillSlot>(skillIdx), runeIdx) : ActivationType::None;

                    // Check hover
                    bool isHovered = (mousePos.x >= runeX && mousePos.x <= runeX + runeWidth &&
                                      mousePos.y >= slotY && mousePos.y <= slotY + runeHeight);

                    // Draw slot
                    const wchar_t* runeName = (runeType == ActivationType::None) ? L"[Empty]" : typeNames[static_cast<int>(runeType)];
                    XMVECTORF32 color = (runeType == ActivationType::None)
                        ? (isHovered ? DirectX::Colors::Yellow : DirectX::Colors::DarkGray)
                        : DirectX::Colors::Cyan;

                    m_spriteFont->DrawString(m_spriteBatch.get(), runeName, XMFLOAT2(runeX, slotY), color);
                }
            }

            // Cancel hint
            const wchar_t* cancelText = L"[ESC] Cancel";
            XMVECTOR cancelSize = m_spriteFont->MeasureString(cancelText);
            m_spriteFont->DrawString(m_spriteBatch.get(), cancelText,
                XMFLOAT2(screenCenterX - XMVectorGetX(cancelSize) / 2.0f, slotStartY + 220.0f),
                DirectX::Colors::Gray);
        }
        else if (m_pScene->IsNearDropItem())
        {
            // Show pickup prompt
            const wchar_t* pickupText = L"[F] Pick up Rune";
            XMVECTOR textSize = m_spriteFont->MeasureString(pickupText);
            m_spriteFont->DrawString(m_spriteBatch.get(), pickupText,
                XMFLOAT2(screenCenterX - XMVectorGetX(textSize) / 2.0f, screenCenterY + 100.0f),
                DirectX::Colors::Cyan);
        }
        else if (m_pScene->IsNearPortalCube())
        {
            // Show portal prompt
            const wchar_t* portalText = L"[F] Enter Portal";
            XMVECTOR textSize = m_spriteFont->MeasureString(portalText);
            m_spriteFont->DrawString(m_spriteBatch.get(), portalText,
                XMFLOAT2(screenCenterX - XMVectorGetX(textSize) / 2.0f, screenCenterY + 100.0f),
                DirectX::Colors::DodgerBlue);
        }
    }

    // ========== Skill UI ==========
    if (m_pScene)
    {
        GameObject* pPlayer = m_pScene->GetPlayer();
        if (pPlayer)
        {
            SkillComponent* pSkill = pPlayer->GetComponent<SkillComponent>();
            if (pSkill)
            {
                float lineHeight = 48.0f;  // Increased line spacing

                // ========== Left Side: Skill Slots ==========
                float leftX = 20.0f;
                float leftY = (float)m_nWndClientHeight - 240.0f;

                const wchar_t* slotNames[] = { L"Q", L"E", L"R", L"RMB" };

                for (size_t i = 0; i < static_cast<size_t>(SkillSlot::Count); ++i)
                {
                    SkillSlot slot = static_cast<SkillSlot>(i);
                    ISkillBehavior* pBehavior = pSkill->GetSkill(slot);

                    std::wstringstream slotText;
                    slotText << L"[" << slotNames[i] << L"] ";

                    if (pBehavior)
                    {
                        const SkillData& data = pBehavior->GetSkillData();
                        std::wstring skillName(data.name.begin(), data.name.end());
                        slotText << skillName;

                        float cooldownRemaining = pSkill->GetCooldownRemaining(slot);
                        if (cooldownRemaining > 0.0f)
                        {
                            slotText << L"  CD: " << std::fixed << std::setprecision(1) << cooldownRemaining << L"s";
                            m_spriteFont->DrawString(m_spriteBatch.get(), slotText.str().c_str(),
                                XMFLOAT2(leftX, leftY), DirectX::Colors::Gray);
                        }
                        else
                        {
                            // Calculate final damage based on per-skill rune combo
                            RuneCombo combo = pSkill->GetRuneCombo(slot);
                            float baseDamage = data.damage;
                            float finalDamage = baseDamage;
                            std::wstringstream dmgNote;

                            bool enhanceOnly = combo.hasEnhance && !combo.hasCharge && !combo.hasChannel && !combo.hasPlace && !combo.hasInstant;

                            if (combo.hasCharge)
                            {
                                if (pSkill->IsCharging())
                                {
                                    float mult = 1.0f + pSkill->GetChargeProgress() * 2.0f;
                                    if (combo.hasEnhance) mult *= 2.0f;
                                    if (combo.hasPlace) mult *= 1.5f;
                                    finalDamage = baseDamage * mult;
                                    dmgNote << L" (charging)";
                                }
                                else
                                {
                                    float maxMult = 3.0f;
                                    if (combo.hasEnhance) maxMult *= 2.0f;
                                    if (combo.hasPlace) maxMult *= 1.5f;
                                    finalDamage = baseDamage * maxMult;
                                    dmgNote << L"~";
                                }
                                if (combo.hasPlace) dmgNote << L"+Place";
                                if (combo.hasEnhance) dmgNote << L"+Enh";
                            }
                            else if (combo.hasChannel)
                            {
                                float tickMult = 0.3f;
                                if (combo.hasEnhance) tickMult *= 2.0f;
                                if (combo.hasPlace) tickMult *= 1.5f;
                                finalDamage = baseDamage * tickMult;
                                dmgNote << L"/tick";
                                if (combo.hasPlace) dmgNote << L"+Place";
                                if (combo.hasEnhance) dmgNote << L"+Enh";
                            }
                            else if (enhanceOnly)
                            {
                                finalDamage = baseDamage * 2.0f;
                                dmgNote << L" (buff)";
                            }
                            else
                            {
                                float mult = 1.0f;
                                if (combo.hasEnhance) mult *= 2.0f;
                                if (combo.hasPlace) mult *= 1.5f;
                                finalDamage = baseDamage * mult;
                                if (combo.hasPlace) dmgNote << L" (trap)";
                                if (combo.hasEnhance && !combo.hasPlace) dmgNote << L"+Enh";
                            }

                            // Apply existing enhance buff
                            if (pSkill->IsEnhanced() && !enhanceOnly)
                            {
                                finalDamage *= 2.0f;
                            }

                            slotText << L"  DMG: " << (int)finalDamage << dmgNote.str();
                            m_spriteFont->DrawString(m_spriteBatch.get(), slotText.str().c_str(),
                                XMFLOAT2(leftX, leftY), DirectX::Colors::White);
                        }
                    }
                    else
                    {
                        slotText << L"Empty";
                        m_spriteFont->DrawString(m_spriteBatch.get(), slotText.str().c_str(),
                            XMFLOAT2(leftX, leftY), DirectX::Colors::DarkGray);
                    }

                    leftY += lineHeight;
                }

                // ========== Rune Info (right bottom) ==========
                float rightX = (float)m_nWndClientWidth - 400.0f;
                float rightY = (float)m_nWndClientHeight - 240.0f;

                const wchar_t* activationNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance", L"Split" };
                const wchar_t* runeSlotNames[] = { L"Q", L"E", L"R", L"RMB" };

                m_spriteFont->DrawString(m_spriteBatch.get(), L"[Rune Combos]",
                    XMFLOAT2(rightX, rightY), DirectX::Colors::Cyan);
                rightY += lineHeight * 0.8f;

                // Per-skill rune combo display
                for (size_t si = 0; si < static_cast<size_t>(SkillSlot::Count); ++si)
                {
                    SkillSlot sSlot = static_cast<SkillSlot>(si);
                    RuneCombo combo = pSkill->GetRuneCombo(sSlot);

                    std::wstringstream comboLine;
                    comboLine << runeSlotNames[si] << L": ";

                    if (combo.count == 0)
                    {
                        comboLine << L"(none)";
                        m_spriteFont->DrawString(m_spriteBatch.get(), comboLine.str().c_str(),
                            XMFLOAT2(rightX, rightY), DirectX::Colors::DarkGray);
                    }
                    else
                    {
                        // List equipped rune types
                        bool first = true;
                        if (combo.hasCharge)  { if (!first) comboLine << L"+"; comboLine << L"Charge"; first = false; }
                        if (combo.hasChannel) { if (!first) comboLine << L"+"; comboLine << L"Channel"; first = false; }
                        if (combo.hasPlace)   { if (!first) comboLine << L"+"; comboLine << L"Place"; first = false; }
                        if (combo.hasEnhance) { if (!first) comboLine << L"+"; comboLine << L"Enhance"; first = false; }
                        if (combo.hasSplit)   { if (!first) comboLine << L"+"; comboLine << L"Split";   first = false; }
                        if (combo.hasInstant) { if (!first) comboLine << L"+"; comboLine << L"Instant"; first = false; }

                        m_spriteFont->DrawString(m_spriteBatch.get(), comboLine.str().c_str(),
                            XMFLOAT2(rightX, rightY), DirectX::Colors::LightGray);
                    }
                    rightY += lineHeight * 0.7f;
                }

                rightY += lineHeight * 0.3f;

                // Status indicators
                if (pSkill->IsCharging())
                {
                    float chargeProgress = pSkill->GetChargeProgress();
                    float mult = 1.0f + chargeProgress * 2.0f;
                    std::wstringstream chargeText;
                    chargeText << L"CHARGING " << (int)(chargeProgress * 100) << L"% ("
                        << std::fixed << std::setprecision(1) << mult << L"x)";
                    m_spriteFont->DrawString(m_spriteBatch.get(), chargeText.str().c_str(),
                        XMFLOAT2(rightX, rightY), DirectX::Colors::Orange);
                    rightY += lineHeight;
                }

                if (pSkill->IsChanneling())
                {
                    float channelProgress = pSkill->GetChannelProgress();
                    std::wstringstream channelText;
                    channelText << L"CHANNELING " << (int)(channelProgress * 100) << L"%";
                    m_spriteFont->DrawString(m_spriteBatch.get(), channelText.str().c_str(),
                        XMFLOAT2(rightX, rightY), DirectX::Colors::LightBlue);
                    rightY += lineHeight;
                }

                if (pSkill->IsEnhanced())
                {
                    std::wstringstream enhanceText;
                    enhanceText << L"ENHANCED 2x (" << std::fixed << std::setprecision(1)
                        << pSkill->GetEnhanceTimeRemaining() << L"s)";
                    m_spriteFont->DrawString(m_spriteBatch.get(), enhanceText.str().c_str(),
                        XMFLOAT2(rightX, rightY), DirectX::Colors::Gold);
                }
            }
        }
    }

    // Floating damage numbers (world → screen projection)
    if (m_pScene && m_pScene->GetCamera())
    {
        CCamera* pCam = m_pScene->GetCamera();
        XMMATRIX vp = XMLoadFloat4x4(&pCam->GetViewMatrix()) *
                      XMLoadFloat4x4(&pCam->GetProjectionMatrix());
        XMFLOAT4X4 vp4x4;
        XMStoreFloat4x4(&vp4x4, vp);
        DamageNumberManager::Get().Render(m_spriteBatch.get(), m_spriteFont.get(),
                                          vp4x4, (int)m_nWndClientWidth, (int)m_nWndClientHeight);
    }

    m_spriteBatch->End();
}

void Dx12App::InitializeNetwork()
{
    // NetworkManager 초기화
    m_pNetworkManager = NetworkManager::GetInstance();

    if (!m_pNetworkManager->Initialize())
    {
        OutputDebugString(L"[Network] Failed to initialize NetworkManager\n");
        return;
    }

    // 서버에 연결 (127.0.0.1:7777)
    if (!m_pNetworkManager->Connect(L"127.0.0.1", 7777))
    {
        OutputDebugString(L"[Network] Failed to connect to server\n");
        // 연결 실패해도 게임은 계속 진행 (싱글 플레이)
    }
    else
    {
        OutputDebugString(L"[Network] Connecting to server...\n");
    }
}

void Dx12App::UpdateNetwork(float deltaTime)
{
    if (!m_pNetworkManager || !m_pNetworkManager->IsConnected())
        return;

    // 원격 플레이어 idle 전환 체크 (항상 실행)
    m_pNetworkManager->CheckRemotePlayerIdle(deltaTime);

    // 원격 플레이어 VFX 타임아웃 체크
    m_pNetworkManager->CheckRemotePlayerVFXTimeout(m_pScene.get(), deltaTime);

    // 서버 몬스터 idle 전환 체크
    m_pNetworkManager->CheckServerMonsterIdle(deltaTime);

    // 서버 몬스터 위치/회전 보간 (MOVE 패킷 간격 사이 부드럽게 이동)
    m_pNetworkManager->InterpolateServerMonsters(deltaTime);

    // 이동 패킷 전송 간격 체크
    m_fNetworkSendTimer += deltaTime;
    if (m_fNetworkSendTimer < m_fNetworkSendInterval)
        return;

    m_fNetworkSendTimer = 0.0f;

    // 로컬 플레이어 위치 가져오기
    if (!m_pScene)
        return;

    GameObject* pPlayer = m_pScene->GetPlayer();
    if (!pPlayer)
        return;

    TransformComponent* pTransform = pPlayer->GetTransform();
    if (!pTransform)
        return;

    const XMFLOAT3& currentPos = pTransform->GetPosition();

    // 위치가 변경되었는지 확인 (오차 범위 0.01)
    float dx = currentPos.x - m_lastSentPosition.x;
    float dy = currentPos.y - m_lastSentPosition.y;
    float dz = currentPos.z - m_lastSentPosition.z;
    float distSq = dx * dx + dy * dy + dz * dz;

    if (distSq > 0.0001f)  // 0.01 squared
    {
        // 방향 벡터 가져오기 (Look 방향)
        XMVECTOR lookVec = pTransform->GetLook();
        XMFLOAT3 lookDir;
        XMStoreFloat3(&lookDir, lookVec);

        // 위치와 방향 전송
        m_pNetworkManager->SendMove(currentPos.x, currentPos.y, currentPos.z,
                                    lookDir.x, lookDir.y, lookDir.z);
        m_lastSentPosition = currentPos;
    }
}