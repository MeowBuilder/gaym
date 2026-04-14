#pragma once
#include "stdafx.h"
#include "DescriptorHeap.h"

// ────────────────────────────────────────────────────────────────
// 최대 레이어 수 (Phase 1: 4레이어, Phase 2: 8레이어 확장 예정)
// ────────────────────────────────────────────────────────────────
static constexpr int TERRAIN_MAX_LAYERS    = 4;
static constexpr int TERRAIN_MAX_SPLATMAPS = 1;  // 4레이어 → splatmap 1장

// ────────────────────────────────────────────────────────────────
// 정점 구조
// ────────────────────────────────────────────────────────────────
struct TerrainVertex
{
    XMFLOAT3 position;   // 월드 공간 (높이 구워진 상태)
    XMFLOAT3 normal;     // 이웃 높이 차분으로 계산
    XMFLOAT2 uv;         // 0~1 (스플랫맵 샘플링용)
};

// ────────────────────────────────────────────────────────────────
// 레이어 정보 (config.json에서 파싱)
// ────────────────────────────────────────────────────────────────
struct TerrainLayerInfo
{
    std::string diffusePath;
    std::string normalPath;
    float tileSizeX   = 10.f;
    float tileSizeZ   = 10.f;
    float tileOffsetX = 0.f;
    float tileOffsetZ = 0.f;
    int   splatmapIndex   = 0;
    int   splatmapChannel = 0;
};

// ────────────────────────────────────────────────────────────────
// GPU Constant Buffer (256-byte align)
// ────────────────────────────────────────────────────────────────
struct TerrainCB
{
    XMFLOAT4X4 World;                    // 64 bytes
    // xy = tileSizeX / tileSizeZ,  zw = tileOffsetX / tileOffsetZ
    XMFLOAT4   LayerTiling[TERRAIN_MAX_LAYERS];  // 64 bytes
    int        LayerCount;               // 4 bytes
    float      TerrainSizeX;            // 4 bytes
    float      TerrainSizeZ;            // 4 bytes
    float      pad0;                    // 4 bytes
    // 총 144 bytes → 256으로 맞추기
    float      pad1[28];                // 112 bytes
};
static_assert(sizeof(TerrainCB) == 256, "TerrainCB must be 256 bytes");

// ────────────────────────────────────────────────────────────────
// Terrain 클래스
// - 완전 독립 (Scene 메인 힙과 분리된 전용 SRV 힙 사용)
// - 장식용 정적 터레인 (충돌 없음, 업데이트 없음)
// ────────────────────────────────────────────────────────────────
class Terrain
{
public:
    Terrain()  = default;
    ~Terrain() = default;

    // configJsonPath: "Assets/Terrain/terrain_config.json" 형식
    // subdivisionStep: 1=원본해상도, 2=절반, 4=1/4 (성능 조절)
    bool Load(ID3D12Device*              pDevice,
              ID3D12GraphicsCommandList* pCommandList,
              const char*               configJsonPath,
              int                       subdivisionStep = 4);

    // 메인 렌더 패스 (불투명)
    // Terrain 렌더 후 Scene은 자신의 힙을 다시 바인딩해야 함
    void Render(ID3D12GraphicsCommandList* pCommandList,
                D3D12_GPU_VIRTUAL_ADDRESS  passCBVAddress);

    void SetPosition(float x, float y, float z);

    bool IsLoaded() const { return m_bLoaded; }

    // Terrain 렌더 후 Scene이 자신의 힙을 복구할 수 있도록 노출
    ID3D12DescriptorHeap* GetSrvHeap() const { return m_pSrvHeap->GetHeap(); }

private:
    // ── 메쉬 ─────────────────────────────────────────────────────
    ComPtr<ID3D12Resource>   m_pVB, m_pVBUpload;
    ComPtr<ID3D12Resource>   m_pIB, m_pIBUpload;
    D3D12_VERTEX_BUFFER_VIEW m_VBView = {};
    D3D12_INDEX_BUFFER_VIEW  m_IBView = {};
    UINT                     m_nIndexCount = 0;

    // ── 텍스처 리소스 ─────────────────────────────────────────────
    // SRV 힙 슬롯 레이아웃:
    //   [0]        : splatmap0
    //   [1..4]     : layer 0~3 diffuse
    //   [5..8]     : layer 0~3 normal (없으면 dummy)
    //   [9]        : dummy (1x1 white, 미사용 슬롯 채우기용)
    static constexpr int SRV_SLOT_SPLATMAP  = 0;
    static constexpr int SRV_SLOT_DIFFUSE0  = 1;
    static constexpr int SRV_SLOT_NORMAL0   = 5;
    static constexpr int SRV_SLOT_DUMMY     = 9;
    static constexpr int SRV_HEAP_SIZE      = 10;

    ComPtr<ID3D12Resource> m_pSplatmap[TERRAIN_MAX_SPLATMAPS];
    ComPtr<ID3D12Resource> m_pSplatmapUpload[TERRAIN_MAX_SPLATMAPS];
    ComPtr<ID3D12Resource> m_pLayerDiffuse[TERRAIN_MAX_LAYERS];
    ComPtr<ID3D12Resource> m_pLayerDiffuseUpload[TERRAIN_MAX_LAYERS];
    ComPtr<ID3D12Resource> m_pLayerNormal[TERRAIN_MAX_LAYERS];
    ComPtr<ID3D12Resource> m_pLayerNormalUpload[TERRAIN_MAX_LAYERS];
    ComPtr<ID3D12Resource> m_pDummyTexture;
    ComPtr<ID3D12Resource> m_pDummyUpload;

    // ── 전용 SRV 힙 ──────────────────────────────────────────────
    std::unique_ptr<CDescriptorHeap> m_pSrvHeap;

    // ── 전용 Root Signature + PSO ─────────────────────────────────
    ComPtr<ID3D12RootSignature> m_pRootSig;
    ComPtr<ID3D12PipelineState> m_pPSO;

    // ── Constant Buffer ───────────────────────────────────────────
    ComPtr<ID3D12Resource> m_pCB;
    TerrainCB*             m_pMappedCB = nullptr;

    // ── Config 파싱 결과 ──────────────────────────────────────────
    std::string            m_strHeightmapFile;
    int                    m_nHeightmapRes  = 513;
    XMFLOAT3               m_xmf3TerrainPos  = {0, 0, 0};
    XMFLOAT3               m_xmf3TerrainSize = {500, 100, 500};
    XMFLOAT3               m_xmf3WorldOffset = {0, 0, 0};
    int                    m_nLayerCount    = 0;
    int                    m_nSplatmapCount = 0;
    TerrainLayerInfo       m_Layers[TERRAIN_MAX_LAYERS];
    std::string            m_sSplatmapPaths[TERRAIN_MAX_SPLATMAPS];

    bool m_bLoaded = false;

    // ── 내부 구현 ─────────────────────────────────────────────────
    bool ParseConfig(const char* configPath, const std::string& baseDir);
    bool BuildMesh  (ID3D12Device*, ID3D12GraphicsCommandList*,
                     const std::string& heightmapPath, int step);
    bool LoadTextures(ID3D12Device*, ID3D12GraphicsCommandList*,
                      const std::string& baseDir);

    // WICTextureLoader12 래퍼
    bool LoadWICTexture(ID3D12Device*, ID3D12GraphicsCommandList*,
                        const std::string& path,
                        ComPtr<ID3D12Resource>& outTex,
                        ComPtr<ID3D12Resource>& outUpload);

    void CreateDummyTexture(ID3D12Device*, ID3D12GraphicsCommandList*);
    void BuildSrvHeap      (ID3D12Device*);
    void BuildRootSigAndPSO(ID3D12Device*);
    void CreateConstantBuffer(ID3D12Device*);
    void UpdateConstantBuffer();

    // SRV 생성 헬퍼
    void CreateSRV(ID3D12Device* pDevice, ID3D12Resource* pResource,
                   int heapSlot, DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN);
};
