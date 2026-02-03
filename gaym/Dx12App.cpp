#include "stdafx.h"
#include "Dx12App.h"
#include "SkillComponent.h"
#include "ISkillBehavior.h"
#include "SkillData.h"
#include "DropItemComponent.h"
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
    

    // CommandList를 열고 리소스 생성을 기록합니다.
    CHECK_HR(m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), NULL));

    m_pScene = std::make_unique<Scene>();
    m_pScene->Init(m_pd3dDevice.Get(), m_pd3dCommandList.Get());

    // CommandList를 닫고 실행하여 리소스 업로드를 완료합니다.
    CHECK_HR(m_pd3dCommandList->Close());
    ID3D12CommandList* ppd3dCommandLists[] = { m_pd3dCommandList.Get() };
    m_pd3dCommandQueue->ExecuteCommandLists(_countof(ppd3dCommandLists), ppd3dCommandLists);
    WaitForGpuComplete();

    // 텍스트 렌더링 초기화
    InitializeText();

    m_GameTimer.Reset();
}

void Dx12App::OnDestroy()
{
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

    WaitForGpuComplete();

    CHECK_HR(m_pd3dCommandAllocator->Reset());
    CHECK_HR(m_pd3dCommandList->Reset(m_pd3dCommandAllocator.Get(), NULL));

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

    m_pScene->Update(m_GameTimer.GetTimeElapsed(), &m_inputSystem);

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
            for (int i = 0; i < 3; ++i)
            {
                float optionY = screenCenterY + i * 40.0f;
                float optionHeight = 36.0f;
                float optionWidth = 400.0f;

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
            float lineHeight = 40.0f;

            for (int skillIdx = 0; skillIdx < static_cast<int>(SkillSlot::Count); ++skillIdx)
            {
                float slotY = slotStartY + skillIdx * lineHeight;

                for (int runeIdx = 0; runeIdx < RUNES_PER_SKILL; ++runeIdx)
                {
                    float runeX = screenCenterX - 80.0f + runeIdx * 80.0f;
                    float runeWidth = 70.0f;
                    float runeHeight = 28.0f;

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
            else if (m_pScene->IsNearInteractionCube())
            {
                m_pScene->TriggerInteraction();
            }
        }
    }

    m_pScene->Render(m_pd3dCommandList.Get());

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

    // 폰트용 디스크립터 힙 생성
    m_fontDescriptorHeap = std::make_unique<DirectX::DescriptorHeap>(
        m_pd3dDevice.Get(),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
        1  // 폰트 1개
    );

    // 리소스 업로드 배치
    DirectX::ResourceUploadBatch resourceUpload(m_pd3dDevice.Get());
    resourceUpload.Begin();

    // SpriteBatch 생성
    DirectX::RenderTargetState rtState(
        DXGI_FORMAT_R8G8B8A8_UNORM,      // 백버퍼 포맷
        DXGI_FORMAT_D24_UNORM_S8_UINT    // 깊이버퍼 포맷
    );

    DirectX::SpriteBatchPipelineStateDescription pd(rtState);
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
}

void Dx12App::RenderText()
{
    // Bind descriptor heap
    ID3D12DescriptorHeap* heaps[] = { m_fontDescriptorHeap->Heap() };
    m_pd3dCommandList->SetDescriptorHeaps(1, heaps);

    m_spriteBatch->Begin(m_pd3dCommandList.Get());

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
                        const wchar_t* typeNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance" };
                        const wchar_t* typeDescs[] = {
                            L"Empty",
                            L"1x damage",
                            L"Hold: 1x~3x damage",
                            L"Hold: 0.3x per tick",
                            L"1.5x trap damage",
                            L"Buff: 2x next attack"
                        };

                        // Title
                        const wchar_t* titleText = L"=== Click to Select a Rune ===";
                        XMVECTOR titleSize = m_spriteFont->MeasureString(titleText);
                        m_spriteFont->DrawString(m_spriteBatch.get(), titleText,
                            XMFLOAT2(screenCenterX - XMVectorGetX(titleSize) / 2.0f, screenCenterY - 60.0f),
                            DirectX::Colors::Gold);

                        // Rune options (clickable)
                        XMFLOAT2 mousePos = m_inputSystem.GetMousePosition();
                        for (int i = 0; i < 3; ++i)
                        {
                            ActivationType runeType = pDropComp->GetRuneOption(i);
                            int typeIndex = static_cast<int>(runeType);

                            std::wstringstream optionText;
                            optionText << L"> " << typeNames[typeIndex] << L" - " << typeDescs[typeIndex];

                            float optionY = screenCenterY + i * 40.0f;
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
                            XMFLOAT2(screenCenterX - XMVectorGetX(cancelSize) / 2.0f, screenCenterY + 140.0f),
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
            const wchar_t* typeNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance" };
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
            float lineHeight = 40.0f;

            for (int skillIdx = 0; skillIdx < static_cast<int>(SkillSlot::Count); ++skillIdx)
            {
                float slotY = slotStartY + skillIdx * lineHeight;

                // Skill name
                std::wstringstream skillText;
                skillText << L"[" << slotNames[skillIdx] << L"] ";
                m_spriteFont->DrawString(m_spriteBatch.get(), skillText.str().c_str(),
                    XMFLOAT2(screenCenterX - 150.0f, slotY), DirectX::Colors::White);

                // Rune slots (3 boxes)
                for (int runeIdx = 0; runeIdx < RUNES_PER_SKILL; ++runeIdx)
                {
                    float runeX = screenCenterX - 80.0f + runeIdx * 80.0f;
                    float runeWidth = 70.0f;
                    float runeHeight = 28.0f;

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
                XMFLOAT2(screenCenterX - XMVectorGetX(cancelSize) / 2.0f, slotStartY + 180.0f),
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
                ActivationType activationType = pSkill->GetActivationType();

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
                            // Calculate final damage based on rune
                            float baseDamage = data.damage;
                            float finalDamage = baseDamage;
                            const wchar_t* dmgNote = L"";

                            switch (activationType)
                            {
                            case ActivationType::Instant:
                                finalDamage = baseDamage;
                                break;
                            case ActivationType::Charge:
                                if (pSkill->IsCharging())
                                {
                                    float mult = 1.0f + pSkill->GetChargeProgress() * 2.0f;
                                    finalDamage = baseDamage * mult;
                                    dmgNote = L" (charging)";
                                }
                                else
                                {
                                    finalDamage = baseDamage;
                                    dmgNote = L"~";  // Can go up to 3x
                                }
                                break;
                            case ActivationType::Channel:
                                finalDamage = baseDamage * 0.3f;
                                dmgNote = L"/tick";
                                break;
                            case ActivationType::Place:
                                finalDamage = baseDamage * 1.5f;
                                dmgNote = L" (trap)";
                                break;
                            case ActivationType::Enhance:
                                finalDamage = baseDamage * 2.0f;
                                dmgNote = L" (buff)";
                                break;
                            }

                            // Apply enhance multiplier if active
                            if (pSkill->IsEnhanced() && activationType != ActivationType::Enhance)
                            {
                                finalDamage *= 2.0f;
                            }

                            slotText << L"  DMG: " << (int)finalDamage << dmgNote;
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

                // ========== Rune Info (below skills) ==========
                const wchar_t* activationNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance" };
                const wchar_t* runeDescriptions[] = {
                    L"No rune equipped",
                    L"1x damage",
                    L"Hold: 1x~3x damage",
                    L"Hold: 0.3x per tick",
                    L"1.5x trap damage",
                    L"Buff: 2x next attack"
                };

                leftY += 10.0f;  // Add spacing

                // Show current activation type
                std::wstringstream runeText;
                runeText << L"[Rune] " << activationNames[static_cast<int>(activationType)]
                         << L" - " << runeDescriptions[static_cast<int>(activationType)];
                m_spriteFont->DrawString(m_spriteBatch.get(), runeText.str().c_str(),
                    XMFLOAT2(leftX, leftY), DirectX::Colors::Cyan);
                leftY += lineHeight;

                // Status indicators
                if (pSkill->IsCharging())
                {
                    float chargeProgress = pSkill->GetChargeProgress();
                    float mult = 1.0f + chargeProgress * 2.0f;
                    std::wstringstream chargeText;
                    chargeText << L"CHARGING " << (int)(chargeProgress * 100) << L"% ("
                        << std::fixed << std::setprecision(1) << mult << L"x)";
                    m_spriteFont->DrawString(m_spriteBatch.get(), chargeText.str().c_str(),
                        XMFLOAT2(leftX, leftY), DirectX::Colors::Orange);
                    leftY += lineHeight;
                }

                if (pSkill->IsChanneling())
                {
                    float channelProgress = pSkill->GetChannelProgress();
                    std::wstringstream channelText;
                    channelText << L"CHANNELING " << (int)(channelProgress * 100) << L"%";
                    m_spriteFont->DrawString(m_spriteBatch.get(), channelText.str().c_str(),
                        XMFLOAT2(leftX, leftY), DirectX::Colors::LightBlue);
                    leftY += lineHeight;
                }

                if (pSkill->IsEnhanced())
                {
                    std::wstringstream enhanceText;
                    enhanceText << L"ENHANCED 2x (" << std::fixed << std::setprecision(1)
                        << pSkill->GetEnhanceTimeRemaining() << L"s)";
                    m_spriteFont->DrawString(m_spriteBatch.get(), enhanceText.str().c_str(),
                        XMFLOAT2(leftX, leftY), DirectX::Colors::Gold);
                }
            }
        }
    }

    m_spriteBatch->End();
}