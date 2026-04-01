#include "stdafx.h"
#include "FlyingStrafeAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ProjectileManager.h"
#include "AnimationComponent.h"

FlyingStrafeAttackBehavior::FlyingStrafeAttackBehavior(ProjectileManager* pProjectileManager,
                                                       float fDamagePerHit,
                                                       float fProjectileSpeed,
                                                       float fStrafeSpeed,
                                                       float fStrafeDistance,
                                                       float fFireInterval,
                                                       int nShotsPerBurst,
                                                       float fFlyHeight,
                                                       float fTakeOffDuration,
                                                       float fLandingDuration)
    : m_pProjectileManager(pProjectileManager)
    , m_fDamagePerHit(fDamagePerHit)
    , m_fProjectileSpeed(fProjectileSpeed)
    , m_fStrafeSpeed(fStrafeSpeed)
    , m_fStrafeDistance(fStrafeDistance)
    , m_fFireInterval(fFireInterval)
    , m_nShotsPerBurst(nShotsPerBurst)
    , m_fFlyHeight(fFlyHeight)
    , m_fTakeOffDuration(fTakeOffDuration)
    , m_fLandingDuration(fLandingDuration)
{
}

void FlyingStrafeAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (pEnemy)
    {
        GameObject* pOwner = pEnemy->GetOwner();
        if (pOwner && pOwner->GetTransform())
        {
            XMFLOAT3 pos = pOwner->GetTransform()->GetPosition();
            m_fOriginalY = pos.y;
            m_xmf3StartPosition = pos;
        }

        // Choose strafe direction (perpendicular to player)
        ChooseStrafeDirection(pEnemy);

        // Make boss invincible during flight
        pEnemy->SetInvincible(true);

        AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
        if (pAnimComp)
        {
            pAnimComp->CrossFade("Take Off", 0.15f, false);
        }
    }

    m_ePhase = Phase::TakeOff;
    OutputDebugString(L"[FlyingStrafe] Starting TakeOff phase\n");
}

void FlyingStrafeAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
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
                m_ePhase = Phase::Strafe;
                m_fTimer = 0.0f;
                m_fDistanceTraveled = 0.0f;
                m_fNextFireTime = 0.0f;
                m_nShotsFired = 0;

                AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
                if (pAnimComp)
                {
                    pAnimComp->CrossFade("Fly Glide", 0.2f, true);
                }

                OutputDebugString(L"[FlyingStrafe] Strafe phase started!\n");
            }
        }
        break;

    case Phase::Strafe:
        {
            // Move sideways
            XMFLOAT3 pos = pTransform->GetPosition();
            float moveAmount = m_fStrafeSpeed * dt;
            pos.x += m_xmf3StrafeDirection.x * moveAmount;
            pos.z += m_xmf3StrafeDirection.z * moveAmount;
            pTransform->SetPosition(pos);

            m_fDistanceTraveled += moveAmount;

            // Fire at player while strafing
            if (m_fTimer >= m_fNextFireTime)
            {
                FireAtPlayer(pEnemy);
                m_nShotsFired++;
                m_fNextFireTime = m_fTimer + m_fFireInterval;
            }

            // Check if strafe is complete
            if (m_fDistanceTraveled >= m_fStrafeDistance)
            {
                m_ePhase = Phase::Landing;
                m_fTimer = 0.0f;

                AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
                if (pAnimComp)
                {
                    pAnimComp->CrossFade("Land", 0.15f, false);
                }

                OutputDebugString(L"[FlyingStrafe] Landing phase started\n");
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
                OutputDebugString(L"[FlyingStrafe] Attack finished\n");
            }
        }
        break;
    }
}

bool FlyingStrafeAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void FlyingStrafeAttackBehavior::Reset()
{
    m_ePhase = Phase::TakeOff;
    m_fTimer = 0.0f;
    m_fOriginalY = 0.0f;
    m_fDistanceTraveled = 0.0f;
    m_fNextFireTime = 0.0f;
    m_nShotsFired = 0;
    m_xmf3StrafeDirection = { 0.0f, 0.0f, 0.0f };
    m_xmf3StartPosition = { 0.0f, 0.0f, 0.0f };
    m_bFinished = false;
}

void FlyingStrafeAttackBehavior::ChooseStrafeDirection(EnemyComponent* pEnemy)
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

    // Direction to player
    float dx = targetPos.x - myPos.x;
    float dz = targetPos.z - myPos.z;
    float len = sqrtf(dx * dx + dz * dz);
    if (len > 0.0f)
    {
        dx /= len;
        dz /= len;
    }

    // Perpendicular direction (randomly left or right)
    bool bGoLeft = (rand() % 2) == 0;
    if (bGoLeft)
    {
        m_xmf3StrafeDirection.x = -dz;
        m_xmf3StrafeDirection.z = dx;
    }
    else
    {
        m_xmf3StrafeDirection.x = dz;
        m_xmf3StrafeDirection.z = -dx;
    }
    m_xmf3StrafeDirection.y = 0.0f;
}

void FlyingStrafeAttackBehavior::FireAtPlayer(EnemyComponent* pEnemy)
{
    if (!pEnemy || !m_pProjectileManager) return;

    GameObject* pOwner = pEnemy->GetOwner();
    GameObject* pTarget = pEnemy->GetTarget();
    if (!pOwner || !pTarget) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    TransformComponent* pTargetTransform = pTarget->GetTransform();
    if (!pTransform || !pTargetTransform) return;

    XMFLOAT3 startPos = pTransform->GetPosition();
    startPos.y -= 1.5f;

    XMFLOAT3 targetPos = pTargetTransform->GetPosition();
    targetPos.y += 1.0f;

    // Fire burst of projectiles with slight spread
    for (int i = 0; i < m_nShotsPerBurst; i++)
    {
        XMFLOAT3 adjustedTarget = targetPos;

        // Add slight randomness
        adjustedTarget.x += ((float)(rand() % 100) - 50.0f) * 0.04f;
        adjustedTarget.z += ((float)(rand() % 100) - 50.0f) * 0.04f;

        m_pProjectileManager->SpawnProjectile(
            startPos,
            adjustedTarget,
            m_fDamagePerHit,
            m_fProjectileSpeed + (float)i * 2.0f,  // Stagger speeds
            0.5f,
            0.0f,
            ElementType::Fire,
            pOwner,
            false,
            1.0f
        );
    }
}
