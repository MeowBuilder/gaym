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
#include "ProjectileManager.h" // Added ProjectileManager include
#include "ParticleSystem.h" // Added ParticleSystem include
#include "DebugRenderer.h" // Added DebugRenderer include

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

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
};

class Scene
{
public:
    Scene();
    ~Scene();

    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);
    void LoadSceneFromFile(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, const char* pstrFileName);
    void Update(float deltaTime, InputSystem* pInputSystem);
    void RenderShadowPass(ID3D12GraphicsCommandList* pCommandList);
    void Render(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle);

    CCamera* GetCamera() const { return m_pCamera.get(); } // Added getter for CCamera
    CRoom* GetCurrentRoom() const { return m_pCurrentRoom; } // Added getter for current room
    void SetCurrentRoom(CRoom* pRoom) { m_pCurrentRoom = pRoom; }
    ProjectileManager* GetProjectileManager() { return m_pProjectileManager.get(); }
    ParticleSystem* GetParticleSystem() { return m_pParticleSystem.get(); }
    GameObject* GetPlayer() const { return m_pPlayerGameObject; }

    // Interaction system
    bool IsNearInteractionCube() const;
    bool IsInteractionCubeActive() const { return m_bInteractionCubeActive; }
    void TriggerInteraction();

    // Portal interaction system
    bool IsNearPortalCube() const;
    void TriggerPortalInteraction();
    void TransitionToNextRoom();

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

    void AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* pCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* pGpuHandle)
    {
        *pCpuHandle = m_pDescriptorHeap->GetCPUHandle(m_nNextDescriptorIndex);
        *pGpuHandle = m_pDescriptorHeap->GetGPUHandle(m_nNextDescriptorIndex);
        m_nNextDescriptorIndex++;
    }

    // Update persistent descriptor watermark (call after allocating permanent descriptors like Shadow Map SRV)
    void UpdatePersistentDescriptorEnd()
    {
        m_nPersistentDescriptorEnd = m_nNextDescriptorIndex;
    }

private:

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

    // Drop interaction
    DropInteractionState m_eDropState = DropInteractionState::None;
    GameObject* m_pCurrentDropItem = nullptr;  // The drop we're interacting with
    float m_fDropInteractionDistance = 5.0f;
    ActivationType m_eSelectedRune = ActivationType::None;  // Selected rune waiting for skill assignment

    // ── Map pool ──────────────────────────────────────────────────────────────
    // Add map JSON paths here. TransitionToNextRoom picks one at random.
    std::vector<std::string> m_vMapPool;
    std::string              m_strCurrentMap;   // Path of the currently loaded map

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

    // Debug Renderer (F1 to toggle)
    std::unique_ptr<DebugRenderer> m_pDebugRenderer;

    void AddRenderComponentsToHierarchy(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, GameObject* pGameObject, Shader* pShader, bool bCastsShadow = false);
    void PrintHierarchy(GameObject* pGameObject, int nDepth);
    void CollectColliders(GameObject* pGameObject, std::vector<ColliderComponent*>& outColliders);
    void ProcessPendingDeletions();
    void UpdateRenderList();  // Update RenderComponent list for current frame
public:
    D3D12_GPU_VIRTUAL_ADDRESS GetPassCBVAddress() const { if(m_pd3dcbPass) return m_pd3dcbPass->GetGPUVirtualAddress(); return 0; }
};