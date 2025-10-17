#include "stdafx.h"
#include "DescriptorHeap.h"

CDescriptorHeap::CDescriptorHeap()
{
}

CDescriptorHeap::~CDescriptorHeap()
{
}

bool CDescriptorHeap::Create(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE eHeapType, UINT nDescriptors, bool bShaderVisible)
{
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.NumDescriptors = nDescriptors;
    desc.Type = eHeapType;
    desc.Flags = bShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 0;

    if (FAILED(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pd3dDescriptorHeap))))
    {
        return false;
    }

    m_d3dCpuHandleStart = m_pd3dDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    if (bShaderVisible)
    {
        m_d3dGpuHandleStart = m_pd3dDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
    }

    m_nDescriptorIncrementSize = pDevice->GetDescriptorHandleIncrementSize(eHeapType);

    return true;
}

D3D12_CPU_DESCRIPTOR_HANDLE CDescriptorHeap::GetCPUHandle(UINT nIndex) const
{
    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_d3dCpuHandleStart;
    handle.ptr += (m_nDescriptorIncrementSize * nIndex);
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE CDescriptorHeap::GetGPUHandle(UINT nIndex) const
{
    D3D12_GPU_DESCRIPTOR_HANDLE handle = m_d3dGpuHandleStart;
    handle.ptr += (m_nDescriptorIncrementSize * nIndex);
    return handle;
}
