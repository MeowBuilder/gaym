#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>

using namespace DirectX;

class ProjectileManager;

// Flying strafe attack: boss flies to the side while shooting at the player
class FlyingStrafeAttackBehavior : public IAttackBehavior
{
public:
    FlyingStrafeAttackBehavior(ProjectileManager* pProjectileManager,
                                float fDamagePerHit = 12.0f,
                                float fProjectileSpeed = 18.0f,
                                float fStrafeSpeed = 15.0f,
                                float fStrafeDistance = 25.0f,
                                float fFireInterval = 0.15f,
                                int nShotsPerBurst = 3,
                                float fFlyHeight = 10.0f,
                                float fTakeOffDuration = 0.6f,
                                float fLandingDuration = 0.6f);
    virtual ~FlyingStrafeAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;

private:
    void FireAtPlayer(EnemyComponent* pEnemy);
    void ChooseStrafeDirection(EnemyComponent* pEnemy);

private:
    ProjectileManager* m_pProjectileManager = nullptr;

    // Attack parameters
    float m_fDamagePerHit = 12.0f;
    float m_fProjectileSpeed = 18.0f;
    float m_fStrafeSpeed = 15.0f;
    float m_fStrafeDistance = 25.0f;
    float m_fFireInterval = 0.15f;
    int m_nShotsPerBurst = 3;

    // Flight parameters
    float m_fFlyHeight = 10.0f;
    float m_fTakeOffDuration = 0.6f;
    float m_fLandingDuration = 0.6f;

    // Runtime state
    enum class Phase { TakeOff, Strafe, Landing };
    Phase m_ePhase = Phase::TakeOff;
    float m_fTimer = 0.0f;
    float m_fOriginalY = 0.0f;
    float m_fDistanceTraveled = 0.0f;
    float m_fNextFireTime = 0.0f;
    int m_nShotsFired = 0;
    XMFLOAT3 m_xmf3StrafeDirection = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_xmf3StartPosition = { 0.0f, 0.0f, 0.0f };
    bool m_bFinished = false;
};
