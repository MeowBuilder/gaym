#include "stdafx.h"
#include "Shader.h"
#include "RenderComponent.h"

Shader::Shader()
{
}

Shader::~Shader()
{
}

void Shader::AddRenderComponent(RenderComponent* pRenderComponent)
{
    m_vRenderComponents.push_back(pRenderComponent);
}

void Shader::Render(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_VIRTUAL_ADDRESS d3dPassCBVAddress)
{
    pCommandList->SetPipelineState(m_pd3dPipelineState.Get());
    pCommandList->SetGraphicsRootSignature(m_pd3dRootSignature.Get());

    // Set the pass constant buffer view for root parameter 1
    pCommandList->SetGraphicsRootConstantBufferView(1, d3dPassCBVAddress);

    for (auto& pRenderComp : m_vRenderComponents)
    {
        pRenderComp->Render(pCommandList);
    }
}

void Shader::Build(ID3D12Device* pDevice)
{
    // Create a root signature with 3 parameters: Object CBV, Pass CBV, Texture SRV
    D3D12_ROOT_PARAMETER d3dRootParameters[3];

    // Parameter 0: Descriptor table for the per-object constant buffer (b0)
    D3D12_DESCRIPTOR_RANGE d3dDescriptorRange;
    d3dDescriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    d3dDescriptorRange.NumDescriptors = 1;
    d3dDescriptorRange.BaseShaderRegister = 0; // b0
    d3dDescriptorRange.RegisterSpace = 0;
    d3dDescriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    d3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    d3dRootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    d3dRootParameters[0].DescriptorTable.pDescriptorRanges = &d3dDescriptorRange;
    d3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Parameter 1: Root CBV for the per-pass constant buffer (b1)
    d3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameters[1].Descriptor.ShaderRegister = 1; // b1
    d3dRootParameters[1].Descriptor.RegisterSpace = 0;
    d3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // Parameter 2: Descriptor table for the texture SRV (t0)
    D3D12_DESCRIPTOR_RANGE d3dTextureRange;
    d3dTextureRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    d3dTextureRange.NumDescriptors = 1;
    d3dTextureRange.BaseShaderRegister = 0; // t0
    d3dTextureRange.RegisterSpace = 0;
    d3dTextureRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    d3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    d3dRootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
    d3dRootParameters[2].DescriptorTable.pDescriptorRanges = &d3dTextureRange;
    d3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static Sampler (s0)
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.MipLODBias = 0;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.MinLOD = 0.0f;
    sampler.MaxLOD = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    d3dRootSignatureDesc.NumParameters = 3;
    d3dRootSignatureDesc.pParameters = d3dRootParameters;
    d3dRootSignatureDesc.NumStaticSamplers = 1;
    d3dRootSignatureDesc.pStaticSamplers = &sampler;
    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> pd3dSignatureBlob;
    ComPtr<ID3DBlob> pd3dErrorBlob;
    CHECK_HR(D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob));
    CHECK_HR(pDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&m_pd3dRootSignature));

    ComPtr<ID3DBlob> vsBlob, psBlob;
    UINT compileFlags = 0;
#if defined(_DEBUG)
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    CHECK_HR(D3DCompileFromFile(L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_5_1", compileFlags, 0, &vsBlob, &pd3dErrorBlob));
    CHECK_HR(D3DCompileFromFile(L"shaders.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS", "ps_5_1", compileFlags, 0, &psBlob, &pd3dErrorBlob));

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 2, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEINDICES", 0, DXGI_FORMAT_R32G32B32A32_SINT, 3, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BONEWEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 4, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.pRootSignature = m_pd3dRootSignature.Get();
    psoDesc.VS = { reinterpret_cast<BYTE*>(vsBlob->GetBufferPointer()), vsBlob->GetBufferSize() };
    psoDesc.PS = { reinterpret_cast<BYTE*>(psBlob->GetBufferPointer()), psBlob->GetBufferSize() };
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
    psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
    psoDesc.BlendState.IndependentBlendEnable = FALSE;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;

    CHECK_HR(pDevice->CreateGraphicsPipelineState(&psoDesc, __uuidof(ID3D12PipelineState), (void**)&m_pd3dPipelineState));
}