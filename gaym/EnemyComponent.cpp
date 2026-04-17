#include "stdafx.h"
#include "EnemyComponent.h"
#include "DamageNumberManager.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "IAttackBehavior.h"
#include "AnimationComponent.h"
#include "Room.h"
#include "Scene.h"
#include "Dx12App.h"
#include "MathUtils.h"
#include "BossPhaseController.h"
#include <algorithm>

EnemyComponent::EnemyComponent(GameObject* pOwner)
    : Component(pOwner)
{
}

EnemyComponent::~EnemyComponent()
{
}

void EnemyComponent::Update(float deltaTime)
{
    // Decay hit flash every frame
    if (m_fHitFlashTimer > 0.f)
    {
        m_fHitFlashTimer -= deltaTime;
        float flash = (m_fHitFlashTimer > 0.f) ? (m_fHitFlashTimer / FLASH_DURATION) : 0.f;
        if (m_pOwner) m_pOwner->SetHitFlash(flash);
    }

    // Boss intro cutscene takes priority
    if (IsInIntro())
    {
        UpdateBossIntro(deltaTime);
        return;
    }

    // Boss phase transition takes priority
    if (m_pPhaseController && m_pPhaseController->IsInTransition())
    {
        m_pPhaseController->Update(deltaTime);
        return;  // 페이즈 전환 중에는 일반 AI 스킵
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
    if (m_fFlyingCooldownTimer > 0.0f)
    {
        m_fFlyingCooldownTimer -= deltaTime;
    }

    // Threat table update (distance-based threat decay/gain)
    auto* pTransform = m_pOwner ? m_pOwner->GetTransform() : nullptr;
    if (pTransform)
    {
        m_ThreatTable.Update(deltaTime, pTransform->GetPosition());
    }

    // Target reevaluation (every 0.5 seconds)
    m_fTargetReevaluationTimer += deltaTime;
    if (m_fTargetReevaluationTimer >= ThreatConstants::TARGET_REEVALUATION_INTERVAL)
    {
        m_fTargetReevaluationTimer = 0.0f;
        ReevaluateTarget();
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
        {
            // 행동별 전용 클립이 있으면 그걸 우선 사용
            IAttackBehavior* pActiveBehavior = m_bUsingFlyingAttack  ? m_pFlyingAttackBehavior.get()
                                             : m_bUsingSpecialAttack ? m_pSpecialAttackBehavior.get()
                                             :                         m_pAttackBehavior.get();
            const char* pClip = (pActiveBehavior && pActiveBehavior->GetAnimClipName()[0] != '\0')
                               ? pActiveBehavior->GetAnimClipName()
                               : m_AnimConfig.m_strAttackClip.c_str();
            m_pAnimationComp->CrossFade(pClip, 0.15f, m_AnimConfig.m_bLoopAttack);
            break;
        }
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
            // Execute the appropriate attack behavior (priority: flying > special > primary)
            IAttackBehavior* pBehavior = nullptr;
            if (m_bUsingFlyingAttack && m_pFlyingAttackBehavior)
            {
                pBehavior = m_pFlyingAttackBehavior.get();
            }
            else if (m_bUsingSpecialAttack && m_pSpecialAttackBehavior)
            {
                pBehavior = m_pSpecialAttackBehavior.get();
            }
            else
            {
                pBehavior = m_pAttackBehavior.get();
            }

            if (pBehavior)
            {
                pBehavior->Execute(this);
            }
            ShowIndicators();
        }
        break;
    }
}

void EnemyComponent::TakeDamage(float fDamage, bool bTriggerStagger)
{
    if (m_eCurrentState == EnemyState::Dead) return;

    // Invincible enemies ignore all damage (during special attacks like flying)
    if (m_bInvincible)
    {
        OutputDebugString(L"[Enemy] Damage blocked - Invincible!\n");
        return;
    }

    m_Stats.m_fCurrentHP -= fDamage;

    // Hit flash
    m_fHitFlashTimer = FLASH_DURATION;
    if (m_pOwner) m_pOwner->SetHitFlash(1.f);

    // Floating damage number
    if (m_pOwner && m_pOwner->GetTransform())
    {
        XMFLOAT3 pos = m_pOwner->GetTransform()->GetPosition();
        pos.y += 2.0f;
        DamageNumberManager::Get().AddNumber(pos, fDamage);
    }

    wchar_t buffer[128];
    swprintf_s(buffer, L"[Enemy] Took %.1f damage, HP: %.1f/%.1f\n",
        fDamage, m_Stats.m_fCurrentHP, m_Stats.m_fMaxHP);
    OutputDebugString(buffer);

    // Boss Phase System: HP 변화 알림
    if (m_pPhaseController)
    {
        m_pPhaseController->OnHealthChanged(m_Stats.m_fCurrentHP, m_Stats.m_fMaxHP);
    }

    if (m_Stats.m_fCurrentHP <= 0.0f)
    {
        m_Stats.m_fCurrentHP = 0.0f;
        ChangeState(EnemyState::Dead);
    }
    else if (bTriggerStagger && !m_bIsBoss)
    {
        // 경직 허용 + 일반 적만 경직 (보스는 피해만 받고 패턴 유지)
        ChangeState(EnemyState::Stagger);
    }
}

void EnemyComponent::SetAttackBehavior(std::unique_ptr<IAttackBehavior> pBehavior)
{
    m_pAttackBehavior = std::move(pBehavior);
}

void EnemyComponent::SetSpecialAttackBehavior(std::unique_ptr<IAttackBehavior> pBehavior)
{
    m_pSpecialAttackBehavior = std::move(pBehavior);
}

void EnemyComponent::SetFlyingAttackBehavior(std::unique_ptr<IAttackBehavior> pBehavior)
{
    m_pFlyingAttackBehavior = std::move(pBehavior);
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
    float currentYaw = currentRot.y;

    // Calculate shortest angle difference (-180 to 180)
    float angleDiff = targetYawDeg - currentYaw;
    while (angleDiff > 180.0f) angleDiff -= 360.0f;
    while (angleDiff < -180.0f) angleDiff += 360.0f;

    if (bInstant || dt <= 0.0f)
    {
        // Boss: limit instant rotation to prevent sudden 180 degree turns
        // This prevents the jarring "snap" when player moves behind boss
        if (m_bIsBoss)
        {
            const float MAX_INSTANT_ROTATION = 90.0f;  // Max 90 degree turn at once
            if (fabsf(angleDiff) > MAX_INSTANT_ROTATION)
            {
                // Clamp the rotation
                float rotation = (angleDiff > 0.0f) ? MAX_INSTANT_ROTATION : -MAX_INSTANT_ROTATION;
                float newYaw = currentYaw + rotation;
                pMyTransform->SetRotation(currentRot.x, newYaw, currentRot.z);
                return;
            }
        }
        // Non-boss or small angle: instant rotation
        pMyTransform->SetRotation(currentRot.x, targetYawDeg, currentRot.z);
    }
    else
    {
        // Smooth rotation
        // Boss has faster rotation speed for better tracking
        float rotSpeed = m_bIsBoss ? m_fRotationSpeed * 1.5f : m_fRotationSpeed;
        float maxRotation = rotSpeed * dt;
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

    // 보스 페이즈 전환 중이면 대기
    if (m_pPhaseController && m_pPhaseController->IsInTransition())
    {
        return;
    }

    float distance = GetDistanceToTarget();

    // Boss: 거리 기반 공격 선택 (카이팅 방지)
    if (m_bIsBoss && m_fAttackCooldownTimer <= 0.0f)
    {
        m_bUsingSpecialAttack = false;
        m_bUsingFlyingAttack = false;

        // 원거리 (30+ units): 비행 공격 또는 브레스 우선
        if (distance >= m_Stats.m_fLongRangeThreshold)
        {
            FaceTarget(dt, true);

            // 비행 공격 시도 (높은 확률)
            if (CanUseFlyingAttack() && m_fFlyingCooldownTimer <= 0.0f)
            {
                int flyChance = 70;  // 원거리에서는 70% 확률로 비행 공격
                if (m_pPhaseController)
                {
                    flyChance = std::max(flyChance, m_pPhaseController->GetFlyingAttackChance());
                }
                if ((rand() % 100) < flyChance)
                {
                    if (m_pPhaseController)
                    {
                        const BossPhaseData& phase = m_pPhaseController->GetCurrentPhaseData();
                        if (phase.m_fnFlyingAttack)
                        {
                            m_pFlyingAttackBehavior = phase.m_fnFlyingAttack();
                        }
                    }
                    if (m_pFlyingAttackBehavior)
                    {
                        m_bUsingFlyingAttack = true;
                        OutputDebugString(L"[Boss] Long range - Using FLYING attack!\n");
                        ChangeState(EnemyState::Attack);
                        return;
                    }
                }
            }

            // 비행 불가 시 기본 공격 (팩토리 있으면 새로 생성)
            if (m_fnAttackFactory) m_pAttackBehavior = m_fnAttackFactory();
            else if (m_pAttackBehavior) m_pAttackBehavior->Reset();
            OutputDebugString(L"[Boss] Long range - Using PRIMARY attack!\n");
            ChangeState(EnemyState::Attack);
            return;
        }
        // 중거리 (15-30 units): 특수 공격 또는 비행 공격
        else if (distance >= m_Stats.m_fMidRangeThreshold)
        {
            FaceTarget(dt, true);

            // 비행 공격 시도
            if (CanUseFlyingAttack() && m_fFlyingCooldownTimer <= 0.0f)
            {
                int flyChance = m_nFlyingAttackChance;
                if (m_pPhaseController)
                {
                    flyChance = m_pPhaseController->GetFlyingAttackChance();
                }
                if ((rand() % 100) < flyChance)
                {
                    if (m_pPhaseController)
                    {
                        const BossPhaseData& phase = m_pPhaseController->GetCurrentPhaseData();
                        if (phase.m_fnFlyingAttack)
                        {
                            m_pFlyingAttackBehavior = phase.m_fnFlyingAttack();
                        }
                    }
                    if (m_pFlyingAttackBehavior)
                    {
                        m_bUsingFlyingAttack = true;
                        OutputDebugString(L"[Boss] Mid range - Using FLYING attack!\n");
                        ChangeState(EnemyState::Attack);
                        return;
                    }
                }
            }

            // 특수 공격 시도 (중거리에서 높은 확률)
            if (m_fSpecialCooldownTimer <= 0.0f)
            {
                int specialChance = 50;  // 중거리에서 50% 확률
                if (m_pPhaseController)
                {
                    const BossPhaseData& phase = m_pPhaseController->GetCurrentPhaseData();
                    if (phase.m_fnSpecialAttack && (rand() % 100) < specialChance)
                    {
                        m_pSpecialAttackBehavior = phase.m_fnSpecialAttack();
                        m_bUsingSpecialAttack = true;
                        OutputDebugString(L"[Boss] Mid range - Using SPECIAL attack (phase)!\n");
                        ChangeState(EnemyState::Attack);
                        return;
                    }
                }
                else if ((m_fnSpecialAttackFactory || m_pSpecialAttackBehavior) && (rand() % 100) < m_nSpecialAttackChance)
                {
                    if (m_fnSpecialAttackFactory)
                        m_pSpecialAttackBehavior = m_fnSpecialAttackFactory();
                    else
                        m_pSpecialAttackBehavior->Reset();
                    m_bUsingSpecialAttack = true;
                    OutputDebugString(L"[Boss] Mid range - Using SPECIAL attack!\n");
                    ChangeState(EnemyState::Attack);
                    return;
                }
            }

            // 그 외엔 기본 공격 (팩토리 있으면 새로 생성)
            if (m_fnAttackFactory) m_pAttackBehavior = m_fnAttackFactory();
            else if (m_pAttackBehavior) m_pAttackBehavior->Reset();
            OutputDebugString(L"[Boss] Mid range - Using PRIMARY attack!\n");
            ChangeState(EnemyState::Attack);
            return;
        }
        // 근거리 (< 15 units): 근접 콤보 / 특수 공격
        else
        {
            FaceTarget(dt, true);

            // 비행 공격 (낮은 확률)
            if (CanUseFlyingAttack() && m_fFlyingCooldownTimer <= 0.0f)
            {
                int flyChance = m_nFlyingAttackChance / 2;  // 근접에서는 확률 절반
                if (m_pPhaseController)
                {
                    flyChance = m_pPhaseController->GetFlyingAttackChance() / 2;
                }
                if ((rand() % 100) < flyChance)
                {
                    if (m_pPhaseController)
                    {
                        const BossPhaseData& phase = m_pPhaseController->GetCurrentPhaseData();
                        if (phase.m_fnFlyingAttack)
                        {
                            m_pFlyingAttackBehavior = phase.m_fnFlyingAttack();
                        }
                    }
                    if (m_pFlyingAttackBehavior)
                    {
                        m_bUsingFlyingAttack = true;
                        OutputDebugString(L"[Boss] Close range - Using FLYING attack!\n");
                        ChangeState(EnemyState::Attack);
                        return;
                    }
                }
            }

            // 특수 공격 시도
            if (m_fSpecialCooldownTimer <= 0.0f)
            {
                if (m_pPhaseController)
                {
                    const BossPhaseData& phase = m_pPhaseController->GetCurrentPhaseData();
                    if (phase.m_fnSpecialAttack && (rand() % 100) < m_nSpecialAttackChance)
                    {
                        m_pSpecialAttackBehavior = phase.m_fnSpecialAttack();
                        m_bUsingSpecialAttack = true;
                        OutputDebugString(L"[Boss] Close range - Using SPECIAL attack (phase)!\n");
                        ChangeState(EnemyState::Attack);
                        return;
                    }
                }
                else if ((m_fnSpecialAttackFactory || m_pSpecialAttackBehavior) && (rand() % 100) < m_nSpecialAttackChance)
                {
                    if (m_fnSpecialAttackFactory)
                        m_pSpecialAttackBehavior = m_fnSpecialAttackFactory();
                    else
                        m_pSpecialAttackBehavior->Reset();
                    m_bUsingSpecialAttack = true;
                    OutputDebugString(L"[Boss] Close range - Using SPECIAL attack!\n");
                    ChangeState(EnemyState::Attack);
                    return;
                }
            }

            // 기본 공격 (팩토리 있으면 새로 생성해 패턴 변화)
            if (m_fnAttackFactory) m_pAttackBehavior = m_fnAttackFactory();
            else if (m_pAttackBehavior) m_pAttackBehavior->Reset();
            OutputDebugString(L"[Boss] Close range - Using PRIMARY attack!\n");
            ChangeState(EnemyState::Attack);
            return;
        }
    }

    // 일반 적: 기존 사거리 기반 공격
    if (distance <= m_Stats.m_fAttackRange)
    {
        if (m_fAttackCooldownTimer <= 0.0f)
        {
            FaceTarget(dt, true);
            ChangeState(EnemyState::Attack);
        }
        else
        {
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
    // Select which behavior to use (priority: flying > special > primary)
    IAttackBehavior* pCurrentBehavior = nullptr;
    if (m_bUsingFlyingAttack && m_pFlyingAttackBehavior)
    {
        pCurrentBehavior = m_pFlyingAttackBehavior.get();
    }
    else if (m_bUsingSpecialAttack && m_pSpecialAttackBehavior)
    {
        pCurrentBehavior = m_pSpecialAttackBehavior.get();
    }
    else
    {
        pCurrentBehavior = m_pAttackBehavior.get();
    }

    if (pCurrentBehavior)
    {
        pCurrentBehavior->Update(dt, this);

        if (pCurrentBehavior->IsFinished())
        {
            // Reset cooldowns
            m_fAttackCooldownTimer = m_Stats.m_fAttackCooldown;

            if (m_bUsingFlyingAttack)
            {
                m_fFlyingCooldownTimer = m_fFlyingAttackCooldown;
                m_bUsingFlyingAttack = false;
                // 비행 공격 후 비행 공격 behavior 리셋
                if (m_pFlyingAttackBehavior)
                {
                    m_pFlyingAttackBehavior->Reset();
                }
                OutputDebugString(L"[Boss] Flying attack finished, cooldown started\n");
            }
            else if (m_bUsingSpecialAttack)
            {
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

    // 보스는 죽음 애니메이션 끝난 후에도 시체로 남아있음
    if (m_bIsBoss)
    {
        // 애니메이션 재생 중이면 타이머 일시정지
        if (m_pAnimationComp && m_pAnimationComp->IsPlaying())
        {
            // 무한 대기 방지 - 최대 8초
            if (m_fDeadTimer > -6.0f)
            {
                m_fDeadTimer = 0.1f;  // 타이머 유지 (삭제 안 함)
                return;
            }
        }
        // 애니메이션 끝났으면 추가 3초 대기 (시체 상태)
        else if (m_fDeadTimer > -3.0f)
        {
            return;  // 3초 더 기다림
        }
    }

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

// ─────────────────────────────────────────────────────────────────────────────
// Threat (Aggro) System
// ─────────────────────────────────────────────────────────────────────────────

void EnemyComponent::RegisterAllPlayers(const std::vector<GameObject*>& players)
{
    m_ThreatTable.Clear();
    for (GameObject* pPlayer : players)
    {
        if (pPlayer)
        {
            m_ThreatTable.RegisterPlayer(pPlayer, ThreatConstants::INITIAL_THREAT);
        }
    }

    // 첫 번째 타겟 설정
    if (!m_pTarget && !players.empty())
    {
        m_pTarget = m_ThreatTable.GetHighestThreatTarget();
    }

#ifdef _DEBUG
    wchar_t buf[128];
    swprintf_s(buf, L"[Enemy] Registered %zu players to threat table\n", players.size());
    OutputDebugString(buf);
#endif
}

void EnemyComponent::AddThreat(GameObject* pPlayer, float fAmount)
{
    m_ThreatTable.AddThreat(pPlayer, fAmount);
}

void EnemyComponent::ReduceThreat(GameObject* pPlayer, float fAmount)
{
    m_ThreatTable.ReduceThreat(pPlayer, fAmount);
}

void EnemyComponent::ReevaluateTarget()
{
    // 죽은 플레이어 정리
    m_ThreatTable.CleanupDeadPlayers();

    // 가장 높은 위협도의 플레이어를 타겟으로 설정
    GameObject* pNewTarget = m_ThreatTable.GetHighestThreatTarget(m_pTarget);
    if (pNewTarget && pNewTarget != m_pTarget)
    {
        m_pTarget = pNewTarget;

#ifdef _DEBUG
        OutputDebugString(L"[Enemy] Target changed due to threat!\n");
#endif
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Boss Phase System
// ─────────────────────────────────────────────────────────────────────────────

void EnemyComponent::SetBossPhaseController(std::unique_ptr<BossPhaseController> pController)
{
    m_pPhaseController = std::move(pController);
}

bool EnemyComponent::CanUseFlyingAttack() const
{
    // 보스가 아니거나 페이즈 컨트롤러가 없으면 항상 false
    if (!m_bIsBoss || !m_pPhaseController) return false;

    // 현재 페이즈에서 비행이 허용되는지 확인
    return m_pPhaseController->CanFly();
}
