#include "stdafx.h"
#include "Room.h"
#include "EnemyComponent.h"
#include "EnemySpawner.h"
#include "Scene.h"
#include "Dx12App.h"
#include "DropItemComponent.h"
#include "InteractableComponent.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "Mesh.h"
#include "Shader.h"
#include "LavaGeyserManager.h"
#include "NetworkManager.h"

CRoom::CRoom()
{
}

CRoom::~CRoom()
{
}

void CRoom::Update(float deltaTime)
{
    // Inactive 상태에서는 아무것도 하지 않음
    if (m_eState == RoomState::Inactive)
        return;

    // Active 상태: 적 스폰 및 클리어 체크
    if (m_eState == RoomState::Active)
    {
        // Spawn enemies if not yet spawned
        if (!m_bEnemiesSpawned)
        {
            SpawnEnemies();
        }

        // Update lava geyser manager
        if (m_pGeyserManager)
        {
            m_pGeyserManager->Update(deltaTime);
        }

        CheckClearCondition();
    }

    // Active 및 Cleared 상태 모두에서 오브젝트 업데이트 (드랍 아이템 등)
    for (auto& pGameObject : m_vGameObjects)
    {
        pGameObject->Update(deltaTime);
    }
}

void CRoom::Render(ID3D12GraphicsCommandList* pCommandList)
{
    // Inactive가 아닐 때만 렌더링 (또는 거리에 따라 판단 가능)
    if (m_eState != RoomState::Inactive)
    {
        for (auto& pGameObject : m_vGameObjects)
        {
            pGameObject->Render(pCommandList);
        }
    }
}

void CRoom::AddGameObject(std::unique_ptr<GameObject> pGameObject)
{
    m_vGameObjects.push_back(std::move(pGameObject));
}

void CRoom::RemoveGameObject(GameObject* pGameObject)
{
    if (!pGameObject) return;

    // Also remove from enemies list if it's an enemy
    auto* pEnemyComp = pGameObject->GetComponent<EnemyComponent>();
    if (pEnemyComp)
    {
        auto it = std::find(m_vEnemies.begin(), m_vEnemies.end(), pEnemyComp);
        if (it != m_vEnemies.end())
        {
            m_vEnemies.erase(it);
        }
    }

    // Remove from game objects list
    for (auto it = m_vGameObjects.begin(); it != m_vGameObjects.end(); ++it)
    {
        if (it->get() == pGameObject)
        {
            m_vGameObjects.erase(it);
            OutputDebugString(L"[Room] Deleted GameObject from Room\n");
            return;
        }
    }
}

void CRoom::SetState(RoomState state)
{
    if (m_eState == state) return;

    RoomState oldState = m_eState;
    m_eState = state;

    // Debug output
    const wchar_t* stateNames[] = { L"Inactive", L"Active", L"Cleared" };
    wchar_t buffer[128];
    swprintf_s(buffer, L"[Room] State changed: %s -> %s\n",
        stateNames[static_cast<int>(oldState)],
        stateNames[static_cast<int>(state)]);
    OutputDebugString(buffer);

    // 상태 변경 시 필요한 로직
    switch (m_eState)
    {
    case RoomState::Active:
        OutputDebugString(L"[Room] Room activated - enemies will spawn\n");
        // Enemies will be spawned in Update
        // Activate lava geyser manager
        if (m_pGeyserManager)
        {
            m_pGeyserManager->SetActive(true);
        }
        break;
    case RoomState::Cleared:
        OutputDebugString(L"[Room] Room cleared!\n");
        // Deactivate lava geyser manager
        if (m_pGeyserManager)
        {
            m_pGeyserManager->SetActive(false);
        }
        break;
    }
}

bool CRoom::IsPlayerInside(const XMFLOAT3& playerPos)
{
    return m_BoundingBox.Contains(XMLoadFloat3(&playerPos)) != DISJOINT;
}

void CRoom::CheckClearCondition()
{
    // Only check if active and enemies have been spawned
    if (m_eState != RoomState::Active || !m_bEnemiesSpawned)
        return;

    // Check if all enemies are dead
    if (m_nTotalEnemies > 0 && m_nDeadEnemies >= m_nTotalEnemies)
    {
        SetState(RoomState::Cleared);
        SpawnDropItem();
        SpawnPortalCube();
    }
}

void CRoom::RegisterEnemy(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    m_vEnemies.push_back(pEnemy);
    m_nTotalEnemies++;

    wchar_t buffer[64];
    swprintf_s(buffer, L"[Room] Registered enemy %d\n", m_nTotalEnemies);
    OutputDebugString(buffer);
}

void CRoom::OnEnemyDeath(EnemyComponent* pEnemy)
{
    m_nDeadEnemies++;

    wchar_t buffer[128];
    swprintf_s(buffer, L"[Room] Enemy died! (%d/%d dead)\n", m_nDeadEnemies, m_nTotalEnemies);
    OutputDebugString(buffer);

    // Clear condition will be checked in Update
}

void CRoom::SpawnEnemies()
{
    if (m_bEnemiesSpawned) return;

    m_bEnemiesSpawned = true;

    // 네트워크 연결 상태면 서버가 몬스터를 관리하므로 로컬 스폰 스킵
    NetworkManager* pNet = NetworkManager::GetInstance();
    if (pNet && pNet->IsConnected())
    {
        OutputDebugString(L"[Room] Skipping local enemy spawn — server authoritative mode\n");
        return;
    }

    if (!m_pSpawner)
    {
        OutputDebugString(L"[Room] No spawner set - cannot spawn enemies\n");
        return;
    }

    if (m_SpawnConfig.m_vEnemySpawns.empty())
    {
        OutputDebugString(L"[Room] No spawn config set - no enemies to spawn\n");
        return;
    }

    wchar_t buffer[128];
    swprintf_s(buffer, L"[Room] Spawning %zu enemies...\n", m_SpawnConfig.m_vEnemySpawns.size());
    OutputDebugString(buffer);

    m_pSpawner->SpawnRoomEnemies(this, m_SpawnConfig, m_pPlayerTarget);
}

void CRoom::SpawnDropItem()
{
    if (m_pDropItem) return;  // Already spawned
    if (!m_pScene)
    {
        OutputDebugString(L"[Room] Cannot spawn drop - no Scene pointer\n");
        return;
    }

    OutputDebugString(L"[Room] Spawning drop item...\n");

    // Spawn at player's position
    XMFLOAT3 spawnPos = XMFLOAT3(0.0f, 1.5f, 0.0f);
    if (m_pPlayerTarget)
    {
        spawnPos = m_pPlayerTarget->GetTransform()->GetPosition();
        spawnPos.y += 1.5f;  // Float slightly above the floor (player's actual Y)
    }

    // Create drop item as a room object
    m_pDropItem = m_pScene->CreateGameObject(Dx12App::GetInstance()->GetDevice(),
                                              Dx12App::GetInstance()->GetCommandList());

    if (!m_pDropItem)
    {
        OutputDebugString(L"[Room] Failed to create drop item GameObject\n");
        return;
    }

    // Set position and scale
    m_pDropItem->GetTransform()->SetPosition(spawnPos.x, spawnPos.y, spawnPos.z);
    m_pDropItem->GetTransform()->SetScale(1.5f, 1.5f, 1.5f);

    // Create white cube mesh
    CubeMesh* pCubeMesh = new CubeMesh(Dx12App::GetInstance()->GetDevice(),
                                        Dx12App::GetInstance()->GetCommandList(),
                                        1.0f, 1.0f, 1.0f);
    m_pDropItem->SetMesh(pCubeMesh);

    MATERIAL whiteMaterial;
    whiteMaterial.m_cAmbient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
    whiteMaterial.m_cDiffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    whiteMaterial.m_cSpecular = XMFLOAT4(1.0f, 1.0f, 1.0f, 64.0f);
    whiteMaterial.m_cEmissive = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
    m_pDropItem->SetMaterial(whiteMaterial);

    // Add RenderComponent for visibility
    m_pDropItem->AddComponent<RenderComponent>()->SetMesh(pCubeMesh);

    // Add DropItemComponent
    m_pDropItem->AddComponent<DropItemComponent>();

    OutputDebugString(L"[Room] Drop item spawned successfully!\n");
}

void CRoom::SpawnPortalCube()
{
    if (m_pPortalCube) return;  // Already spawned
    if (!m_pScene)
    {
        OutputDebugString(L"[Room] Cannot spawn portal - no Scene pointer\n");
        return;
    }

    OutputDebugString(L"[Room] Spawning portal cube...\n");

    // Spawn at player's position + Z offset (so it doesn't overlap with drop item)
    XMFLOAT3 spawnPos = XMFLOAT3(0.0f, 1.5f, 5.0f);
    if (m_pPlayerTarget)
    {
        spawnPos = m_pPlayerTarget->GetTransform()->GetPosition();
        spawnPos.y += 1.5f;  // Float slightly above the floor (player's actual Y)
        spawnPos.z += 5.0f;  // Offset in Z to avoid overlapping with drop item
    }

    // Create portal cube as a room object
    m_pPortalCube = m_pScene->CreateGameObject(Dx12App::GetInstance()->GetDevice(),
                                                Dx12App::GetInstance()->GetCommandList());

    if (!m_pPortalCube)
    {
        OutputDebugString(L"[Room] Failed to create portal cube GameObject\n");
        return;
    }

    // Set position and scale
    m_pPortalCube->GetTransform()->SetPosition(spawnPos.x, spawnPos.y, spawnPos.z);
    m_pPortalCube->GetTransform()->SetScale(2.0f, 2.0f, 2.0f);

    // Create blue cube mesh
    CubeMesh* pCubeMesh = new CubeMesh(Dx12App::GetInstance()->GetDevice(),
                                        Dx12App::GetInstance()->GetCommandList(),
                                        1.0f, 1.0f, 1.0f);
    m_pPortalCube->SetMesh(pCubeMesh);

    MATERIAL blueMaterial;
    blueMaterial.m_cAmbient = XMFLOAT4(0.0f, 0.0f, 0.3f, 1.0f);
    blueMaterial.m_cDiffuse = XMFLOAT4(0.2f, 0.4f, 1.0f, 1.0f);
    blueMaterial.m_cSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 32.0f);
    blueMaterial.m_cEmissive = XMFLOAT4(0.0f, 0.2f, 0.6f, 1.0f);
    m_pPortalCube->SetMaterial(blueMaterial);

    // Add RenderComponent for visibility
    m_pPortalCube->AddComponent<RenderComponent>()->SetMesh(pCubeMesh);

    // Add InteractableComponent
    auto* pInteractable = m_pPortalCube->AddComponent<InteractableComponent>();
    pInteractable->SetPromptText(L"[F] Enter Portal");
    pInteractable->SetInteractionDistance(5.0f);
    pInteractable->SetOnInteract([this](InteractableComponent* pComp) {
        if (!m_pScene)
            return;

        // 서버 연결 시 서버 권위 방 전환 (S_ROOM_TRANSITION 수신 시 실제 전환)
        NetworkManager* pNet = NetworkManager::GetInstance();
        if (pNet && pNet->IsConnected())
        {
            pNet->SendPortalInteract();
        }
        else
        {
            // 오프라인 폴백
            m_pScene->TransitionToNextRoom();
        }
    });

    OutputDebugString(L"[Room] Portal cube spawned successfully!\n");
}

void CRoom::InitLavaGeyserManager(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                                   Shader* pShader, CDescriptorHeap* pDescriptorHeap, UINT nDescriptorIndex)
{
    if (m_pGeyserManager)
    {
        OutputDebugString(L"[Room] LavaGeyserManager already initialized\n");
        return;
    }

    m_pGeyserManager = std::make_unique<LavaGeyserManager>();
    m_pGeyserManager->Init(pDevice, pCommandList, this, pShader, pDescriptorHeap, nDescriptorIndex);

    OutputDebugString(L"[Room] LavaGeyserManager initialized\n");
}

void CRoom::SetLavaGeyserEnabled(bool bEnabled)
{
    if (m_pGeyserManager)
    {
        m_pGeyserManager->SetActive(bEnabled);
    }
}
