#include "stdafx.h"
#include "GameObject.h"
#include "Component.h"
#include "TransformComponent.h"
#include "WICTextureLoader12.h"
#include "D3dx12.h"

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

    if (HasTexture())
    {
        pCommandList->SetGraphicsRootDescriptorTable(2, m_srvGPUDescriptorHandle);
    }

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

void GameObject::LoadTexture(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, D3D12_CPU_DESCRIPTOR_HANDLE srvCpuHandle)
{
    if (m_strTextureName.empty()) return;

    // Prepend "Animation/" directory
    std::string fullPath = "Animation/" + m_strTextureName;
    std::wstring wstrTextureName(fullPath.begin(), fullPath.end());

    std::unique_ptr<uint8_t[]> decodedData;
    D3D12_SUBRESOURCE_DATA subresource;

    HRESULT hr = DirectX::LoadWICTextureFromFile(pd3dDevice, wstrTextureName.c_str(), m_pd3dTexture.GetAddressOf(), decodedData, subresource);
    if (FAILED(hr))
    {
        char buffer[256];
        sprintf_s(buffer, "Failed to load texture: %s\n", m_strTextureName.c_str());
        OutputDebugStringA(buffer);
        return;
    }

    UINT64 nBytes = GetRequiredIntermediateSize(m_pd3dTexture.Get(), 0, 1);

    m_pd3dTextureUploadBuffer = CreateBufferResource(pd3dDevice, pd3dCommandList, NULL, nBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, NULL);

    UpdateSubresources(pd3dCommandList, m_pd3dTexture.Get(), m_pd3dTextureUploadBuffer.Get(), 0, 0, 1, &subresource);

    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pd3dTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    pd3dCommandList->ResourceBarrier(1, &barrier);

    // Create Shader Resource View (SRV) at the specified handle
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = m_pd3dTexture->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = m_pd3dTexture->GetDesc().MipLevels;
    pd3dDevice->CreateShaderResourceView(m_pd3dTexture.Get(), &srvDesc, srvCpuHandle);
}

void GameObject::ReleaseUploadBuffers()
{
    if (m_pd3dTextureUploadBuffer) m_pd3dTextureUploadBuffer = nullptr;
}
