#include "stdafx.h"
#include "RenderComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"

RenderComponent::RenderComponent(GameObject* pOwner) : Component(pOwner), m_nIndexCount(0)
{
}

RenderComponent::~RenderComponent()
{
}

void RenderComponent::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    Vertex cubeVertices[] =
    {
        { XMFLOAT3(-0.5f, 0.5f, -0.5f), RANDOM_COLOR },
        { XMFLOAT3(0.5f, 0.5f, -0.5f), RANDOM_COLOR },
        { XMFLOAT3(0.5f, 0.5f, 0.5f), RANDOM_COLOR },
        { XMFLOAT3(-0.5f, 0.5f, 0.5f), RANDOM_COLOR },
        { XMFLOAT3(-0.5f, -0.5f, -0.5f), RANDOM_COLOR },
        { XMFLOAT3(0.5f, -0.5f, -0.5f), RANDOM_COLOR },
        { XMFLOAT3(0.5f, -0.5f, 0.5f), RANDOM_COLOR },
        { XMFLOAT3(-0.5f, -0.5f, 0.5f), RANDOM_COLOR }
    };

    DWORD cubeIndices[] =
    {
        // Front Face
        0, 1, 5,
        0, 5, 4,

        // Back Face
        2, 3, 7,
        2, 7, 6,

        // Top Face
        3, 2, 1,
        3, 1, 0,

        // Bottom Face
        4, 5, 6,
        4, 6, 7,

        // Left Face
        3, 0, 4,
        3, 4, 7,

        // Right Face
        1, 2, 6,
        1, 6, 5
    };

    m_nIndexCount = _countof(cubeIndices);

    m_pVertexBuffer = CreateBufferResource(pDevice, pCommandList, cubeVertices, sizeof(cubeVertices), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pVertexUploadBuffer);
    m_pIndexBuffer = CreateBufferResource(pDevice, pCommandList, cubeIndices, sizeof(cubeIndices), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_pIndexUploadBuffer);

    m_VertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
    m_VertexBufferView.StrideInBytes = sizeof(Vertex);
    m_VertexBufferView.SizeInBytes = sizeof(cubeVertices);

    m_IndexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
    m_IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
    m_IndexBufferView.SizeInBytes = sizeof(cubeIndices);

    m_pConstantBuffer = CreateBufferResource(pDevice, pCommandList, nullptr, sizeof(XMFLOAT4X4), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr);
    m_pConstantBuffer->Map(0, nullptr, (void**)&m_pConstantBufferWO);
}

void RenderComponent::Update(float deltaTime)
{
}

void RenderComponent::Render(ID3D12GraphicsCommandList* pCommandList)
{
    XMMATRIX world = XMLoadFloat4x4(&GetOwner()->GetTransform()->GetWorldMatrix());
    XMMATRIX view = XMMatrixLookAtLH(XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f), XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 1920.f / 1080.f, 0.1f, 100.f);
    XMMATRIX wvp = XMMatrixTranspose(world * view * proj);
    XMStoreFloat4x4((XMFLOAT4X4*)m_pConstantBufferWO, wvp);

    pCommandList->SetGraphicsRootConstantBufferView(0, m_pConstantBuffer->GetGPUVirtualAddress());

    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
    pCommandList->IASetIndexBuffer(&m_IndexBufferView);
    pCommandList->DrawIndexedInstanced(m_nIndexCount, 1, 0, 0, 0);
}