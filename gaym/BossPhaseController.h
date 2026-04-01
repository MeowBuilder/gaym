#pragma once

#include <memory>
#include <vector>
#include "BossPhaseConfig.h"

class EnemyComponent;
class IAttackBehavior;
class CRoom;

enum class PhaseTransitionState
{
    None,            // 일반 상태 (전환 중 아님)
    TransitionAttack,  // 전환 공격 실행 중 (메가 브레스 등)
    TransitionAnim,    // 전환 애니메이션 (포효 등)
    TransitionDone     // 전환 완료
};

class BossPhaseController
{
public:
    BossPhaseController(EnemyComponent* pOwner);
    ~BossPhaseController() = default;

    // 페이즈 설정
    void SetPhaseConfig(std::unique_ptr<BossPhaseConfig> pConfig);
    const BossPhaseConfig* GetPhaseConfig() const { return m_pPhaseConfig.get(); }

    // 업데이트 (전환 공격 및 애니메이션 처리)
    void Update(float deltaTime);

    // 체력 변화 시 호출 (EnemyComponent::TakeDamage에서)
    void OnHealthChanged(float fCurrentHP, float fMaxHP);

    // 현재 페이즈 정보
    int GetCurrentPhase() const { return m_nCurrentPhase; }
    bool IsInTransition() const { return m_eTransitionState != PhaseTransitionState::None &&
                                         m_eTransitionState != PhaseTransitionState::TransitionDone; }
    bool IsInTransitionAttack() const { return m_eTransitionState == PhaseTransitionState::TransitionAttack; }
    PhaseTransitionState GetTransitionState() const { return m_eTransitionState; }
    const BossPhaseData& GetCurrentPhaseData() const { return m_pPhaseConfig->GetPhase(m_nCurrentPhase); }

    // 비행 가능 여부 (현재 페이즈 기준)
    bool CanFly() const;
    int GetFlyingAttackChance() const;

    // Room 참조 (쫄 소환용)
    void SetRoom(CRoom* pRoom) { m_pRoom = pRoom; }

private:
    void TransitionToPhase(int nNewPhase);
    void StartTransitionAttack(const BossPhaseData& phase);
    void ApplyPhaseData(const BossPhaseData& phase);
    void SpawnPhaseAdds(const BossPhaseData& phase);
    void FinishTransition();

private:
    EnemyComponent* m_pOwner = nullptr;
    std::unique_ptr<BossPhaseConfig> m_pPhaseConfig;
    CRoom* m_pRoom = nullptr;

    int m_nCurrentPhase = 0;
    int m_nPendingPhase = -1;  // 전환 공격 후 이동할 페이즈
    PhaseTransitionState m_eTransitionState = PhaseTransitionState::None;
    float m_fTransitionTimer = 0.0f;

    // 전환 공격 행동 (일시적으로 보관)
    std::unique_ptr<IAttackBehavior> m_pTransitionAttack;

    // 페이즈 전환 기록 (한 번만 전환)
    std::vector<bool> m_vPhaseTriggered;
};
