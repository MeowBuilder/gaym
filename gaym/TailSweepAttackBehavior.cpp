#include "stdafx.h"
#include "TailSweepAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "PlayerComponent.h"
#include "AnimationComponent.h"
#include "MathUtils.h"

TailSweepAttackBehavior::TailSweepAttackBehavior(float fDamage,
                                                 float fWindupTime,
                                                 float fSweepTime,
                                                 float fRecoveryTime,
                                                 float fHitRange,
                                                 float fSweepArc,
                                                 bool bHitBehind)
    : m_fDamage(fDamage)
    , m_fWindupTime(fWindupTime)
    , m_fSweepTime(fSweepTime)
    , m_fRecoveryTime(fRecoveryTime)
    , m_fHitRange(fHitRange)
    , m_fSweepArc(fSweepArc)
    , m_bHitBehind(bHitBehind)
{
}

void TailSweepAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (pEnemy)
    {
        // Store initial rotation for sweep animation
        GameObject* pOwner = pEnemy->GetOwner();
        if (pOwner && pOwner->GetTransform())
        {
            m_fInitialRotation = pOwner->GetTransform()->GetRotation().y;
        }

        AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
        if (pAnimComp)
        {
            pAnimComp->CrossFade("Claw Attack", 0.1f, false);
        }
    }

    m_ePhase = Phase::Windup;
    OutputDebugString(L"[TailSweep] Attack started - windup phase\n");
}

void TailSweepAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
{
    if (m_bFinished) return;

    m_fTimer += dt;

    switch (m_ePhase)
    {
    case Phase::Windup:
        if (m_fTimer >= m_fWindupTime)
        {
            m_ePhase = Phase::Sweep;
            m_fTimer = 0.0f;
            OutputDebugString(L"[TailSweep] Sweep phase\n");
        }
        break;

    case Phase::Sweep:
        {
            // Rotate boss during sweep for visual effect
            if (pEnemy)
            {
                GameObject* pOwner = pEnemy->GetOwner();
                if (pOwner && pOwner->GetTransform())
                {
                    float t = m_fTimer / m_fSweepTime;
                    if (t > 1.0f) t = 1.0f;

                    // Rotate through the sweep arc
                    float sweepProgress = t * m_fSweepArc;
                    float newRotation = m_fInitialRotation + (m_bHitBehind ? sweepProgress : -sweepProgress);

                    XMFLOAT3 rot = pOwner->GetTransform()->GetRotation();
                    rot.y = newRotation;
                    pOwner->GetTransform()->SetRotation(rot);
                }
            }

            // Deal damage at the middle of sweep
            if (!m_bHitDealt && m_fTimer >= m_fSweepTime * 0.5f)
            {
                DealSweepDamage(pEnemy);
                m_bHitDealt = true;
            }

            if (m_fTimer >= m_fSweepTime)
            {
                m_ePhase = Phase::Recovery;
                m_fTimer = 0.0f;
                OutputDebugString(L"[TailSweep] Recovery phase\n");
            }
        }
        break;

    case Phase::Recovery:
        if (m_fTimer >= m_fRecoveryTime)
        {
            m_bFinished = true;
            OutputDebugString(L"[TailSweep] Attack finished\n");
        }
        break;
    }
}

bool TailSweepAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void TailSweepAttackBehavior::Reset()
{
    m_ePhase = Phase::Windup;
    m_fTimer = 0.0f;
    m_fInitialRotation = 0.0f;
    m_bHitDealt = false;
    m_bFinished = false;
}

void TailSweepAttackBehavior::DealSweepDamage(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pOwner = pEnemy->GetOwner();
    GameObject* pTarget = pEnemy->GetTarget();
    if (!pOwner || !pTarget) return;

    TransformComponent* pMyTransform = pOwner->GetTransform();
    TransformComponent* pTargetTransform = pTarget->GetTransform();
    if (!pMyTransform || !pTargetTransform) return;

    // Check distance
    float distance = pEnemy->GetDistanceToTarget();
    if (distance > m_fHitRange)
    {
        OutputDebugString(L"[TailSweep] Missed - target out of range\n");
        return;
    }

    // Check angle - tail sweep hits in a wide arc
    XMFLOAT3 myPos = pMyTransform->GetPosition();
    XMFLOAT3 targetPos = pTargetTransform->GetPosition();
    XMFLOAT3 myRot = pMyTransform->GetRotation();

    // Direction to target
    float dx = targetPos.x - myPos.x;
    float dz = targetPos.z - myPos.z;
    float angleToTarget = XMConvertToDegrees(atan2f(dx, dz));

    // Boss facing direction
    float facingAngle = myRot.y;

    // For tail sweep, check if target is within the sweep arc
    // If hitting behind, the arc is centered at 180 degrees from facing
    float centerAngle = m_bHitBehind ? facingAngle + 180.0f : facingAngle;

    // Normalize angles
    while (angleToTarget < 0.0f) angleToTarget += 360.0f;
    while (angleToTarget >= 360.0f) angleToTarget -= 360.0f;
    while (centerAngle < 0.0f) centerAngle += 360.0f;
    while (centerAngle >= 360.0f) centerAngle -= 360.0f;

    // Calculate angle difference
    float angleDiff = fabsf(angleToTarget - centerAngle);
    if (angleDiff > 180.0f) angleDiff = 360.0f - angleDiff;

    // Check if within sweep arc
    if (angleDiff > m_fSweepArc * 0.5f)
    {
        OutputDebugString(L"[TailSweep] Missed - target outside sweep arc\n");
        return;
    }

    // Deal damage
    PlayerComponent* pPlayer = pTarget->GetComponent<PlayerComponent>();
    if (pPlayer)
    {
        pPlayer->TakeDamage(m_fDamage);

        wchar_t buffer[128];
        swprintf_s(buffer, L"[TailSweep] HIT! Dealt %.1f damage\n", m_fDamage);
        OutputDebugString(buffer);
    }
}
