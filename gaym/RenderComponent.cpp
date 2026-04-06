#include "stdafx.h"
#include "RenderComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "Mesh.h"
#include "Shader.h"

RenderComponent::RenderComponent(GameObject* pOwner) : Component(pOwner)
{
}

RenderComponent::~RenderComponent()
{
    if (m_pOwnerShader)
        m_pOwnerShader->RemoveRenderComponent(this);
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

    if (m_pOwner->HasTexture())
        pCommandList->SetGraphicsRootDescriptorTable(2, m_pOwner->GetSrvDescriptorHandle());

    if (m_pOwner->HasNormalMap())
        pCommandList->SetGraphicsRootDescriptorTable(4, m_pOwner->GetNormalMapSrvHandle());

    if (m_pOwner->HasHeightMap())
        pCommandList->SetGraphicsRootDescriptorTable(5, m_pOwner->GetHeightMapSrvHandle());

    if (m_pOwner->HasEmissiveTexture())
        pCommandList->SetGraphicsRootDescriptorTable(6, m_pOwner->GetEmissiveSrvDescriptorHandle());

    if (m_pOwner->HasAOMap())
        pCommandList->SetGraphicsRootDescriptorTable(7, m_pOwner->GetAOMapSrvHandle());

    if (m_pOwner->HasRoughnessMap())
        pCommandList->SetGraphicsRootDescriptorTable(8, m_pOwner->GetRoughnessMapSrvHandle());

    m_pMesh->Render(pCommandList, 0);
}
