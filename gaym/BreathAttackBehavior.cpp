#include "stdafx.h"
#include "BreathAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ProjectileManager.h"
#include "MathUtils.h"

BreathAttackBehavior::BreathAttackBehavior(ProjectileManager* pProjectileManager,
                                           float fDamagePerHit,
                                           float fProjectileSpeed,
                                           int nProjectileCount,
                                           float fSpreadAngle,
                                           float fWindupTime,
                                           float fBreathDuration,
                                           float fRecoveryTime,
                                           float fProjectileRadius,
                                           float fProjectileScale,
                                           ElementType eElement)
    : m_pProjectileManager(pProjectileManager)
    , m_fDamagePerHit(fDamagePerHit)
    , m_fProjectileSpeed(fProjectileSpeed)
    , m_nProjectileCount(nProjectileCount)
    , m_fSpreadAngle(fSpreadAngle)
    , m_fWindupTime(fWindupTime)
    , m_fBreathDuration(fBreathDuration)
    , m_fRecoveryTime(fRecoveryTime)
    , m_fProjectileRadius(fProjectileRadius)
    , m_fProjectileScale(fProjectileScale)
    , m_eElement(eElement)
{
}

void BreathAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (pEnemy)
    {
        pEnemy->FaceTarget();
    }

    m_ePhase = Phase::Windup;

    OutputDebugString(L"[Breath] Attack started - windup phase\n");
}

void BreathAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
{
    if (m_bFinished) return;

    m_fTimer += dt;

    switch (m_ePhase)
    {
    case Phase::Windup:
        if (pEnemy) pEnemy->FaceTarget();
        if (m_fTimer >= m_fWindupTime)
        {
            m_ePhase = Phase::Breath;
            m_fTimer = 0.0f;
            m_nProjectilesFired = 0;
            m_fNextFireTime = 0.0f;
            OutputDebugString(L"[Breath] Breath phase started\n");
        }
        break;

    case Phase::Breath:
        {
            // Fire projectiles at regular intervals during breath duration
            float fireInterval = m_fBreathDuration / (float)m_nProjectileCount;

            while (m_fTimer >= m_fNextFireTime && m_nProjectilesFired < m_nProjectileCount)
            {
                // Calculate angle offset for this projectile (spread from -half to +half)
                float angleOffset = 0.0f;
                if (m_nProjectileCount > 1)
                {
                    float halfSpread = m_fSpreadAngle * 0.5f;
                    float t = (float)m_nProjectilesFired / (float)(m_nProjectileCount - 1);
                    angleOffset = -halfSpread + t * m_fSpreadAngle;
                }

                FireBreathProjectile(pEnemy, angleOffset);
                m_nProjectilesFired++;
                m_fNextFireTime += fireInterval;
            }

            if (m_fTimer >= m_fBreathDuration)
            {
                m_ePhase = Phase::Recovery;
                m_fTimer = 0.0f;
                OutputDebugString(L"[Breath] Recovery phase\n");
            }
        }
        break;

    case Phase::Recovery:
        if (m_fTimer >= m_fRecoveryTime)
        {
            m_bFinished = true;
            OutputDebugString(L"[Breath] Attack finished\n");
        }
        break;
    }
}

bool BreathAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void BreathAttackBehavior::Reset()
{
    m_ePhase = Phase::Windup;
    m_fTimer = 0.0f;
    m_nProjectilesFired = 0;
    m_fNextFireTime = 0.0f;
    m_bFinished = false;
}

void BreathAttackBehavior::FireBreathProjectile(EnemyComponent* pEnemy, float angleOffset)
{
    if (!pEnemy || !m_pProjectileManager) return;

    GameObject* pOwner = pEnemy->GetOwner();
    GameObject* pTarget = pEnemy->GetTarget();
    if (!pOwner || !pTarget) return;

    TransformComponent* pMyTransform = pOwner->GetTransform();
    TransformComponent* pTargetTransform = pTarget->GetTransform();
    if (!pMyTransform || !pTargetTransform) return;

    XMFLOAT3 startPos = pMyTransform->GetPosition();
    // Fire from dragon's mouth (forward and slightly down)
    XMFLOAT3 rotation = pMyTransform->GetRotation();
    float yawRad = XMConvertToRadians(rotation.y);
    startPos.x += sinf(yawRad) * 5.0f;  // Forward offset
    startPos.z += cosf(yawRad) * 5.0f;
    startPos.y -= 2.0f;  // Slightly below dragon

    XMFLOAT3 targetPos = pTargetTransform->GetPosition();
    targetPos.y += 1.0f;  // Aim at player center

    // Calculate direction and apply angle offset
    XMFLOAT3 dir;
    dir.x = targetPos.x - startPos.x;
    dir.y = targetPos.y - startPos.y;
    dir.z = targetPos.z - startPos.z;

    // Normalize direction
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 0.0f)
    {
        dir.x /= len;
        dir.y /= len;
        dir.z /= len;
    }

    // Apply horizontal angle offset
    float offsetRad = XMConvertToRadians(angleOffset);
    float newX = dir.x * cosf(offsetRad) - dir.z * sinf(offsetRad);
    float newZ = dir.x * sinf(offsetRad) + dir.z * cosf(offsetRad);
    dir.x = newX;
    dir.z = newZ;

    // Calculate target position from direction
    XMFLOAT3 fireTarget;
    fireTarget.x = startPos.x + dir.x * 50.0f;
    fireTarget.y = startPos.y + dir.y * 50.0f;
    fireTarget.z = startPos.z + dir.z * 50.0f;

    m_pProjectileManager->SpawnProjectile(
        startPos,
        fireTarget,
        m_fDamagePerHit,
        m_fProjectileSpeed,
        m_fProjectileRadius,
        0.0f,
        m_eElement,
        pOwner,
        false,
        m_fProjectileScale
    );
}
