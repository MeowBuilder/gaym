#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>

#include <string>
#include <vector>
#include <iostream>

#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <D3Dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <mmsystem.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "winmm.lib")

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#ifndef CHECK_HR
#define CHECK_HR(x) do{ HRESULT _hr=(x); if(FAILED(_hr)){ char _msg[256]; sprintf_s(_msg,"HRESULT=0x%08X at %s:%d",_hr,__FILE__,__LINE__); OutputDebugStringA(_msg); throw std::runtime_error(_msg);} }while(0)
#endif

#define RANDOM_COLOR XMFLOAT4(rand() / float(RAND_MAX), rand() / float(RAND_MAX), rand() / float(RAND_MAX), 1.0f)

namespace Vector3
{
    inline XMFLOAT3 Add(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
    {
        XMFLOAT3 xmf3Result;
        XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) + XMLoadFloat3(&xmf3Vector2));
        return(xmf3Result);
    }

    inline XMFLOAT3 Add(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2, float fScalar)
    {
        XMFLOAT3 xmf3Result;
        XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) + (XMLoadFloat3(&xmf3Vector2) * fScalar));
        return(xmf3Result);
    }

    inline XMFLOAT3 Subtract(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
    {
        XMFLOAT3 xmf3Result;
        XMStoreFloat3(&xmf3Result, XMLoadFloat3(&xmf3Vector1) - XMLoadFloat3(&xmf3Vector2));
        return(xmf3Result);
    }

    inline float DotProduct(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2)
    {
        XMFLOAT3 xmf3Result;
        XMStoreFloat3(&xmf3Result, XMVector3Dot(XMLoadFloat3(&xmf3Vector1), XMLoadFloat3(&xmf3Vector2)));
        return(xmf3Result.x);
    }

    inline XMFLOAT3 CrossProduct(const XMFLOAT3& xmf3Vector1, const XMFLOAT3& xmf3Vector2, bool bNormalize = true)
    {
        XMFLOAT3 xmf3Result;
        if (bNormalize)
            XMStoreFloat3(&xmf3Result, XMVector3Normalize(XMVector3Cross(XMLoadFloat3(&xmf3Vector1), XMLoadFloat3(&xmf3Vector2))));
        else
            XMStoreFloat3(&xmf3Result, XMVector3Cross(XMLoadFloat3(&xmf3Vector1), XMLoadFloat3(&xmf3Vector2)));
        return(xmf3Result);
    }

    inline XMFLOAT3 Normalize(const XMFLOAT3& xmf3Vector)
    {
        XMFLOAT3 m_xmf3Normal;
        XMStoreFloat3(&m_xmf3Normal, XMVector3Normalize(XMLoadFloat3(&xmf3Vector)));
        return(m_xmf3Normal);
    }

    inline float Length(const XMFLOAT3& xmf3Vector)
    {
        XMFLOAT3 xmf3Result;
        XMStoreFloat3(&xmf3Result, XMVector3Length(XMLoadFloat3(&xmf3Vector)));
        return(xmf3Result.x);
    }
}

namespace Vector4
{
    inline XMFLOAT4 Add(const XMFLOAT4& xmf4Vector1, const XMFLOAT4& xmf4Vector2)
    {
        XMFLOAT4 xmf4Result;
        XMStoreFloat4(&xmf4Result, XMLoadFloat4(&xmf4Vector1) + XMLoadFloat4(&xmf4Vector2));
        return(xmf4Result);
    }
}

namespace Matrix4x4
{
    inline XMFLOAT4X4 Identity()
    {
        XMFLOAT4X4 xmf4x4Result;
        XMStoreFloat4x4(&xmf4x4Result, XMMatrixIdentity());
        return(xmf4x4Result);
    }

    inline XMFLOAT4X4 Multiply(const XMFLOAT4X4& xmf4x4Matrix1, const XMFLOAT4X4& xmf4x4Matrix2)
    {
        XMFLOAT4X4 xmf4x4Result;
        XMStoreFloat4x4(&xmf4x4Result, XMLoadFloat4x4(&xmf4x4Matrix1) * XMLoadFloat4x4(&xmf4x4Matrix2));
        return(xmf4x4Result);
    }

    inline XMFLOAT4X4 Inverse(const XMFLOAT4X4& xmf4x4Matrix)
    {
        XMFLOAT4X4 xmf4x4Result;
        XMStoreFloat4x4(&xmf4x4Result, XMMatrixInverse(NULL, XMLoadFloat4x4(&xmf4x4Matrix)));
        return(xmf4x4Result);
    }

    inline XMFLOAT4X4 Transpose(const XMFLOAT4X4& xmf4x4Matrix)
    {
        XMFLOAT4X4 xmf4x4Result;
        XMStoreFloat4x4(&xmf4x4Result, XMMatrixTranspose(XMLoadFloat4x4(&xmf4x4Matrix)));
        return(xmf4x4Result);
    }

    inline XMFLOAT4X4 PerspectiveFovLH(float FovAngleY, float AspectRatio, float NearZ, float FarZ)
    {
        XMFLOAT4X4 xmf4x4Result;
        XMStoreFloat4x4(&xmf4x4Result, XMMatrixPerspectiveFovLH(FovAngleY, AspectRatio, NearZ, FarZ));
        return(xmf4x4Result);
    }

    inline XMFLOAT4X4 LookAtLH(const XMFLOAT3& EyePosition, const XMFLOAT3& FocusPosition, const XMFLOAT3& UpDirection)
    {
        XMFLOAT4X4 xmf4x4Result;
        XMStoreFloat4x4(&xmf4x4Result, XMMatrixLookAtLH(XMLoadFloat3(&EyePosition), XMLoadFloat3(&FocusPosition), XMLoadFloat3(&UpDirection)));
        return(xmf4x4Result);
    }
}

ID3D12Resource* CreateBufferResource(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, void* pData, UINT nBytes, D3D12_HEAP_TYPE d3dHeapType = D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATES d3dResourceStates = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, ID3D12Resource** ppd3dUploadBuffer = NULL);

static constexpr UINT kFrameCount = 2;
static constexpr UINT kWindowWidth = 1920;
static constexpr UINT kWindowHeight = 1080;
