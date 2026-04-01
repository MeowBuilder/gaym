#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>

using namespace DirectX;

class ProjectileManager;

// Flying circle attack: boss flies in a circle around the player, shooting inward
class FlyingCircleAttackBehavior : public IAttackBehavior
{
public:
    FlyingCircleAttackBehavior(ProjectileManager* pProjectileManager,
                                float fDamagePerHit = 10.0f,
                                float fProjectileSpeed = 16.0f,
                                float fCircleRadius = 20.0f,
                                float fAngularSpeed = 60.0f,  // degrees per second
                                float fTotalRotation = 540.0f,  // 1.5 rotations
                                float fFireInterval = 0.2f,
                                int nProjectilesPerShot = 3,
                                float fFlyHeight = 12.0f,
                                float fTakeOffDuration = 0.7f,
                                float fLandingDuration = 0.7f);
    virtual ~FlyingCircleAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;

private:
    void FireInward(EnemyComponent* pEnemy);

private:
    ProjectileManager* m_pProjectileManager = nullptr;

    // Attack parameters
    float m_fDamagePerHit = 10.0f;
    float m_fProjectileSpeed = 16.0f;
    float m_fCircleRadius = 20.0f;
    float m_fAngularSpeed = 60.0f;
    float m_fTotalRotation = 540.0f;
    float m_fFireInterval = 0.2f;
    int m_nProjectilesPerShot = 3;

    // Flight parameters
    float m_fFlyHeight = 12.0f;
    float m_fTakeOffDuration = 0.7f;
    float m_fLandingDuration = 0.7f;

    // Runtime state
    enum class Phase { TakeOff, MoveToOrbit, Circle, Landing };
    Phase m_ePhase = Phase::TakeOff;
    float m_fTimer = 0.0f;
    float m_fOriginalY = 0.0f;
    float m_fCurrentAngle = 0.0f;
    float m_fRotationCompleted = 0.0f;
    float m_fNextFireTime = 0.0f;
    XMFLOAT3 m_xmf3CircleCenter = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_xmf3StartPosition = { 0.0f, 0.0f, 0.0f };
    float m_fMoveToOrbitDuration = 0.5f;
    bool m_bFinished = false;
};
