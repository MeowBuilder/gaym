#include "stdafx.h"
#include "RushFrontAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "MathUtils.h"

RushFrontAttackBehavior::RushFrontAttackBehavior(float fDamage, float fRushSpeed, float fRushDuration,
                                                 float fWindupTime, float fHitTime, float fRecoveryTime,
                                                 float fHitRange, float fConeAngleDeg)
    : m_fDamage(fDamage)
    , m_fRushSpeed(fRushSpeed)
    , m_fRushDuration(fRushDuration)
    , m_fWindupTime(fWindupTime)
    , m_fHitTime(fHitTime)
    , m_fRecoveryTime(fRecoveryTime)
    , m_fHitRange(fHitRange)
    , m_fConeAngleDeg(fConeAngleDeg)
{
    // Precompute cos of half-cone angle for dot product comparison
    float halfConeRad = XMConvertToRadians(fConeAngleDeg * 0.5f);
    m_fCosHalfCone = cosf(halfConeRad);
}

void RushFrontAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (!pEnemy) return;

    // Face and lock direction to target
    pEnemy->FaceTarget();

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

    OutputDebugString(L"[RushFront] Attack started - rush phase\n");
}

void RushFrontAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
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
            OutputDebugString(L"[RushFront] Windup phase\n");
        }
        break;

    case Phase::Windup:
        if (m_fTimer >= m_fWindupTime)
        {
            m_ePhase = Phase::Hit;
            m_fTimer = 0.0f;
            OutputDebugString(L"[RushFront] Hit phase\n");
        }
        break;

    case Phase::Hit:
        if (!m_bHitDealt)
        {
            DealConeDamage(pEnemy);
            m_bHitDealt = true;
        }
        if (m_fTimer >= m_fHitTime)
        {
            m_ePhase = Phase::Recovery;
            m_fTimer = 0.0f;
            OutputDebugString(L"[RushFront] Recovery phase\n");
        }
        break;

    case Phase::Recovery:
        if (m_fTimer >= m_fRecoveryTime)
        {
            m_bFinished = true;
            OutputDebugString(L"[RushFront] Attack finished\n");
        }
        break;
    }
}

bool RushFrontAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void RushFrontAttackBehavior::Reset()
{
    m_ePhase = Phase::Rush;
    m_fTimer = 0.0f;
    m_bHitDealt = false;
    m_bFinished = false;
    m_xmf3RushDirection = XMFLOAT3(0.0f, 0.0f, 0.0f);
}

void RushFrontAttackBehavior::UpdateRush(float dt, EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    XMFLOAT3 pos = pTransform->GetPosition();
    float moveAmount = m_fRushSpeed * dt;
    pos.x += m_xmf3RushDirection.x * moveAmount;
    pos.z += m_xmf3RushDirection.z * moveAmount;
    pTransform->SetPosition(pos);
}

void RushFrontAttackBehavior::DealConeDamage(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pTarget = pEnemy->GetTarget();
    if (!pTarget) return;

    // Distance check
    float distance = pEnemy->GetDistanceToTarget();
    if (distance > m_fHitRange)
    {
        OutputDebugString(L"[RushFront] Cone attack missed - target out of range\n");
        return;
    }

    // Cone angle check using dot product
    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pMyTransform = pOwner->GetTransform();
    TransformComponent* pTargetTransform = pTarget->GetTransform();
    if (!pMyTransform || !pTargetTransform) return;

    // Get forward direction (look vector) on XZ plane
    XMVECTOR vLook = pMyTransform->GetLook();
    XMFLOAT3 look;
    XMStoreFloat3(&look, vLook);
    XMFLOAT2 forward2D = MathUtils::Normalize2D(look.x, look.z);

    // Get direction to target on XZ plane
    XMFLOAT3 myPos = pMyTransform->GetPosition();
    XMFLOAT3 targetPos = pTargetTransform->GetPosition();
    XMFLOAT2 toTarget = MathUtils::Direction2D(myPos, targetPos);

    // Dot product to check if target is within cone
    float dot = forward2D.x * toTarget.x + forward2D.y * toTarget.y;

    if (dot < m_fCosHalfCone)
    {
        OutputDebugString(L"[RushFront] Cone attack missed - target outside cone\n");
        return;
    }

    wchar_t buffer[128];
    swprintf_s(buffer, L"[RushFront] Cone HIT! Dealing %.1f damage (dist: %.1f, dot: %.2f)\n", m_fDamage, distance, dot);
    OutputDebugString(buffer);

    // TODO: PlayerComponent::TakeDamage
}
