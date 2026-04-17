#pragma once
#include "IAttackBehavior.h"
#include "SkillTypes.h"
#include <DirectXMath.h>

using namespace DirectX;

class ProjectileManager;

// Flying breath attack: fires multiple projectiles in a cone pattern from above
class BreathAttackBehavior : public IAttackBehavior
{
public:
    BreathAttackBehavior(ProjectileManager* pProjectileManager,
                         float fDamagePerHit = 15.0f,
                         float fProjectileSpeed = 25.0f,
                         int nProjectileCount = 5,
                         float fSpreadAngle = 30.0f,
                         float fWindupTime = 0.8f,
                         float fBreathDuration = 1.0f,
                         float fRecoveryTime = 0.5f,
                         float fProjectileRadius = 0.8f,
                         float fProjectileScale = 1.5f,
                         ElementType eElement = ElementType::Fire);
    virtual ~BreathAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;
    virtual const char* GetAnimClipName() const override { return "Fireball Shoot"; }

private:
    void FireBreathProjectile(EnemyComponent* pEnemy, float angleOffset);

private:
    // Dependencies
    ProjectileManager* m_pProjectileManager = nullptr;

    // Parameters
    float m_fDamagePerHit = 15.0f;
    float m_fProjectileSpeed = 25.0f;
    int m_nProjectileCount = 5;
    float m_fSpreadAngle = 30.0f;  // Total spread angle in degrees
    float m_fWindupTime = 0.8f;
    float m_fBreathDuration = 1.0f;
    float m_fRecoveryTime = 0.5f;
    float m_fProjectileRadius = 0.8f;
    float m_fProjectileScale = 1.5f;
    ElementType m_eElement = ElementType::Fire;

    // Runtime state
    enum class Phase { Windup, Breath, Recovery };
    Phase m_ePhase = Phase::Windup;
    float m_fTimer = 0.0f;
    int m_nProjectilesFired = 0;
    float m_fNextFireTime = 0.0f;
    bool m_bFinished = false;
};
