#include "stdafx.h"
#include "Room.h"
#include "EnemyComponent.h"
#include "EnemySpawner.h"

CRoom::CRoom()
{
}

CRoom::~CRoom()
{
}

void CRoom::Update(float deltaTime)
{
    // Active 상태일 때만 내부 오브젝트 업데이트
    if (m_eState == RoomState::Active)
    {
        // Spawn enemies if not yet spawned
        if (!m_bEnemiesSpawned)
        {
            SpawnEnemies();
        }

        for (auto& pGameObject : m_vGameObjects)
        {
            pGameObject->Update(deltaTime);
        }

        CheckClearCondition();
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
        break;
    case RoomState::Cleared:
        OutputDebugString(L"[Room] Room cleared!\n");
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
