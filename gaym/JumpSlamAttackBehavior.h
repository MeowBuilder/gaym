#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>

using namespace DirectX;

// Jump slam attack: boss jumps to target location and creates AoE damage on landing
class JumpSlamAttackBehavior : public IAttackBehavior
{
public:
    JumpSlamAttackBehavior(float fDamage = 25.0f,
                           float fJumpHeight = 10.0f,
                           float fJumpDuration = 0.6f,
                           float fSlamRadius = 7.0f,
                           float fWindupTime = 0.3f,
                           float fRecoveryTime = 0.5f,
                           bool bTrackTarget = true);  // Jump to where target is
    virtual ~JumpSlamAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;

private:
    void DealSlamDamage(EnemyComponent* pEnemy);

private:
    // Parameters
    float m_fDamage = 25.0f;
    float m_fJumpHeight = 10.0f;
    float m_fJumpDuration = 0.6f;
    float m_fSlamRadius = 7.0f;
    float m_fWindupTime = 0.3f;
    float m_fRecoveryTime = 0.5f;
    bool m_bTrackTarget = true;

    // Runtime state
    enum class Phase { Windup, Jump, Slam, Recovery };
    Phase m_ePhase = Phase::Windup;
    float m_fTimer = 0.0f;
    float m_fOriginalY = 0.0f;
    XMFLOAT3 m_xmf3StartPosition = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 m_xmf3TargetPosition = { 0.0f, 0.0f, 0.0f };
    bool m_bSlamDealt = false;
    bool m_bFinished = false;
};
