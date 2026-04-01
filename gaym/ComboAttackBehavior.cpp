#include "stdafx.h"
#include "ComboAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "PlayerComponent.h"
#include "AnimationComponent.h"
#include "MathUtils.h"

ComboAttackBehavior::ComboAttackBehavior(const std::vector<ComboHit>& hits)
    : m_vHits(hits)
{
}

void ComboAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (m_vHits.empty())
    {
        m_bFinished = true;
        return;
    }

    if (pEnemy)
    {
        // Face target for first hit
        if (m_vHits[0].bTrackTarget)
        {
            pEnemy->FaceTarget();
        }

        AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
        if (pAnimComp)
        {
            pAnimComp->CrossFade(m_vHits[0].strAnimation, 0.1f, false);
        }
    }

    m_eHitPhase = HitPhase::Windup;
    OutputDebugString(L"[Combo] Attack started - hit 1 windup\n");
}

void ComboAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
{
    if (m_bFinished || m_vHits.empty()) return;

    m_fTimer += dt;

    const ComboHit& currentHit = m_vHits[m_nCurrentHit];

    switch (m_eHitPhase)
    {
    case HitPhase::Windup:
        if (m_fTimer >= currentHit.fWindupTime)
        {
            m_eHitPhase = HitPhase::Hit;
            m_fTimer = 0.0f;

            wchar_t buffer[64];
            swprintf_s(buffer, L"[Combo] Hit %d - hit phase\n", m_nCurrentHit + 1);
            OutputDebugString(buffer);
        }
        break;

    case HitPhase::Hit:
        if (!m_bHitDealt)
        {
            DealConeDamage(pEnemy, currentHit);
            m_bHitDealt = true;
        }

        if (m_fTimer >= currentHit.fHitTime)
        {
            m_eHitPhase = HitPhase::Recovery;
            m_fTimer = 0.0f;

            wchar_t buffer[64];
            swprintf_s(buffer, L"[Combo] Hit %d - recovery phase\n", m_nCurrentHit + 1);
            OutputDebugString(buffer);
        }
        break;

    case HitPhase::Recovery:
        if (m_fTimer >= currentHit.fRecoveryTime)
        {
            // Move to next hit or finish
            m_nCurrentHit++;
            m_bHitDealt = false;
            m_fTimer = 0.0f;

            if (m_nCurrentHit >= (int)m_vHits.size())
            {
                m_bFinished = true;
                OutputDebugString(L"[Combo] Attack finished\n");
            }
            else
            {
                m_eHitPhase = HitPhase::Windup;

                const ComboHit& nextHit = m_vHits[m_nCurrentHit];

                // Re-face target if specified
                if (nextHit.bTrackTarget && pEnemy)
                {
                    pEnemy->FaceTarget();
                }

                // Play next animation
                if (pEnemy)
                {
                    AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
                    if (pAnimComp)
                    {
                        pAnimComp->CrossFade(nextHit.strAnimation, 0.08f, false);
                    }
                }

                wchar_t buffer[64];
                swprintf_s(buffer, L"[Combo] Hit %d - windup phase\n", m_nCurrentHit + 1);
                OutputDebugString(buffer);
            }
        }
        break;
    }
}

bool ComboAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void ComboAttackBehavior::Reset()
{
    m_eHitPhase = HitPhase::Windup;
    m_nCurrentHit = 0;
    m_fTimer = 0.0f;
    m_bHitDealt = false;
    m_bFinished = false;
}

void ComboAttackBehavior::DealConeDamage(EnemyComponent* pEnemy, const ComboHit& hit)
{
    if (!pEnemy) return;

    GameObject* pOwner = pEnemy->GetOwner();
    GameObject* pTarget = pEnemy->GetTarget();
    if (!pOwner || !pTarget) return;

    // Check distance
    float distance = pEnemy->GetDistanceToTarget();
    if (distance > hit.fHitRange)
    {
        OutputDebugString(L"[Combo] Missed - target out of range\n");
        return;
    }

    // Check angle (cone attack)
    TransformComponent* pMyTransform = pOwner->GetTransform();
    TransformComponent* pTargetTransform = pTarget->GetTransform();
    if (!pMyTransform || !pTargetTransform) return;

    XMFLOAT3 myPos = pMyTransform->GetPosition();
    XMFLOAT3 targetPos = pTargetTransform->GetPosition();
    XMFLOAT3 myRot = pMyTransform->GetRotation();

    // Direction to target
    float dx = targetPos.x - myPos.x;
    float dz = targetPos.z - myPos.z;
    float len = sqrtf(dx * dx + dz * dz);
    if (len > 0.0f)
    {
        dx /= len;
        dz /= len;
    }

    // Boss facing direction
    float facingRad = XMConvertToRadians(myRot.y);
    float facingX = sinf(facingRad);
    float facingZ = cosf(facingRad);

    // Dot product to check angle
    float dot = dx * facingX + dz * facingZ;
    float halfConeRad = XMConvertToRadians(hit.fConeAngle * 0.5f);
    float cosHalfCone = cosf(halfConeRad);

    if (dot < cosHalfCone)
    {
        OutputDebugString(L"[Combo] Missed - target outside cone\n");
        return;
    }

    // Deal damage
    PlayerComponent* pPlayer = pTarget->GetComponent<PlayerComponent>();
    if (pPlayer)
    {
        pPlayer->TakeDamage(hit.fDamage);

        wchar_t buffer[128];
        swprintf_s(buffer, L"[Combo] Hit %d HIT! Dealt %.1f damage\n", m_nCurrentHit + 1, hit.fDamage);
        OutputDebugString(buffer);
    }
}

ComboAttackBehavior* ComboAttackBehavior::CreateLightCombo()
{
    std::vector<ComboHit> hits;

    ComboHit hit1;
    hit1.fDamage = 8.0f;
    hit1.fWindupTime = 0.15f;
    hit1.fHitTime = 0.1f;
    hit1.fRecoveryTime = 0.1f;
    hit1.fHitRange = 5.0f;
    hit1.fConeAngle = 90.0f;
    hit1.strAnimation = "Basic Attack";
    hit1.bTrackTarget = true;
    hits.push_back(hit1);

    ComboHit hit2;
    hit2.fDamage = 8.0f;
    hit2.fWindupTime = 0.1f;
    hit2.fHitTime = 0.1f;
    hit2.fRecoveryTime = 0.1f;
    hit2.fHitRange = 5.0f;
    hit2.fConeAngle = 90.0f;
    hit2.strAnimation = "Claw Attack";
    hit2.bTrackTarget = false;
    hits.push_back(hit2);

    ComboHit hit3;
    hit3.fDamage = 12.0f;
    hit3.fWindupTime = 0.15f;
    hit3.fHitTime = 0.15f;
    hit3.fRecoveryTime = 0.25f;
    hit3.fHitRange = 6.0f;
    hit3.fConeAngle = 120.0f;
    hit3.strAnimation = "Basic Attack";
    hit3.bTrackTarget = false;
    hits.push_back(hit3);

    return new ComboAttackBehavior(hits);
}

ComboAttackBehavior* ComboAttackBehavior::CreateHeavyCombo()
{
    std::vector<ComboHit> hits;

    ComboHit hit1;
    hit1.fDamage = 20.0f;
    hit1.fWindupTime = 0.4f;
    hit1.fHitTime = 0.2f;
    hit1.fRecoveryTime = 0.2f;
    hit1.fHitRange = 6.0f;
    hit1.fConeAngle = 120.0f;
    hit1.strAnimation = "Claw Attack";
    hit1.bTrackTarget = true;
    hits.push_back(hit1);

    ComboHit hit2;
    hit2.fDamage = 30.0f;
    hit2.fWindupTime = 0.5f;
    hit2.fHitTime = 0.25f;
    hit2.fRecoveryTime = 0.5f;
    hit2.fHitRange = 7.0f;
    hit2.fConeAngle = 150.0f;
    hit2.strAnimation = "Basic Attack";
    hit2.bTrackTarget = true;
    hits.push_back(hit2);

    return new ComboAttackBehavior(hits);
}

ComboAttackBehavior* ComboAttackBehavior::CreateFuryCombo()
{
    std::vector<ComboHit> hits;

    for (int i = 0; i < 5; i++)
    {
        ComboHit hit;
        hit.fDamage = 5.0f + (float)i;
        hit.fWindupTime = 0.08f;
        hit.fHitTime = 0.05f;
        hit.fRecoveryTime = 0.05f;
        hit.fHitRange = 4.5f;
        hit.fConeAngle = 70.0f;
        hit.strAnimation = (i % 2 == 0) ? "Basic Attack" : "Claw Attack";
        hit.bTrackTarget = (i == 0);
        hits.push_back(hit);
    }

    return new ComboAttackBehavior(hits);
}
