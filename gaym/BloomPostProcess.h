#pragma once

#include "stdafx.h"

// LDR bloom post-process.
//
// Scene is rendered straight into the swap-chain back buffer as usual. At the
// end of the frame we snapshot the back buffer, run a bright-pass + gaussian
// blur chain, then additive-blend the result back onto the back buffer.
// Bright pixels (particle emissives pushed >= 1.0 before saturation) produce
// the glow; regular geometry stays below threshold and is unaffected.
class BloomPostProcess
{
public:
    BloomPostProcess() = default;
    ~BloomPostProcess() = default;

    void Init(ID3D12Device* pDevice, UINT width, UINT height);
    void OnResize(ID3D12Device* pDevice, UINT width, UINT height);

    // Snapshot back buffer, bright-pass, blur, additive blend back on top.
    // Caller guarantees pBackBuffer is in RENDER_TARGET when entering and wants
    // it left in RENDER_TARGET when Apply returns (so UI/text can draw next).
    void Apply(ID3D12GraphicsCommandList* pCmd,
               ID3D12Resource* pBackBuffer,
               D3D12_CPU_DESCRIPTOR_HANDLE backBufferRTV,
               UINT targetWidth, UINT targetHeight);

    // Tuning
    void  SetThreshold(float v) { m_threshold = v; }
    void  SetIntensity(float v) { m_intensity = v; }
    float GetThreshold() const  { return m_threshold; }
    float GetIntensity() const  { return m_intensity; }

    // Runtime toggle for before/after comparison.
    void SetEnabled(bool v) { m_enabled = v; }
    bool IsEnabled() const  { return m_enabled; }
    void ToggleEnabled()    { m_enabled = !m_enabled; }

private:
    struct BloomCB
    {
        float texelSizeX;
        float texelSizeY;
        float threshold;
        float intensity;
        float exposure;
        float pad0, pad1, pad2;
    };

    void CreateRootSignature(ID3D12Device* pDevice);
    void CreatePipelineStates(ID3D12Device* pDevice);
    void CreateDescriptorHeaps(ID3D12Device* pDevice);
    void CreateRenderTargets(ID3D12Device* pDevice, UINT width, UINT height);
    void CreateViews(ID3D12Device* pDevice);
    void CreateConstantBuffer(ID3D12Device* pDevice);

    void WriteCB(const BloomCB& data);
    void DrawFullscreen(ID3D12GraphicsCommandList* pCmd,
                        ID3D12PipelineState* pPSO,
                        D3D12_CPU_DESCRIPTOR_HANDLE rtv,
                        UINT width, UINT height,
                        D3D12_GPU_DESCRIPTOR_HANDLE srvTable,
                        const BloomCB& cb);

    void Barrier(ID3D12GraphicsCommandList* pCmd,
                 ID3D12Resource* pResource,
                 D3D12_RESOURCE_STATES before,
                 D3D12_RESOURCE_STATES after);

private:
    static constexpr DXGI_FORMAT kBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

    ComPtr<ID3D12RootSignature> m_pRootSig;
    ComPtr<ID3D12PipelineState> m_pPSOBright;
    ComPtr<ID3D12PipelineState> m_pPSOBlurH;
    ComPtr<ID3D12PipelineState> m_pPSOBlurV;
    ComPtr<ID3D12PipelineState> m_pPSOComposite;

    ComPtr<ID3D12DescriptorHeap> m_pSrvHeap;
    UINT m_srvIncr = 0;
    static constexpr UINT kSrvCapture = 0;
    static constexpr UINT kSrvBright  = 1;
    static constexpr UINT kSrvBlurA   = 2;
    static constexpr UINT kSrvBlurB   = 3;
    static constexpr UINT kSrvHeapSize = 4;

    ComPtr<ID3D12DescriptorHeap> m_pRtvHeap;
    UINT m_rtvIncr = 0;

    ComPtr<ID3D12Resource> m_pCaptureRT;  // full-res copy of back buffer
    ComPtr<ID3D12Resource> m_pBrightRT;   // half-res
    ComPtr<ID3D12Resource> m_pBlurA;      // half-res
    ComPtr<ID3D12Resource> m_pBlurB;      // half-res

    D3D12_CPU_DESCRIPTOR_HANDLE m_brightRTV = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_blurARTV  = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_blurBRTV  = {};

    D3D12_RESOURCE_STATES m_captureState = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES m_brightState  = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES m_blurAState   = D3D12_RESOURCE_STATE_COMMON;
    D3D12_RESOURCE_STATES m_blurBState   = D3D12_RESOURCE_STATE_COMMON;

    ComPtr<ID3D12Resource> m_pCB;
    UINT8* m_pCBMapped    = nullptr;
    UINT   m_cbSlotBytes  = 0;
    UINT   m_cbSlotCount  = 0;
    UINT   m_cbNextSlot   = 0;

    UINT m_width      = 0;
    UINT m_height     = 0;
    UINT m_halfWidth  = 0;
    UINT m_halfHeight = 0;

    // Cartoon/stylized tuning: brightness threshold plus a chroma gate in the
    // shader keep character specular/diffuse highlights from blooming while
    // letting saturated skill-particle colors glow.
    float m_threshold       = 0.8f;   // LDR source: soft-knee start value.
    float m_intensity       = 1.8f;   // Bloom add-strength in composite.
    UINT  m_blurIterations  = 3;      // Number of H+V passes (each widens blur radius).
    bool  m_enabled         = true;
};
