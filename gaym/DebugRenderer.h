#pragma once

#include "stdafx.h"
#include <vector>
#include <DirectXCollision.h>

class ColliderComponent;

// Simple debug renderer for visualizing colliders
class DebugRenderer
{
public:
    DebugRenderer();
    ~DebugRenderer();

    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);
    void Render(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_VIRTUAL_ADDRESS passCBV,
                const std::vector<ColliderComponent*>& colliders);

    void SetEnabled(bool bEnabled) { m_bEnabled = bEnabled; }
    bool IsEnabled() const { return m_bEnabled; }
    void Toggle() { m_bEnabled = !m_bEnabled; }

private:
    void CreatePipelineState(ID3D12Device* pDevice);
    void CreateWireframeCube(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);

    bool m_bEnabled = false;

    ComPtr<ID3D12RootSignature> m_pRootSignature;
    ComPtr<ID3D12PipelineState> m_pPipelineState;

    // Wireframe cube (unit cube centered at origin)
    ComPtr<ID3D12Resource> m_pVertexBuffer;
    ComPtr<ID3D12Resource> m_pVertexUploadBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;

    ComPtr<ID3D12Resource> m_pIndexBuffer;
    ComPtr<ID3D12Resource> m_pIndexUploadBuffer;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
    UINT m_nIndices = 0;

    // Per-box constant buffer (world matrix + color)
    ComPtr<ID3D12Resource> m_pBoxCB;
    UINT8* m_pBoxCBData = nullptr;
    static constexpr UINT MAX_DEBUG_BOXES = 256;
    static constexpr UINT BOX_CB_SIZE = 256; // Aligned to 256 bytes
};
