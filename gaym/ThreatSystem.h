#pragma once

#include <unordered_map>
#include <DirectXMath.h>
#include "ThreatConstants.h"

class GameObject;

struct ThreatEntry
{
    float m_fThreatValue = 0.0f;
    float m_fDecayTimer = 0.0f;
};

class ThreatTable
{
public:
    ThreatTable() = default;
    ~ThreatTable() = default;

    // 위협도 추가/감소/설정
    void AddThreat(GameObject* pPlayer, float fAmount);
    void ReduceThreat(GameObject* pPlayer, float fAmount);
    void SetThreat(GameObject* pPlayer, float fAmount);
    float GetThreat(GameObject* pPlayer) const;

    // 타겟 선정 (현재 타겟에 보너스 적용)
    GameObject* GetHighestThreatTarget(GameObject* pCurrentTarget = nullptr) const;

    // 업데이트 (거리 기반 위협도 증감)
    void Update(float deltaTime, const DirectX::XMFLOAT3& enemyPos);

    // 플레이어 관리
    void RegisterPlayer(GameObject* pPlayer, float fInitialThreat = ThreatConstants::INITIAL_THREAT);
    void RemovePlayer(GameObject* pPlayer);
    void CleanupDeadPlayers();
    void Clear();

    // 디버그
    size_t GetPlayerCount() const { return m_mapThreat.size(); }
    bool HasPlayer(GameObject* pPlayer) const;

private:
    std::unordered_map<GameObject*, ThreatEntry> m_mapThreat;
};
