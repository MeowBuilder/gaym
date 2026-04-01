#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>

using namespace DirectX;

class ProjectileManager;

// Flying sweep attack: boss flies in a straight line while sweeping projectiles side to side
class FlyingSweepAttackBehavior : public IAttackBehavior
{
public:
    FlyingSweepAttackBehavior(ProjectileManager* pProjectileManager,
                               float fDamagePerHit = 12.0f,
                               float fProjectileSpeed = 20.0f,
                               float fFlySpeed = 12.0f,
                               float fFlyDistance = 35.0f,
                               float fSweepAngle = 120.0f,   // Total sweep arc
                               float fSweepSpeed = 180.0f,   // Degrees per second
                               float fFireInterval = 0.08f,
                               int nProjectilesPerShot = 2,
                               float fFlyHeight = 8.0f,
                               float fTakeOffDuration = 0.5f,
                               float fLandingDuration = 0.5f);
    virtual ~FlyingSweepAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;

private:
    void FireSweepProjectiles(EnemyComponent* pEnemy);
    void ChooseFlyDirection(EnemyComponent* pEnemy);

private:
    ProjectileManager* m_pProjectileManager = nullptr;

    // Attack parameters
    float m_fDamagePerHit = 12.0f;
    float m_fProjectileSpeed = 20.0f;
    float m_fFlySpeed = 12.0f;
    float m_fFlyDistance = 35.0f;
    float m_fSweepAngle = 120.0f;
    float m_fSweepSpeed = 180.0f;
    float m_fFireInterval = 0.08f;
    int m_nProjectilesPerShot = 2;

    // Flight parameters
    float m_fFlyHeight = 8.0f;
    float m_fTakeOffDuration = 0.5f;
    float m_fLandingDuration = 0.5f;

    // Runtime state
    enum class Phase { TakeOff, Sweep, Landing };
    Phase m_ePhase = Phase::TakeOff;
    float m_fTimer = 0.0f;
    float m_fOriginalY = 0.0f;
    float m_fDistanceTraveled = 0.0f;
    float m_fCurrentSweepAngle = 0.0f;
    float m_fNextFireTime = 0.0f;
    int m_nSweepDirection = 1;  // 1 or -1
    XMFLOAT3 m_xmf3FlyDirection = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_xmf3BaseFireDirection = { 0.0f, 0.0f, 0.0f };
    bool m_bFinished = false;
};
