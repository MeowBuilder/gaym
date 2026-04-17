#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>

using namespace DirectX;

// Tail sweep attack: boss spins and hits in 180-degree arc behind/around it
class TailSweepAttackBehavior : public IAttackBehavior
{
public:
    TailSweepAttackBehavior(float fDamage = 18.0f,
                            float fWindupTime = 0.4f,
                            float fSweepTime = 0.3f,
                            float fRecoveryTime = 0.4f,
                            float fHitRange = 6.0f,
                            float fSweepArc = 180.0f,    // Degrees
                            bool bHitBehind = true);     // Start from behind
    virtual ~TailSweepAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;
    virtual const char* GetAnimClipName() const override { return "Tail Attack"; }

private:
    void DealSweepDamage(EnemyComponent* pEnemy);

private:
    // Parameters
    float m_fDamage = 18.0f;
    float m_fWindupTime = 0.4f;
    float m_fSweepTime = 0.3f;
    float m_fRecoveryTime = 0.4f;
    float m_fHitRange = 6.0f;
    float m_fSweepArc = 180.0f;
    bool m_bHitBehind = true;

    // Runtime state
    enum class Phase { Windup, Sweep, Recovery };
    Phase m_ePhase = Phase::Windup;
    float m_fTimer = 0.0f;
    float m_fInitialRotation = 0.0f;
    bool m_bHitDealt = false;
    bool m_bFinished = false;
};
