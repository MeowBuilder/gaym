#include "stdafx.h"
#include "RenderComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "Mesh.h"

RenderComponent::RenderComponent(GameObject* pOwner) : Component(pOwner)
{
}

RenderComponent::~RenderComponent()
{
}

void RenderComponent::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    m_pConstantBuffer = CreateBufferResource(pDevice, pCommandList, nullptr, sizeof(XMFLOAT4X4), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, nullptr);
    m_pConstantBuffer->Map(0, nullptr, (void**)&m_pConstantBufferWO);
}

void RenderComponent::Update(float deltaTime)
{
}

void RenderComponent::Render(ID3D12GraphicsCommandList* pCommandList)
{
    if (!GetMesh())
        return;

    XMMATRIX world = XMLoadFloat4x4(&GetOwner()->GetTransform()->GetWorldMatrix());
    XMMATRIX view = XMMatrixLookAtLH(XMVectorSet(0.0f, 0.0f, -5.0f, 1.0f), XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));
    XMMATRIX proj = XMMatrixPerspectiveFovLH(XMConvertToRadians(60.0f), 1920.f / 1080.f, 0.1f, 100.f);
    XMMATRIX wvp = XMMatrixTranspose(world * view * proj);
    XMStoreFloat4x4((XMFLOAT4X4*)m_pConstantBufferWO, wvp);

    pCommandList->SetGraphicsRootConstantBufferView(0, m_pConstantBuffer->GetGPUVirtualAddress());

    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    
    auto mesh = GetMesh();
    const auto& vbv = mesh->GetVertexBufferView();
    const auto& ibv = mesh->GetIndexBufferView();
    pCommandList->IASetVertexBuffers(0, 1, &vbv);
    pCommandList->IASetIndexBuffer(&ibv);
    pCommandList->DrawIndexedInstanced(mesh->GetIndexCount(), 1, 0, 0, 0);
}
