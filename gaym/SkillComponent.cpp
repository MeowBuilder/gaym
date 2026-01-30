#include "stdafx.h"
#include "SkillComponent.h"
#include "ISkillBehavior.h"
#include "SkillData.h"
#include "InputSystem.h"
#include "Camera.h"
#include "GameObject.h"
#include "TransformComponent.h"

SkillComponent::SkillComponent(GameObject* pOwner)
    : Component(pOwner)
    , m_ChargeTargetPosition{ 0.0f, 0.0f, 0.0f }
    , m_ChannelTargetPosition{ 0.0f, 0.0f, 0.0f }
{
    // Initialize all cooldowns to 0 (ready)
    m_CooldownTimers.fill(0.0f);

    // Initialize all skill states to Ready
    m_SkillStates.fill(SkillState::Ready);
}

SkillComponent::~SkillComponent()
{
}

void SkillComponent::Update(float deltaTime)
{
    // Update cooldown timers
    for (size_t i = 0; i < static_cast<size_t>(SkillSlot::Count); ++i)
    {
        if (m_CooldownTimers[i] > 0.0f)
        {
            m_CooldownTimers[i] -= deltaTime;
            if (m_CooldownTimers[i] <= 0.0f)
            {
                m_CooldownTimers[i] = 0.0f;
                if (m_SkillStates[i] == SkillState::Cooldown)
                {
                    m_SkillStates[i] = SkillState::Ready;
                }
            }
        }
    }

    // Update charge timer
    if (m_bIsCharging)
    {
        m_fChargeTime += deltaTime;
        // Visual/audio feedback could be added here based on charge level
    }

    // Update channel timer
    if (m_bIsChanneling)
    {
        m_fChannelTime += deltaTime;
        m_fChannelTickAccum += deltaTime;

        // Fire tick if enough time passed
        if (m_fChannelTickAccum >= m_fChannelTickRate)
        {
            m_fChannelTickAccum -= m_fChannelTickRate;

            size_t index = static_cast<size_t>(m_ActiveSkillSlot);
            if (index < m_Skills.size() && m_Skills[index])
            {
                // Use stored target position for channel ticks
                m_Skills[index]->Execute(m_pOwner, m_ChannelTargetPosition, 0.3f);  // 30% damage per tick
            }
        }

        // Check if channel duration expired
        if (m_fChannelTime >= m_fChannelDuration)
        {
            OutputDebugString(L"[Skill] Channel complete!\n");
            m_bIsChanneling = false;
            m_fChannelTime = 0.0f;
            m_fChannelTickAccum = 0.0f;  // Reset tick accumulator

            size_t index = static_cast<size_t>(m_ActiveSkillSlot);
            if (index < m_Skills.size() && m_Skills[index])
            {
                float cooldown = m_Skills[index]->GetSkillData().cooldown;
                m_CooldownTimers[index] = cooldown;
                m_SkillStates[index] = SkillState::Cooldown;
                m_Skills[index]->Reset();
            }
            m_ActiveSkillSlot = SkillSlot::Count;
        }
    }

    // Update enhance timer
    if (m_bIsEnhanced)
    {
        m_fEnhanceTimer -= deltaTime;
        if (m_fEnhanceTimer <= 0.0f)
        {
            m_bIsEnhanced = false;
            m_fEnhanceTimer = 0.0f;
            OutputDebugString(L"[Skill] Enhancement expired\n");
        }
    }

    // Update active skill if any
    // Skip IsFinished() check if channeling or charging - those have their own completion logic
    if (m_ActiveSkillSlot != SkillSlot::Count && !m_bIsChanneling && !m_bIsCharging)
    {
        size_t slotIndex = static_cast<size_t>(m_ActiveSkillSlot);
        if (slotIndex < m_Skills.size() && m_Skills[slotIndex])
        {
            m_Skills[slotIndex]->Update(deltaTime);

            // Check if skill finished
            if (m_Skills[slotIndex]->IsFinished())
            {
                // Start cooldown
                float cooldown = m_Skills[slotIndex]->GetSkillData().cooldown;
                m_CooldownTimers[slotIndex] = cooldown;
                m_SkillStates[slotIndex] = SkillState::Cooldown;
                m_Skills[slotIndex]->Reset();
                m_ActiveSkillSlot = SkillSlot::Count;
            }
        }
    }
}

void SkillComponent::ProcessSkillInput(InputSystem* pInputSystem, CCamera* pCamera)
{
    if (!pInputSystem || !pCamera) return;

    // Process rune input (1-5 keys to change activation type)
    ProcessRuneInput(pInputSystem);

    // Calculate target position
    DirectX::XMFLOAT3 targetPos = CalculateTargetPosition(pInputSystem, pCamera);

    // Handle charging state
    if (m_bIsCharging)
    {
        // Check if key is still held
        if (IsSkillKeyPressed(m_ChargingSlot, pInputSystem))
        {
            // Continue charging
            // (charge time is updated in ExecuteWithActivationType via deltaTime from Update)
        }
        else
        {
            // Key released - fire the charged skill
            size_t index = static_cast<size_t>(m_ChargingSlot);
            if (m_Skills[index])
            {
                float chargeRatio = m_fChargeTime / m_fMaxChargeTime;
                chargeRatio = min(1.0f, chargeRatio);

                // Apply charge multiplier (1.0x to 3.0x based on charge)
                float damageMultiplier = 1.0f + chargeRatio * 2.0f;

                wchar_t buffer[128];
                swprintf_s(buffer, 128, L"[Skill] Charge released! Charge: %.0f%%, Multiplier: %.1fx\n",
                    chargeRatio * 100.0f, damageMultiplier);
                OutputDebugString(buffer);

                // Execute with multiplier
                m_Skills[index]->Execute(m_pOwner, m_ChargeTargetPosition, damageMultiplier);
                m_SkillStates[index] = SkillState::Casting;
                m_ActiveSkillSlot = m_ChargingSlot;

                // Start cooldown
                float cooldown = m_Skills[index]->GetSkillData().cooldown;
                m_CooldownTimers[index] = cooldown;
            }

            m_bIsCharging = false;
            m_fChargeTime = 0.0f;
            m_ChargingSlot = SkillSlot::Count;
        }
        return;  // Don't process other inputs while charging
    }

    // Handle channeling state
    if (m_bIsChanneling)
    {
        if (IsSkillKeyPressed(m_ActiveSkillSlot, pInputSystem))
        {
            // Continue channeling - handled in Update
        }
        else
        {
            // Key released - stop channeling
            OutputDebugString(L"[Skill] Channel interrupted\n");
            m_bIsChanneling = false;
            m_fChannelTime = 0.0f;
            m_fChannelTickAccum = 0.0f;  // Reset tick accumulator

            size_t index = static_cast<size_t>(m_ActiveSkillSlot);
            if (index < m_Skills.size() && m_Skills[index])
            {
                float cooldown = m_Skills[index]->GetSkillData().cooldown * 0.5f; // Half cooldown on interrupt
                m_CooldownTimers[index] = cooldown;
                m_SkillStates[index] = SkillState::Cooldown;
                m_Skills[index]->Reset();
            }
            m_ActiveSkillSlot = SkillSlot::Count;
        }
        return;
    }

    // Don't process new skill input if a skill is currently active
    if (m_ActiveSkillSlot != SkillSlot::Count) return;

    // Check each skill slot for input
    for (size_t i = 0; i < static_cast<size_t>(SkillSlot::Count); ++i)
    {
        SkillSlot slot = static_cast<SkillSlot>(i);
        if (IsSkillKeyPressed(slot, pInputSystem))
        {
            ExecuteWithActivationType(slot, targetPos);
            break;  // Only use one skill per frame
        }
    }
}

void SkillComponent::EquipSkill(SkillSlot slot, std::unique_ptr<ISkillBehavior> pBehavior)
{
    size_t index = static_cast<size_t>(slot);
    if (index < m_Skills.size())
    {
        m_Skills[index] = std::move(pBehavior);
        m_CooldownTimers[index] = 0.0f;
        m_SkillStates[index] = SkillState::Ready;
    }
}

void SkillComponent::UnequipSkill(SkillSlot slot)
{
    size_t index = static_cast<size_t>(slot);
    if (index < m_Skills.size())
    {
        m_Skills[index].reset();
        m_CooldownTimers[index] = 0.0f;
        m_SkillStates[index] = SkillState::Ready;
    }
}

ISkillBehavior* SkillComponent::GetSkill(SkillSlot slot) const
{
    size_t index = static_cast<size_t>(slot);
    if (index < m_Skills.size())
    {
        return m_Skills[index].get();
    }
    return nullptr;
}

bool SkillComponent::IsSkillReady(SkillSlot slot) const
{
    size_t index = static_cast<size_t>(slot);
    if (index >= m_Skills.size() || !m_Skills[index])
    {
        return false;
    }
    return m_SkillStates[index] == SkillState::Ready;
}

float SkillComponent::GetCooldownRemaining(SkillSlot slot) const
{
    size_t index = static_cast<size_t>(slot);
    if (index < m_CooldownTimers.size())
    {
        return m_CooldownTimers[index];
    }
    return 0.0f;
}

float SkillComponent::GetCooldownProgress(SkillSlot slot) const
{
    size_t index = static_cast<size_t>(slot);
    if (index >= m_Skills.size() || !m_Skills[index])
    {
        return 1.0f;
    }

    float cooldown = m_Skills[index]->GetSkillData().cooldown;
    if (cooldown <= 0.0f)
    {
        return 1.0f;
    }

    float remaining = m_CooldownTimers[index];
    return 1.0f - (remaining / cooldown);
}

bool SkillComponent::TryUseSkill(SkillSlot slot, const DirectX::XMFLOAT3& targetPosition)
{
    size_t index = static_cast<size_t>(slot);

    // Check if skill exists and is ready
    if (index >= m_Skills.size() || !m_Skills[index])
    {
        return false;
    }

    if (m_SkillStates[index] != SkillState::Ready)
    {
        return false;
    }

    // Execute the skill
    m_Skills[index]->Execute(m_pOwner, targetPosition);
    m_SkillStates[index] = SkillState::Casting;
    m_ActiveSkillSlot = slot;

    return true;
}

DirectX::XMFLOAT3 SkillComponent::CalculateTargetPosition(InputSystem* pInputSystem, CCamera* pCamera) const
{
    using namespace DirectX;

    // Get mouse position in screen space
    XMFLOAT2 mousePos = pInputSystem->GetMousePosition();

    // Convert to Normalized Device Coordinates (NDC)
    float ndcX = (2.0f * mousePos.x / kWindowWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mousePos.y / kWindowHeight);

    // Unproject from NDC to World Space to form a ray
    XMMATRIX viewMatrix = XMLoadFloat4x4(&pCamera->GetViewMatrix());
    XMMATRIX projMatrix = XMLoadFloat4x4(&pCamera->GetProjectionMatrix());
    XMMATRIX viewProjMatrix = viewMatrix * projMatrix;
    XMMATRIX invViewProjMatrix = XMMatrixInverse(nullptr, viewProjMatrix);

    XMVECTOR rayOrigin = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invViewProjMatrix);
    XMVECTOR rayEnd = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invViewProjMatrix);
    XMVECTOR rayDir = XMVector3Normalize(rayEnd - rayOrigin);

    // Define the ground plane (Y=0)
    XMVECTOR groundPlane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    // Find intersection of the ray and the ground plane
    XMVECTOR intersectionPoint = XMPlaneIntersectLine(groundPlane, rayOrigin, rayOrigin + rayDir * 1000.0f);

    XMFLOAT3 result;
    XMStoreFloat3(&result, intersectionPoint);
    return result;
}

bool SkillComponent::IsSkillKeyPressed(SkillSlot slot, InputSystem* pInputSystem) const
{
    switch (slot)
    {
    case SkillSlot::Q:
        return pInputSystem->IsKeyDown('Q');
    case SkillSlot::E:
        return pInputSystem->IsKeyDown('E');
    case SkillSlot::R:
        return pInputSystem->IsKeyDown('R');
    case SkillSlot::RightClick:
        return pInputSystem->IsMouseButtonDown(1);  // Right mouse button
    default:
        return false;
    }
}

void SkillComponent::SetActivationType(ActivationType type)
{
    if (m_CurrentActivationType != type)
    {
        m_CurrentActivationType = type;

        const wchar_t* typeNames[] = { L"Instant", L"Charge", L"Channel", L"Place", L"Enhance" };
        wchar_t buffer[128];
        swprintf_s(buffer, 128, L"[Skill] Activation type changed to: %s\n", typeNames[static_cast<int>(type)]);
        OutputDebugString(buffer);
    }
}

float SkillComponent::GetChargeProgress() const
{
    if (!m_bIsCharging) return 0.0f;
    return min(1.0f, m_fChargeTime / m_fMaxChargeTime);
}

void SkillComponent::ProcessRuneInput(InputSystem* pInputSystem)
{
    // 1-5 keys change activation type
    if (pInputSystem->IsKeyDown('1'))
    {
        SetActivationType(ActivationType::Instant);
    }
    else if (pInputSystem->IsKeyDown('2'))
    {
        SetActivationType(ActivationType::Charge);
    }
    else if (pInputSystem->IsKeyDown('3'))
    {
        SetActivationType(ActivationType::Channel);
    }
    else if (pInputSystem->IsKeyDown('4'))
    {
        SetActivationType(ActivationType::Place);
    }
    else if (pInputSystem->IsKeyDown('5'))
    {
        SetActivationType(ActivationType::Enhance);
    }
}

void SkillComponent::ExecuteWithActivationType(SkillSlot slot, const DirectX::XMFLOAT3& targetPosition)
{
    size_t index = static_cast<size_t>(slot);

    // Check if skill exists and is ready
    if (index >= m_Skills.size() || !m_Skills[index])
    {
        return;
    }

    if (m_SkillStates[index] != SkillState::Ready)
    {
        return;
    }

    float damageMultiplier = 1.0f;

    // Apply enhance multiplier if active
    if (m_bIsEnhanced)
    {
        damageMultiplier = m_fEnhanceMultiplier;
        m_bIsEnhanced = false;  // Consume enhancement
        m_fEnhanceTimer = 0.0f;
        OutputDebugString(L"[Skill] Enhancement consumed! 2x damage!\n");
    }

    switch (m_CurrentActivationType)
    {
    case ActivationType::Instant:
        // Immediate execution
        m_Skills[index]->Execute(m_pOwner, targetPosition, damageMultiplier);
        m_SkillStates[index] = SkillState::Casting;
        m_ActiveSkillSlot = slot;
        OutputDebugString(L"[Skill] Instant cast!\n");
        break;

    case ActivationType::Charge:
        // Start charging
        m_bIsCharging = true;
        m_fChargeTime = 0.0f;
        m_ChargingSlot = slot;
        m_ChargeTargetPosition = targetPosition;
        m_SkillStates[index] = SkillState::Casting;
        OutputDebugString(L"[Skill] Charging started... Hold to charge, release to fire\n");
        break;

    case ActivationType::Channel:
        // Start channeling
        m_bIsChanneling = true;
        m_fChannelTime = 0.0f;
        m_fChannelTickAccum = 0.0f;
        m_ActiveSkillSlot = slot;
        m_ChannelTargetPosition = targetPosition;  // Store target for channel ticks
        m_SkillStates[index] = SkillState::Casting;
        OutputDebugString(L"[Skill] Channeling started... Hold to continue\n");
        // First tick immediately
        m_Skills[index]->Execute(m_pOwner, targetPosition, damageMultiplier * 0.3f);  // 30% damage per tick
        break;

    case ActivationType::Place:
        // Place at target location (trap/turret style)
        {
            OutputDebugString(L"[Skill] Placing at target location!\n");
            // Execute with special flag for placement (use negative multiplier as signal)
            m_Skills[index]->Execute(m_pOwner, targetPosition, -1.0f);  // -1 signals placement mode
            m_SkillStates[index] = SkillState::Casting;
            m_ActiveSkillSlot = slot;
        }
        break;

    case ActivationType::Enhance:
        // Self-buff - enhance next attack
        m_bIsEnhanced = true;
        m_fEnhanceTimer = m_fEnhanceDuration;
        m_SkillStates[index] = SkillState::Casting;
        m_ActiveSkillSlot = slot;

        // Visual feedback - execute at self position with 0 damage (just for VFX)
        {
            DirectX::XMFLOAT3 selfPos = m_pOwner->GetTransform()->GetPosition();
            m_Skills[index]->Execute(m_pOwner, selfPos, 0.0f);  // 0 damage, just VFX
        }
        OutputDebugString(L"[Skill] Enhanced! Next attack deals 2x damage for 5 seconds\n");
        break;
    }
}
