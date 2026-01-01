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
    // This component no longer needs to create its own constant buffer.
    // The GameObject now manages it.
}

void RenderComponent::Update(float deltaTime)
{
}

void RenderComponent::Render(ID3D12GraphicsCommandList* pCommandList)
{
    if (!m_pMesh)
        return;

    // Set the per-object descriptor table to root parameter 0 (CBV)
    pCommandList->SetGraphicsRootDescriptorTable(0, m_pOwner->GetGpuDescriptorHandle());

    // NEW: Set the texture descriptor table to root parameter 2 (SRV) if exists
    if (m_pOwner->HasTexture())
    {
        // We need to access m_srvGPUDescriptorHandle. 
        // Since it's private in GameObject, we should use a getter or make RenderComponent a friend.
        // I will assume we can add a getter GetSrvDescriptorHandle() to GameObject.
        pCommandList->SetGraphicsRootDescriptorTable(2, m_pOwner->GetSrvDescriptorHandle());
    }

    m_pMesh->Render(pCommandList, 0);
}
