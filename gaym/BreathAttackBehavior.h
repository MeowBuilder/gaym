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
                         ElementType eElement = ElementType::Fire,
                         const char* pClipOverride = nullptr,   // nullptr → AnimConfig 기본값
                         bool bVariedProjectiles = false);       // 투사체별 크기/속도/각도 랜덤 변주
    virtual ~BreathAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;
    // 오버라이드 지정 시 해당 클립, 아니면 m_strAttackClip 기본값 사용
    virtual const char* GetAnimClipName() const override {
        return m_strClipOverride ? m_strClipOverride : "";
    }
    virtual float GetTimeToHit() const override { return m_fWindupTime; }
    // 넓은 스프레이(≥270°, 사실상 전방향)는 ForwardBox 같은 방향성 인디케이터가 오히려 혼란
    // → 자동 억제. 좁은 cone(Kraken 기본 55°, Dragon 38° 등)은 텔레그래프 표시
    virtual bool ShouldShowHitZone() const override { return m_fSpreadAngle < 270.0f; }

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
    const char* m_strClipOverride = nullptr;  // nullptr → AnimConfig.m_strAttackClip 사용
    bool  m_bVariedProjectiles = false;        // 투사체별 크기/속도/각도 랜덤

    // Runtime state
    enum class Phase { Windup, Breath, Recovery };
    Phase m_ePhase = Phase::Windup;
    float m_fTimer = 0.0f;
    int m_nProjectilesFired = 0;
    float m_fNextFireTime = 0.0f;
    bool m_bFinished = false;
};
