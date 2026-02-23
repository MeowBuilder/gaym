#include "stdafx.h"
#include "DebugRenderer.h"
#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "CollisionLayer.h"

DebugRenderer::DebugRenderer()
{
}

DebugRenderer::~DebugRenderer()
{
    if (m_pBoxCB && m_pBoxCBData)
    {
        m_pBoxCB->Unmap(0, nullptr);
    }
}

void DebugRenderer::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    CreatePipelineState(pDevice);
    CreateWireframeCube(pDevice, pCommandList);

    // Create constant buffer for per-box transforms
    UINT cbSize = BOX_CB_SIZE * MAX_DEBUG_BOXES;
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = cbSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    CHECK_HR(pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_pBoxCB)));

    m_pBoxCB->Map(0, nullptr, reinterpret_cast<void**>(&m_pBoxCBData));
}

void DebugRenderer::CreatePipelineState(ID3D12Device* pDevice)
{
    // Simple root signature: root constant for world matrix + color, pass CBV
    D3D12_ROOT_PARAMETER rootParams[2] = {};

    // Root parameter 0: 32-bit constants (world matrix 16 floats + color 4 floats = 20)
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    rootParams[0].Constants.ShaderRegister = 0; // b0
    rootParams[0].Constants.RegisterSpace = 0;
    rootParams[0].Constants.Num32BitValues = 20; // 4x4 matrix + RGBA color
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Root parameter 1: Pass CBV
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[1].Descriptor.ShaderRegister = 1; // b1
    rootParams[1].Descriptor.RegisterSpace = 0;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
    rootSigDesc.NumParameters = 2;
    rootSigDesc.pParameters = rootParams;
    rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sigBlob, errorBlob;
    CHECK_HR(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errorBlob));
    CHECK_HR(pDevice->CreateRootSignature(0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&m_pRootSignature)));

    // Compile simple debug shaders inline
    const char* shaderCode = R"(
        cbuffer DebugCB : register(b0)
        {
            float4x4 gWorld;
            float4 gColor;
        };

        cbuffer PassCB : register(b1)
        {
            float4x4 gViewProj;
        };

        struct VSInput
        {
            float3 position : POSITION;
        };

        struct PSInput
        {
            float4 position : SV_POSITION;
            float4 color : COLOR;
        };

        PSInput VSDebug(VSInput input)
        {
            PSInput output;
            float4 worldPos = mul(float4(input.position, 1.0f), gWorld);
            output.position = mul(worldPos, gViewProj);
            output.color = gColor;
            return output;
        }

        float4 PSDebug(PSInput input) : SV_TARGET
        {
            return input.color;
        }
    )";

    ComPtr<ID3DBlob> vsBlob, psBlob;
    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    CHECK_HR(D3DCompile(shaderCode, strlen(shaderCode), "DebugShader", nullptr, nullptr,
        "VSDebug", "vs_5_0", compileFlags, 0, &vsBlob, &errorBlob));
    CHECK_HR(D3DCompile(shaderCode, strlen(shaderCode), "DebugShader", nullptr, nullptr,
        "PSDebug", "ps_5_0", compileFlags, 0, &psBlob, &errorBlob));

    // Input layout - just position
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.pRootSignature = m_pRootSignature.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.DepthClipEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO; // Don't write depth
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    CHECK_HR(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));
}

void DebugRenderer::CreateWireframeCube(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    // Unit cube vertices (centered at origin, size 2x2x2 from -1 to 1)
    XMFLOAT3 vertices[] = {
        { -1.0f, -1.0f, -1.0f },
        {  1.0f, -1.0f, -1.0f },
        {  1.0f,  1.0f, -1.0f },
        { -1.0f,  1.0f, -1.0f },
        { -1.0f, -1.0f,  1.0f },
        {  1.0f, -1.0f,  1.0f },
        {  1.0f,  1.0f,  1.0f },
        { -1.0f,  1.0f,  1.0f }
    };

    // Indices for 12 triangles (6 faces, 2 triangles each)
    UINT16 indices[] = {
        // Front
        0, 2, 1, 0, 3, 2,
        // Back
        4, 5, 6, 4, 6, 7,
        // Left
        0, 4, 7, 0, 7, 3,
        // Right
        1, 2, 6, 1, 6, 5,
        // Top
        3, 7, 6, 3, 6, 2,
        // Bottom
        0, 1, 5, 0, 5, 4
    };
    m_nIndices = _countof(indices);

    // Create vertex buffer
    UINT vbSize = sizeof(vertices);
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Width = vbSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    CHECK_HR(pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&m_pVertexBuffer)));

    // Upload buffer
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    CHECK_HR(pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_pVertexUploadBuffer)));

    // Copy data
    void* pData;
    m_pVertexUploadBuffer->Map(0, nullptr, &pData);
    memcpy(pData, vertices, vbSize);
    m_pVertexUploadBuffer->Unmap(0, nullptr);

    pCommandList->CopyResource(m_pVertexBuffer.Get(), m_pVertexUploadBuffer.Get());

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_pVertexBuffer.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    pCommandList->ResourceBarrier(1, &barrier);

    m_VertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
    m_VertexBufferView.StrideInBytes = sizeof(XMFLOAT3);
    m_VertexBufferView.SizeInBytes = vbSize;

    // Create index buffer
    UINT ibSize = sizeof(indices);
    resourceDesc.Width = ibSize;
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    CHECK_HR(pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(&m_pIndexBuffer)));

    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    CHECK_HR(pDevice->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_pIndexUploadBuffer)));

    m_pIndexUploadBuffer->Map(0, nullptr, &pData);
    memcpy(pData, indices, ibSize);
    m_pIndexUploadBuffer->Unmap(0, nullptr);

    pCommandList->CopyResource(m_pIndexBuffer.Get(), m_pIndexUploadBuffer.Get());

    barrier.Transition.pResource = m_pIndexBuffer.Get();
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
    pCommandList->ResourceBarrier(1, &barrier);

    m_IndexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
    m_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
    m_IndexBufferView.SizeInBytes = ibSize;
}

void DebugRenderer::Render(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_VIRTUAL_ADDRESS passCBV,
                           const std::vector<ColliderComponent*>& colliders)
{
    if (!m_bEnabled || colliders.empty()) return;

    pCommandList->SetPipelineState(m_pPipelineState.Get());
    pCommandList->SetGraphicsRootSignature(m_pRootSignature.Get());
    pCommandList->SetGraphicsRootConstantBufferView(1, passCBV);

    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
    pCommandList->IASetIndexBuffer(&m_IndexBufferView);

    UINT boxIndex = 0;
    for (const auto* pCollider : colliders)
    {
        if (!pCollider || !pCollider->IsEnabled() || boxIndex >= MAX_DEBUG_BOXES) continue;

        const DirectX::BoundingOrientedBox& obb = pCollider->GetBoundingBox();

        // Build world matrix from OBB (center, extents, orientation)
        XMMATRIX scale = XMMatrixScaling(obb.Extents.x, obb.Extents.y, obb.Extents.z);
        XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&obb.Orientation));
        XMMATRIX translation = XMMatrixTranslation(obb.Center.x, obb.Center.y, obb.Center.z);
        XMMATRIX world = scale * rotation * translation;

        // Choose color based on collision layer
        XMFLOAT4 color;
        switch (pCollider->GetLayer())
        {
        case CollisionLayer::Player:
            color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f); // Green
            break;
        case CollisionLayer::Enemy:
            color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f); // Red
            break;
        case CollisionLayer::PlayerBullet:
            color = XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f); // Cyan
            break;
        case CollisionLayer::EnemyBullet:
            color = XMFLOAT4(1.0f, 0.5f, 0.0f, 1.0f); // Orange
            break;
        case CollisionLayer::Wall:
            color = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f); // Gray
            break;
        case CollisionLayer::Pickup:
            color = XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f); // Magenta
            break;
        case CollisionLayer::Trigger:
            color = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow
            break;
        default:
            color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f); // White
            break;
        }

        // Set root constants (world matrix + color)
        struct DebugConstants {
            XMFLOAT4X4 world;
            XMFLOAT4 color;
        } constants;
        XMStoreFloat4x4(&constants.world, XMMatrixTranspose(world));
        constants.color = color;

        pCommandList->SetGraphicsRoot32BitConstants(0, 20, &constants, 0);
        pCommandList->DrawIndexedInstanced(m_nIndices, 1, 0, 0, 0);

        boxIndex++;
    }
}
