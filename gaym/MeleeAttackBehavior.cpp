#include "stdafx.h"
#include "MeleeAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "PlayerComponent.h"

MeleeAttackBehavior::MeleeAttackBehavior(float fDamage, float fWindupTime, float fHitTime, float fRecoveryTime)
    : m_fDamage(fDamage)
    , m_fWindupTime(fWindupTime)
    , m_fHitTime(fHitTime)
    , m_fRecoveryTime(fRecoveryTime)
{
    m_fTotalDuration = m_fWindupTime + m_fHitTime + m_fRecoveryTime;
}

void MeleeAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    OutputDebugString(L"[MeleeAttack] Attack started - windup phase\n");

    // Face target at start of attack
    if (pEnemy)
    {
        pEnemy->FaceTarget();
    }
}

void MeleeAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
{
    if (m_bFinished) return;

    m_fTimer += dt;

    // Check phases
    if (m_fTimer < m_fWindupTime)
    {
        // Windup phase - could play animation here
    }
    else if (m_fTimer < m_fWindupTime + m_fHitTime)
    {
        // Hit phase - deal damage once
        if (!m_bHitDealt)
        {
            DealDamage(pEnemy);
            m_bHitDealt = true;
        }
    }
    else if (m_fTimer >= m_fTotalDuration)
    {
        // Attack finished
        m_bFinished = true;
        OutputDebugString(L"[MeleeAttack] Attack finished\n");
    }
}

bool MeleeAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void MeleeAttackBehavior::Reset()
{
    m_fTimer = 0.0f;
    m_bHitDealt = false;
    m_bFinished = false;
    m_fTotalDuration = m_fWindupTime + m_fHitTime + m_fRecoveryTime;
}

void MeleeAttackBehavior::DealDamage(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pTarget = pEnemy->GetTarget();
    if (!pTarget) return;

    // Check if target is in range
    float distance = pEnemy->GetDistanceToTarget();
    if (distance > m_fHitRange)
    {
        OutputDebugString(L"[MeleeAttack] Attack missed - target out of range\n");
        return;
    }

    // Deal damage to player
    // For now, just output debug message since we don't have player health yet
    wchar_t buffer[128];
    swprintf_s(buffer, L"[MeleeAttack] HIT! Dealing %.1f damage to player\n", m_fDamage);
    OutputDebugString(buffer);

    // TODO: Add player health system and deal actual damage
    // PlayerComponent* pPlayer = pTarget->GetComponent<PlayerComponent>();
    // if (pPlayer) pPlayer->TakeDamage(m_fDamage);
}
