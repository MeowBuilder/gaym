#pragma once

#include "stdafx.h"
#include <SpriteBatch.h>
#include "WICTextureLoader12.h"  // Use local version
#include <DescriptorHeap.h>

class HealthBarUI
{
public:
    HealthBarUI();
    ~HealthBarUI();

    void Initialize(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                    DirectX::DescriptorHeap* pDescriptorHeap, UINT nBaseDescriptorIndex);

    void Render(DirectX::SpriteBatch* pSpriteBatch, float fHPRatio,
                float fScreenWidth, float fScreenHeight);

private:
    ComPtr<ID3D12Resource> m_pBaseTexture;
    ComPtr<ID3D12Resource> m_pFillTexture;

    // Upload buffers (must stay alive until GPU upload completes)
    ComPtr<ID3D12Resource> m_pBaseUploadBuffer;
    ComPtr<ID3D12Resource> m_pFillUploadBuffer;

    D3D12_GPU_DESCRIPTOR_HANDLE m_hBaseGPU;
    D3D12_GPU_DESCRIPTOR_HANDLE m_hFillGPU;

    UINT m_nBaseWidth;
    UINT m_nBaseHeight;
    UINT m_nFillWidth;
    UINT m_nFillHeight;

    // Layout constants
    static constexpr float PADDING_LEFT = 20.0f;
    static constexpr float PADDING_TOP = 20.0f;
    static constexpr float SCALE = 0.15f;  // Scale factor for the UI (assets are large, ~3700px)

    // Fill bar offset within the base frame (adjust based on actual sprite)
    // base.png: 3708x718, large_bar.png: 3243x161
    // Top bar frame starts around x=470, y=195 in base.png
    static constexpr float FILL_OFFSET_X = 490.0f;  // Offset from left of base
    static constexpr float FILL_OFFSET_Y = 230.0f;  // Offset from top of base

    // Scale factor for fill bar to fit inside frame
    static constexpr float FILL_SCALE_X = 0.95f;  // Shrink width to fit inside frame
};
