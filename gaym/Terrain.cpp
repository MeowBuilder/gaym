#include "stdafx.h"
#undef min
#undef max
#include "Terrain.h"
#include "MapLoader.h"       // JsonVal
#include "WICTextureLoader12.h"
#include "D3dx12.h"
#include "Dx12App.h"
#include <fstream>
#include <algorithm>

// ================================================================
// Load  (진입점)
// ================================================================
bool Terrain::Load(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                   const char* configJsonPath, int subdivisionStep)
{
    // baseDir = config 파일이 있는 폴더 (텍스처 경로 기준)
    std::string configPath = configJsonPath;
    std::string baseDir;
    size_t slash = configPath.find_last_of("/\\");
    baseDir = (slash != std::string::npos) ? configPath.substr(0, slash + 1) : "";

    OutputDebugStringA("[Terrain] Loading terrain config...\n");

    if (!ParseConfig(configJsonPath, baseDir))
    {
        OutputDebugStringA("[Terrain] Failed to parse config.\n");
        return false;
    }

    std::string hmPath = baseDir + m_strHeightmapFile;
    if (!BuildMesh(pDevice, pCommandList, hmPath, subdivisionStep))
    {
        OutputDebugStringA("[Terrain] Failed to build mesh.\n");
        return false;
    }

    CreateDummyTexture(pDevice, pCommandList);

    if (!LoadTextures(pDevice, pCommandList, baseDir))
    {
        OutputDebugStringA("[Terrain] Failed to load textures.\n");
        return false;
    }

    BuildSrvHeap(pDevice);
    BuildRootSigAndPSO(pDevice);
    CreateConstantBuffer(pDevice);
    UpdateConstantBuffer();

    m_bLoaded = true;
    OutputDebugStringA("[Terrain] Terrain loaded successfully.\n");
    return true;
}

// ================================================================
// ParseConfig  (terrain_config.json 파싱)
// ================================================================
bool Terrain::ParseConfig(const char* configPath, const std::string& baseDir)
{
    JsonVal root = JsonVal::parseFile(configPath);
    if (root.isNull())
    {
        char msg[256];
        sprintf_s(msg, "[Terrain] Cannot open config: %s\n", configPath);
        OutputDebugStringA(msg);
        return false;
    }

    // ── terrain 기본 정보 ──
    const JsonVal& t = root["terrain"];
    m_xmf3TerrainPos  = { t["posX"].f(),  t["posY"].f(),  t["posZ"].f()  };
    m_xmf3TerrainSize = { t["sizeX"].f(), t["sizeY"].f(), t["sizeZ"].f() };
    m_nHeightmapRes   = t["heightmapResolution"].i();
    m_strHeightmapFile = t["heightmapFile"].str;

    // ── 레이어 정보 ──
    const JsonVal& layers = root["layers"];
    m_nLayerCount = (int)std::min(layers.size(), (size_t)TERRAIN_MAX_LAYERS);

    for (int i = 0; i < m_nLayerCount; i++)
    {
        const JsonVal& l = layers[i];
        m_Layers[i].diffusePath     = l["diffuse"].str;
        m_Layers[i].normalPath      = l["normal"].str;
        m_Layers[i].tileSizeX       = l["tileSizeX"].f();
        m_Layers[i].tileSizeZ       = l["tileSizeZ"].f();
        m_Layers[i].tileOffsetX     = l["tileOffsetX"].f();
        m_Layers[i].tileOffsetZ     = l["tileOffsetZ"].f();
        m_Layers[i].splatmapIndex   = l["splatmapIndex"].i();
        m_Layers[i].splatmapChannel = l["splatmapChannel"].i();
    }

    // ── 플레이 공간 홀 (선택 항목, 없으면 기본값 유지) ──
    if (root.has("playAreaHole"))
    {
        const JsonVal& hole = root["playAreaHole"];
        m_fHoleMinX = hole["minX"].f();
        m_fHoleMaxX = hole["maxX"].f();
        m_fHoleMinZ = hole["minZ"].f();
        m_fHoleMaxZ = hole["maxZ"].f();
    }

    // ── 스플랫맵 경로 ──
    const JsonVal& splats = root["splatmaps"];
    m_nSplatmapCount = (int)std::min(splats.size(), (size_t)TERRAIN_MAX_SPLATMAPS);
    for (int s = 0; s < m_nSplatmapCount; s++)
        m_sSplatmapPaths[s] = splats[s].str;

    char msg[256];
    sprintf_s(msg, "[Terrain] Config parsed: size=(%.0f,%.0f,%.0f) layers=%d splatmaps=%d\n",
              m_xmf3TerrainSize.x, m_xmf3TerrainSize.y, m_xmf3TerrainSize.z,
              m_nLayerCount, m_nSplatmapCount);
    OutputDebugStringA(msg);
    return true;
}

// ================================================================
// BuildMesh  (heightmap.r16 → 정점/인덱스 버퍼)
// ================================================================
bool Terrain::BuildMesh(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                        const std::string& heightmapPath, int step)
{
    // ── .r16 로드 (16-bit little-endian) ──
    std::ifstream file(heightmapPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        char msg[256];
        sprintf_s(msg, "[Terrain] Cannot open heightmap: %s\n", heightmapPath.c_str());
        OutputDebugStringA(msg);
        return false;
    }

    size_t fileSize = (size_t)file.tellg();
    file.seekg(0);
    std::vector<uint8_t> rawBytes(fileSize);
    file.read(reinterpret_cast<char*>(rawBytes.data()), fileSize);
    file.close();

    int fullRes = m_nHeightmapRes;  // 예: 513

    // 16-bit little-endian → float [0~1]
    std::vector<float> heights(fullRes * fullRes);
    for (int i = 0; i < fullRes * fullRes; i++)
    {
        uint16_t raw = (uint16_t)(rawBytes[i * 2]) | ((uint16_t)(rawBytes[i * 2 + 1]) << 8);
        heights[i] = raw / 65535.0f;
    }

    // ── 다운샘플링된 그리드 크기 ──
    // gridRes = (fullRes-1)/step + 1  (항상 endpoints 포함)
    int gridRes = (fullRes - 1) / step + 1;
    float cellX = m_xmf3TerrainSize.x / (float)(gridRes - 1);
    float cellZ = m_xmf3TerrainSize.z / (float)(gridRes - 1);

    // ── 높이 샘플 헬퍼 (step 고려, 경계 클램프) ──
    auto getH = [&](int gridRow, int gridCol) -> float
    {
        int r = std::min(gridRow * step, fullRes - 1);
        int c = std::min(gridCol * step, fullRes - 1);
        return heights[r * fullRes + c] * m_xmf3TerrainSize.y;
    };

    // ── 정점 생성 ──
    std::vector<TerrainVertex> verts;
    verts.reserve(gridRes * gridRes);

    for (int row = 0; row < gridRes; row++)
    {
        for (int col = 0; col < gridRes; col++)
        {
            float worldX = m_xmf3TerrainPos.x + col * cellX;
            float worldY = m_xmf3TerrainPos.y + getH(row, col);
            float worldZ = m_xmf3TerrainPos.z + row * cellZ;

            // 이웃 높이로 노멀 계산 (중앙 차분)
            float hL = getH(row, std::max(col - 1, 0));
            float hR = getH(row, std::min(col + 1, gridRes - 1));
            float hD = getH(std::max(row - 1, 0), col);
            float hU = getH(std::min(row + 1, gridRes - 1), col);

            // tangentX: X 방향 탄젠트, tangentZ: Z 방향 탄젠트
            XMVECTOR tangentX = XMVectorSet(2.0f * cellX, hR - hL, 0.0f, 0.0f);
            XMVECTOR tangentZ = XMVectorSet(0.0f, hU - hD, 2.0f * cellZ, 0.0f);

            // 왼손계 Y-up: normal = cross(tangentZ, tangentX) → 위 방향
            XMFLOAT3 normal;
            XMStoreFloat3(&normal, XMVector3Normalize(XMVector3Cross(tangentZ, tangentX)));

            // UV: 0~1, 스플랫맵 샘플링용
            float u = (float)col / (float)(gridRes - 1);
            float v = (float)row / (float)(gridRes - 1);

            verts.push_back({ {worldX, worldY, worldZ}, normal, {u, v} });
        }
    }

    // ── 인덱스 생성 (CW winding, FrontCounterClockwise=TRUE → CCW=앞면, CW=뒷면 컬링) ──
    std::vector<UINT> indices;
    indices.reserve((gridRes - 1) * (gridRes - 1) * 6);

    for (int row = 0; row < gridRes - 1; row++)
    {
        for (int col = 0; col < gridRes - 1; col++)
        {
            UINT tl = row * gridRes + col;
            UINT tr = tl + 1;
            UINT bl = tl + gridRes;
            UINT br = bl + 1;

            // 삼각형 1: TL → TR → BL
            indices.push_back(tl); indices.push_back(tr); indices.push_back(bl);
            // 삼각형 2: TR → BR → BL
            indices.push_back(tr); indices.push_back(br); indices.push_back(bl);
        }
    }

    m_nIndexCount = (UINT)indices.size();

    // ── 버퍼 생성 ──
    UINT vbBytes = (UINT)(verts.size() * sizeof(TerrainVertex));
    UINT ibBytes = (UINT)(indices.size() * sizeof(UINT));

    m_pVB.Attach(CreateBufferResource(pDevice, pCommandList,
        verts.data(), vbBytes,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
        m_pVBUpload.GetAddressOf()));

    m_pIB.Attach(CreateBufferResource(pDevice, pCommandList,
        indices.data(), ibBytes,
        D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER,
        m_pIBUpload.GetAddressOf()));

    m_VBView.BufferLocation = m_pVB->GetGPUVirtualAddress();
    m_VBView.SizeInBytes    = vbBytes;
    m_VBView.StrideInBytes  = sizeof(TerrainVertex);

    m_IBView.BufferLocation = m_pIB->GetGPUVirtualAddress();
    m_IBView.SizeInBytes    = ibBytes;
    m_IBView.Format         = DXGI_FORMAT_R32_UINT;

    char msg[256];
    sprintf_s(msg, "[Terrain] Mesh built: grid=%dx%d verts=%zu tris=%u\n",
              gridRes, gridRes, verts.size(), m_nIndexCount / 3);
    OutputDebugStringA(msg);
    return true;
}

// ================================================================
// CreateDummyTexture  (빈 슬롯 채우기용 1x1 흰색 텍스처)
// ================================================================
void Terrain::CreateDummyTexture(ID3D12Device* pDevice,
                                  ID3D12GraphicsCommandList* pCommandList)
{
    // 1x1 RGBA8 흰색
    uint8_t pixel[4] = { 255, 255, 255, 255 };

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width            = 1;
    texDesc.Height           = 1;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels        = 1;
    texDesc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc       = { 1, 0 };
    texDesc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags            = D3D12_RESOURCE_FLAG_NONE;

    CD3DX12_HEAP_PROPERTIES defaultHeap(D3D12_HEAP_TYPE_DEFAULT);
    pDevice->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE,
        &texDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
        IID_PPV_ARGS(m_pDummyTexture.GetAddressOf()));

    UINT64 uploadSize = GetRequiredIntermediateSize(m_pDummyTexture.Get(), 0, 1);
    m_pDummyUpload.Attach(CreateBufferResource(pDevice, pCommandList, nullptr,
        uploadSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr));

    D3D12_SUBRESOURCE_DATA sub = {};
    sub.pData      = pixel;
    sub.RowPitch   = 4;
    sub.SlicePitch = 4;
    UpdateSubresources(pCommandList, m_pDummyTexture.Get(), m_pDummyUpload.Get(), 0, 0, 1, &sub);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pDummyTexture.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    pCommandList->ResourceBarrier(1, &barrier);
}

// ================================================================
// LoadTextures  (splatmap + layer diffuse/normal)
// ================================================================
bool Terrain::LoadTextures(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                            const std::string& baseDir)
{
    // ── 스플랫맵 ──
    for (int s = 0; s < m_nSplatmapCount; s++)
    {
        std::string fullPath = baseDir + m_sSplatmapPaths[s];
        if (!LoadWICTexture(pDevice, pCommandList, fullPath,
                            m_pSplatmap[s], m_pSplatmapUpload[s]))
        {
            char msg[256];
            sprintf_s(msg, "[Terrain] Splatmap %d load failed: %s\n", s, fullPath.c_str());
            OutputDebugStringA(msg);
            return false;
        }
        char msg[256];
        sprintf_s(msg, "[Terrain] Splatmap %d loaded: %s\n", s, fullPath.c_str());
        OutputDebugStringA(msg);
    }

    // ── 레이어 텍스처 ──
    for (int i = 0; i < m_nLayerCount; i++)
    {
        // Diffuse (필수)
        if (!m_Layers[i].diffusePath.empty())
        {
            std::string path = baseDir + m_Layers[i].diffusePath;
            if (!LoadWICTexture(pDevice, pCommandList, path,
                                m_pLayerDiffuse[i], m_pLayerDiffuseUpload[i]))
            {
                char msg[256];
                sprintf_s(msg, "[Terrain] Layer %d diffuse load failed: %s\n", i, path.c_str());
                OutputDebugStringA(msg);
                // 실패해도 계속 (dummy로 대체)
            }
        }

        // Normal (선택)
        if (!m_Layers[i].normalPath.empty())
        {
            std::string path = baseDir + m_Layers[i].normalPath;
            if (!LoadWICTexture(pDevice, pCommandList, path,
                                m_pLayerNormal[i], m_pLayerNormalUpload[i]))
            {
                char msg[256];
                sprintf_s(msg, "[Terrain] Layer %d normal load failed (skip): %s\n", i, path.c_str());
                OutputDebugStringA(msg);
            }
        }
    }

    return true;
}

// ================================================================
// LoadWICTexture  (WICTextureLoader12 래퍼)
// ================================================================
bool Terrain::LoadWICTexture(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                              const std::string& path,
                              ComPtr<ID3D12Resource>& outTex,
                              ComPtr<ID3D12Resource>& outUpload)
{
    std::wstring wpath(path.begin(), path.end());
    std::unique_ptr<uint8_t[]> decodedData;
    D3D12_SUBRESOURCE_DATA subresource;

    HRESULT hr = DirectX::LoadWICTextureFromFile(
        pDevice, wpath.c_str(), outTex.GetAddressOf(), decodedData, subresource);
    if (FAILED(hr)) return false;

    UINT64 nBytes = GetRequiredIntermediateSize(outTex.Get(), 0, 1);
    outUpload.Attach(CreateBufferResource(pDevice, pCommandList, nullptr,
        nBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr));

    UpdateSubresources(pCommandList, outTex.Get(), outUpload.Get(), 0, 0, 1, &subresource);

    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(outTex.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    pCommandList->ResourceBarrier(1, &barrier);

    return true;
}

// ================================================================
// BuildSrvHeap  (전용 SRV 힙 구성)
// ================================================================
void Terrain::BuildSrvHeap(ID3D12Device* pDevice)
{
    m_pSrvHeap = std::make_unique<CDescriptorHeap>();
    m_pSrvHeap->Create(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                       SRV_HEAP_SIZE, true);

    // ── Slot 0: splatmap0 ──
    if (m_pSplatmap[0])
        CreateSRV(pDevice, m_pSplatmap[0].Get(), SRV_SLOT_SPLATMAP);
    else
        CreateSRV(pDevice, m_pDummyTexture.Get(), SRV_SLOT_SPLATMAP);

    // ── Slot 1~4: layer diffuse ──
    for (int i = 0; i < TERRAIN_MAX_LAYERS; i++)
    {
        ID3D12Resource* res = (m_pLayerDiffuse[i]) ? m_pLayerDiffuse[i].Get()
                                                    : m_pDummyTexture.Get();
        CreateSRV(pDevice, res, SRV_SLOT_DIFFUSE0 + i);
    }

    // ── Slot 5~8: layer normal ──
    for (int i = 0; i < TERRAIN_MAX_LAYERS; i++)
    {
        ID3D12Resource* res = (m_pLayerNormal[i]) ? m_pLayerNormal[i].Get()
                                                   : m_pDummyTexture.Get();
        CreateSRV(pDevice, res, SRV_SLOT_NORMAL0 + i);
    }

    // ── Slot 9: dummy ──
    CreateSRV(pDevice, m_pDummyTexture.Get(), SRV_SLOT_DUMMY);
}

void Terrain::CreateSRV(ID3D12Device* pDevice, ID3D12Resource* pResource,
                         int heapSlot, DXGI_FORMAT fmt)
{
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = (fmt != DXGI_FORMAT_UNKNOWN) ? fmt : pResource->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    pDevice->CreateShaderResourceView(pResource, &srvDesc,
                                       m_pSrvHeap->GetCPUHandle(heapSlot));
}

// ================================================================
// BuildRootSigAndPSO
// ================================================================
void Terrain::BuildRootSigAndPSO(ID3D12Device* pDevice)
{
    // ── Root Signature ──
    // Param 0: Root CBV b0 → TerrainCB
    // Param 1: Root CBV b1 → PassCB
    // Param 2: Table → t0 (splatmap)          힙 슬롯 0
    // Param 3: Table → t1~t4 (layer diffuse)  힙 슬롯 1~4 (4개 연속)
    // Param 4: Table → t5~t8 (layer normal)   힙 슬롯 5~8 (4개 연속)

    D3D12_ROOT_PARAMETER params[5] = {};

    // Param 0: TerrainCB (b0)
    params[0].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[0].Descriptor.ShaderRegister = 0;
    params[0].Descriptor.RegisterSpace  = 0;
    params[0].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

    // Param 1: PassCB (b1)
    params[1].ParameterType             = D3D12_ROOT_PARAMETER_TYPE_CBV;
    params[1].Descriptor.ShaderRegister = 1;
    params[1].Descriptor.RegisterSpace  = 0;
    params[1].ShaderVisibility          = D3D12_SHADER_VISIBILITY_ALL;

    // Param 2: splatmap0 (t0, 1개)
    D3D12_DESCRIPTOR_RANGE rangeSplat = {};
    rangeSplat.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeSplat.NumDescriptors     = 1;
    rangeSplat.BaseShaderRegister = 0; // t0
    rangeSplat.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    params[2].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[2].DescriptorTable.NumDescriptorRanges = 1;
    params[2].DescriptorTable.pDescriptorRanges   = &rangeSplat;
    params[2].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    // Param 3: layer diffuse t1~t4 (4개 연속)
    D3D12_DESCRIPTOR_RANGE rangeDiff = {};
    rangeDiff.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeDiff.NumDescriptors     = TERRAIN_MAX_LAYERS;
    rangeDiff.BaseShaderRegister = 1; // t1~t4
    rangeDiff.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    params[3].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[3].DescriptorTable.NumDescriptorRanges = 1;
    params[3].DescriptorTable.pDescriptorRanges   = &rangeDiff;
    params[3].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    // Param 4: layer normal t5~t8 (4개 연속)
    D3D12_DESCRIPTOR_RANGE rangeNorm = {};
    rangeNorm.RangeType          = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    rangeNorm.NumDescriptors     = TERRAIN_MAX_LAYERS;
    rangeNorm.BaseShaderRegister = 5; // t5~t8
    rangeNorm.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    params[4].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[4].DescriptorTable.NumDescriptorRanges = 1;
    params[4].DescriptorTable.pDescriptorRanges   = &rangeNorm;
    params[4].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;

    // Static Sampler: s0 LINEAR_WRAP
    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_ALWAYS;
    sampler.MaxLOD           = D3D12_FLOAT32_MAX;
    sampler.ShaderRegister   = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.NumParameters     = 5;
    rsDesc.pParameters       = params;
    rsDesc.NumStaticSamplers = 1;
    rsDesc.pStaticSamplers   = &sampler;
    rsDesc.Flags             = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> sigBlob, errBlob;
    CHECK_HR(D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                          &sigBlob, &errBlob));
    CHECK_HR(pDevice->CreateRootSignature(0, sigBlob->GetBufferPointer(),
                                           sigBlob->GetBufferSize(),
                                           IID_PPV_ARGS(&m_pRootSig)));

    // ── 셰이더 컴파일 ──
    ComPtr<ID3DBlob> vsBlob, psBlob, errB;
    UINT flags = 0;
#if defined(_DEBUG)
    flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    HRESULT hr;
    hr = D3DCompileFromFile(L"terrain.hlsl", nullptr,
                             D3D_COMPILE_STANDARD_FILE_INCLUDE,
                             "VS_Terrain", "vs_5_1", flags, 0, &vsBlob, &errB);
    if (FAILED(hr))
    {
        if (errB)
            OutputDebugStringA((char*)errB->GetBufferPointer());
        throw std::runtime_error("terrain.hlsl VS compile failed");
    }

    hr = D3DCompileFromFile(L"terrain.hlsl", nullptr,
                             D3D_COMPILE_STANDARD_FILE_INCLUDE,
                             "PS_Terrain", "ps_5_1", flags, 0, &psBlob, &errB);
    if (FAILED(hr))
    {
        if (errB)
            OutputDebugStringA((char*)errB->GetBufferPointer());
        throw std::runtime_error("terrain.hlsl PS compile failed");
    }

    // ── Input Layout ──
    D3D12_INPUT_ELEMENT_DESC inputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    // ── PSO ──
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout    = { inputLayout, _countof(inputLayout) };
    psoDesc.pRootSignature = m_pRootSig.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    psoDesc.RasterizerState.FillMode              = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode              = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;  // 인덱스가 CW 와인딩이므로 CCW=앞면

    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.DepthStencilState.DepthEnable    = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc      = D3D12_COMPARISON_FUNC_LESS;

    psoDesc.SampleMask             = UINT_MAX;
    psoDesc.PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets       = 1;
    psoDesc.RTVFormats[0]          = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat              = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count       = 1;

    CHECK_HR(pDevice->CreateGraphicsPipelineState(&psoDesc,
                                                   IID_PPV_ARGS(&m_pPSO)));
    OutputDebugStringA("[Terrain] Root Signature + PSO built.\n");
}

// ================================================================
// CreateConstantBuffer
// ================================================================
void Terrain::CreateConstantBuffer(ID3D12Device* pDevice)
{
    UINT cbSize = (sizeof(TerrainCB) + 255) & ~255;
    m_pCB.Attach(CreateBufferResource(pDevice, nullptr, nullptr, cbSize,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr));
    m_pCB->Map(0, nullptr, reinterpret_cast<void**>(&m_pMappedCB));
}

// ================================================================
// UpdateConstantBuffer  (월드 행렬 + 레이어 타일링 업로드)
// ================================================================
void Terrain::UpdateConstantBuffer()
{
    if (!m_pMappedCB) return;

    // 월드 행렬 = 오프셋 이동 (TerrainPos는 이미 정점에 구워져 있음 → WorldOffset만)
    XMMATRIX world = XMMatrixTranslation(
        m_xmf3WorldOffset.x,
        m_xmf3WorldOffset.y,
        m_xmf3WorldOffset.z);
    XMStoreFloat4x4(&m_pMappedCB->World, XMMatrixTranspose(world));

    // 레이어 타일링
    for (int i = 0; i < TERRAIN_MAX_LAYERS; i++)
    {
        if (i < m_nLayerCount)
        {
            m_pMappedCB->LayerTiling[i] = {
                m_Layers[i].tileSizeX,
                m_Layers[i].tileSizeZ,
                m_Layers[i].tileOffsetX,
                m_Layers[i].tileOffsetZ
            };
        }
        else
        {
            m_pMappedCB->LayerTiling[i] = { 10.f, 10.f, 0.f, 0.f };
        }
    }

    m_pMappedCB->LayerCount    = m_nLayerCount;
    m_pMappedCB->TerrainSizeX  = m_xmf3TerrainSize.x;
    m_pMappedCB->TerrainSizeZ  = m_xmf3TerrainSize.z;
    m_pMappedCB->HoleMinX      = m_fHoleMinX;
    m_pMappedCB->HoleMaxX      = m_fHoleMaxX;
    m_pMappedCB->HoleMinZ      = m_fHoleMinZ;
    m_pMappedCB->HoleMaxZ      = m_fHoleMaxZ;
}

// ================================================================
// SetPosition  (월드 오프셋 조정)
// ================================================================
void Terrain::SetPosition(float x, float y, float z)
{
    m_xmf3WorldOffset = { x, y, z };
    UpdateConstantBuffer();
}

// ================================================================
// Render
// ================================================================
void Terrain::Render(ID3D12GraphicsCommandList* pCommandList,
                      D3D12_GPU_VIRTUAL_ADDRESS  passCBVAddress)
{
    if (!m_bLoaded) return;

    // ── 전용 힙으로 교체 ──
    ID3D12DescriptorHeap* heaps[] = { m_pSrvHeap->GetHeap() };
    pCommandList->SetDescriptorHeaps(1, heaps);

    // ── 파이프라인 설정 ──
    pCommandList->SetGraphicsRootSignature(m_pRootSig.Get());
    pCommandList->SetPipelineState(m_pPSO.Get());

    // ── CBV 바인딩 ──
    pCommandList->SetGraphicsRootConstantBufferView(0, m_pCB->GetGPUVirtualAddress());
    pCommandList->SetGraphicsRootConstantBufferView(1, passCBVAddress);

    // ── SRV 테이블 바인딩 (힙 슬롯 순서대로) ──
    // Param 2: splatmap (슬롯 0)
    pCommandList->SetGraphicsRootDescriptorTable(2, m_pSrvHeap->GetGPUHandle(SRV_SLOT_SPLATMAP));
    // Param 3: layer diffuse (슬롯 1~4 시작)
    pCommandList->SetGraphicsRootDescriptorTable(3, m_pSrvHeap->GetGPUHandle(SRV_SLOT_DIFFUSE0));
    // Param 4: layer normal (슬롯 5~8 시작)
    pCommandList->SetGraphicsRootDescriptorTable(4, m_pSrvHeap->GetGPUHandle(SRV_SLOT_NORMAL0));

    // ── 드로우 ──
    pCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pCommandList->IASetVertexBuffers(0, 1, &m_VBView);
    pCommandList->IASetIndexBuffer(&m_IBView);
    pCommandList->DrawIndexedInstanced(m_nIndexCount, 1, 0, 0, 0);
}
