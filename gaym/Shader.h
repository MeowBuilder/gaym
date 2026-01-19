#pragma once

#include "stdafx.h"

class RenderComponent;

class Shader
{
public:
    Shader();
    ~Shader();

    void Render(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_VIRTUAL_ADDRESS d3dPassCBVAddress);

    void AddRenderComponent(RenderComponent* pRenderComponent);
    void ClearRenderComponents() { m_vRenderComponents.clear(); } // Added method

    virtual void Build(ID3D12Device* pDevice);

private:
    ComPtr<ID3D12RootSignature> m_pd3dRootSignature;
    ComPtr<ID3D12PipelineState> m_pd3dPipelineState;

    std::vector<RenderComponent*> m_vRenderComponents;
};