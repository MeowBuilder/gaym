#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include "GameObject.h"
#include "SkillTypes.h"  // For ActivationType
#include "Shader.h"
#include "Mesh.h"
#include "DescriptorHeap.h"
#include "InputSystem.h"
#include "Camera.h"
#include "Room.h" // Added Room.h include
#include "CollisionManager.h" // Added CollisionManager include
#include "EnemySpawner.h" // Added EnemySpawner include
#include "EnemyComponent.h" // For BossIntroPhase, EnemyComponent*
#include "ProjectileManager.h" // Added ProjectileManager include
#include "ParticleSystem.h" // Added ParticleSystem include
#include "FluidParticleSystem.h" // Added FluidParticleSystem include
#include "FluidSkillEffect.h"   // Added FluidSkillEffect include
#include "FluidSkillVFXManager.h" // Added FluidSkillVFXManager include
#include "ScreenSpaceFluid.h" // Screen-Space Fluid Renderer
#include "DebugRenderer.h" // Added DebugRenderer include
#include "Terrain.h"       // Decorative terrain

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

// Stage theme for different environments
enum class StageTheme
{
    Fire,   // Volcanic/lava theme (default)
    Water,  // Ocean/water theme
    Earth,  // Ground/rock theme
    Grass   // Wind/forest theme
};

// Drop interaction state machine
enum class DropInteractionState
{
    None,           // No drop nearby
    NearDrop,       // Near a drop, can press F
    SelectingRune,  // Choosing from 3 runes (click one)
    SelectingSkill  // Choosing which skill slot to assign rune to
};

struct SpotLight
{
    XMFLOAT4 m_xmf4SpotLightColor;
    XMFLOAT3 m_xmf3SpotLightPosition; float m_fSpotLightRange;
    XMFLOAT3 m_xmf3SpotLightDirection; float m_fSpotLightInnerCone;
    float m_fSpotLightOuterCone; float m_fPad5; float m_fPad6; float m_fPad7;
};

// Torch point light data (for GPU constant buffer)
static constexpr int MAX_TORCH_LIGHTS = 8;

struct TorchLight
{
    XMFLOAT3 m_xmf3Position; float m_fRange;
    XMFLOAT3 m_xmf3Color;    float m_fIntensity;
};

// Gerstner Wave Parameters
struct WaveParams
{
    float m_fWavelength;
    float m_fAmplitude;
    float m_fSteepness;
    float m_fSpeed;
    XMFLOAT2 m_xmf2Direction;
    float m_fFadeSpeed;
    float m_fPad;
};

struct PassConstants
{
    XMFLOAT4X4 m_xmf4x4ViewProj;
    XMFLOAT4X4 m_xmf4x4LightViewProj;  // Shadow Map용 Light View-Projection
    XMFLOAT4 m_xmf4LightColor;
    XMFLOAT3 m_xmf3LightDirection; float m_fPad0;
    XMFLOAT4 m_xmf4PointLightColor;
    XMFLOAT3 m_xmf3PointLightPosition; float m_fPad1;
    float m_fPointLightRange; float m_fPad2; float m_fPad3; float m_fPad4;
    XMFLOAT4 m_xmf4AmbientLight;
    XMFLOAT3 m_xmf3CameraPosition; float m_fPadCam; // Camera World Position
    SpotLight m_SpotLight;
    float m_fTime; float m_fTimePad1; float m_fTimePad2; float m_fTimePad3; // 게임 시간 (용암 애니메이션용)

    // Torch lights array
    TorchLight m_TorchLights[MAX_TORCH_LIGHTS];
    int m_nActiveTorchLights; int m_nTorchPad1; int m_nTorchPad2; int m_nTorchPad3;

    // Gerstner Waves (5 waves for ocean simulation)
    WaveParams m_Waves[5];
};

// Include TorchSystem after PassConstants is defined (avoid circular include)
#include "TorchSystem.h"

class Scene
{
public:
    Scene();
    ~Scene();

    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);
    void LoadSceneFromFile(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, const char* pstrFileName);
    void Update(float deltaTime, InputSystem* pInputSystem);
    void RenderShadowPass(ID3D12GraphicsCommandList* pCommandList);
    void Render(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle,
                D3D12_CPU_DESCRIPTOR_HANDLE mainRTV, D3D12_CPU_DESCRIPTOR_HANDLE mainDSV,
                ID3D12Resource* pMainRTBuffer = nullptr);
    void OnResizeSSF(UINT width, UINT height);

    CCamera* GetCamera() const { return m_pCamera.get(); } // Added getter for CCamera
    CRoom* GetCurrentRoom() const { return m_pCurrentRoom; } // Added getter for current room
    void SetCurrentRoom(CRoom* pRoom) { m_pCurrentRoom = pRoom; }
    ProjectileManager* GetProjectileManager() { return m_pProjectileManager.get(); }
    ParticleSystem* GetParticleSystem() { return m_pParticleSystem.get(); }
    FluidParticleSystem* GetFluidParticleSystem() { return m_pFluidParticleSystem.get(); }
    FluidSkillVFXManager* GetFluidVFXManager() { return m_pFluidVFXManager.get(); }
    TorchSystem* GetTorchSystem() { return m_pTorchSystem.get(); }
    GameObject* GetPlayer() const { return m_pPlayerGameObject; }
    std::vector<GameObject*> GetAllPlayers() const;  // 로컬 + 원격 플레이어 목록 반환
    void RegisterPlayersToEnemy(class EnemyComponent* pEnemy);  // 적에게 플레이어 등록
    Shader* GetDefaultShader() const { return m_vShaders.empty() ? nullptr : m_vShaders[0].get(); }

    // Interaction system
    bool IsNearInteractionCube() const;
    bool IsInteractionCubeActive() const { return m_bInteractionCubeActive; }
    void TriggerInteraction();

    // Portal interaction system
    bool IsNearPortalCube() const;
    void TriggerPortalInteraction();
    void TransitionToNextRoom();
    void TransitionToRoomByIndex(int index); // pool 인덱스 직접 지정 이동 (서버 동기화 / 9·0 디버그)
    void TransitionToBossRoom();        // 불 보스전 (Dragon)
    void TransitionToWaterStage();      // 물 스테이지 (N: 불→물)
    void TransitionToWaterBossRoom();   // 물 보스전 (Kraken)
    void TransitionToEarthStage();      // 땅 스테이지 (N: 물→땅)
    void TransitionToEarthBossRoom();   // 땅 보스전 (Golem)
    void TransitionToGrassStage();      // 풀 스테이지 (N: 땅→풀)
    void TransitionToGrassBossRoom();   // 풀 보스전 (Demon)

    // Drop interaction system
    DropInteractionState GetDropInteractionState() const { return m_eDropState; }
    bool IsNearDropItem() const;
    void StartDropInteraction();  // Called when F pressed near drop
    void SelectRune(int choice);  // Called when 1/2/3 pressed during selection
    void SelectRuneByClick(int runeIndex);  // Called when mouse clicks on a rune option
    void SelectSkillSlot(SkillSlot slot, int runeSlotIndex);  // Called when clicking skill's rune slot
    void CancelDropInteraction(); // Cancel selection (e.g., ESC or walk away)
    bool IsSelectingRune() const { return m_eDropState == DropInteractionState::SelectingRune; }
    bool IsSelectingSkill() const { return m_eDropState == DropInteractionState::SelectingSkill; }
    ActivationType GetSelectedRune() const { return m_eSelectedRune; }

    GameObject* CreateGameObject(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);

    // GameObject deletion (deferred to end of frame)
    void MarkForDeletion(GameObject* pGameObject);
    CollisionManager* GetCollisionManager() { return m_pCollisionManager.get(); }

    // MapLoader support
    CRoom* CreateRoomFromBounds(const XMFLOAT3& center, const XMFLOAT3& extents);
    EnemySpawner* GetEnemySpawner() { return m_pEnemySpawner.get(); }
    const std::vector<std::unique_ptr<CRoom>>& GetRooms() const { return m_vRooms; }

    // Network support
    void AddRenderComponentsToHierarchy(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, GameObject* pGameObject, Shader* pShader, bool bCastsShadow = false);

    void AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* pCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* pGpuHandle)
    {
        if (m_nNextDescriptorIndex >= 16384)
        {
            OutputDebugString(L"[Scene] ERROR: Descriptor heap overflow! Increase heap size.\n");
            *pCpuHandle = m_pDescriptorHeap->GetCPUHandle(0);
            *pGpuHandle = m_pDescriptorHeap->GetGPUHandle(0);
            return;
        }
        *pCpuHandle = m_pDescriptorHeap->GetCPUHandle(m_nNextDescriptorIndex);
        *pGpuHandle = m_pDescriptorHeap->GetGPUHandle(m_nNextDescriptorIndex);
        m_nNextDescriptorIndex++;
    }

    // Update persistent descriptor watermark (call after allocating permanent descriptors like Shadow Map SRV)
    void UpdatePersistentDescriptorEnd()
    {
        m_nPersistentDescriptorEnd = m_nNextDescriptorIndex;
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetPassCBVAddress() const { if(m_pd3dcbPass) return m_pd3dcbPass->GetGPUVirtualAddress(); return 0; }

private:
    float m_fTotalTime = 0.0f;
    float m_fLastDeltaTime = 0.016f;
    bool m_bInBossRoom = false;  // 보스 룸 여부 (클리어 시 다음 스테이지 전환)
    StageTheme m_eCurrentTheme = StageTheme::Fire; // 현재 스테이지 테마
    GameObject* m_pLavaPlane = nullptr; // 용암 바닥 평면
    GameObject* m_pWaterPlane = nullptr; // 물 바닥 평면

    // Additional water textures (Water_6 + foam4)
    ComPtr<ID3D12Resource> m_pd3dWaterNormal2 = nullptr;      // Water_6_Normal.png (t7)
    ComPtr<ID3D12Resource> m_pd3dWaterHeight2 = nullptr;      // Water_6_Height.png (t8)
    ComPtr<ID3D12Resource> m_pd3dFoamOpacity = nullptr;       // foam4_Opacity.tga (t9)
    ComPtr<ID3D12Resource> m_pd3dFoamDiffuse = nullptr;       // foam4_Diffuse.tif (t10)
    ComPtr<ID3D12Resource> m_pd3dWaterNormal2Upload = nullptr;
    ComPtr<ID3D12Resource> m_pd3dWaterHeight2Upload = nullptr;
    ComPtr<ID3D12Resource> m_pd3dFoamOpacityUpload = nullptr;
    ComPtr<ID3D12Resource> m_pd3dFoamDiffuseUpload = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE m_d3dWaterNormal2GpuHandle = {};  // GPU handle for t7
    D3D12_GPU_DESCRIPTOR_HANDLE m_d3dWaterHeight2GpuHandle = {};  // GPU handle for t8
    D3D12_GPU_DESCRIPTOR_HANDLE m_d3dFoamOpacityGpuHandle = {};   // GPU handle for t9
    D3D12_GPU_DESCRIPTOR_HANDLE m_d3dFoamDiffuseGpuHandle = {};   // GPU handle for t10

    std::vector<std::unique_ptr<GameObject>> m_vGameObjects; // Global Objects (Player, etc.)
    std::vector<GameObject*> m_vPendingDeletions; // Objects marked for deletion (processed at end of frame)
    std::vector<std::unique_ptr<CRoom>> m_vRooms; // Room List
    CRoom* m_pCurrentRoom = nullptr; // Pointer to the current active room
    int m_nRoomCount = 0; // Room counter for tracking progression

    // Interaction Cube
    GameObject* m_pInteractionCube = nullptr;
    bool m_bInteractionCubeActive = true;
    bool m_bEnemiesSpawned = false;
    float m_fInteractionDistance = 5.0f;

    // Fire boss Dragon intro cutscene
    EnemyComponent* m_pDragonIntroEnemy = nullptr;
    BossIntroPhase  m_eLastDragonPhase  = BossIntroPhase::None;

    // Water boss 2-phase: Kraken pre-spawned, emerges after Blue Dragon dies
    bool m_bPendingKrakenSpawn = false;          // death callback set this
    XMFLOAT3 m_xmf3PendingKrakenPos = {};        // dragon death position
    EnemyComponent* m_pPreloadedKraken = nullptr; // pre-spawned but hidden

    // Kraken 컷씬: Rumble→Rise→Burst→Reveal→Roar→Jump→Slam→WaterRise→None
    //  - Jump: Roar 위치 → 맵 바깥 수면 위로 포물선 점프
    //  - Slam: 수면 쾅 내려치기 (카메라 쉐이크)
    //  - WaterRise: 서서히 차오르는 수면이 플레이어를 기존 맵보다 높은 곳으로 띄움
    enum class KrakenCutsceneStage {
        None,
        Rumble, Rise, Burst, Reveal,
        Roar,
        Jump,
        Slam,
        WaterRise
    };
    KrakenCutsceneStage m_eKrakenStage = KrakenCutsceneStage::None;
    float m_fKrakenEmergeTimer = 0.0f;
    bool  m_bSlamShakeTriggered = false;
    XMFLOAT3 m_xmf3KrakenJumpStart = {}; // Roar 종료 시 크라켄 위치 (점프 시작점)
    XMFLOAT3 m_xmf3KrakenJumpEnd   = {}; // 점프 착지점 (맵 바깥 수면 위 슬램 지점)
    static constexpr float KRAKEN_SCALE = 2.0f;
    // Stage durations (cumulative)
    static constexpr float KRAKEN_T_RUMBLE     = 0.8f;
    static constexpr float KRAKEN_T_RISE       = 1.7f;
    static constexpr float KRAKEN_T_BURST      = 3.0f;
    static constexpr float KRAKEN_T_REVEAL     = 4.5f;
    static constexpr float KRAKEN_T_ROAR       = 6.5f;   // Reveal + 2.0s
    static constexpr float KRAKEN_T_JUMP       = 8.3f;   // Roar + 1.8s (빠른 점프 arc)
    static constexpr float KRAKEN_T_SLAM       = 9.3f;   // Jump + 1.0s (임팩트)
    static constexpr float KRAKEN_T_WATER_RISE = 17.3f;  // Slam + 8s (서서히 차오름)

    // 슬램/점프 착지점 (맵 바깥 수면 위)
    static constexpr float KRAKEN_SLAM_OFFSET_X = 0.0f;
    static constexpr float KRAKEN_SLAM_OFFSET_Z = -70.0f;
    static constexpr float KRAKEN_LAND_Y        = -2.0f;   // 수면(-4) 위 몸체 노출 Y
    static constexpr float KRAKEN_JUMP_PEAK_DY  = 25.0f;   // 점프 최고점 추가 Y

    // 물 상승: 기존 맵 타일(Y=0)보다 훨씬 위로 → "더 높은 곳에서 전투" 연출
    static constexpr float KRAKEN_WATER_Y_START = -4.0f;
    static constexpr float KRAKEN_WATER_Y_END   = 15.0f;

    // Drop interaction
    DropInteractionState m_eDropState = DropInteractionState::None;
    GameObject* m_pCurrentDropItem = nullptr;  // The drop we're interacting with
    float m_fDropInteractionDistance = 5.0f;
    ActivationType m_eSelectedRune = ActivationType::None;  // Selected rune waiting for skill assignment

    // ── Map pool ──────────────────────────────────────────────────────────────
    // Add map JSON paths here. TransitionToNextRoom picks one at random.
    std::vector<std::string> m_vMapPool;
    std::string              m_strCurrentMap;   // Path of the currently loaded map
    std::string              m_strBossMap;       // Path of the boss room JSON (from rooms.json "bossRoom")
    int                      m_nCurrentPoolIndex = 0; // Index into m_vMapPool for 9/0 nav

    void ReAddRenderComponentsToShader(GameObject* pGO);  // Traverse hierarchy

    std::vector<std::unique_ptr<Shader>> m_vShaders;

    std::unique_ptr<CDescriptorHeap> m_pDescriptorHeap;
    UINT m_nNextDescriptorIndex = 0;
    UINT m_nPersistentDescriptorEnd = 0;  // Watermark after permanent objects; recyclable slots start here

    // 재활용 구간의 CB 리소스 캐시 — 맵 전환마다 CreateCommittedResource 호출 방지
    // key = 디스크립터 슬롯 번호 (SRV 슬롯이 끼어들어도 정확히 매핑됨)
    std::unordered_map<UINT, ComPtr<ID3D12Resource>> m_vCBCache;

    // Pass Constant Buffer
    ComPtr<ID3D12Resource> m_pd3dcbPass = nullptr;
    PassConstants* m_pcbMappedPass = nullptr;

    std::unique_ptr<CCamera> m_pCamera; // Added CCamera member
    GameObject* m_pPlayerGameObject = nullptr; // Added player GameObject pointer

    // Collision System
    std::unique_ptr<CollisionManager> m_pCollisionManager;

    // Enemy System
    std::unique_ptr<EnemySpawner> m_pEnemySpawner;

    // Projectile System
    std::unique_ptr<ProjectileManager> m_pProjectileManager;

    // Particle System
    std::unique_ptr<ParticleSystem> m_pParticleSystem;
    int m_nEmberEmitterId = -1; // Floating embers emitter ID

    // Fluid Particle System (SPH)
    std::unique_ptr<FluidParticleSystem> m_pFluidParticleSystem;

    // Fluid Skill Effect (connects SkillComponent to FluidParticleSystem)
    std::unique_ptr<FluidSkillEffect> m_pFluidSkillEffect;

    // Fluid Skill VFX Manager (투사체 유체 이펙트, 최대 8개 동시)
    std::unique_ptr<FluidSkillVFXManager> m_pFluidVFXManager;

    // Screen-Space Fluid Renderer
    std::unique_ptr<ScreenSpaceFluid> m_pSSF;

    // Debug Renderer (F1 to toggle)
    std::unique_ptr<DebugRenderer> m_pDebugRenderer;

    // Torch System (flickering lights and flame billboards)
    std::unique_ptr<TorchSystem> m_pTorchSystem;

    // Decorative terrain (장식용, 충돌 없음)
    std::unique_ptr<Terrain> m_pTerrain;
    void LoadTerrain(const char* configJsonPath, int subdivisionStep = 4);

    void PrintHierarchy(GameObject* pGameObject, int nDepth);
    void CollectColliders(GameObject* pGameObject, std::vector<ColliderComponent*>& outColliders);
    void ProcessPendingDeletions();
    void UpdateRenderList();  // Update RenderComponent list for current frame
};