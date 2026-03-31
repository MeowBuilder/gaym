#include "ThreatSystem.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "PlayerComponent.h"
#include <cmath>
#include <algorithm>
#include <Windows.h>

// Windows max/min 매크로 충돌 방지
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

using namespace DirectX;

void ThreatTable::AddThreat(GameObject* pPlayer, float fAmount)
{
    if (!pPlayer || fAmount <= 0.0f) return;

    auto it = m_mapThreat.find(pPlayer);
    if (it != m_mapThreat.end())
    {
        it->second.m_fThreatValue += fAmount;

#ifdef _DEBUG
        wchar_t buf[128];
        swprintf_s(buf, L"[Threat] Player threat +%.1f = %.1f\n", fAmount, it->second.m_fThreatValue);
        OutputDebugString(buf);
#endif
    }
}

void ThreatTable::ReduceThreat(GameObject* pPlayer, float fAmount)
{
    if (!pPlayer || fAmount <= 0.0f) return;

    auto it = m_mapThreat.find(pPlayer);
    if (it != m_mapThreat.end())
    {
        it->second.m_fThreatValue = std::max(0.0f, it->second.m_fThreatValue - fAmount);
    }
}

void ThreatTable::SetThreat(GameObject* pPlayer, float fAmount)
{
    if (!pPlayer) return;

    auto it = m_mapThreat.find(pPlayer);
    if (it != m_mapThreat.end())
    {
        it->second.m_fThreatValue = std::max(0.0f, fAmount);
    }
}

float ThreatTable::GetThreat(GameObject* pPlayer) const
{
    if (!pPlayer) return 0.0f;

    auto it = m_mapThreat.find(pPlayer);
    if (it != m_mapThreat.end())
    {
        return it->second.m_fThreatValue;
    }
    return 0.0f;
}

GameObject* ThreatTable::GetHighestThreatTarget(GameObject* pCurrentTarget) const
{
    if (m_mapThreat.empty()) return nullptr;

    GameObject* pBestTarget = nullptr;
    float fHighestThreat = -1.0f;

    for (const auto& pair : m_mapThreat)
    {
        GameObject* pPlayer = pair.first;
        if (!pPlayer) continue;

        // 죽은 플레이어는 스킵
        PlayerComponent* pPlayerComp = pPlayer->GetComponent<PlayerComponent>();
        if (pPlayerComp && pPlayerComp->IsDead()) continue;

        float fThreat = pair.second.m_fThreatValue;

        // 현재 타겟이면 보너스 적용 (빈번한 타겟 전환 방지)
        if (pPlayer == pCurrentTarget)
        {
            fThreat *= ThreatConstants::CURRENT_TARGET_BONUS;
        }

        if (fThreat > fHighestThreat)
        {
            fHighestThreat = fThreat;
            pBestTarget = pPlayer;
        }
    }

    return pBestTarget;
}

void ThreatTable::Update(float deltaTime, const XMFLOAT3& enemyPos)
{
    for (auto& pair : m_mapThreat)
    {
        GameObject* pPlayer = pair.first;
        if (!pPlayer) continue;

        TransformComponent* pTransform = pPlayer->GetTransform();
        if (!pTransform) continue;

        XMFLOAT3 playerPos = pTransform->GetPosition();

        // 거리 계산 (XZ 평면)
        float dx = playerPos.x - enemyPos.x;
        float dz = playerPos.z - enemyPos.z;
        float distance = sqrtf(dx * dx + dz * dz);

        // 거리에 따른 위협도 변화
        if (distance > ThreatConstants::THREAT_DECAY_DISTANCE)
        {
            // 멀리 있으면 위협도 감소
            pair.second.m_fThreatValue -= ThreatConstants::THREAT_DECAY_RATE * deltaTime;
            pair.second.m_fThreatValue = std::max(0.0f, pair.second.m_fThreatValue);
        }
        else if (distance < ThreatConstants::THREAT_GAIN_DISTANCE)
        {
            // 가까이 있으면 위협도 증가
            pair.second.m_fThreatValue += ThreatConstants::THREAT_GAIN_RATE * deltaTime;
        }
    }
}

void ThreatTable::RegisterPlayer(GameObject* pPlayer, float fInitialThreat)
{
    if (!pPlayer) return;

    // 이미 등록된 플레이어면 무시
    if (m_mapThreat.find(pPlayer) != m_mapThreat.end()) return;

    ThreatEntry entry;
    entry.m_fThreatValue = fInitialThreat;
    entry.m_fDecayTimer = 0.0f;
    m_mapThreat[pPlayer] = entry;

#ifdef _DEBUG
    OutputDebugString(L"[Threat] Player registered to threat table\n");
#endif
}

void ThreatTable::RemovePlayer(GameObject* pPlayer)
{
    if (!pPlayer) return;
    m_mapThreat.erase(pPlayer);
}

void ThreatTable::CleanupDeadPlayers()
{
    for (auto it = m_mapThreat.begin(); it != m_mapThreat.end(); )
    {
        GameObject* pPlayer = it->first;
        if (!pPlayer)
        {
            it = m_mapThreat.erase(it);
            continue;
        }

        PlayerComponent* pPlayerComp = pPlayer->GetComponent<PlayerComponent>();
        if (pPlayerComp && pPlayerComp->IsDead())
        {
            it = m_mapThreat.erase(it);
            continue;
        }

        ++it;
    }
}

void ThreatTable::Clear()
{
    m_mapThreat.clear();
}

bool ThreatTable::HasPlayer(GameObject* pPlayer) const
{
    if (!pPlayer) return false;
    return m_mapThreat.find(pPlayer) != m_mapThreat.end();
}
