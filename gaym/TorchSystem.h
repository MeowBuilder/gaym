#pragma once

#include "stdafx.h"
#include "DescriptorHeap.h"
#include <vector>

// Forward declarations
class Mesh;
class Shader;
class GameObject;
class Scene;
struct PassConstants;
struct TorchLight;

// Individual torch data
struct TorchData
{
    XMFLOAT3 m_xmf3Position;       // World position
    float    m_fBaseIntensity;     // Base brightness (0.7~1.0)
    float    m_fCurrentIntensity;  // Current brightness after flicker
    float    m_fFlickerTimer;      // Timer for flicker animation
    float    m_fFlickerOffset;     // Random offset for varied flicker
    GameObject* m_pMeshObject;     // Torch mesh object
    float    m_fFlameScale = 1.0f; // Per-torch flame scale (1.0 = default, 2.0 = double)
    float    m_fHeightOffset = 4.0f; // 기본 flame 높이 오프셋 (Brazier 같은 큰 받침엔 따로 지정)
};

// Torch system manages torch placement, flickering lights, and flame billboards
class TorchSystem
{
public:
    TorchSystem();
    ~TorchSystem();

    // Initialize the system (load torch mesh, flame texture)
    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
              class Scene* pScene, Shader* pShader,
              CDescriptorHeap* pDescriptorHeap, UINT nDescriptorStart);

    // Add a torch at the specified position
    //   flameScale: flame billboard/light 크기 배율 (1.0 = 기본, 2.0 = 두 배)
    //   spawnMesh : torch mesh(TorchHighPoly) 함께 스폰할지. Brazier 처럼 mesh 가 이미 맵에 있으면 false.
    //   heightOffset: position 위로 flame 얼마만큼 띄울지 (기본 4.0, Brazier 큰 받침이면 더 크게)
    void AddTorch(const XMFLOAT3& position, ID3D12Device* pDevice,
                  ID3D12GraphicsCommandList* pCommandList,
                  float flameScale = 1.0f,
                  bool  spawnMesh  = true,
                  float heightOffset = 4.0f);

    // Update all torches (flicker effect)
    void Update(float deltaTime);

    // Fill PassConstants with torch light data
    void FillLightData(PassConstants* pPassConstants);

    // Render flame billboards
    void Render(ID3D12GraphicsCommandList* pCommandList,
                const XMFLOAT4X4& viewProj,
                const XMFLOAT3& camRight, const XMFLOAT3& camUp);

    // Clear all torches (for map transitions)
    void Clear();

    // Get torch count
    size_t GetTorchCount() const { return m_vTorches.size(); }

    // Check if system is initialized
    bool IsInitialized() const { return m_bInitialized; }

private:
    void CreateBillboardResources(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);
    void LoadFlameTexture(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);

private:
    std::vector<TorchData> m_vTorches;

    // Torch mesh (shared by all torches)
    Mesh* m_pTorchMesh = nullptr;

    // Flame billboard resources
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pFlameTexture;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pFlameTextureUpload;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pBillboardPSO;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_pBillboardRootSig;

    // Vertex buffer for billboards
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pBillboardVB;
    D3D12_VERTEX_BUFFER_VIEW m_BillboardVBView;

    // Constant buffer for billboard rendering
    Microsoft::WRL::ComPtr<ID3D12Resource> m_pBillboardCB;
    UINT8* m_pBillboardCBMapped = nullptr;

    // Descriptor handles
    D3D12_GPU_DESCRIPTOR_HANDLE m_FlameTextureSrvGpu;
    CDescriptorHeap* m_pDescriptorHeap = nullptr;
    UINT m_nDescriptorStart = 0;

    // Scene reference
    Scene* m_pScene = nullptr;
    Shader* m_pShader = nullptr;

    // Light settings
    XMFLOAT3 m_xmf3LightColor = XMFLOAT3(1.0f, 0.5f, 0.15f);  // Brighter orange
    float m_fLightRange = 40.0f;   // Larger range
    float m_fMinIntensity = 0.85f;
    float m_fMaxIntensity = 1.0f;
    float m_fFlickerSpeed = 1.5f;  // Slower, more subtle flicker

    // Flame billboard settings
    float m_fFlameWidth = 2.5f;   // Flame width
    float m_fFlameHeight = 4.0f;  // Flame height
    XMFLOAT3 m_xmf3FlameOffset = XMFLOAT3(0.0f, 4.0f, 0.0f);  // Position at torch top

    bool m_bInitialized = false;
};
