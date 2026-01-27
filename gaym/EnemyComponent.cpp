#include "stdafx.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "IAttackBehavior.h"
#include "Room.h"

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

    // Debug output
    const wchar_t* stateNames[] = { L"Idle", L"Chase", L"Attack", L"Stagger", L"Dead" };
    wchar_t buffer[128];
    swprintf_s(buffer, L"[Enemy] State changed: %s -> %s\n",
        stateNames[static_cast<int>(oldState)],
        stateNames[static_cast<int>(newState)]);
    OutputDebugString(buffer);

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

    XMFLOAT3 myPos = pMyTransform->GetPosition();
    XMFLOAT3 targetPos = pTargetTransform->GetPosition();

    float dx = targetPos.x - myPos.x;
    float dz = targetPos.z - myPos.z;

    return sqrtf(dx * dx + dz * dz);
}

void EnemyComponent::FaceTarget()
{
    if (!m_pTarget || !m_pOwner) return;

    TransformComponent* pMyTransform = m_pOwner->GetTransform();
    TransformComponent* pTargetTransform = m_pTarget->GetTransform();

    if (!pMyTransform || !pTargetTransform) return;

    XMFLOAT3 myPos = pMyTransform->GetPosition();
    XMFLOAT3 targetPos = pTargetTransform->GetPosition();

    // Calculate direction to target on XZ plane
    float dx = targetPos.x - myPos.x;
    float dz = targetPos.z - myPos.z;

    float length = sqrtf(dx * dx + dz * dz);
    if (length < 0.001f) return;

    // Calculate yaw angle
    float yawRad = atan2f(dx, dz);
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

    // Calculate direction to target on XZ plane
    float dx = targetPos.x - myPos.x;
    float dz = targetPos.z - myPos.z;

    float length = sqrtf(dx * dx + dz * dz);
    if (length < 0.001f) return;

    // Normalize direction
    dx /= length;
    dz /= length;

    // Move towards target
    float moveAmount = m_Stats.m_fMoveSpeed * dt;
    myPos.x += dx * moveAmount;
    myPos.z += dz * moveAmount;

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

void EnemyComponent::Die()
{
    OutputDebugString(L"[Enemy] Died!\n");

    // Notify room/callback
    if (m_OnDeathCallback)
    {
        m_OnDeathCallback(this);
    }
}
