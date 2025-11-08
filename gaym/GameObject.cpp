#include "stdafx.h"
#include "GameObject.h"
#include "Component.h"
#include "TransformComponent.h"

GameObject::GameObject()
    : m_Material({XMFLOAT4(0.1f, 0.1f, 0.1f, 1.0f), // Ambient
                  XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f), // Diffuse
                  XMFLOAT4(1.0f, 1.0f, 1.0f, 10.0f), // Specular (power 10.0f in alpha)
                  XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)}) // Emissive
{
    m_pTransform = AddComponent<TransformComponent>();
}

GameObject::~GameObject()
{
    if (m_pd3dcbGameObject) m_pd3dcbGameObject->Unmap(0, NULL);
}

void GameObject::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    for (auto& component : m_vComponents)
    {
        component->Init(pDevice, pCommandList);
    }
}

void GameObject::Update(float deltaTime)
{
    for (auto& component : m_vComponents)
    {
        component->Update(deltaTime);
    }

    // Update constant buffer
    if (m_pcbMappedGameObject)
    {
        XMMATRIX worldMatrix = XMLoadFloat4x4(&m_pTransform->GetWorldMatrix());
        XMStoreFloat4x4(&m_pcbMappedGameObject->m_xmf4x4World, XMMatrixTranspose(worldMatrix));
        m_pcbMappedGameObject->m_nMaterialIndex = m_nMaterialIndex;
        m_pcbMappedGameObject->mMaterial = m_Material; // Copy the new material struct
    }

    // Recurse for children and siblings
    if (m_pChild) m_pChild->Update(deltaTime);
    if (m_pSibling) m_pSibling->Update(deltaTime);
}

void GameObject::Render(ID3D12GraphicsCommandList* pCommandList)
{
    // Set the descriptor table for this object
    pCommandList->SetGraphicsRootDescriptorTable(0, m_cbvGPUDescriptorHandle);

    for (auto& component : m_vComponents)
    {
        component->Render(pCommandList);
    }

    if (m_pMesh) m_pMesh->Render(pCommandList, 0);

	if (m_pSibling) m_pSibling->Render(pCommandList);
	if (m_pChild) m_pChild->Render(pCommandList);
}

void GameObject::SetMesh(Mesh* pMesh)
{
	if (m_pMesh) m_pMesh->Release();
	m_pMesh = pMesh;
	if (m_pMesh) m_pMesh->AddRef();
}

void GameObject::SetChild(GameObject* pChild)
{
    if (pChild)
    {
        pChild->m_pParent = this;

        if (m_pChild) // If this object already has a first child
        {
            // Find the last sibling in the current child list
            GameObject* pSibling = m_pChild;
            while (pSibling->m_pSibling)
            {
                pSibling = pSibling->m_pSibling;
            }
            // Add the new child as the next sibling
            pSibling->m_pSibling = pChild;
        }
        else // This is the first child
        {
            m_pChild = pChild;
        }
    }
}

void GameObject::SetTransform(const XMFLOAT4X4& transform)
{
	GetTransform()->SetLocalMatrix(transform);
}

void GameObject::SetMaterial(const MATERIAL& material)
{
    m_Material = material;
}

void GameObject::CreateConstantBuffer(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, UINT nBufferSize, D3D12_CPU_DESCRIPTOR_HANDLE d3dCbvCPUDescriptorHandle)
{
    UINT nConstantBufferSize = (sizeof(ObjectConstants) + 255) & ~255; // Align to 256 bytes
    m_pd3dcbGameObject = CreateBufferResource(pDevice, pCommandList, NULL, nConstantBufferSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, NULL);

    m_pd3dcbGameObject->Map(0, NULL, (void**)&m_pcbMappedGameObject);

    D3D12_CONSTANT_BUFFER_VIEW_DESC d3dcbvDesc;
    d3dcbvDesc.BufferLocation = m_pd3dcbGameObject->GetGPUVirtualAddress();
    d3dcbvDesc.SizeInBytes = nConstantBufferSize;
    pDevice->CreateConstantBufferView(&d3dcbvDesc, d3dCbvCPUDescriptorHandle);
}
