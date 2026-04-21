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
	ElementType eElement,
	const char* pClipOverride,
	bool bVariedProjectiles)
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
    , m_strClipOverride((pClipOverride && pClipOverride[0] != '\0') ? pClipOverride : nullptr)
    , m_bVariedProjectiles(bVariedProjectiles)
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
            }
        }
        break;

    case Phase::Recovery:
        if (m_fTimer >= m_fRecoveryTime)
        {
            m_bFinished = true;
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

    // ── Varied projectiles: 시작점·발사각·크기·속도·데미지를 랜덤화 ───────────────
    // rand() 는 [0, RAND_MAX] — [-1, 1] 로 정규화
    auto frand = []() { return (float)rand() / (float)RAND_MAX; };
    auto frandBi = [&frand]() { return frand() * 2.0f - 1.0f; };

    float angleJitter      = 0.0f;
    float pitchJitter      = 0.0f;
    float scaleMult        = 1.0f;
    float speedMult        = 1.0f;
    float damageMult       = 1.0f;
    float radiusMult       = 1.0f;
    float startPosJitterXZ = 0.0f;
    float startPosJitterY  = 0.0f;

    if (m_bVariedProjectiles)
    {
        // 크기는 bimodal 분포로 혼란스럽게 — 30% 확률로 "거대 덩어리", 나머지는 작은~중간 크기 랜덤
        if (frand() < 0.3f)
            scaleMult = 1.8f + frand() * 1.4f;      // 1.8 ~ 3.2 (거대)
        else
            scaleMult = 0.35f + frand() * 1.1f;     // 0.35 ~ 1.45 (작음~중간)

        angleJitter      = frandBi() * 15.0f;       // ±15° (8 → 15 더 산만)
        pitchJitter      = frandBi() * 0.25f;
        speedMult        = 0.45f + frand() * 1.6f;  // 0.45 ~ 2.05 (느림↔빠름 대비)
        damageMult       = 0.5f  + frand() * 1.0f;  // 0.5 ~ 1.5
        radiusMult       = 0.6f  + frand() * 0.8f;
        startPosJitterXZ = frandBi() * 2.2f;
        startPosJitterY  = frandBi() * 1.3f;
    }

    // 시작 위치 변위 (yaw 기준 측면 방향)
    if (startPosJitterXZ != 0.0f)
    {
        startPos.x += cosf(yawRad) * startPosJitterXZ;  // 옆으로
        startPos.z -= sinf(yawRad) * startPosJitterXZ;
    }
    startPos.y += startPosJitterY;

    // Calculate direction and apply angle offset
    XMFLOAT3 dir;
    dir.x = targetPos.x - startPos.x;
    dir.y = targetPos.y - startPos.y + pitchJitter;
    dir.z = targetPos.z - startPos.z;

    // Normalize direction
    float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (len > 0.0f)
    {
        dir.x /= len;
        dir.y /= len;
        dir.z /= len;
    }

    // Apply horizontal angle offset (+ jitter)
    float offsetRad = XMConvertToRadians(angleOffset + angleJitter);
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
        m_fDamagePerHit * damageMult,
        m_fProjectileSpeed * speedMult,
        m_fProjectileRadius * radiusMult,
        0.0f,
        m_eElement,
        pOwner,
        false,
        m_fProjectileScale * scaleMult
    );
}
