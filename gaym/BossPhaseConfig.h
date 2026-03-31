#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <string>
#include <DirectXMath.h>

class IAttackBehavior;

struct BossPhaseData
{
    float m_fHealthThreshold = 1.0f;  // 이 체력 비율 이하일 때 전환 (예: 0.7 = 70%)

    // 일반 공격 생성 함수
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnPrimaryAttack = nullptr;

    // 특수 공격 (지상용)
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnSpecialAttack = nullptr;

    // 비행 공격 (특정 페이즈 전용) - nullptr이면 비행 공격 불가
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnFlyingAttack = nullptr;
    bool m_bCanFly = false;  // 이 페이즈에서 비행 가능 여부

    // 페이즈 전환 공격 (메가 브레스 등) - 이 공격 완료 후 페이즈 전환
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnTransitionAttack = nullptr;
    bool m_bHasTransitionAttack = false;

    // 스탯 변경
    float m_fSpeedMultiplier = 1.0f;          // 이동 속도 배율
    float m_fAttackSpeedMultiplier = 1.0f;    // 공격 쿨다운 배율 (낮을수록 빠름)
    int m_nSpecialAttackChance = 30;          // 특수 공격 확률 (0-100)
    int m_nFlyingAttackChance = 0;            // 비행 공격 확률 (m_bCanFly가 true일 때만)

    // 페이즈 전환 시 행동
    bool m_bInvincibleDuringTransition = false;
    float m_fTransitionDuration = 2.0f;
    std::string m_strTransitionAnimation = "Scream";

    // 쫄 소환 설정
    bool m_bSpawnAdds = false;
    std::vector<std::pair<std::string, DirectX::XMFLOAT3>> m_vAddSpawns;
};

class BossPhaseConfig
{
public:
    BossPhaseConfig() = default;
    ~BossPhaseConfig() = default;

    // 페이즈 추가 (체력 임계값 기준 내림차순으로 정렬됨)
    void AddPhase(const BossPhaseData& phase);

    // 체력 비율에 해당하는 페이즈 인덱스 반환
    int GetPhaseIndexForHealth(float fHealthRatio) const;

    // 페이즈 개수
    int GetPhaseCount() const { return static_cast<int>(m_vPhases.size()); }

    // 특정 인덱스의 페이즈 데이터 반환
    const BossPhaseData& GetPhase(int index) const { return m_vPhases[index]; }
    BossPhaseData& GetPhase(int index) { return m_vPhases[index]; }

private:
    std::vector<BossPhaseData> m_vPhases;  // 체력 임계값 기준 내림차순 정렬
};
