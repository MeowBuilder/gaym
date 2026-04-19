#include "stdafx.h"
#include "FlyingBarrageAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ProjectileManager.h"
#include "AnimationComponent.h"

FlyingBarrageAttackBehavior::FlyingBarrageAttackBehavior(ProjectileManager* pProjectileManager,
                                                           float fDamagePerHit,
                                                           float fProjectileSpeed,
                                                           int nProjectilesPerWave,
                                                           int nTotalWaves,
                                                           float fWaveInterval,
                                                           float fFlyHeight,
                                                           float fTakeOffDuration,
                                                           float fLandingDuration)
    : m_pProjectileManager(pProjectileManager)
    , m_fDamagePerHit(fDamagePerHit)
    , m_fProjectileSpeed(fProjectileSpeed)
    , m_nProjectilesPerWave(nProjectilesPerWave)
    , m_nTotalWaves(nTotalWaves)
    , m_fWaveInterval(fWaveInterval)
    , m_fFlyHeight(fFlyHeight)
    , m_fTakeOffDuration(fTakeOffDuration)
    , m_fLandingDuration(fLandingDuration)
{
}

void FlyingBarrageAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (pEnemy)
    {
        // Store original ground position
        GameObject* pOwner = pEnemy->GetOwner();
        if (pOwner && pOwner->GetTransform())
        {
            m_fOriginalY = pOwner->GetTransform()->GetPosition().y;
        }

        // Make boss invincible during flight
        pEnemy->SetInvincible(true);

        // Play takeoff animation
        AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
        if (pAnimComp)
        {
            pAnimComp->CrossFade("Take Off", 0.15f, false);
        }
    }

    m_ePhase = Phase::TakeOff;
    OutputDebugString(L"[FlyingBarrage] Starting TakeOff phase - Boss is now INVINCIBLE!\n");
}

void FlyingBarrageAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
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
            // Smoothly rise to fly height
            float t = m_fTimer / m_fTakeOffDuration;
            if (t > 1.0f) t = 1.0f;

            // Ease-out curve for smooth takeoff
            float easeT = 1.0f - (1.0f - t) * (1.0f - t);
            float newY = m_fOriginalY + m_fFlyHeight * easeT;

            XMFLOAT3 pos = pTransform->GetPosition();
            pos.y = newY;
            pTransform->SetPosition(pos);

            if (m_fTimer >= m_fTakeOffDuration)
            {
                m_ePhase = Phase::Barrage;
                m_fTimer = 0.0f;
                m_nWavesFired = 0;
                m_fNextWaveTime = 0.0f;
                m_fSpiralAngle = 0.0f;

                // Play flying/glide animation (looping)
                AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
                if (pAnimComp)
                {
                    pAnimComp->CrossFade("Fly Glide", 0.2f, true);
                }
            }
        }
        break;

    case Phase::Barrage:
        {
            // Fire single rings with time delay between each
            while (m_fTimer >= m_fNextWaveTime && m_nWavesFired < m_nTotalWaves)
            {
                // Fire one ring at a time with rotation
                FireCircularWave(pEnemy, m_fSpiralAngle);

                // Rotate for next ring
                m_fSpiralAngle += 7.5f;

                m_nWavesFired++;
                m_fNextWaveTime += m_fWaveInterval;
            }

            // Check if barrage is complete
            if (m_nWavesFired >= m_nTotalWaves)
            {
                m_ePhase = Phase::Landing;
                m_fTimer = 0.0f;

                // Play landing animation
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
            // Smoothly descend to ground
            float t = m_fTimer / m_fLandingDuration;
            if (t > 1.0f) t = 1.0f;

            // Ease-in curve for smooth landing
            float easeT = t * t;
            float newY = m_fOriginalY + m_fFlyHeight * (1.0f - easeT);

            XMFLOAT3 pos = pTransform->GetPosition();
            pos.y = newY;
            pTransform->SetPosition(pos);

            if (m_fTimer >= m_fLandingDuration)
            {
                // Ensure we're exactly at ground level
                pos.y = m_fOriginalY;
                pTransform->SetPosition(pos);

                // Remove invincibility
                pEnemy->SetInvincible(false);
                m_bFinished = true;
                OutputDebugString(L"[FlyingBarrage] Attack finished - Boss is VULNERABLE again!\n");
            }
        }
        break;
    }
}

bool FlyingBarrageAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void FlyingBarrageAttackBehavior::Reset()
{
    m_ePhase = Phase::TakeOff;
    m_fTimer = 0.0f;
    m_fOriginalY = 0.0f;
    m_nWavesFired = 0;
    m_fNextWaveTime = 0.0f;
    m_fSpiralAngle = 0.0f;
    m_bFinished = false;
}

void FlyingBarrageAttackBehavior::FireCircularWave(EnemyComponent* pEnemy, float fAngleOffset)
{
    if (!pEnemy || !m_pProjectileManager) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    XMFLOAT3 startPos = pTransform->GetPosition();
    // Fire from slightly below the dragon
    startPos.y -= 2.0f;

    // Fire projectiles in a full circle
    float angleStep = 360.0f / (float)m_nProjectilesPerWave;

    for (int i = 0; i < m_nProjectilesPerWave; i++)
    {
        float angle = fAngleOffset + (float)i * angleStep;
        float angleRad = XMConvertToRadians(angle);

        // Calculate direction (horizontal, slightly downward)
        XMFLOAT3 dir;
        dir.x = sinf(angleRad);
        dir.y = -0.3f;  // Slight downward angle
        dir.z = cosf(angleRad);

        // Normalize
        float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        dir.x /= len;
        dir.y /= len;
        dir.z /= len;

        // Calculate target position
        XMFLOAT3 targetPos;
        targetPos.x = startPos.x + dir.x * 50.0f;
        targetPos.y = startPos.y + dir.y * 50.0f;
        targetPos.z = startPos.z + dir.z * 50.0f;

        m_pProjectileManager->SpawnProjectile(
            startPos,
            targetPos,
            m_fDamagePerHit,
            m_fProjectileSpeed,
            0.6f,           // radius
            0.0f,           // no explosion
            ElementType::Fire,
            pOwner,
            false,          // isPlayerProjectile = false
            1.2f            // scale
        );
    }
}

void FlyingBarrageAttackBehavior::FireSpiralWave(EnemyComponent* pEnemy, float fBaseAngle)
{
    if (!pEnemy || !m_pProjectileManager) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    XMFLOAT3 startPos = pTransform->GetPosition();
    startPos.y -= 2.0f;

    // Fire fewer projectiles but in a tighter spiral pattern
    int nSpiralProjectiles = m_nProjectilesPerWave / 2;
    float angleStep = 180.0f / (float)nSpiralProjectiles;

    for (int i = 0; i < nSpiralProjectiles; i++)
    {
        float angle = fBaseAngle + (float)i * angleStep;
        float angleRad = XMConvertToRadians(angle);

        // Calculate direction with varying downward angle for spiral effect
        float downAngle = -0.2f - (float)i * 0.05f;

        XMFLOAT3 dir;
        dir.x = sinf(angleRad);
        dir.y = downAngle;
        dir.z = cosf(angleRad);

        // Normalize
        float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
        dir.x /= len;
        dir.y /= len;
        dir.z /= len;

        XMFLOAT3 targetPos;
        targetPos.x = startPos.x + dir.x * 50.0f;
        targetPos.y = startPos.y + dir.y * 50.0f;
        targetPos.z = startPos.z + dir.z * 50.0f;

        m_pProjectileManager->SpawnProjectile(
            startPos,
            targetPos,
            m_fDamagePerHit,
            m_fProjectileSpeed * 0.8f,  // Slightly slower for spiral
            0.5f,
            0.0f,
            ElementType::Fire,
            pOwner,
            false,
            1.0f
        );
    }
}

void FlyingBarrageAttackBehavior::FireCrossWave(EnemyComponent* pEnemy, float fAngleOffset)
{
    if (!pEnemy || !m_pProjectileManager) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    XMFLOAT3 startPos = pTransform->GetPosition();
    startPos.y -= 2.0f;

    // Fire in 8 directions (cross + X pattern) with multiple projectiles per arm
    const int nArms = 8;
    const int nProjectilesPerArm = 4;

    for (int arm = 0; arm < nArms; arm++)
    {
        float baseAngle = fAngleOffset + (float)arm * 45.0f;

        for (int i = 0; i < nProjectilesPerArm; i++)
        {
            float angleRad = XMConvertToRadians(baseAngle);

            // Vary speed for each projectile in the arm (creates trailing effect)
            float speedMult = 0.6f + (float)i * 0.2f;

            XMFLOAT3 dir;
            dir.x = sinf(angleRad);
            dir.y = -0.25f;
            dir.z = cosf(angleRad);

            float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
            dir.x /= len;
            dir.y /= len;
            dir.z /= len;

            XMFLOAT3 targetPos;
            targetPos.x = startPos.x + dir.x * 50.0f;
            targetPos.y = startPos.y + dir.y * 50.0f;
            targetPos.z = startPos.z + dir.z * 50.0f;

            m_pProjectileManager->SpawnProjectile(
                startPos,
                targetPos,
                m_fDamagePerHit,
                m_fProjectileSpeed * speedMult,
                0.7f,
                0.0f,
                ElementType::Fire,
                pOwner,
                false,
                1.3f
            );
        }
    }
}

void FlyingBarrageAttackBehavior::FireAimedWave(EnemyComponent* pEnemy, int nProjectiles)
{
    if (!pEnemy || !m_pProjectileManager) return;

    GameObject* pOwner = pEnemy->GetOwner();
    GameObject* pTarget = pEnemy->GetTarget();
    if (!pOwner || !pTarget) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    TransformComponent* pTargetTransform = pTarget->GetTransform();
    if (!pTransform || !pTargetTransform) return;

    XMFLOAT3 startPos = pTransform->GetPosition();
    startPos.y -= 2.0f;

    XMFLOAT3 targetPos = pTargetTransform->GetPosition();
    targetPos.y += 1.0f;

    // Calculate base direction to player
    XMFLOAT3 baseDir;
    baseDir.x = targetPos.x - startPos.x;
    baseDir.y = targetPos.y - startPos.y;
    baseDir.z = targetPos.z - startPos.z;

    float len = sqrtf(baseDir.x * baseDir.x + baseDir.y * baseDir.y + baseDir.z * baseDir.z);
    if (len > 0.0f)
    {
        baseDir.x /= len;
        baseDir.y /= len;
        baseDir.z /= len;
    }

    // Fire spread of projectiles aimed at player
    float spreadAngle = 60.0f;
    float angleStep = spreadAngle / (float)(nProjectiles - 1);
    float startAngle = -spreadAngle * 0.5f;

    for (int i = 0; i < nProjectiles; i++)
    {
        float offsetAngle = startAngle + (float)i * angleStep;
        float offsetRad = XMConvertToRadians(offsetAngle);

        // Rotate direction around Y axis
        float newX = baseDir.x * cosf(offsetRad) - baseDir.z * sinf(offsetRad);
        float newZ = baseDir.x * sinf(offsetRad) + baseDir.z * cosf(offsetRad);

        XMFLOAT3 dir;
        dir.x = newX;
        dir.y = baseDir.y;
        dir.z = newZ;

        XMFLOAT3 fireTarget;
        fireTarget.x = startPos.x + dir.x * 60.0f;
        fireTarget.y = startPos.y + dir.y * 60.0f;
        fireTarget.z = startPos.z + dir.z * 60.0f;

        m_pProjectileManager->SpawnProjectile(
            startPos,
            fireTarget,
            m_fDamagePerHit * 1.2f,  // Aimed shots do more damage
            m_fProjectileSpeed * 1.3f,  // Faster
            0.5f,
            0.0f,
            ElementType::Fire,
            pOwner,
            false,
            1.0f
        );
    }
}

void FlyingBarrageAttackBehavior::FireRainWave(EnemyComponent* pEnemy, int nProjectiles)
{
    if (!pEnemy || !m_pProjectileManager) return;

    GameObject* pOwner = pEnemy->GetOwner();
    GameObject* pTarget = pEnemy->GetTarget();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    XMFLOAT3 bossPos = pTransform->GetPosition();

    // Get target position for rain center
    XMFLOAT3 rainCenter = bossPos;
    if (pTarget)
    {
        TransformComponent* pTargetTransform = pTarget->GetTransform();
        if (pTargetTransform)
        {
            rainCenter = pTargetTransform->GetPosition();
        }
    }

    // Spawn projectiles raining down in area around target
    float rainRadius = 15.0f;

    for (int i = 0; i < nProjectiles; i++)
    {
        // Random position in circle around target
        float angle = (float)(rand() % 360);
        float radius = (float)(rand() % 100) / 100.0f * rainRadius;
        float angleRad = XMConvertToRadians(angle);

        XMFLOAT3 startPos;
        startPos.x = rainCenter.x + cosf(angleRad) * radius;
        startPos.y = bossPos.y + 5.0f;  // Start above boss
        startPos.z = rainCenter.z + sinf(angleRad) * radius;

        XMFLOAT3 targetPos;
        targetPos.x = startPos.x + ((float)(rand() % 100) - 50.0f) * 0.1f;  // Slight scatter
        targetPos.y = 0.0f;  // Hit ground
        targetPos.z = startPos.z + ((float)(rand() % 100) - 50.0f) * 0.1f;

        m_pProjectileManager->SpawnProjectile(
            startPos,
            targetPos,
            m_fDamagePerHit,
            m_fProjectileSpeed * 0.7f,  // Slower falling rain
            0.6f,
            0.0f,
            ElementType::Fire,
            pOwner,
            false,
            1.1f
        );
    }
}
