#pragma once

// �ּ� ����
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include <stdexcept>
#include <cassert>

using Microsoft::WRL::ComPtr;

// ���� ���� üũ
#ifndef CHECK_HR
#define CHECK_HR(x) do{ HRESULT _hr=(x); if(FAILED(_hr)){ char _msg[256]; sprintf_s(_msg,"HRESULT=0x%08X at %s:%d",_hr,__FILE__,__LINE__); OutputDebugStringA(_msg); throw std::runtime_error(_msg);} }while(0)
#endif

// ������ ���� ����
static constexpr UINT kFrameCount = 2;