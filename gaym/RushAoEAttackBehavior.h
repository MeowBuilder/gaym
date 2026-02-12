#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>

using namespace DirectX;

class RushAoEAttackBehavior : public IAttackBehavior
{
public:
    RushAoEAttackBehavior(float fDamage = 15.0f, float fRushSpeed = 15.0f, float fRushDuration = 0.5f,
                          float fWindupTime = 0.3f, float fHitTime = 0.2f, float fRecoveryTime = 0.3f,
                          float fAoERadius = 5.0f);
    virtual ~RushAoEAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;

private:
    enum class Phase { Rush, Windup, Hit, Recovery };

    void UpdateRush(float dt, EnemyComponent* pEnemy);
    void DealAoEDamage(EnemyComponent* pEnemy);

private:
    // Parameters
    float m_fDamage = 15.0f;
    float m_fRushSpeed = 15.0f;
    float m_fRushDuration = 0.5f;
    float m_fWindupTime = 0.3f;
    float m_fHitTime = 0.2f;
    float m_fRecoveryTime = 0.3f;
    float m_fAoERadius = 5.0f;

    // Runtime state
    Phase m_ePhase = Phase::Rush;
    float m_fTimer = 0.0f;
    float m_fPhaseDuration = 0.0f;
    bool m_bHitDealt = false;
    bool m_bFinished = false;
    XMFLOAT3 m_xmf3RushDirection = { 0.0f, 0.0f, 0.0f };
};
