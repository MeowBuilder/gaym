#pragma once
#include "Component.h"

struct Vertex
{
    XMFLOAT3 pos;
    XMFLOAT4 color;
};

class RenderComponent : public Component
{
public:
    RenderComponent(GameObject* pOwner);
    ~RenderComponent();

    virtual void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList) override;
    virtual void Update(float deltaTime) override;
    virtual void Render(ID3D12GraphicsCommandList* pCommandList) override;

private:
    ComPtr<ID3D12Resource> m_pVertexBuffer;
    ComPtr<ID3D12Resource> m_pIndexBuffer;
    ComPtr<ID3D12Resource> m_pVertexUploadBuffer;
    ComPtr<ID3D12Resource> m_pIndexUploadBuffer;

    ComPtr<ID3D12Resource> m_pConstantBuffer;
    void* m_pConstantBufferWO = nullptr;

    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;

    UINT m_nIndexCount;
};