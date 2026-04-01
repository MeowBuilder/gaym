#include "stdafx.h"
#include "JumpSlamAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "PlayerComponent.h"
#include "AnimationComponent.h"

JumpSlamAttackBehavior::JumpSlamAttackBehavior(float fDamage,
                                               float fJumpHeight,
                                               float fJumpDuration,
                                               float fSlamRadius,
                                               float fWindupTime,
                                               float fRecoveryTime,
                                               bool bTrackTarget)
    : m_fDamage(fDamage)
    , m_fJumpHeight(fJumpHeight)
    , m_fJumpDuration(fJumpDuration)
    , m_fSlamRadius(fSlamRadius)
    , m_fWindupTime(fWindupTime)
    , m_fRecoveryTime(fRecoveryTime)
    , m_bTrackTarget(bTrackTarget)
{
}

void JumpSlamAttackBehavior::Execute(EnemyComponent* pEnemy)
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

        // Set target position
        if (m_bTrackTarget)
        {
            GameObject* pTarget = pEnemy->GetTarget();
            if (pTarget && pTarget->GetTransform())
            {
                m_xmf3TargetPosition = pTarget->GetTransform()->GetPosition();
                m_xmf3TargetPosition.y = m_fOriginalY;  // Keep same Y level
            }
            else
            {
                m_xmf3TargetPosition = m_xmf3StartPosition;
            }
        }
        else
        {
            m_xmf3TargetPosition = m_xmf3StartPosition;
        }

        // Face target before jumping
        pEnemy->FaceTarget();

        AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
        if (pAnimComp)
        {
            pAnimComp->CrossFade("Take Off", 0.1f, false);
        }
    }

    m_ePhase = Phase::Windup;
    OutputDebugString(L"[JumpSlam] Attack started - windup phase\n");
}

void JumpSlamAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
{
    if (m_bFinished || !pEnemy) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    m_fTimer += dt;

    switch (m_ePhase)
    {
    case Phase::Windup:
        if (m_fTimer >= m_fWindupTime)
        {
            m_ePhase = Phase::Jump;
            m_fTimer = 0.0f;
            OutputDebugString(L"[JumpSlam] Jump phase\n");
        }
        break;

    case Phase::Jump:
        {
            float t = m_fTimer / m_fJumpDuration;
            if (t > 1.0f) t = 1.0f;

            // Parabolic jump arc
            float heightT = 4.0f * t * (1.0f - t);  // Peaks at t=0.5
            float newY = m_fOriginalY + m_fJumpHeight * heightT;

            // Lerp horizontal position
            XMFLOAT3 pos;
            pos.x = m_xmf3StartPosition.x + (m_xmf3TargetPosition.x - m_xmf3StartPosition.x) * t;
            pos.y = newY;
            pos.z = m_xmf3StartPosition.z + (m_xmf3TargetPosition.z - m_xmf3StartPosition.z) * t;
            pTransform->SetPosition(pos);

            if (m_fTimer >= m_fJumpDuration)
            {
                m_ePhase = Phase::Slam;
                m_fTimer = 0.0f;

                // Snap to ground at target
                pos.y = m_fOriginalY;
                pTransform->SetPosition(pos);

                AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
                if (pAnimComp)
                {
                    pAnimComp->CrossFade("Land", 0.05f, false);
                }

                OutputDebugString(L"[JumpSlam] Slam phase!\n");
            }
        }
        break;

    case Phase::Slam:
        {
            // Deal damage immediately on slam
            if (!m_bSlamDealt)
            {
                DealSlamDamage(pEnemy);
                m_bSlamDealt = true;
            }

            // Short slam animation
            if (m_fTimer >= 0.2f)
            {
                m_ePhase = Phase::Recovery;
                m_fTimer = 0.0f;
                OutputDebugString(L"[JumpSlam] Recovery phase\n");
            }
        }
        break;

    case Phase::Recovery:
        if (m_fTimer >= m_fRecoveryTime)
        {
            m_bFinished = true;
            OutputDebugString(L"[JumpSlam] Attack finished\n");
        }
        break;
    }
}

bool JumpSlamAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void JumpSlamAttackBehavior::Reset()
{
    m_ePhase = Phase::Windup;
    m_fTimer = 0.0f;
    m_fOriginalY = 0.0f;
    m_xmf3StartPosition = { 0.0f, 0.0f, 0.0f };
    m_xmf3TargetPosition = { 0.0f, 0.0f, 0.0f };
    m_bSlamDealt = false;
    m_bFinished = false;
}

void JumpSlamAttackBehavior::DealSlamDamage(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pTarget = pEnemy->GetTarget();
    if (!pTarget) return;

    // Check distance from slam point to target
    float distance = pEnemy->GetDistanceToTarget();

    if (distance > m_fSlamRadius)
    {
        OutputDebugString(L"[JumpSlam] Missed - target out of AoE range\n");
        return;
    }

    // Deal damage to player
    PlayerComponent* pPlayer = pTarget->GetComponent<PlayerComponent>();
    if (pPlayer)
    {
        // Damage scales with proximity (closer = more damage)
        float damageMultiplier = 1.0f - (distance / m_fSlamRadius) * 0.5f;  // 50% - 100% damage
        float actualDamage = m_fDamage * damageMultiplier;

        pPlayer->TakeDamage(actualDamage);

        wchar_t buffer[128];
        swprintf_s(buffer, L"[JumpSlam] HIT! Dealt %.1f damage (%.0f%% of max)\n",
            actualDamage, damageMultiplier * 100.0f);
        OutputDebugString(buffer);
    }
}
