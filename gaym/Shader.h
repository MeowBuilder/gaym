#pragma once

#include "stdafx.h"

class RenderComponent;

class Shader
{
public:
    Shader();
    ~Shader();

    void Render(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_VIRTUAL_ADDRESS d3dPassCBVAddress, D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle);
    void RenderShadowPass(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_VIRTUAL_ADDRESS d3dPassCBVAddress);

    void AddRenderComponent(RenderComponent* pRenderComponent);
    void RemoveRenderComponent(RenderComponent* pRenderComponent);
    void ClearRenderComponents() { m_vRenderComponents.clear(); }

    virtual void Build(ID3D12Device* pDevice);

    ID3D12RootSignature* GetRootSignature() const { return m_pd3dRootSignature.Get(); }

private:
    ComPtr<ID3D12RootSignature> m_pd3dRootSignature;
    ComPtr<ID3D12PipelineState> m_pd3dPipelineState;
    ComPtr<ID3D12PipelineState> m_pd3dShadowPSO;  // Shadow Pass PSO
    ComPtr<ID3D12PipelineState> m_pd3dWaterPSO;   // Water PSO (alpha blending)

    std::vector<RenderComponent*> m_vRenderComponents;
};