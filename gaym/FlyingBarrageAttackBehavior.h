#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>

using namespace DirectX;

class ProjectileManager;

// Flying barrage attack: boss flies up, becomes invincible, and fires circular bullet patterns
class FlyingBarrageAttackBehavior : public IAttackBehavior
{
public:
    FlyingBarrageAttackBehavior(ProjectileManager* pProjectileManager,
                                 float fDamagePerHit = 15.0f,
                                 float fProjectileSpeed = 20.0f,
                                 int nProjectilesPerWave = 24,
                                 int nTotalWaves = 12,
                                 float fWaveInterval = 0.4f,
                                 float fFlyHeight = 18.0f,
                                 float fTakeOffDuration = 1.0f,
                                 float fLandingDuration = 1.0f);
    virtual ~FlyingBarrageAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;

private:
    void FireCircularWave(EnemyComponent* pEnemy, float fAngleOffset);
    void FireSpiralWave(EnemyComponent* pEnemy, float fBaseAngle);       // Spiral pattern
    void FireCrossWave(EnemyComponent* pEnemy, float fAngleOffset);      // Cross + X pattern
    void FireAimedWave(EnemyComponent* pEnemy, int nProjectiles);        // Aimed at player
    void FireRainWave(EnemyComponent* pEnemy, int nProjectiles);         // Random rain from above

private:
    // Dependencies
    ProjectileManager* m_pProjectileManager = nullptr;

    // Attack parameters
    float m_fDamagePerHit = 15.0f;
    float m_fProjectileSpeed = 20.0f;
    int m_nProjectilesPerWave = 24;     // 360/24 = 15 degree spacing
    int m_nTotalWaves = 12;             // Total waves to fire
    float m_fWaveInterval = 0.4f;       // Time between waves

    // Flight parameters
    float m_fFlyHeight = 18.0f;         // How high to fly
    float m_fTakeOffDuration = 1.0f;    // Time to reach full height
    float m_fLandingDuration = 1.0f;    // Time to land

    // Runtime state
    enum class Phase { TakeOff, Barrage, Landing };
    Phase m_ePhase = Phase::TakeOff;
    float m_fTimer = 0.0f;
    float m_fOriginalY = 0.0f;          // Ground position Y
    int m_nWavesFired = 0;
    float m_fNextWaveTime = 0.0f;
    float m_fSpiralAngle = 0.0f;        // For spiral pattern
    bool m_bFinished = false;
};
