#include "stdafx.h"
#include "FlyingCircleAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ProjectileManager.h"
#include "AnimationComponent.h"

FlyingCircleAttackBehavior::FlyingCircleAttackBehavior(ProjectileManager* pProjectileManager,
                                                       float fDamagePerHit,
                                                       float fProjectileSpeed,
                                                       float fCircleRadius,
                                                       float fAngularSpeed,
                                                       float fTotalRotation,
                                                       float fFireInterval,
                                                       int nProjectilesPerShot,
                                                       float fFlyHeight,
                                                       float fTakeOffDuration,
                                                       float fLandingDuration)
    : m_pProjectileManager(pProjectileManager)
    , m_fDamagePerHit(fDamagePerHit)
    , m_fProjectileSpeed(fProjectileSpeed)
    , m_fCircleRadius(fCircleRadius)
    , m_fAngularSpeed(fAngularSpeed)
    , m_fTotalRotation(fTotalRotation)
    , m_fFireInterval(fFireInterval)
    , m_nProjectilesPerShot(nProjectilesPerShot)
    , m_fFlyHeight(fFlyHeight)
    , m_fTakeOffDuration(fTakeOffDuration)
    , m_fLandingDuration(fLandingDuration)
{
}

void FlyingCircleAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (pEnemy)
    {
        GameObject* pOwner = pEnemy->GetOwner();
        GameObject* pTarget = pEnemy->GetTarget();

        if (pOwner && pOwner->GetTransform())
        {
            XMFLOAT3 pos = pOwner->GetTransform()->GetPosition();
            m_fOriginalY = pos.y;
            m_xmf3StartPosition = pos;
        }

        // Set circle center to player position
        if (pTarget && pTarget->GetTransform())
        {
            m_xmf3CircleCenter = pTarget->GetTransform()->GetPosition();
        }

        // Calculate starting angle based on boss position relative to center
        if (pOwner && pOwner->GetTransform())
        {
            XMFLOAT3 pos = pOwner->GetTransform()->GetPosition();
            float dx = pos.x - m_xmf3CircleCenter.x;
            float dz = pos.z - m_xmf3CircleCenter.z;
            m_fCurrentAngle = XMConvertToDegrees(atan2f(dx, dz));
        }

        pEnemy->SetInvincible(true);

        AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
        if (pAnimComp)
        {
            pAnimComp->CrossFade("Take Off", 0.15f, false);
        }
    }

    m_ePhase = Phase::TakeOff;
    OutputDebugString(L"[FlyingCircle] Starting TakeOff phase\n");
}

void FlyingCircleAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
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
                m_ePhase = Phase::MoveToOrbit;
                m_fTimer = 0.0f;

                OutputDebugString(L"[FlyingCircle] Moving to orbit position\n");
            }
        }
        break;

    case Phase::MoveToOrbit:
        {
            float t = m_fTimer / m_fMoveToOrbitDuration;
            if (t > 1.0f) t = 1.0f;

            // Calculate target orbit position
            float angleRad = XMConvertToRadians(m_fCurrentAngle);
            XMFLOAT3 orbitPos;
            orbitPos.x = m_xmf3CircleCenter.x + sinf(angleRad) * m_fCircleRadius;
            orbitPos.y = m_fOriginalY + m_fFlyHeight;
            orbitPos.z = m_xmf3CircleCenter.z + cosf(angleRad) * m_fCircleRadius;

            // Lerp from current to orbit position
            XMFLOAT3 currentPos = pTransform->GetPosition();
            currentPos.x = m_xmf3StartPosition.x + (orbitPos.x - m_xmf3StartPosition.x) * t;
            currentPos.z = m_xmf3StartPosition.z + (orbitPos.z - m_xmf3StartPosition.z) * t;
            currentPos.y = orbitPos.y;
            pTransform->SetPosition(currentPos);

            if (m_fTimer >= m_fMoveToOrbitDuration)
            {
                m_ePhase = Phase::Circle;
                m_fTimer = 0.0f;
                m_fNextFireTime = 0.0f;
                m_fRotationCompleted = 0.0f;

                AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
                if (pAnimComp)
                {
                    pAnimComp->CrossFade("Fly Glide", 0.2f, true);
                }

                OutputDebugString(L"[FlyingCircle] Circle phase started!\n");
            }
        }
        break;

    case Phase::Circle:
        {
            // Rotate around center
            float rotationThisFrame = m_fAngularSpeed * dt;
            m_fCurrentAngle += rotationThisFrame;
            m_fRotationCompleted += rotationThisFrame;

            // Update position on circle
            float angleRad = XMConvertToRadians(m_fCurrentAngle);
            XMFLOAT3 pos;
            pos.x = m_xmf3CircleCenter.x + sinf(angleRad) * m_fCircleRadius;
            pos.y = m_fOriginalY + m_fFlyHeight;
            pos.z = m_xmf3CircleCenter.z + cosf(angleRad) * m_fCircleRadius;
            pTransform->SetPosition(pos);

            // Face center (toward player)
            float facingAngle = m_fCurrentAngle + 180.0f;
            pTransform->SetRotation(XMFLOAT3(0.0f, facingAngle, 0.0f));

            // Fire toward center
            if (m_fTimer >= m_fNextFireTime)
            {
                FireInward(pEnemy);
                m_fNextFireTime = m_fTimer + m_fFireInterval;
            }

            // Check if rotation complete
            if (m_fRotationCompleted >= m_fTotalRotation)
            {
                m_ePhase = Phase::Landing;
                m_fTimer = 0.0f;
                m_xmf3StartPosition = pos;  // Remember current position for landing

                AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
                if (pAnimComp)
                {
                    pAnimComp->CrossFade("Land", 0.15f, false);
                }

                OutputDebugString(L"[FlyingCircle] Landing phase started\n");
            }
        }
        break;

    case Phase::Landing:
        {
            float t = m_fTimer / m_fLandingDuration;
            if (t > 1.0f) t = 1.0f;

            float easeT = t * t;
            float newY = (m_fOriginalY + m_fFlyHeight) - m_fFlyHeight * easeT;

            XMFLOAT3 pos = pTransform->GetPosition();
            pos.y = newY;
            pTransform->SetPosition(pos);

            if (m_fTimer >= m_fLandingDuration)
            {
                pos.y = m_fOriginalY;
                pTransform->SetPosition(pos);

                pEnemy->SetInvincible(false);
                m_bFinished = true;
                OutputDebugString(L"[FlyingCircle] Attack finished\n");
            }
        }
        break;
    }
}

bool FlyingCircleAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void FlyingCircleAttackBehavior::Reset()
{
    m_ePhase = Phase::TakeOff;
    m_fTimer = 0.0f;
    m_fOriginalY = 0.0f;
    m_fCurrentAngle = 0.0f;
    m_fRotationCompleted = 0.0f;
    m_fNextFireTime = 0.0f;
    m_xmf3CircleCenter = { 0.0f, 0.0f, 0.0f };
    m_xmf3StartPosition = { 0.0f, 0.0f, 0.0f };
    m_bFinished = false;
}

void FlyingCircleAttackBehavior::FireInward(EnemyComponent* pEnemy)
{
    if (!pEnemy || !m_pProjectileManager) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    XMFLOAT3 startPos = pTransform->GetPosition();
    startPos.y -= 1.0f;

    // Direction toward center
    XMFLOAT3 dir;
    dir.x = m_xmf3CircleCenter.x - startPos.x;
    dir.y = m_xmf3CircleCenter.y - startPos.y;
    dir.z = m_xmf3CircleCenter.z - startPos.z;

    float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 0.0f)
    {
        dir.x /= len;
        dir.y /= len;
        dir.z /= len;
    }

    // Fire multiple projectiles with slight spread
    float spreadAngle = 20.0f;
    float angleStep = spreadAngle / (float)(m_nProjectilesPerShot - 1);
    float startAngle = -spreadAngle * 0.5f;

    for (int i = 0; i < m_nProjectilesPerShot; i++)
    {
        float offsetAngle = startAngle + (float)i * angleStep;
        float offsetRad = XMConvertToRadians(offsetAngle);

        // Rotate around Y axis
        float newX = dir.x * cosf(offsetRad) - dir.z * sinf(offsetRad);
        float newZ = dir.x * sinf(offsetRad) + dir.z * cosf(offsetRad);

        XMFLOAT3 fireDir = { newX, dir.y - 0.15f, newZ };

        // Normalize
        len = sqrtf(fireDir.x * fireDir.x + fireDir.y * fireDir.y + fireDir.z * fireDir.z);
        if (len > 0.0f)
        {
            fireDir.x /= len;
            fireDir.y /= len;
            fireDir.z /= len;
        }

        XMFLOAT3 targetPos;
        targetPos.x = startPos.x + fireDir.x * 50.0f;
        targetPos.y = startPos.y + fireDir.y * 50.0f;
        targetPos.z = startPos.z + fireDir.z * 50.0f;

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
