#include "stdafx.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "IAttackBehavior.h"
#include "AnimationComponent.h"
#include "Room.h"
#include "MathUtils.h"

EnemyComponent::EnemyComponent(GameObject* pOwner)
    : Component(pOwner)
{
}

EnemyComponent::~EnemyComponent()
{
}

void EnemyComponent::Update(float deltaTime)
{
    // Update cooldown timer
    if (m_fAttackCooldownTimer > 0.0f)
    {
        m_fAttackCooldownTimer -= deltaTime;
    }

    // State machine
    switch (m_eCurrentState)
    {
    case EnemyState::Idle:
        UpdateIdle(deltaTime);
        break;
    case EnemyState::Chase:
        UpdateChase(deltaTime);
        break;
    case EnemyState::Attack:
        UpdateAttack(deltaTime);
        break;
    case EnemyState::Stagger:
        UpdateStagger(deltaTime);
        break;
    case EnemyState::Dead:
        UpdateDead(deltaTime);
        break;
    }
}

void EnemyComponent::ChangeState(EnemyState newState)
{
    if (m_eCurrentState == newState) return;
    if (m_eCurrentState == EnemyState::Dead) return; // Can't change state if dead

    EnemyState oldState = m_eCurrentState;
    m_eCurrentState = newState;

    // Hide indicators when leaving Attack state
    if (oldState == EnemyState::Attack)
    {
        HideIndicators();
    }

    // Debug output
    const wchar_t* stateNames[] = { L"Idle", L"Chase", L"Attack", L"Stagger", L"Dead" };
    wchar_t buffer[128];
    swprintf_s(buffer, L"[Enemy] State changed: %s -> %s\n",
        stateNames[static_cast<int>(oldState)],
        stateNames[static_cast<int>(newState)]);
    OutputDebugString(buffer);

    // Animation transition
    if (m_pAnimationComp)
    {
        switch (newState)
        {
        case EnemyState::Idle:
            m_pAnimationComp->CrossFade(m_AnimConfig.m_strIdleClip, 0.2f, m_AnimConfig.m_bLoopIdle);
            break;
        case EnemyState::Chase:
            m_pAnimationComp->CrossFade(m_AnimConfig.m_strChaseClip, 0.2f, m_AnimConfig.m_bLoopChase);
            break;
        case EnemyState::Attack:
            m_pAnimationComp->CrossFade(m_AnimConfig.m_strAttackClip, 0.15f, m_AnimConfig.m_bLoopAttack);
            break;
        case EnemyState::Stagger:
            m_pAnimationComp->CrossFade(m_AnimConfig.m_strStaggerClip, 0.1f, m_AnimConfig.m_bLoopStagger);
            break;
        case EnemyState::Dead:
            m_pAnimationComp->CrossFade(m_AnimConfig.m_strDeathClip, 0.1f, m_AnimConfig.m_bLoopDeath);
            break;
        }
    }

    // State entry actions
    switch (newState)
    {
    case EnemyState::Stagger:
        m_fStaggerTimer = STAGGER_DURATION;
        break;
    case EnemyState::Dead:
        m_fDeadTimer = DEAD_LINGER_TIME;
        Die();
        break;
    case EnemyState::Attack:
        if (m_pAttackBehavior)
        {
            m_pAttackBehavior->Execute(this);
        }
        ShowIndicators();
        break;
    }
}

void EnemyComponent::TakeDamage(float fDamage)
{
    if (m_eCurrentState == EnemyState::Dead) return;

    m_Stats.m_fCurrentHP -= fDamage;

    wchar_t buffer[128];
    swprintf_s(buffer, L"[Enemy] Took %.1f damage, HP: %.1f/%.1f\n",
        fDamage, m_Stats.m_fCurrentHP, m_Stats.m_fMaxHP);
    OutputDebugString(buffer);

    if (m_Stats.m_fCurrentHP <= 0.0f)
    {
        m_Stats.m_fCurrentHP = 0.0f;
        ChangeState(EnemyState::Dead);
    }
    else
    {
        // Enter stagger state on damage
        ChangeState(EnemyState::Stagger);
    }
}

void EnemyComponent::SetAttackBehavior(std::unique_ptr<IAttackBehavior> pBehavior)
{
    m_pAttackBehavior = std::move(pBehavior);
}

float EnemyComponent::GetDistanceToTarget() const
{
    if (!m_pTarget || !m_pOwner) return FLT_MAX;

    TransformComponent* pMyTransform = m_pOwner->GetTransform();
    TransformComponent* pTargetTransform = m_pTarget->GetTransform();

    if (!pMyTransform || !pTargetTransform) return FLT_MAX;

    return MathUtils::Distance2D(pMyTransform->GetPosition(), pTargetTransform->GetPosition());
}

void EnemyComponent::FaceTarget()
{
    if (!m_pTarget || !m_pOwner) return;

    TransformComponent* pMyTransform = m_pOwner->GetTransform();
    TransformComponent* pTargetTransform = m_pTarget->GetTransform();

    if (!pMyTransform || !pTargetTransform) return;

    XMFLOAT3 myPos = pMyTransform->GetPosition();
    XMFLOAT3 targetPos = pTargetTransform->GetPosition();

    // Get normalized direction to target on XZ plane
    XMFLOAT2 dir = MathUtils::Direction2D(myPos, targetPos);
    if (dir.x == 0.0f && dir.y == 0.0f) return;

    // Calculate yaw angle
    float yawRad = atan2f(dir.x, dir.y);
    float yawDeg = XMConvertToDegrees(yawRad);

    // Set rotation (only yaw)
    const XMFLOAT3& currentRot = pMyTransform->GetRotation();
    pMyTransform->SetRotation(currentRot.x, yawDeg, currentRot.z);
}

void EnemyComponent::MoveTowardsTarget(float dt)
{
    if (!m_pTarget || !m_pOwner) return;

    TransformComponent* pMyTransform = m_pOwner->GetTransform();
    TransformComponent* pTargetTransform = m_pTarget->GetTransform();

    if (!pMyTransform || !pTargetTransform) return;

    XMFLOAT3 myPos = pMyTransform->GetPosition();
    XMFLOAT3 targetPos = pTargetTransform->GetPosition();

    // Get normalized direction to target on XZ plane
    XMFLOAT2 dir = MathUtils::Direction2D(myPos, targetPos);
    if (dir.x == 0.0f && dir.y == 0.0f) return;

    // Move towards target
    float moveAmount = m_Stats.m_fMoveSpeed * dt;
    myPos.x += dir.x * moveAmount;
    myPos.z += dir.y * moveAmount;

    pMyTransform->SetPosition(myPos);
}

void EnemyComponent::UpdateIdle(float dt)
{
    // If target exists, immediately start chasing
    if (m_pTarget)
    {
        ChangeState(EnemyState::Chase);
    }
}

void EnemyComponent::UpdateChase(float dt)
{
    if (!m_pTarget)
    {
        ChangeState(EnemyState::Idle);
        return;
    }

    float distance = GetDistanceToTarget();

    // Check if within attack range
    if (distance <= m_Stats.m_fAttackRange)
    {
        // Check if can attack (cooldown)
        if (m_fAttackCooldownTimer <= 0.0f)
        {
            ChangeState(EnemyState::Attack);
        }
        else
        {
            // Wait for cooldown, face target
            FaceTarget();
        }
    }
    else
    {
        // Move towards target
        FaceTarget();
        MoveTowardsTarget(dt);
    }
}

void EnemyComponent::UpdateAttack(float dt)
{
    if (m_pAttackBehavior)
    {
        m_pAttackBehavior->Update(dt, this);

        if (m_pAttackBehavior->IsFinished())
        {
            // Reset cooldown
            m_fAttackCooldownTimer = m_Stats.m_fAttackCooldown;

            // Return to chase
            ChangeState(EnemyState::Chase);
        }
    }
    else
    {
        // No attack behavior, just go back to chase
        m_fAttackCooldownTimer = m_Stats.m_fAttackCooldown;
        ChangeState(EnemyState::Chase);
    }
}

void EnemyComponent::UpdateStagger(float dt)
{
    m_fStaggerTimer -= dt;

    if (m_fStaggerTimer <= 0.0f)
    {
        ChangeState(EnemyState::Chase);
    }
}

void EnemyComponent::UpdateDead(float dt)
{
    m_fDeadTimer -= dt;

    // Could add death animation or fade out here
    // For now, just wait for cleanup
}

void EnemyComponent::ShowIndicators()
{
    if (!m_pOwner || !m_pTarget) return;
    if (m_IndicatorConfig.m_eType == IndicatorType::None) return;

    TransformComponent* pMyTransform = m_pOwner->GetTransform();
    TransformComponent* pTargetTransform = m_pTarget->GetTransform();
    if (!pMyTransform || !pTargetTransform) return;

    XMFLOAT3 myPos = pMyTransform->GetPosition();
    XMFLOAT3 targetPos = pTargetTransform->GetPosition();

    // Direction from enemy to target on XZ plane (fixed at attack start)
    XMFLOAT2 dir = MathUtils::Direction2D(myPos, targetPos);
    if (dir.x == 0.0f && dir.y == 0.0f) return;

    float yawRad = atan2f(dir.x, dir.y);
    float yawDeg = XMConvertToDegrees(yawRad);

    if (m_IndicatorConfig.m_eType == IndicatorType::Circle)
    {
        // Circle around enemy at current position
        if (m_pHitZoneIndicator)
        {
            TransformComponent* pT = m_pHitZoneIndicator->GetTransform();
            if (pT)
            {
                pT->SetPosition(myPos.x, 0.15f, myPos.z);
                float r = m_IndicatorConfig.m_fHitRadius;
                pT->SetScale(r, 1.0f, r);
            }
        }
    }
    else if (m_IndicatorConfig.m_eType == IndicatorType::RushCircle ||
             m_IndicatorConfig.m_eType == IndicatorType::RushCone)
    {
        float rushDist = m_IndicatorConfig.m_fRushDistance;

        // Rush line: from enemy position, pointing toward target
        if (m_pRushLineIndicator)
        {
            TransformComponent* pT = m_pRushLineIndicator->GetTransform();
            if (pT)
            {
                pT->SetPosition(myPos.x, 0.15f, myPos.z);
                pT->SetRotation(0.0f, yawDeg, 0.0f);
                pT->SetScale(1.0f, 1.0f, rushDist);
            }
        }

        // Hit zone: at rush destination
        if (m_pHitZoneIndicator)
        {
            float destX = myPos.x + dir.x * rushDist;
            float destZ = myPos.z + dir.y * rushDist;
            TransformComponent* pT = m_pHitZoneIndicator->GetTransform();
            if (pT)
            {
                pT->SetPosition(destX, 0.15f, destZ);
                float r = m_IndicatorConfig.m_fHitRadius;
                pT->SetScale(r, 1.0f, r);

                if (m_IndicatorConfig.m_eType == IndicatorType::RushCone)
                {
                    pT->SetRotation(0.0f, yawDeg, 0.0f);
                }
            }
        }
    }
}

void EnemyComponent::HideIndicators()
{
    if (m_pRushLineIndicator)
    {
        TransformComponent* pT = m_pRushLineIndicator->GetTransform();
        if (pT)
            pT->SetPosition(0.0f, -1000.0f, 0.0f);
    }
    if (m_pHitZoneIndicator)
    {
        TransformComponent* pT = m_pHitZoneIndicator->GetTransform();
        if (pT)
            pT->SetPosition(0.0f, -1000.0f, 0.0f);
    }
}

void EnemyComponent::Die()
{
    OutputDebugString(L"[Enemy] Died!\n");

    // Hide and release indicators
    HideIndicators();
    m_pRushLineIndicator = nullptr;
    m_pHitZoneIndicator = nullptr;

    // Notify room/callback
    if (m_OnDeathCallback)
    {
        m_OnDeathCallback(this);
    }
}
