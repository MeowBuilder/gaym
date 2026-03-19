#include "stdafx.h"
#include "EnemySpawner.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "EnemyComponent.h"
#include "ColliderComponent.h"
#include "RenderComponent.h"
#include "AnimationComponent.h"
#include "CollisionLayer.h"
#include "MeleeAttackBehavior.h"
#include "RushAoEAttackBehavior.h"
#include "RushFrontAttackBehavior.h"
#include "RangedAttackBehavior.h"
#include "BreathAttackBehavior.h"
#include "Room.h"
#include "Scene.h"
#include "ProjectileManager.h"
#include "Shader.h"
#include "Mesh.h"
#include "MeshLoader.h"

EnemySpawner::EnemySpawner()
{
}

EnemySpawner::~EnemySpawner()
{
}

void EnemySpawner::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, Scene* pScene, Shader* pShader)
{
    m_pDevice = pDevice;
    m_pCommandList = pCommandList;
    m_pScene = pScene;
    m_pShader = pShader;

    // Register default test enemy preset
    EnemySpawnData testEnemy;
    testEnemy.m_xmf3Scale = XMFLOAT3(1.0f, 2.0f, 1.0f);  // Human-like proportions
    testEnemy.m_xmf4Color = XMFLOAT4(1.0f, 0.2f, 0.2f, 1.0f);  // Red
    testEnemy.m_Stats.m_fMaxHP = 50.0f;
    testEnemy.m_Stats.m_fCurrentHP = 50.0f;
    testEnemy.m_Stats.m_fMoveSpeed = 4.0f;
    testEnemy.m_Stats.m_fAttackRange = 3.0f;
    testEnemy.m_Stats.m_fAttackCooldown = 1.5f;
    testEnemy.m_IndicatorConfig.m_eType = IndicatorType::Circle;
    testEnemy.m_IndicatorConfig.m_fHitRadius = 3.0f;
    testEnemy.m_fnCreateAttack = []() {
        return std::make_unique<MeleeAttackBehavior>(10.0f, 0.3f, 0.2f, 0.3f);
    };

    RegisterEnemyPreset("TestEnemy", testEnemy);

    // Shared animation config for Elemental enemies
    EnemyAnimationConfig elementalAnim;
    elementalAnim.m_strIdleClip    = "idle";
    elementalAnim.m_strChaseClip   = "Run_Forward";
    elementalAnim.m_strAttackClip  = "Combat_Unarmed_Attack";
    elementalAnim.m_strStaggerClip = "Combat_Stun";
    elementalAnim.m_strDeathClip   = "Death";

    // Register AirElemental preset (Melee - light blue air)
    EnemySpawnData airElemental;
    airElemental.m_strMeshPath      = "Assets/Enemies/Elementals/AirElemental_Bl/AirElemental_Bl.bin";
    airElemental.m_strAnimationPath = "Assets/Enemies/Elementals/AirElemental_Bl/AirElemental_Bl_Anim.bin";
    airElemental.m_strTexturePath   = "Assets/Enemies/Elementals/AirElemental_Bl/Textures/T_AirElemental_Body_Bl_D.png";
    airElemental.m_xmf3Scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
    airElemental.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    airElemental.m_Stats.m_fMaxHP          = 80.0f;
    airElemental.m_Stats.m_fCurrentHP      = 80.0f;
    airElemental.m_Stats.m_fMoveSpeed      = 5.0f;
    airElemental.m_Stats.m_fAttackRange    = 4.0f;
    airElemental.m_Stats.m_fAttackCooldown = 2.0f;

    airElemental.m_AnimConfig = elementalAnim;

    airElemental.m_IndicatorConfig.m_eType      = IndicatorType::Circle;
    airElemental.m_IndicatorConfig.m_fHitRadius = 4.0f;
    airElemental.m_fnCreateAttack = []() {
        return std::make_unique<MeleeAttackBehavior>(15.0f, 0.4f, 0.2f, 0.4f);
    };

    RegisterEnemyPreset("AirElemental", airElemental);

    // Register RushAoEEnemy preset (Rush + 360 AoE - FireGolem_Rd)
    EnemySpawnData rushAoE;
    rushAoE.m_strMeshPath      = "Assets/Enemies/Elementals/FireGolem_Rd/FireGolem_Rd.bin";
    rushAoE.m_strAnimationPath = "Assets/Enemies/Elementals/FireGolem_Rd/FireGolem_Rd_Anim.bin";
    rushAoE.m_strTexturePath   = "Assets/Enemies/Elementals/FireGolem_Rd/Textures/T_FireGolem_Rd_D.png";
    rushAoE.m_xmf3Scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
    rushAoE.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    rushAoE.m_Stats.m_fMaxHP          = 100.0f;
    rushAoE.m_Stats.m_fCurrentHP      = 100.0f;
    rushAoE.m_Stats.m_fMoveSpeed      = 5.0f;
    rushAoE.m_Stats.m_fAttackRange    = 20.0f;
    rushAoE.m_Stats.m_fAttackCooldown = 3.0f;

    rushAoE.m_AnimConfig = elementalAnim;

    // rushSpeed=15, rushDuration=1.2 → rushDistance=18
    rushAoE.m_IndicatorConfig.m_eType         = IndicatorType::RushCircle;
    rushAoE.m_IndicatorConfig.m_fRushDistance = 18.0f;
    rushAoE.m_IndicatorConfig.m_fHitRadius    = 5.0f;
    rushAoE.m_fnCreateAttack = []() {
        return std::make_unique<RushAoEAttackBehavior>(15.0f, 15.0f, 1.2f, 0.3f, 0.2f, 0.3f, 5.0f);
    };

    RegisterEnemyPreset("RushAoEEnemy", rushAoE);

    // Register RushFrontEnemy preset (Rush + cone - EarthElemental_Gn)
    EnemySpawnData rushFront;
    rushFront.m_strMeshPath      = "Assets/Enemies/Elementals/EarthElemental_Gn/EarthElemental_Gn.bin";
    rushFront.m_strAnimationPath = "Assets/Enemies/Elementals/EarthElemental_Gn/EarthElemental_Gn_Anim.bin";
    rushFront.m_strTexturePath   = "Assets/Enemies/Elementals/EarthElemental_Gn/Textures/T_EarthElemental_Gn_D.png";
    rushFront.m_xmf3Scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
    rushFront.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    rushFront.m_Stats.m_fMaxHP          = 80.0f;
    rushFront.m_Stats.m_fCurrentHP      = 80.0f;
    rushFront.m_Stats.m_fMoveSpeed      = 5.0f;
    rushFront.m_Stats.m_fAttackRange    = 18.0f;
    rushFront.m_Stats.m_fAttackCooldown = 2.5f;

    rushFront.m_AnimConfig = elementalAnim;

    // rushSpeed=18, rushDuration=1.0 → rushDistance=18
    rushFront.m_IndicatorConfig.m_eType         = IndicatorType::RushCone;
    rushFront.m_IndicatorConfig.m_fRushDistance = 18.0f;
    rushFront.m_IndicatorConfig.m_fHitRadius    = 4.0f;
    rushFront.m_IndicatorConfig.m_fConeAngle    = 90.0f;
    rushFront.m_fnCreateAttack = []() {
        return std::make_unique<RushFrontAttackBehavior>(20.0f, 18.0f, 1.0f, 0.2f, 0.2f, 0.3f, 4.0f, 90.0f);
    };

    RegisterEnemyPreset("RushFrontEnemy", rushFront);

    // Register RangedEnemy preset (Projectile - StormElemental_Bl)
    EnemySpawnData ranged;
    ranged.m_strMeshPath      = "Assets/Enemies/Elementals/StormElemental_Bl/StormElemental_Bl.bin";
    ranged.m_strAnimationPath = "Assets/Enemies/Elementals/StormElemental_Bl/StormElemental_Bl_Anim.bin";
    ranged.m_strTexturePath   = "Assets/Enemies/Elementals/StormElemental_Bl/Textures/T_StormElemental_Bl_D.png";
    ranged.m_xmf3Scale = XMFLOAT3(2.0f, 2.0f, 2.0f);
    ranged.m_xmf4Color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

    ranged.m_Stats.m_fMaxHP          = 60.0f;
    ranged.m_Stats.m_fCurrentHP      = 60.0f;
    ranged.m_Stats.m_fMoveSpeed      = 3.0f;
    ranged.m_Stats.m_fAttackRange    = 30.0f;
    ranged.m_Stats.m_fAttackCooldown = 2.0f;

    ranged.m_AnimConfig = elementalAnim;

    ProjectileManager* pProjMgr = pScene->GetProjectileManager();
    ranged.m_fnCreateAttack = [pProjMgr]() {
        return std::make_unique<RangedAttackBehavior>(pProjMgr, 10.0f, 20.0f, 0.5f, 0.1f, 0.5f);
    };

    RegisterEnemyPreset("RangedEnemy", ranged);

    // Register Dragon Boss preset (Flying)
    EnemySpawnData dragon;
    dragon.m_strMeshPath = "Assets/Enemies/Dragon/Red.bin";
    dragon.m_strAnimationPath = "Assets/Enemies/Dragon/Red_Anim.bin";
    dragon.m_strTexturePath = "Assets/Enemies/Dragon/Textures/RedHP.png";
    dragon.m_xmf3Scale = XMFLOAT3(2.5f, 2.5f, 2.5f);
    dragon.m_xmf4Color = XMFLOAT4(1.0f, 0.3f, 0.1f, 1.0f);
    dragon.m_Stats.m_fMaxHP = 150.0f;
    dragon.m_Stats.m_fCurrentHP = 150.0f;
    dragon.m_Stats.m_fMoveSpeed = 6.0f;
    dragon.m_Stats.m_fAttackRange = 15.0f;
    dragon.m_Stats.m_fAttackCooldown = 2.5f;

    // Flying mode disabled - boss intro handles the entrance
    dragon.m_bIsFlying = false;
    dragon.m_fFlyHeight = 0.0f;

    // Ground combat animations (intro handles flying entrance)
    dragon.m_AnimConfig.m_strIdleClip = "Idle01";
    dragon.m_AnimConfig.m_strChaseClip = "Walk";
    dragon.m_AnimConfig.m_strAttackClip = "Flame Attack";
    dragon.m_AnimConfig.m_strStaggerClip = "Get Hit";
    dragon.m_AnimConfig.m_strDeathClip = "Die";

    dragon.m_IndicatorConfig.m_eType = IndicatorType::Circle;
    dragon.m_IndicatorConfig.m_fHitRadius = 15.0f;

    // Breath attack for flying dragon
    dragon.m_fnCreateAttack = [pProjMgr]() {
        // damage, speed, count, spread, windup, duration, recovery
        return std::make_unique<BreathAttackBehavior>(pProjMgr, 20.0f, 30.0f, 7, 45.0f, 0.6f, 1.2f, 0.4f);
    };
    RegisterEnemyPreset("Dragon", dragon);

    // Create shared meshes for attack indicators
    m_pRingMesh = new RingMesh(pDevice, pCommandList, 1.0f, 0.93f, 48);
    m_pRingMesh->AddRef();
    m_pLineMesh = new LineMesh(pDevice, pCommandList, 0.4f);
    m_pLineMesh->AddRef();

    m_pFanMesh = new FanMesh(pDevice, pCommandList, 90.0f, 24);
    m_pFanMesh->AddRef();

    OutputDebugString(L"[EnemySpawner] Initialized with default presets\n");
}

void EnemySpawner::RegisterEnemyPreset(const std::string& name, const EnemySpawnData& data)
{
    m_mapPresets[name] = data;

    wchar_t buffer[128];
    swprintf_s(buffer, L"[EnemySpawner] Registered preset: %hs\n", name.c_str());
    OutputDebugString(buffer);
}

bool EnemySpawner::HasPreset(const std::string& name) const
{
    return m_mapPresets.find(name) != m_mapPresets.end();
}

GameObject* EnemySpawner::SpawnEnemy(CRoom* pRoom, const std::string& preset, const XMFLOAT3& position, GameObject* pTarget)
{
    auto it = m_mapPresets.find(preset);
    if (it == m_mapPresets.end())
    {
        wchar_t buffer[128];
        swprintf_s(buffer, L"[EnemySpawner] Preset not found: %hs, using TestEnemy\n", preset.c_str());
        OutputDebugString(buffer);

        // Fallback to test enemy
        return SpawnTestEnemy(pRoom, position, pTarget);
    }

    const EnemySpawnData& data = it->second;

    // Create enemy game object
    GameObject* pEnemy = nullptr;

    if (data.m_strMeshPath.empty())
    {
        // Use CubeMesh
        pEnemy = CreateCubeEnemy(pRoom, position, data.m_xmf3Scale, data.m_xmf4Color);
    }
    else
    {
        // Load mesh from file
        pEnemy = CreateMeshEnemy(pRoom, position, data);
    }

    if (pEnemy)
    {
        SetupEnemyComponents(pEnemy, data, pRoom, pTarget);
    }

    return pEnemy;
}

GameObject* EnemySpawner::SpawnTestEnemy(CRoom* pRoom, const XMFLOAT3& position, GameObject* pTarget)
{
    return SpawnEnemy(pRoom, "TestEnemy", position, pTarget);
}

void EnemySpawner::SpawnRoomEnemies(CRoom* pRoom, const RoomSpawnConfig& config, GameObject* pTarget)
{
    if (!pRoom) return;

    for (const auto& spawn : config.m_vEnemySpawns)
    {
        SpawnEnemy(pRoom, spawn.first, spawn.second, pTarget);
    }

    wchar_t buffer[128];
    swprintf_s(buffer, L"[EnemySpawner] Spawned %zu enemies in room\n", config.m_vEnemySpawns.size());
    OutputDebugString(buffer);
}

GameObject* EnemySpawner::CreateCubeEnemy(CRoom* pRoom, const XMFLOAT3& position, const XMFLOAT3& scale, const XMFLOAT4& color)
{
    if (!m_pDevice || !m_pCommandList || !m_pScene) return nullptr;

    // Create game object via Scene (handles descriptor allocation)
    GameObject* pEnemy = m_pScene->CreateGameObject(m_pDevice, m_pCommandList);
    if (!pEnemy) return nullptr;

    // Set position and scale
    TransformComponent* pTransform = pEnemy->GetTransform();
    if (pTransform)
    {
        pTransform->SetPosition(position);
        pTransform->SetScale(scale);
    }

    // Create cube mesh (1x1x1 base, scaled by transform)
    CubeMesh* pCubeMesh = new CubeMesh(m_pDevice, m_pCommandList, 2.0f, 2.0f, 2.0f);
    pCubeMesh->AddRef();
    pEnemy->SetMesh(pCubeMesh);

    // Set material (red color)
    MATERIAL material;
    material.m_cAmbient = XMFLOAT4(color.x * 0.3f, color.y * 0.3f, color.z * 0.3f, 1.0f);
    material.m_cDiffuse = color;
    material.m_cSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 32.0f);
    material.m_cEmissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    pEnemy->SetMaterial(material);

    // Add RenderComponent
    if (m_pShader)
    {
        auto* pRenderComp = pEnemy->AddComponent<RenderComponent>();
        pRenderComp->SetMesh(pCubeMesh);
        m_pShader->AddRenderComponent(pRenderComp);
    }

    // Add ColliderComponent (reduced size to avoid getting stuck on terrain)
    auto* pCollider = pEnemy->AddComponent<ColliderComponent>();
    pCollider->SetExtents(scale.x * 0.5f, scale.y, scale.z * 0.5f);  // Half extents, narrower XZ
    pCollider->SetCenter(0.0f, scale.y, 0.0f);  // Center at mid-height
    pCollider->SetLayer(CollisionLayer::Enemy);
    pCollider->SetCollisionMask(CollisionMask::Enemy);

    wchar_t buffer[128];
    swprintf_s(buffer, L"[EnemySpawner] Created cube enemy at (%.1f, %.1f, %.1f)\n",
        position.x, position.y, position.z);
    OutputDebugString(buffer);

    return pEnemy;
}

void EnemySpawner::SetupEnemyComponents(GameObject* pEnemy, const EnemySpawnData& data, CRoom* pRoom, GameObject* pTarget)
{
    if (!pEnemy) return;

    // Add EnemyComponent
    auto* pEnemyComp = pEnemy->AddComponent<EnemyComponent>();
    pEnemyComp->SetStats(data.m_Stats);
    pEnemyComp->SetTarget(pTarget);
    pEnemyComp->SetRoom(pRoom);

    // Set flying mode if enabled
    if (data.m_bIsFlying)
    {
        pEnemyComp->SetFlying(true, data.m_fFlyHeight);
    }

    // Create attack behavior
    if (data.m_fnCreateAttack)
    {
        pEnemyComp->SetAttackBehavior(data.m_fnCreateAttack());
    }
    else
    {
        // Default melee attack
        pEnemyComp->SetAttackBehavior(std::make_unique<MeleeAttackBehavior>());
    }

    // Set death callback to notify room
    pEnemyComp->SetOnDeathCallback([pRoom](EnemyComponent* pDeadEnemy) {
        if (pRoom)
        {
            pRoom->OnEnemyDeath(pDeadEnemy);
        }
    });

    // Register enemy with room
    if (pRoom)
    {
        pRoom->RegisterEnemy(pEnemyComp);
    }

    // Connect AnimationComponent if present
    auto* pAnimComp = pEnemy->GetComponent<AnimationComponent>();
    if (pAnimComp)
    {
        pEnemyComp->SetAnimationComponent(pAnimComp);
        pEnemyComp->SetAnimationConfig(data.m_AnimConfig);
        pAnimComp->Play(data.m_AnimConfig.m_strIdleClip, data.m_AnimConfig.m_bLoopIdle);
    }

    // Create attack indicators
    if (data.m_IndicatorConfig.m_eType != IndicatorType::None && pRoom)
    {
        SetupAttackIndicators(pEnemy, pEnemyComp, data.m_IndicatorConfig, pRoom);
    }

    OutputDebugString(L"[EnemySpawner] Setup enemy components complete\n");
}

GameObject* EnemySpawner::CreateMeshEnemy(CRoom* pRoom, const XMFLOAT3& position, const EnemySpawnData& data)
{
    if (!m_pDevice || !m_pCommandList || !m_pScene) return nullptr;

    // Temporarily set current room to place enemy in room
    CRoom* pPrevRoom = m_pScene->GetCurrentRoom();
    m_pScene->SetCurrentRoom(pRoom);

    // Load mesh from file
    GameObject* pEnemy = MeshLoader::LoadGeometryFromFile(m_pScene, m_pDevice, m_pCommandList, NULL, data.m_strMeshPath.c_str());

    // Restore previous room
    m_pScene->SetCurrentRoom(pPrevRoom);

    if (!pEnemy)
    {
        wchar_t buffer[256];
        swprintf_s(buffer, L"[EnemySpawner] Failed to load mesh: %hs\n", data.m_strMeshPath.c_str());
        OutputDebugString(buffer);
        return nullptr;
    }

    // Set position and scale
    TransformComponent* pTransform = pEnemy->GetTransform();
    if (pTransform)
    {
        pTransform->SetPosition(position);
        pTransform->SetScale(data.m_xmf3Scale);
    }

    // Add AnimationComponent and load animation
    if (!data.m_strAnimationPath.empty())
    {
        auto* pAnimComp = pEnemy->AddComponent<AnimationComponent>();
        pAnimComp->Init(m_pDevice, m_pCommandList);
        pAnimComp->LoadAnimation(data.m_strAnimationPath.c_str());
    }

    // Add RenderComponents to hierarchy
    AddRenderComponentsToHierarchy(pEnemy);

    // Load texture if specified
    if (!data.m_strTexturePath.empty())
    {
        LoadTextureToHierarchy(pEnemy, data.m_strTexturePath);
    }
    else
    {
        // Apply color tint to all meshes in hierarchy (only if no texture)
        ApplyColorToHierarchy(pEnemy, data.m_xmf4Color);
    }

    // Add ColliderComponent (reduced size to avoid getting stuck on terrain)
    auto* pCollider = pEnemy->AddComponent<ColliderComponent>();
    float colliderScale = data.m_xmf3Scale.x;
    if (data.m_xmf3Scale.y > colliderScale) colliderScale = data.m_xmf3Scale.y;
    if (data.m_xmf3Scale.z > colliderScale) colliderScale = data.m_xmf3Scale.z;
    pCollider->SetExtents(colliderScale * 0.3f, colliderScale * 1.0f, colliderScale * 0.3f);  // Narrower XZ
    pCollider->SetCenter(0.0f, colliderScale * 1.0f, 0.0f);
    pCollider->SetLayer(CollisionLayer::Enemy);
    pCollider->SetCollisionMask(CollisionMask::Enemy);

    wchar_t buffer[256];
    swprintf_s(buffer, L"[EnemySpawner] Created mesh enemy at (%.1f, %.1f, %.1f) from %hs\n",
        position.x, position.y, position.z, data.m_strMeshPath.c_str());
    OutputDebugString(buffer);

    return pEnemy;
}

void EnemySpawner::AddRenderComponentsToHierarchy(GameObject* pGameObject)
{
    if (!pGameObject || !m_pShader) return;

    if (pGameObject->GetMesh())
    {
        auto* pRenderComp = pGameObject->AddComponent<RenderComponent>();
        pRenderComp->SetMesh(pGameObject->GetMesh());
        pRenderComp->SetCastsShadow(true);  // Enemies cast shadows
        m_pShader->AddRenderComponent(pRenderComp);

        wchar_t buffer[128];
        swprintf_s(buffer, L"[EnemySpawner] Added RenderComponent to: %hs\n", pGameObject->m_pstrFrameName);
        OutputDebugString(buffer);
    }

    if (pGameObject->m_pChild)
    {
        AddRenderComponentsToHierarchy(pGameObject->m_pChild);
    }
    if (pGameObject->m_pSibling)
    {
        AddRenderComponentsToHierarchy(pGameObject->m_pSibling);
    }
}

void EnemySpawner::ApplyColorToHierarchy(GameObject* pGameObject, const XMFLOAT4& color)
{
    if (!pGameObject) return;

    MATERIAL material;
    material.m_cAmbient = XMFLOAT4(color.x * 0.3f, color.y * 0.3f, color.z * 0.3f, 1.0f);
    material.m_cDiffuse = color;
    material.m_cSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 32.0f);
    material.m_cEmissive = XMFLOAT4(color.x * 0.1f, color.y * 0.1f, color.z * 0.1f, 1.0f);
    pGameObject->SetMaterial(material);

    if (pGameObject->m_pChild)
    {
        ApplyColorToHierarchy(pGameObject->m_pChild, color);
    }
    if (pGameObject->m_pSibling)
    {
        ApplyColorToHierarchy(pGameObject->m_pSibling, color);
    }
}

void EnemySpawner::LoadTextureToHierarchy(GameObject* pGameObject, const std::string& texturePath)
{
    if (!pGameObject || !m_pDevice || !m_pCommandList || !m_pScene) return;

    // Load texture for objects with mesh
    if (pGameObject->GetMesh())
    {
        pGameObject->SetTextureName(texturePath.c_str());

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
        m_pScene->AllocateDescriptor(&cpuHandle, &gpuHandle);

        pGameObject->LoadTexture(m_pDevice, m_pCommandList, cpuHandle);
        pGameObject->SetSrvGpuDescriptorHandle(gpuHandle);

        // Set white material to show texture properly
        MATERIAL material;
        material.m_cAmbient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
        material.m_cDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        material.m_cSpecular = XMFLOAT4(0.3f, 0.3f, 0.3f, 32.0f);
        material.m_cEmissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
        pGameObject->SetMaterial(material);

        wchar_t buffer[256];
        swprintf_s(buffer, L"[EnemySpawner] Loaded texture for: %hs\n", pGameObject->m_pstrFrameName);
        OutputDebugString(buffer);
    }

    if (pGameObject->m_pChild)
    {
        LoadTextureToHierarchy(pGameObject->m_pChild, texturePath);
    }
    if (pGameObject->m_pSibling)
    {
        LoadTextureToHierarchy(pGameObject->m_pSibling, texturePath);
    }
}

GameObject* EnemySpawner::CreateIndicatorObject(CRoom* pRoom, Mesh* pMesh)
{
    if (!m_pDevice || !m_pCommandList || !m_pScene || !pMesh || !m_pShader) return nullptr;

    CRoom* pPrevRoom = m_pScene->GetCurrentRoom();
    m_pScene->SetCurrentRoom(pRoom);

    GameObject* pIndicator = m_pScene->CreateGameObject(m_pDevice, m_pCommandList);

    m_pScene->SetCurrentRoom(pPrevRoom);

    if (!pIndicator) return nullptr;

    // Start hidden (below ground)
    TransformComponent* pTransform = pIndicator->GetTransform();
    if (pTransform)
    {
        pTransform->SetPosition(0.0f, -1000.0f, 0.0f);
    }

    // Set mesh
    pMesh->AddRef();
    pIndicator->SetMesh(pMesh);

    // Red emissive material
    MATERIAL redMaterial;
    redMaterial.m_cAmbient = XMFLOAT4(0.3f, 0.0f, 0.0f, 1.0f);
    redMaterial.m_cDiffuse = XMFLOAT4(1.0f, 0.1f, 0.1f, 1.0f);
    redMaterial.m_cSpecular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    redMaterial.m_cEmissive = XMFLOAT4(0.8f, 0.0f, 0.0f, 1.0f);
    pIndicator->SetMaterial(redMaterial);

    // Add render component
    auto* pRenderComp = pIndicator->AddComponent<RenderComponent>();
    pRenderComp->SetMesh(pMesh);
    m_pShader->AddRenderComponent(pRenderComp);

    return pIndicator;
}

void EnemySpawner::SetupAttackIndicators(GameObject* pEnemy, EnemyComponent* pEnemyComp,
                                          const AttackIndicatorConfig& config, CRoom* pRoom)
{
    pEnemyComp->SetIndicatorConfig(config);

    if (config.m_eType == IndicatorType::Circle)
    {
        // Melee: ring around enemy (positioned by ShowIndicators on attack start)
        GameObject* pHitZone = CreateIndicatorObject(pRoom, m_pRingMesh);
        if (pHitZone)
        {
            pEnemyComp->SetHitZoneIndicator(pHitZone);
        }
    }
    else if (config.m_eType == IndicatorType::RushCircle)
    {
        // Rush + 360 AoE: line + ring at destination
        GameObject* pLine = CreateIndicatorObject(pRoom, m_pLineMesh);
        if (pLine)
        {
            pEnemyComp->SetRushLineIndicator(pLine);
        }

        GameObject* pHitZone = CreateIndicatorObject(pRoom, m_pRingMesh);
        if (pHitZone)
        {
            pEnemyComp->SetHitZoneIndicator(pHitZone);
        }
    }
    else if (config.m_eType == IndicatorType::RushCone)
    {
        // Rush + cone: line + fan at destination
        GameObject* pLine = CreateIndicatorObject(pRoom, m_pLineMesh);
        if (pLine)
        {
            pEnemyComp->SetRushLineIndicator(pLine);
        }

        GameObject* pHitZone = CreateIndicatorObject(pRoom, m_pFanMesh);
        if (pHitZone)
        {
            pEnemyComp->SetHitZoneIndicator(pHitZone);
        }
    }
}
