#include "stdafx.h"
#include "FlyingSweepAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ProjectileManager.h"
#include "AnimationComponent.h"

FlyingSweepAttackBehavior::FlyingSweepAttackBehavior(ProjectileManager* pProjectileManager,
                                                     float fDamagePerHit,
                                                     float fProjectileSpeed,
                                                     float fFlySpeed,
                                                     float fFlyDistance,
                                                     float fSweepAngle,
                                                     float fSweepSpeed,
                                                     float fFireInterval,
                                                     int nProjectilesPerShot,
                                                     float fFlyHeight,
                                                     float fTakeOffDuration,
                                                     float fLandingDuration)
    : m_pProjectileManager(pProjectileManager)
    , m_fDamagePerHit(fDamagePerHit)
    , m_fProjectileSpeed(fProjectileSpeed)
    , m_fFlySpeed(fFlySpeed)
    , m_fFlyDistance(fFlyDistance)
    , m_fSweepAngle(fSweepAngle)
    , m_fSweepSpeed(fSweepSpeed)
    , m_fFireInterval(fFireInterval)
    , m_nProjectilesPerShot(nProjectilesPerShot)
    , m_fFlyHeight(fFlyHeight)
    , m_fTakeOffDuration(fTakeOffDuration)
    , m_fLandingDuration(fLandingDuration)
{
}

void FlyingSweepAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (pEnemy)
    {
        GameObject* pOwner = pEnemy->GetOwner();
        if (pOwner && pOwner->GetTransform())
        {
            m_fOriginalY = pOwner->GetTransform()->GetPosition().y;
        }

        ChooseFlyDirection(pEnemy);

        pEnemy->SetInvincible(true);

        AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
        if (pAnimComp)
        {
            pAnimComp->CrossFade("Take Off", 0.15f, false);
        }
    }

    m_ePhase = Phase::TakeOff;
}

void FlyingSweepAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
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
                m_ePhase = Phase::Sweep;
                m_fTimer = 0.0f;
                m_fDistanceTraveled = 0.0f;
                m_fNextFireTime = 0.0f;
                m_fCurrentSweepAngle = -m_fSweepAngle * 0.5f;
                m_nSweepDirection = 1;

                AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
                if (pAnimComp)
                {
                    pAnimComp->CrossFade("Fly Glide", 0.2f, true);
                }
            }
        }
        break;

    case Phase::Sweep:
        {
            // Move forward
            XMFLOAT3 pos = pTransform->GetPosition();
            float moveAmount = m_fFlySpeed * dt;
            pos.x += m_xmf3FlyDirection.x * moveAmount;
            pos.z += m_xmf3FlyDirection.z * moveAmount;
            pTransform->SetPosition(pos);

            m_fDistanceTraveled += moveAmount;

            // Update sweep angle (oscillate back and forth)
            float sweepDelta = m_fSweepSpeed * dt * (float)m_nSweepDirection;
            m_fCurrentSweepAngle += sweepDelta;

            // Reverse direction at limits
            if (m_fCurrentSweepAngle >= m_fSweepAngle * 0.5f)
            {
                m_fCurrentSweepAngle = m_fSweepAngle * 0.5f;
                m_nSweepDirection = -1;
            }
            else if (m_fCurrentSweepAngle <= -m_fSweepAngle * 0.5f)
            {
                m_fCurrentSweepAngle = -m_fSweepAngle * 0.5f;
                m_nSweepDirection = 1;
            }

            // Fire while sweeping
            if (m_fTimer >= m_fNextFireTime)
            {
                FireSweepProjectiles(pEnemy);
                m_fNextFireTime = m_fTimer + m_fFireInterval;
            }

            // Check if fly distance complete
            if (m_fDistanceTraveled >= m_fFlyDistance)
            {
                m_ePhase = Phase::Landing;
                m_fTimer = 0.0f;

                AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
                if (pAnimComp)
                {
                    pAnimComp->CrossFade("Land", 0.15f, false);
                }
            }
        }
        break;

    case Phase::Landing:
        {
            float t = m_fTimer / m_fLandingDuration;
            if (t > 1.0f) t = 1.0f;

            float easeT = t * t;
            float newY = m_fOriginalY + m_fFlyHeight * (1.0f - easeT);

            XMFLOAT3 pos = pTransform->GetPosition();
            pos.y = newY;
            pTransform->SetPosition(pos);

            if (m_fTimer >= m_fLandingDuration)
            {
                pos.y = m_fOriginalY;
                pTransform->SetPosition(pos);

                pEnemy->SetInvincible(false);
                m_bFinished = true;
            }
        }
        break;
    }
}

bool FlyingSweepAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void FlyingSweepAttackBehavior::Reset()
{
    m_ePhase = Phase::TakeOff;
    m_fTimer = 0.0f;
    m_fOriginalY = 0.0f;
    m_fDistanceTraveled = 0.0f;
    m_fCurrentSweepAngle = 0.0f;
    m_fNextFireTime = 0.0f;
    m_nSweepDirection = 1;
    m_xmf3FlyDirection = { 0.0f, 0.0f, 0.0f };
    m_xmf3BaseFireDirection = { 0.0f, 0.0f, 0.0f };
    m_bFinished = false;
}

void FlyingSweepAttackBehavior::ChooseFlyDirection(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pOwner = pEnemy->GetOwner();
    GameObject* pTarget = pEnemy->GetTarget();
    if (!pOwner || !pTarget) return;

    TransformComponent* pMyTransform = pOwner->GetTransform();
    TransformComponent* pTargetTransform = pTarget->GetTransform();
    if (!pMyTransform || !pTargetTransform) return;

    XMFLOAT3 myPos = pMyTransform->GetPosition();
    XMFLOAT3 targetPos = pTargetTransform->GetPosition();

    // Direction to player (this will be the base fire direction)
    float dx = targetPos.x - myPos.x;
    float dz = targetPos.z - myPos.z;
    float len = sqrtf(dx * dx + dz * dz);
    if (len > 0.0f)
    {
        m_xmf3BaseFireDirection.x = dx / len;
        m_xmf3BaseFireDirection.z = dz / len;
    }
    m_xmf3BaseFireDirection.y = 0.0f;

    // Fly perpendicular to player (strafe across)
    bool bGoLeft = (rand() % 2) == 0;
    if (bGoLeft)
    {
        m_xmf3FlyDirection.x = -m_xmf3BaseFireDirection.z;
        m_xmf3FlyDirection.z = m_xmf3BaseFireDirection.x;
    }
    else
    {
        m_xmf3FlyDirection.x = m_xmf3BaseFireDirection.z;
        m_xmf3FlyDirection.z = -m_xmf3BaseFireDirection.x;
    }
    m_xmf3FlyDirection.y = 0.0f;
}

void FlyingSweepAttackBehavior::FireSweepProjectiles(EnemyComponent* pEnemy)
{
    if (!pEnemy || !m_pProjectileManager) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    XMFLOAT3 startPos = pTransform->GetPosition();
    startPos.y -= 1.0f;

    // Calculate fire direction with sweep offset
    float sweepRad = XMConvertToRadians(m_fCurrentSweepAngle);

    // Rotate base direction by sweep angle
    float newX = m_xmf3BaseFireDirection.x * cosf(sweepRad) - m_xmf3BaseFireDirection.z * sinf(sweepRad);
    float newZ = m_xmf3BaseFireDirection.x * sinf(sweepRad) + m_xmf3BaseFireDirection.z * cosf(sweepRad);

    XMFLOAT3 fireDir = { newX, -0.2f, newZ };

    // Normalize
    float len = sqrtf(fireDir.x * fireDir.x + fireDir.y * fireDir.y + fireDir.z * fireDir.z);
    if (len > 0.0f)
    {
        fireDir.x /= len;
        fireDir.y /= len;
        fireDir.z /= len;
    }

    // Fire multiple projectiles with slight vertical spread
    for (int i = 0; i < m_nProjectilesPerShot; i++)
    {
        XMFLOAT3 adjustedDir = fireDir;
        adjustedDir.y += (float)(i - m_nProjectilesPerShot / 2) * 0.1f;

        // Re-normalize
        len = sqrtf(adjustedDir.x * adjustedDir.x + adjustedDir.y * adjustedDir.y + adjustedDir.z * adjustedDir.z);
        if (len > 0.0f)
        {
            adjustedDir.x /= len;
            adjustedDir.y /= len;
            adjustedDir.z /= len;
        }

        XMFLOAT3 targetPos;
        targetPos.x = startPos.x + adjustedDir.x * 50.0f;
        targetPos.y = startPos.y + adjustedDir.y * 50.0f;
        targetPos.z = startPos.z + adjustedDir.z * 50.0f;

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
            0.9f
        );
    }
}
