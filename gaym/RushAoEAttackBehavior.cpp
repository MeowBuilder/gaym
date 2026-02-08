#include "stdafx.h"
#include "RushAoEAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "MathUtils.h"

RushAoEAttackBehavior::RushAoEAttackBehavior(float fDamage, float fRushSpeed, float fRushDuration,
                                             float fWindupTime, float fHitTime, float fRecoveryTime,
                                             float fAoERadius)
    : m_fDamage(fDamage)
    , m_fRushSpeed(fRushSpeed)
    , m_fRushDuration(fRushDuration)
    , m_fWindupTime(fWindupTime)
    , m_fHitTime(fHitTime)
    , m_fRecoveryTime(fRecoveryTime)
    , m_fAoERadius(fAoERadius)
{
}

void RushAoEAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (!pEnemy) return;

    // Face and lock direction to target
    pEnemy->FaceTarget();

    // Store rush direction
    GameObject* pOwner = pEnemy->GetOwner();
    GameObject* pTarget = pEnemy->GetTarget();
    if (pOwner && pTarget)
    {
        TransformComponent* pMyTransform = pOwner->GetTransform();
        TransformComponent* pTargetTransform = pTarget->GetTransform();
        if (pMyTransform && pTargetTransform)
        {
            XMFLOAT3 myPos = pMyTransform->GetPosition();
            XMFLOAT3 targetPos = pTargetTransform->GetPosition();
            XMFLOAT2 dir = MathUtils::Direction2D(myPos, targetPos);
            m_xmf3RushDirection = XMFLOAT3(dir.x, 0.0f, dir.y);
        }
    }

    m_ePhase = Phase::Rush;
    m_fPhaseDuration = m_fRushDuration;

    OutputDebugString(L"[RushAoE] Attack started - rush phase\n");
}

void RushAoEAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
{
    if (m_bFinished) return;

    m_fTimer += dt;

    switch (m_ePhase)
    {
    case Phase::Rush:
        UpdateRush(dt, pEnemy);
        if (m_fTimer >= m_fRushDuration)
        {
            m_ePhase = Phase::Windup;
            m_fTimer = 0.0f;
            m_fPhaseDuration = m_fWindupTime;
            OutputDebugString(L"[RushAoE] Windup phase\n");
        }
        break;

    case Phase::Windup:
        if (m_fTimer >= m_fWindupTime)
        {
            m_ePhase = Phase::Hit;
            m_fTimer = 0.0f;
            m_fPhaseDuration = m_fHitTime;
            OutputDebugString(L"[RushAoE] Hit phase\n");
        }
        break;

    case Phase::Hit:
        if (!m_bHitDealt)
        {
            DealAoEDamage(pEnemy);
            m_bHitDealt = true;
        }
        if (m_fTimer >= m_fHitTime)
        {
            m_ePhase = Phase::Recovery;
            m_fTimer = 0.0f;
            m_fPhaseDuration = m_fRecoveryTime;
            OutputDebugString(L"[RushAoE] Recovery phase\n");
        }
        break;

    case Phase::Recovery:
        if (m_fTimer >= m_fRecoveryTime)
        {
            m_bFinished = true;
            OutputDebugString(L"[RushAoE] Attack finished\n");
        }
        break;
    }
}

bool RushAoEAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void RushAoEAttackBehavior::Reset()
{
    m_ePhase = Phase::Rush;
    m_fTimer = 0.0f;
    m_bHitDealt = false;
    m_bFinished = false;
    m_xmf3RushDirection = XMFLOAT3(0.0f, 0.0f, 0.0f);
}

void RushAoEAttackBehavior::UpdateRush(float dt, EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    // Move in the locked rush direction
    XMFLOAT3 pos = pTransform->GetPosition();
    float moveAmount = m_fRushSpeed * dt;
    pos.x += m_xmf3RushDirection.x * moveAmount;
    pos.z += m_xmf3RushDirection.z * moveAmount;
    pTransform->SetPosition(pos);
}

void RushAoEAttackBehavior::DealAoEDamage(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pTarget = pEnemy->GetTarget();
    if (!pTarget) return;

    // 360-degree AoE: only check distance, no angle check
    float distance = pEnemy->GetDistanceToTarget();
    if (distance > m_fAoERadius)
    {
        OutputDebugString(L"[RushAoE] AoE missed - target out of range\n");
        return;
    }

    wchar_t buffer[128];
    swprintf_s(buffer, L"[RushAoE] AoE HIT! Dealing %.1f damage (dist: %.1f)\n", m_fDamage, distance);
    OutputDebugString(buffer);

    // TODO: PlayerComponent::TakeDamage
}
