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

void Shader::Render(ID3D12GraphicsCommandList* pCommandList)
{
    pCommandList->SetPipelineState(m_pd3dPipelineState.Get());
    pCommandList->SetGraphicsRootSignature(m_pd3dRootSignature.Get());

    for (auto& pRenderComp : m_vRenderComponents)
    {
        pRenderComp->Render(pCommandList);
    }
}

void Shader::Build(ID3D12Device* pDevice)
{
    D3D12_ROOT_DESCRIPTOR d3dRootDescriptor;
    d3dRootDescriptor.ShaderRegister = 0;
    d3dRootDescriptor.RegisterSpace = 0;

    D3D12_ROOT_PARAMETER d3dRootParameter;
    d3dRootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    d3dRootParameter.Descriptor = d3dRootDescriptor;
    d3dRootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    d3dRootSignatureDesc.NumParameters = 1;
    d3dRootSignatureDesc.pParameters = &d3dRootParameter;
    d3dRootSignatureDesc.NumStaticSamplers = 0;
    d3dRootSignatureDesc.pStaticSamplers = NULL;
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
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
