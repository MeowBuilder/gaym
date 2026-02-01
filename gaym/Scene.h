#pragma once

#include <vector>
#include <memory>
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

struct ID3D12Device;
struct ID3D12GraphicsCommandList;

// Drop interaction state machine
enum class DropInteractionState
{
    None,           // No drop nearby
    NearDrop,       // Near a drop, can press F
    SelectingRune   // Choosing from 3 runes
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
    void Render(ID3D12GraphicsCommandList* pCommandList);

    CCamera* GetCamera() const { return m_pCamera.get(); } // Added getter for CCamera
    CRoom* GetCurrentRoom() const { return m_pCurrentRoom; } // Added getter for current room
    ProjectileManager* GetProjectileManager() { return m_pProjectileManager.get(); }
    ParticleSystem* GetParticleSystem() { return m_pParticleSystem.get(); }
    GameObject* GetPlayer() const { return m_pPlayerGameObject; }

    // Interaction system
    bool IsNearInteractionCube() const;
    bool IsInteractionCubeActive() const { return m_bInteractionCubeActive; }
    void TriggerInteraction();

    // Drop interaction system
    DropInteractionState GetDropInteractionState() const { return m_eDropState; }
    bool IsNearDropItem() const;
    void StartDropInteraction();  // Called when F pressed near drop
    void SelectRune(int choice);  // Called when 1/2/3 pressed during selection
    void CancelDropInteraction(); // Cancel selection (e.g., ESC or walk away)
    bool IsSelectingRune() const { return m_eDropState == DropInteractionState::SelectingRune; }

    GameObject* CreateGameObject(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);

    void AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* pCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* pGpuHandle)
    {
        *pCpuHandle = m_pDescriptorHeap->GetCPUHandle(m_nNextDescriptorIndex);
        *pGpuHandle = m_pDescriptorHeap->GetGPUHandle(m_nNextDescriptorIndex);
        m_nNextDescriptorIndex++;
    }

private:

    std::vector<std::unique_ptr<GameObject>> m_vGameObjects; // Global Objects (Player, etc.)
    std::vector<std::unique_ptr<CRoom>> m_vRooms; // Room List
    CRoom* m_pCurrentRoom = nullptr; // Pointer to the current active room

    // Interaction Cube
    GameObject* m_pInteractionCube = nullptr;
    bool m_bInteractionCubeActive = true;
    bool m_bEnemiesSpawned = false;
    float m_fInteractionDistance = 5.0f;

    // Drop interaction
    DropInteractionState m_eDropState = DropInteractionState::None;
    GameObject* m_pCurrentDropItem = nullptr;  // The drop we're interacting with
    float m_fDropInteractionDistance = 5.0f;

    std::vector<std::unique_ptr<Shader>> m_vShaders;

    std::unique_ptr<CDescriptorHeap> m_pDescriptorHeap;
    UINT m_nNextDescriptorIndex = 0;

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

    void AddRenderComponentsToHierarchy(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, GameObject* pGameObject, Shader* pShader);
    void PrintHierarchy(GameObject* pGameObject, int nDepth);
    void CollectColliders(GameObject* pGameObject, std::vector<ColliderComponent*>& outColliders);
public:
    D3D12_GPU_VIRTUAL_ADDRESS GetPassCBVAddress() const { if(m_pd3dcbPass) return m_pd3dcbPass->GetGPUVirtualAddress(); return 0; }
};