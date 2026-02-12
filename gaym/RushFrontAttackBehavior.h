#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>

using namespace DirectX;

class RushFrontAttackBehavior : public IAttackBehavior
{
public:
    RushFrontAttackBehavior(float fDamage = 20.0f, float fRushSpeed = 18.0f, float fRushDuration = 0.4f,
                            float fWindupTime = 0.2f, float fHitTime = 0.2f, float fRecoveryTime = 0.3f,
                            float fHitRange = 4.0f, float fConeAngleDeg = 90.0f);
    virtual ~RushFrontAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;

private:
    enum class Phase { Rush, Windup, Hit, Recovery };

    void UpdateRush(float dt, EnemyComponent* pEnemy);
    void DealConeDamage(EnemyComponent* pEnemy);

private:
    // Parameters
    float m_fDamage = 20.0f;
    float m_fRushSpeed = 18.0f;
    float m_fRushDuration = 0.4f;
    float m_fWindupTime = 0.2f;
    float m_fHitTime = 0.2f;
    float m_fRecoveryTime = 0.3f;
    float m_fHitRange = 4.0f;
    float m_fConeAngleDeg = 90.0f;
    float m_fCosHalfCone = 0.0f; // Precomputed cos(halfCone)

    // Runtime state
    Phase m_ePhase = Phase::Rush;
    float m_fTimer = 0.0f;
    bool m_bHitDealt = false;
    bool m_bFinished = false;
    XMFLOAT3 m_xmf3RushDirection = { 0.0f, 0.0f, 0.0f };
};
