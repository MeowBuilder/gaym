#pragma once

#include "stdafx.h"

class CDescriptorHeap
{
public:
    CDescriptorHeap();
    virtual ~CDescriptorHeap();

    bool Create(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE eHeapType, UINT nDescriptors, bool bShaderVisible);

    ID3D12DescriptorHeap* GetHeap() const { return m_pd3dDescriptorHeap.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(UINT nIndex) const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(UINT nIndex) const;

protected:
    ComPtr<ID3D12DescriptorHeap> m_pd3dDescriptorHeap = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dCpuHandleStart = { 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE m_d3dGpuHandleStart = { 0 };
    UINT m_nDescriptorIncrementSize = 0;
};
