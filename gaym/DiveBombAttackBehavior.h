#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>

using namespace DirectX;

class ProjectileManager;

// Dive bomb attack: boss flies up, then dives toward player while shooting forward
class DiveBombAttackBehavior : public IAttackBehavior
{
public:
    DiveBombAttackBehavior(ProjectileManager* pProjectileManager,
                           float fDamagePerHit = 15.0f,
                           float fProjectileSpeed = 25.0f,
                           float fDiveSpeed = 30.0f,
                           float fImpactDamage = 25.0f,
                           float fImpactRadius = 5.0f,
                           int nProjectilesPerWave = 5,
                           float fFireInterval = 0.1f,
                           float fFlyHeight = 20.0f,
                           float fTakeOffDuration = 0.8f,
                           float fHoverDuration = 0.5f);
    virtual ~DiveBombAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;

private:
    void FireForwardSpread(EnemyComponent* pEnemy);
    void DealImpactDamage(EnemyComponent* pEnemy);

private:
    ProjectileManager* m_pProjectileManager = nullptr;

    // Attack parameters
    float m_fDamagePerHit = 15.0f;
    float m_fProjectileSpeed = 25.0f;
    float m_fDiveSpeed = 30.0f;
    float m_fImpactDamage = 25.0f;
    float m_fImpactRadius = 5.0f;
    int m_nProjectilesPerWave = 5;
    float m_fFireInterval = 0.1f;

    // Flight parameters
    float m_fFlyHeight = 20.0f;
    float m_fTakeOffDuration = 0.8f;
    float m_fHoverDuration = 0.5f;

    // Runtime state
    enum class Phase { TakeOff, Hover, Dive, Recovery };
    Phase m_ePhase = Phase::TakeOff;
    float m_fTimer = 0.0f;
    float m_fOriginalY = 0.0f;
    float m_fNextFireTime = 0.0f;
    XMFLOAT3 m_xmf3DiveDirection = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_xmf3TargetPosition = { 0.0f, 0.0f, 0.0f };
    bool m_bImpactDealt = false;
    bool m_bFinished = false;
};
