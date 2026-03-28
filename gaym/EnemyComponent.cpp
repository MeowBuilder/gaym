#include "stdafx.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "IAttackBehavior.h"
#include "AnimationComponent.h"
#include "Room.h"
#include "Scene.h"
#include "Dx12App.h"
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
    // Boss intro cutscene takes priority
    if (IsInIntro())
    {
        UpdateBossIntro(deltaTime);
        return;
    }

    // Flying enemies maintain altitude, ground enemies use gravity
    if (m_bIsFlying)
    {
        auto* pTransform = m_pOwner ? m_pOwner->GetTransform() : nullptr;
        if (pTransform)
        {
            XMFLOAT3 pos = pTransform->GetPosition();
            // Smoothly maintain fly height
            float targetY = m_fFlyHeight;
            pos.y = pos.y + (targetY - pos.y) * 3.0f * deltaTime;
            pTransform->SetPosition(pos);
        }
    }
    else if (!m_bOnGround)
    {
        // Apply gravity for ground enemies
        auto* pTransform = m_pOwner ? m_pOwner->GetTransform() : nullptr;
        if (pTransform)
        {
            XMFLOAT3 pos = pTransform->GetPosition();
            m_fVelocityY -= GRAVITY * deltaTime;
            pos.y += m_fVelocityY * deltaTime;

            if (pos.y <= GROUND_Y)
            {
                pos.y = GROUND_Y;
                m_fVelocityY = 0.0f;
                m_bOnGround = true;
            }
            pTransform->SetPosition(pos);
        }
    }

    // Update cooldown timers
    if (m_fAttackCooldownTimer > 0.0f)
    {
        m_fAttackCooldownTimer -= deltaTime;
    }
    if (m_fSpecialCooldownTimer > 0.0f)
    {
        m_fSpecialCooldownTimer -= deltaTime;
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
        {
            // Execute the appropriate attack behavior
            IAttackBehavior* pBehavior = m_bUsingSpecialAttack ? m_pSpecialAttackBehavior.get() : m_pAttackBehavior.get();
            if (pBehavior)
            {
                pBehavior->Execute(this);
            }
            ShowIndicators();
        }
        break;
    }
}

void EnemyComponent::TakeDamage(float fDamage)
{
    if (m_eCurrentState == EnemyState::Dead) return;

    // Invincible enemies ignore all damage (during special attacks like flying)
    if (m_bInvincible)
    {
        OutputDebugString(L"[Enemy] Damage blocked - Invincible!\n");
        return;
    }

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
    else if (!m_bIsBoss)
    {
        // Only non-boss enemies get staggered
        ChangeState(EnemyState::Stagger);
    }
    // Bosses take damage but don't get staggered - pattern continues
}

void EnemyComponent::SetAttackBehavior(std::unique_ptr<IAttackBehavior> pBehavior)
{
    m_pAttackBehavior = std::move(pBehavior);
}

void EnemyComponent::SetSpecialAttackBehavior(std::unique_ptr<IAttackBehavior> pBehavior)
{
    m_pSpecialAttackBehavior = std::move(pBehavior);
}

float EnemyComponent::GetDistanceToTarget() const
{
    if (!m_pTarget || !m_pOwner) return FLT_MAX;

    TransformComponent* pMyTransform = m_pOwner->GetTransform();
    TransformComponent* pTargetTransform = m_pTarget->GetTransform();

    if (!pMyTransform || !pTargetTransform) return FLT_MAX;

    return MathUtils::Distance2D(pMyTransform->GetPosition(), pTargetTransform->GetPosition());
}

void EnemyComponent::FaceTarget(float dt, bool bInstant)
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

    // Calculate target yaw angle
    float targetYawRad = atan2f(dir.x, dir.y);
    float targetYawDeg = XMConvertToDegrees(targetYawRad);

    const XMFLOAT3& currentRot = pMyTransform->GetRotation();

    if (bInstant || dt <= 0.0f)
    {
        // Instant rotation (for attack start, etc.)
        pMyTransform->SetRotation(currentRot.x, targetYawDeg, currentRot.z);
    }
    else
    {
        // Smooth rotation
        float currentYaw = currentRot.y;

        // Calculate shortest angle difference (-180 to 180)
        float angleDiff = targetYawDeg - currentYaw;
        while (angleDiff > 180.0f) angleDiff -= 360.0f;
        while (angleDiff < -180.0f) angleDiff += 360.0f;

        // Rotate towards target at rotation speed
        float maxRotation = m_fRotationSpeed * dt;
        float rotation = 0.0f;

        if (fabsf(angleDiff) <= maxRotation)
        {
            rotation = angleDiff;
        }
        else
        {
            rotation = (angleDiff > 0.0f) ? maxRotation : -maxRotation;
        }

        float newYaw = currentYaw + rotation;
        pMyTransform->SetRotation(currentRot.x, newYaw, currentRot.z);
    }
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

    // Calculate separation force from other enemies in the room
    XMFLOAT2 separationForce = { 0.0f, 0.0f };
    if (m_pRoom)
    {
        const auto& enemies = m_pRoom->GetEnemies();
        for (EnemyComponent* pOther : enemies)
        {
            if (pOther == this || !pOther || pOther->IsDead()) continue;

            GameObject* pOtherOwner = pOther->GetOwner();
            if (!pOtherOwner) continue;

            TransformComponent* pOtherTransform = pOtherOwner->GetTransform();
            if (!pOtherTransform) continue;

            XMFLOAT3 otherPos = pOtherTransform->GetPosition();
            float dist = MathUtils::Distance2D(myPos, otherPos);

            // Apply separation if too close
            if (dist > 0.001f && dist < m_fSeparationRadius)
            {
                // Direction away from other enemy
                XMFLOAT2 awayDir = MathUtils::Direction2D(otherPos, myPos);
                // Stronger force when closer (inverse proportional)
                float strength = (m_fSeparationRadius - dist) / m_fSeparationRadius;
                separationForce.x += awayDir.x * strength;
                separationForce.y += awayDir.y * strength;
            }
        }
    }

    // Combine movement direction with separation force
    float moveX = dir.x * m_Stats.m_fMoveSpeed;
    float moveZ = dir.y * m_Stats.m_fMoveSpeed;

    // Add separation force
    moveX += separationForce.x * m_fSeparationStrength;
    moveZ += separationForce.y * m_fSeparationStrength;

    // Apply movement
    myPos.x += moveX * dt;
    myPos.z += moveZ * dt;
    // Y is controlled by gravity in Update()

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
            // Face target instantly when starting attack
            FaceTarget(dt, true);

            // Boss special attack selection
            m_bUsingSpecialAttack = false;
            if (m_bIsBoss && m_pSpecialAttackBehavior && m_fSpecialCooldownTimer <= 0.0f)
            {
                // Roll for special attack chance
                if ((rand() % 100) < m_nSpecialAttackChance)
                {
                    m_bUsingSpecialAttack = true;
                    OutputDebugString(L"[Boss] Using SPECIAL attack pattern!\n");
                }
            }

            ChangeState(EnemyState::Attack);
        }
        else
        {
            // Wait for cooldown, face target smoothly
            FaceTarget(dt);
        }
    }
    else
    {
        // Move towards target with smooth rotation
        FaceTarget(dt);
        MoveTowardsTarget(dt);
    }
}

void EnemyComponent::UpdateAttack(float dt)
{
    // Select which behavior to use
    IAttackBehavior* pCurrentBehavior = m_bUsingSpecialAttack ? m_pSpecialAttackBehavior.get() : m_pAttackBehavior.get();

    if (pCurrentBehavior)
    {
        pCurrentBehavior->Update(dt, this);

        if (pCurrentBehavior->IsFinished())
        {
            // Reset cooldowns
            m_fAttackCooldownTimer = m_Stats.m_fAttackCooldown;

            if (m_bUsingSpecialAttack)
            {
                // Special attack has longer cooldown
                m_fSpecialCooldownTimer = m_fSpecialAttackCooldown;
                m_bUsingSpecialAttack = false;
                OutputDebugString(L"[Boss] Special attack finished, cooldown started\n");
            }

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

    // When timer expires, request deletion
    if (m_fDeadTimer <= 0.0f)
    {
        // Try to get Scene from Room first, fallback to Dx12App if not available
        Scene* pScene = nullptr;
        if (m_pRoom)
        {
            pScene = m_pRoom->GetScene();
        }

        // Fallback: get Scene directly from Dx12App if Room doesn't have it
        if (!pScene)
        {
            pScene = Dx12App::GetInstance()->GetScene();
        }

        if (pScene)
        {
            // Mark indicator objects for deletion first
            if (m_pRushLineIndicator)
            {
                pScene->MarkForDeletion(m_pRushLineIndicator);
                m_pRushLineIndicator = nullptr;
            }
            if (m_pHitZoneIndicator)
            {
                pScene->MarkForDeletion(m_pHitZoneIndicator);
                m_pHitZoneIndicator = nullptr;
            }

            // Mark self for deletion (will also clean up child hierarchy)
            if (m_pOwner)
            {
                pScene->MarkForDeletion(m_pOwner);
            }
        }
    }
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
                pT->SetPosition(myPos.x, myPos.y + 0.15f, myPos.z);
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
                pT->SetPosition(myPos.x, myPos.y + 0.15f, myPos.z);
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
                pT->SetPosition(destX, myPos.y + 0.15f, destZ);
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

void EnemyComponent::StartBossIntro(float fStartHeight)
{
    m_eIntroPhase = BossIntroPhase::FlyingIn;
    m_fIntroTimer = 0.0f;
    m_fIntroStartHeight = fStartHeight;

    // Set target to ground level
    auto* pTransform = m_pOwner ? m_pOwner->GetTransform() : nullptr;
    if (pTransform)
    {
        XMFLOAT3 pos = pTransform->GetPosition();
        m_fIntroTargetHeight = GROUND_Y;  // Land at ground level (0)
        // Move to start position (high in sky)
        pos.y = fStartHeight;
        pTransform->SetPosition(pos);
    }

    // Start with glide animation
    if (m_pAnimationComp)
    {
        m_pAnimationComp->CrossFade("Fly Glide", 0.2f, true);
    }

    OutputDebugString(L"[Boss] Intro started - Flying In\n");
}

void EnemyComponent::UpdateBossIntro(float dt)
{
    auto* pTransform = m_pOwner ? m_pOwner->GetTransform() : nullptr;
    if (!pTransform) return;

    m_fIntroTimer += dt;

    switch (m_eIntroPhase)
    {
    case BossIntroPhase::FlyingIn:
    {
        // Descend from sky
        XMFLOAT3 pos = pTransform->GetPosition();
        float fDescendSpeed = 8.0f;
        pos.y -= fDescendSpeed * dt;

        // Face the player while descending (smooth rotation)
        if (m_pTarget)
        {
            FaceTarget(dt);
        }

        if (pos.y <= m_fIntroTargetHeight + 0.5f)
        {
            pos.y = m_fIntroTargetHeight;
            pTransform->SetPosition(pos);

            // Transition to landing
            m_eIntroPhase = BossIntroPhase::Landing;
            m_fIntroTimer = 0.0f;

            if (m_pAnimationComp)
            {
                m_pAnimationComp->CrossFade("Land", 0.15f, false);
            }
            OutputDebugString(L"[Boss] Landing\n");
        }
        else
        {
            pTransform->SetPosition(pos);
        }
        break;
    }

    case BossIntroPhase::Landing:
    {
        // Wait for landing animation (approx 1.5 seconds)
        if (m_fIntroTimer >= 1.5f)
        {
            m_eIntroPhase = BossIntroPhase::Roaring;
            m_fIntroTimer = 0.0f;

            if (m_pAnimationComp)
            {
                m_pAnimationComp->CrossFade("Scream", 0.15f, false);
            }
            OutputDebugString(L"[Boss] Roaring\n");
        }
        break;
    }

    case BossIntroPhase::Roaring:
    {
        // Wait for roar animation (approx 2 seconds)
        if (m_fIntroTimer >= 2.0f)
        {
            m_eIntroPhase = BossIntroPhase::Done;
            m_fIntroTimer = 0.0f;

            // Disable flying mode for ground combat
            m_bIsFlying = false;
            m_bOnGround = true;

            // Switch to ground combat animations
            m_AnimConfig.m_strIdleClip = "Idle01";
            m_AnimConfig.m_strChaseClip = "Walk";
            m_AnimConfig.m_strAttackClip = "Flame Attack";

            // Start combat
            ChangeState(EnemyState::Idle);
            OutputDebugString(L"[Boss] Intro complete - Combat started\n");
        }
        break;
    }

    default:
        break;
    }
}
