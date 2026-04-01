#include "stdafx.h"
#include "DiveBombAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ProjectileManager.h"
#include "AnimationComponent.h"
#include "PlayerComponent.h"

DiveBombAttackBehavior::DiveBombAttackBehavior(ProjectileManager* pProjectileManager,
                                               float fDamagePerHit,
                                               float fProjectileSpeed,
                                               float fDiveSpeed,
                                               float fImpactDamage,
                                               float fImpactRadius,
                                               int nProjectilesPerWave,
                                               float fFireInterval,
                                               float fFlyHeight,
                                               float fTakeOffDuration,
                                               float fHoverDuration)
    : m_pProjectileManager(pProjectileManager)
    , m_fDamagePerHit(fDamagePerHit)
    , m_fProjectileSpeed(fProjectileSpeed)
    , m_fDiveSpeed(fDiveSpeed)
    , m_fImpactDamage(fImpactDamage)
    , m_fImpactRadius(fImpactRadius)
    , m_nProjectilesPerWave(nProjectilesPerWave)
    , m_fFireInterval(fFireInterval)
    , m_fFlyHeight(fFlyHeight)
    , m_fTakeOffDuration(fTakeOffDuration)
    , m_fHoverDuration(fHoverDuration)
{
}

void DiveBombAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (pEnemy)
    {
        GameObject* pOwner = pEnemy->GetOwner();
        if (pOwner && pOwner->GetTransform())
        {
            m_fOriginalY = pOwner->GetTransform()->GetPosition().y;
        }

        pEnemy->SetInvincible(true);

        AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
        if (pAnimComp)
        {
            pAnimComp->CrossFade("Take Off", 0.15f, false);
        }
    }

    m_ePhase = Phase::TakeOff;
    OutputDebugString(L"[DiveBomb] Starting TakeOff phase\n");
}

void DiveBombAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
{
    if (m_bFinished || !pEnemy) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    m_fTimer += dt;

    switch (m_ePhase)
    {
    case Phase::TakeOff:
        {
            float t = m_fTimer / m_fTakeOffDuration;
            if (t > 1.0f) t = 1.0f;

            float easeT = 1.0f - (1.0f - t) * (1.0f - t);
            float newY = m_fOriginalY + m_fFlyHeight * easeT;

            XMFLOAT3 pos = pTransform->GetPosition();
            pos.y = newY;
            pTransform->SetPosition(pos);

            if (m_fTimer >= m_fTakeOffDuration)
            {
                m_ePhase = Phase::Hover;
                m_fTimer = 0.0f;

                // Lock onto target position
                GameObject* pTarget = pEnemy->GetTarget();
                if (pTarget && pTarget->GetTransform())
                {
                    m_xmf3TargetPosition = pTarget->GetTransform()->GetPosition();
                }

                // Calculate dive direction
                XMFLOAT3 myPos = pTransform->GetPosition();
                float dx = m_xmf3TargetPosition.x - myPos.x;
                float dy = m_xmf3TargetPosition.y - myPos.y;
                float dz = m_xmf3TargetPosition.z - myPos.z;
                float len = sqrtf(dx * dx + dy * dy + dz * dz);
                if (len > 0.0f)
                {
                    m_xmf3DiveDirection = { dx / len, dy / len, dz / len };
                }

                AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
                if (pAnimComp)
                {
                    pAnimComp->CrossFade("Fly Glide", 0.1f, true);
                }

                OutputDebugString(L"[DiveBomb] Hover phase - locking target\n");
            }
        }
        break;

    case Phase::Hover:
        {
            // Brief hover before dive
            if (m_fTimer >= m_fHoverDuration)
            {
                m_ePhase = Phase::Dive;
                m_fTimer = 0.0f;
                m_fNextFireTime = 0.0f;

                OutputDebugString(L"[DiveBomb] Dive phase started!\n");
            }
        }
        break;

    case Phase::Dive:
        {
            XMFLOAT3 pos = pTransform->GetPosition();

            // Move toward target
            float moveAmount = m_fDiveSpeed * dt;
            pos.x += m_xmf3DiveDirection.x * moveAmount;
            pos.y += m_xmf3DiveDirection.y * moveAmount;
            pos.z += m_xmf3DiveDirection.z * moveAmount;
            pTransform->SetPosition(pos);

            // Fire forward spread while diving
            if (m_fTimer >= m_fNextFireTime)
            {
                FireForwardSpread(pEnemy);
                m_fNextFireTime = m_fTimer + m_fFireInterval;
            }

            // Check if reached ground level
            if (pos.y <= m_fOriginalY + 0.5f)
            {
                pos.y = m_fOriginalY;
                pTransform->SetPosition(pos);

                // Deal impact damage
                if (!m_bImpactDealt)
                {
                    DealImpactDamage(pEnemy);
                    m_bImpactDealt = true;
                }

                m_ePhase = Phase::Recovery;
                m_fTimer = 0.0f;

                AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
                if (pAnimComp)
                {
                    pAnimComp->CrossFade("Land", 0.1f, false);
                }

                OutputDebugString(L"[DiveBomb] Recovery phase - impact!\n");
            }
        }
        break;

    case Phase::Recovery:
        {
            if (m_fTimer >= 0.5f)
            {
                pEnemy->SetInvincible(false);
                m_bFinished = true;
                OutputDebugString(L"[DiveBomb] Attack finished\n");
            }
        }
        break;
    }
}

bool DiveBombAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void DiveBombAttackBehavior::Reset()
{
    m_ePhase = Phase::TakeOff;
    m_fTimer = 0.0f;
    m_fOriginalY = 0.0f;
    m_fNextFireTime = 0.0f;
    m_xmf3DiveDirection = { 0.0f, -1.0f, 0.0f };
    m_xmf3TargetPosition = { 0.0f, 0.0f, 0.0f };
    m_bImpactDealt = false;
    m_bFinished = false;
}

void DiveBombAttackBehavior::FireForwardSpread(EnemyComponent* pEnemy)
{
    if (!pEnemy || !m_pProjectileManager) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    XMFLOAT3 startPos = pTransform->GetPosition();

    // Fire spread of projectiles in dive direction
    float spreadAngle = 45.0f;
    float angleStep = spreadAngle / (float)(m_nProjectilesPerWave - 1);
    float startAngle = -spreadAngle * 0.5f;

    // Get horizontal dive direction for spread calculation
    XMFLOAT2 hDir = { m_xmf3DiveDirection.x, m_xmf3DiveDirection.z };
    float hLen = sqrtf(hDir.x * hDir.x + hDir.y * hDir.y);
    if (hLen > 0.0f)
    {
        hDir.x /= hLen;
        hDir.y /= hLen;
    }

    for (int i = 0; i < m_nProjectilesPerWave; i++)
    {
        float offsetAngle = startAngle + (float)i * angleStep;
        float offsetRad = XMConvertToRadians(offsetAngle);

        // Rotate direction around Y axis
        float newX = hDir.x * cosf(offsetRad) - hDir.y * sinf(offsetRad);
        float newZ = hDir.x * sinf(offsetRad) + hDir.y * cosf(offsetRad);

        XMFLOAT3 dir = { newX, m_xmf3DiveDirection.y * 0.5f, newZ };

        // Normalize
        float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        if (len > 0.0f)
        {
            dir.x /= len;
            dir.y /= len;
            dir.z /= len;
        }

        XMFLOAT3 targetPos;
        targetPos.x = startPos.x + dir.x * 50.0f;
        targetPos.y = startPos.y + dir.y * 50.0f;
        targetPos.z = startPos.z + dir.z * 50.0f;

        m_pProjectileManager->SpawnProjectile(
            startPos,
            targetPos,
            m_fDamagePerHit,
            m_fProjectileSpeed,
            0.5f,
            0.0f,
            ElementType::Fire,
            pOwner,
            false,
            1.0f
        );
    }
}

void DiveBombAttackBehavior::DealImpactDamage(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pTarget = pEnemy->GetTarget();
    if (!pTarget) return;

    float distance = pEnemy->GetDistanceToTarget();
    if (distance > m_fImpactRadius) return;

    PlayerComponent* pPlayer = pTarget->GetComponent<PlayerComponent>();
    if (pPlayer)
    {
        pPlayer->TakeDamage(m_fImpactDamage);
        OutputDebugString(L"[DiveBomb] Impact HIT!\n");
    }
}
