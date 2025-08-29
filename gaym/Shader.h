#pragma once

#include "stdafx.h"

class RenderComponent;

class Shader
{
public:
    Shader();
    ~Shader();

    void Build(ID3D12Device* pDevice);
    void AddRenderComponent(RenderComponent* pRenderComponent);
    void Render(ID3D12GraphicsCommandList* pCommandList);

private:
    ComPtr<ID3D12RootSignature> m_pd3dRootSignature;
    ComPtr<ID3D12PipelineState> m_pd3dPipelineState;

    std::vector<RenderComponent*> m_vRenderComponents;
};
