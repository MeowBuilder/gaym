#include "stdafx.h"
#include "SkillComponent.h"
#include "ISkillBehavior.h"
#include "SkillData.h"
#include "InputSystem.h"
#include "Camera.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "Dx12App.h" // For runtime window size
#include "NetworkManager.h" // For skill sync

SkillComponent::SkillComponent(GameObject* pOwner)
    : Component(pOwner)
    , m_ChargeTargetPosition{ 0.0f, 0.0f, 0.0f }
    , m_ChannelTargetPosition{ 0.0f, 0.0f, 0.0f }
{
    // Initialize all cooldowns to 0 (ready)
    m_CooldownTimers.fill(0.0f);

    // Initialize all skill states to Ready
    m_SkillStates.fill(SkillState::Ready);

    // Initialize all rune slots to None (empty)
    for (auto& skillRunes : m_SkillRunes)
    {
        skillRunes.fill(ActivationType::None);
    }
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
                // Combo-based channel tick damage
                RuneCombo combo = GetRuneCombo(m_ActiveSkillSlot);
                float tickMult = 0.3f;
                if (combo.hasEnhance) tickMult *= 2.0f;

                if (combo.hasPlace)
                {
                    m_Skills[index]->Execute(m_pOwner, m_ChannelTargetPosition, -(tickMult * 1.5f));
                }
                else
                {
                    ExecuteOrSplit(index, m_ChannelTargetPosition, tickMult);
                }
            }
        }

        // 채널링 중에도 스킬 Update() 호출 (방향 추적 등)
        {
            size_t chIndex = static_cast<size_t>(m_ActiveSkillSlot);
            if (chIndex < m_Skills.size() && m_Skills[chIndex])
            {
                m_Skills[chIndex]->Update(deltaTime);
            }
        }

        // 채널링 중 네트워크 동기화 (방향 업데이트 전송)
        NetworkManager* pNetMgr = NetworkManager::GetInstance();
        if (pNetMgr && pNetMgr->IsConnected() && m_pOwner)
        {
            TransformComponent* pTransform = m_pOwner->GetTransform();
            if (pTransform)
            {
                int skillType = 2;  // E = SKILL_TYPE_E
                const DirectX::XMFLOAT3& pos = pTransform->GetPosition();
                DirectX::XMVECTOR lookVec = pTransform->GetLook();
                DirectX::XMFLOAT3 lookDir;
                DirectX::XMStoreFloat3(&lookDir, lookVec);

                pNetMgr->SendSkill(skillType, pos.x, pos.y, pos.z, lookDir.x, lookDir.y, lookDir.z);
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
            if (index < m_Skills.size() && m_Skills[index])
            {
                RuneCombo combo = GetRuneCombo(m_ChargingSlot);

                float chargeRatio = m_fChargeTime / m_fMaxChargeTime;
                chargeRatio = min(1.0f, chargeRatio);

                // Apply charge multiplier (1.0x to 3.0x based on charge)
                float damageMultiplier = 1.0f + chargeRatio * 2.0f;

                // Combo: Charge+Enhance rune
                if (combo.hasEnhance) damageMultiplier *= 2.0f;

                // Consume existing enhance buff
                if (m_bIsEnhanced)
                {
                    damageMultiplier *= m_fEnhanceMultiplier;
                    m_bIsEnhanced = false;
                    m_fEnhanceTimer = 0.0f;
                    OutputDebugString(L"[Skill] Enhancement consumed with Charge!\n");
                }

                wchar_t buffer[128];
                swprintf_s(buffer, 128, L"[Skill] Charge released! Charge: %.0f%%, Multiplier: %.1fx\n",
                    chargeRatio * 100.0f, damageMultiplier);
                OutputDebugString(buffer);

                // Combo: Charge+Place → trap with charge damage
                if (combo.hasPlace)
                {
                    m_Skills[index]->Execute(m_pOwner, m_ChargeTargetPosition, -(damageMultiplier * 1.5f));
                }
                else
                {
                    ExecuteOrSplit(index, m_ChargeTargetPosition, damageMultiplier);
                }
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
        m_ChannelTargetPosition = targetPos;  // 매 프레임 방향 업데이트
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
        m_Skills[index]->SetSlot(slot);
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
    // 런타임 윈도우 크기 사용 (고DPI/해상도 변경 대응)
    float windowWidth = static_cast<float>(Dx12App::GetInstance()->GetWindowWidth());
    float windowHeight = static_cast<float>(Dx12App::GetInstance()->GetWindowHeight());
    float ndcX = (2.0f * mousePos.x / windowWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mousePos.y / windowHeight);

    // Unproject from NDC to World Space to form a ray
    XMMATRIX viewMatrix = XMLoadFloat4x4(&pCamera->GetViewMatrix());
    XMMATRIX projMatrix = XMLoadFloat4x4(&pCamera->GetProjectionMatrix());
    XMMATRIX viewProjMatrix = viewMatrix * projMatrix;
    XMMATRIX invViewProjMatrix = XMMatrixInverse(nullptr, viewProjMatrix);

    XMVECTOR rayOrigin = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invViewProjMatrix);
    XMVECTOR rayEnd = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invViewProjMatrix);
    XMVECTOR rayDir = XMVector3Normalize(rayEnd - rayOrigin);

    // Define the ground plane at the owner's actual floor height
    float ownerY = (m_pOwner && m_pOwner->GetTransform())
        ? m_pOwner->GetTransform()->GetPosition().y : 0.0f;
    XMVECTOR groundPlane = XMPlaneFromPointNormal(XMVectorSet(0.0f, ownerY, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

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

        const wchar_t* typeNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance", L"Split" };
        wchar_t buffer[128];
        swprintf_s(buffer, 128, L"[Skill] Activation type changed to: %s\n", typeNames[static_cast<int>(type)]);
        OutputDebugString(buffer);
    }
}

void SkillComponent::SetRuneSlot(SkillSlot skill, int runeIndex, ActivationType type)
{
    size_t skillIdx = static_cast<size_t>(skill);
    if (skillIdx >= static_cast<size_t>(SkillSlot::Count) || runeIndex < 0 || runeIndex >= RUNES_PER_SKILL)
        return;

    m_SkillRunes[skillIdx][runeIndex] = type;

    const wchar_t* slotNames[] = { L"Q", L"E", L"R", L"RMB" };
    const wchar_t* typeNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance", L"Split" };
    wchar_t buffer[128];
    swprintf_s(buffer, 128, L"[Skill] Rune set: %s slot %d = %s\n",
        slotNames[skillIdx], runeIndex + 1, typeNames[static_cast<int>(type)]);
    OutputDebugString(buffer);
}

ActivationType SkillComponent::GetRuneSlot(SkillSlot skill, int runeIndex) const
{
    size_t skillIdx = static_cast<size_t>(skill);
    if (skillIdx >= static_cast<size_t>(SkillSlot::Count) || runeIndex < 0 || runeIndex >= RUNES_PER_SKILL)
        return ActivationType::None;

    return m_SkillRunes[skillIdx][runeIndex];
}

void SkillComponent::ClearRuneSlot(SkillSlot skill, int runeIndex)
{
    SetRuneSlot(skill, runeIndex, ActivationType::None);
}

int SkillComponent::GetEquippedRuneCount(SkillSlot skill) const
{
    size_t skillIdx = static_cast<size_t>(skill);
    if (skillIdx >= static_cast<size_t>(SkillSlot::Count))
        return 0;

    int count = 0;
    for (int i = 0; i < RUNES_PER_SKILL; ++i)
    {
        if (m_SkillRunes[skillIdx][i] != ActivationType::None)
            ++count;
    }
    return count;
}

ActivationType SkillComponent::GetSkillActivationType(SkillSlot skill) const
{
    RuneCombo combo = GetRuneCombo(skill);

    // Priority: Charge > Channel > Place > Enhance > Split > Instant
    if (combo.hasCharge)   return ActivationType::Charge;
    if (combo.hasChannel)  return ActivationType::Channel;
    if (combo.hasPlace)    return ActivationType::Place;
    if (combo.hasEnhance)  return ActivationType::Enhance;
    if (combo.hasSplit)    return ActivationType::Split;
    return ActivationType::Instant;
}

RuneCombo SkillComponent::GetRuneCombo(SkillSlot skill) const
{
    RuneCombo combo;
    size_t skillIdx = static_cast<size_t>(skill);
    if (skillIdx >= static_cast<size_t>(SkillSlot::Count))
        return combo;

    for (int i = 0; i < RUNES_PER_SKILL; ++i)
    {
        ActivationType type = m_SkillRunes[skillIdx][i];
        if (type == ActivationType::None) continue;

        switch (type)
        {
        case ActivationType::Instant:  combo.hasInstant = true; break;
        case ActivationType::Charge:   combo.hasCharge = true; break;
        case ActivationType::Channel:  combo.hasChannel = true; break;
        case ActivationType::Place:    combo.hasPlace = true; break;
        case ActivationType::Enhance:  combo.hasEnhance = true; break;
        case ActivationType::Split:    combo.hasSplit = true; break;
        }
        ++combo.count;
    }
    return combo;
}

float SkillComponent::GetChargeProgress() const
{
    if (!m_bIsCharging) return 0.0f;
    return min(1.0f, m_fChargeTime / m_fMaxChargeTime);
}

void SkillComponent::ProcessRuneInput(InputSystem* pInputSystem)
{
    // Rune input is now handled through the drop item UI system
    // This function is kept for potential future use
}

void SkillComponent::ExecuteOrSplit(size_t index, const XMFLOAT3& target, float mult)
{
    using namespace DirectX;

    RuneCombo combo = GetRuneCombo(static_cast<SkillSlot>(index));
    if (!combo.hasSplit) {
        m_Skills[index]->Execute(m_pOwner, target, mult);
        return;
    }
    // Split: 2개 투사체 좌우로 퍼뜨림
    XMVECTOR originV = (m_pOwner && m_pOwner->GetTransform())
        ? XMLoadFloat3(&m_pOwner->GetTransform()->GetPosition())
        : XMVectorZero();
    XMVECTOR toTarget = XMVector3Normalize(XMLoadFloat3(&target) - originV);
    XMVECTOR worldUp  = XMVectorSet(0, 1, 0, 0);
    float dot = XMVectorGetX(XMVector3Dot(toTarget, worldUp));
    XMVECTOR right = (fabsf(dot) > 0.99f)
        ? XMVectorSet(1, 0, 0, 0)
        : XMVector3Normalize(XMVector3Cross(worldUp, toTarget));
    constexpr float SPREAD = 1.5f;
    XMFLOAT3 t1, t2;
    XMStoreFloat3(&t1, XMLoadFloat3(&target) + right * SPREAD);
    XMStoreFloat3(&t2, XMLoadFloat3(&target) - right * SPREAD);
    m_Skills[index]->Execute(m_pOwner, t1, mult);
    m_Skills[index]->Execute(m_pOwner, t2, mult);
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

    RuneCombo combo = GetRuneCombo(slot);
    bool enhanceOnly = combo.hasEnhance && !combo.hasCharge && !combo.hasChannel && !combo.hasPlace && !combo.hasInstant;

    // 스킬의 기본 activationType을 fallback으로 사용
    ActivationType defaultType = (m_Skills[index])
        ? m_Skills[index]->GetSkillData().activationType
        : ActivationType::Instant;

    if (combo.hasCharge)
    {
        // Charge mode: hold-release (combo modifiers applied on release)
        m_bIsCharging = true;
        m_fChargeTime = 0.0f;
        m_ChargingSlot = slot;
        m_ChargeTargetPosition = targetPosition;
        m_SkillStates[index] = SkillState::Casting;
        OutputDebugString(L"[Skill] Charging started... Hold to charge, release to fire\n");
    }
    else if (combo.hasChannel || defaultType == ActivationType::Channel)
    {
        // Channel mode: hold-sustain, ticks apply combo modifiers
        m_bIsChanneling = true;
        m_fChannelTime = 0.0f;
        m_fChannelTickAccum = 0.0f;
        m_ActiveSkillSlot = slot;
        m_ChannelTargetPosition = targetPosition;
        m_SkillStates[index] = SkillState::Casting;
        OutputDebugString(L"[Skill] Channeling started... Hold to continue\n");

        // First tick immediately
        float tickMult = 0.3f;
        if (combo.hasEnhance) tickMult *= 2.0f;
        if (m_bIsEnhanced)
        {
            tickMult *= m_fEnhanceMultiplier;
            m_bIsEnhanced = false;
            m_fEnhanceTimer = 0.0f;
            OutputDebugString(L"[Skill] Enhancement consumed with Channel!\n");
        }

        if (combo.hasPlace)
        {
            m_Skills[index]->Execute(m_pOwner, targetPosition, -(tickMult * 1.5f));
        }
        else
        {
            ExecuteOrSplit(index, targetPosition, tickMult);
        }
    }
    else if (enhanceOnly)
    {
        // Enhance-only: self buff
        m_bIsEnhanced = true;
        m_fEnhanceTimer = m_fEnhanceDuration;
        m_SkillStates[index] = SkillState::Casting;
        m_ActiveSkillSlot = slot;

        DirectX::XMFLOAT3 selfPos = m_pOwner->GetTransform()->GetPosition();
        m_Skills[index]->Execute(m_pOwner, selfPos, 0.0f);
        OutputDebugString(L"[Skill] Enhanced! Next attack deals 2x damage for 5 seconds\n");
    }
    else
    {
        // Instant / Place / Instant+Place / Instant+Enhance etc.
        float damageMultiplier = 1.0f;
        if (combo.hasEnhance) damageMultiplier *= 2.0f;

        // Consume existing enhance buff
        if (m_bIsEnhanced)
        {
            damageMultiplier *= m_fEnhanceMultiplier;
            m_bIsEnhanced = false;
            m_fEnhanceTimer = 0.0f;
            OutputDebugString(L"[Skill] Enhancement consumed! 2x damage!\n");
        }

        if (combo.hasPlace)
        {
            OutputDebugString(L"[Skill] Placing at target location!\n");
            m_Skills[index]->Execute(m_pOwner, targetPosition, -(damageMultiplier * 1.5f));
        }
        else
        {
            ExecuteOrSplit(index, targetPosition, damageMultiplier);
            OutputDebugString(L"[Skill] Instant cast!\n");
        }
        m_SkillStates[index] = SkillState::Casting;
        m_ActiveSkillSlot = slot;
    }

    // 네트워크로 스킬 전송
    NetworkManager* pNetMgr = NetworkManager::GetInstance();
    if (pNetMgr && pNetMgr->IsConnected())
    {
        // 스킬 슬롯을 Protocol::SkillType으로 변환
        int skillType = 0;
        switch (slot)
        {
        case SkillSlot::Q:          skillType = 1; break; // SKILL_TYPE_Q
        case SkillSlot::E:          skillType = 2; break; // SKILL_TYPE_E
        case SkillSlot::R:          skillType = 3; break; // SKILL_TYPE_R
        case SkillSlot::RightClick: skillType = 4; break; // SKILL_TYPE_MOUSE_RIGHT
        default:                    skillType = 0; break;
        }

        // 플레이어 위치와 방향 가져오기
        TransformComponent* pTransform = m_pOwner->GetTransform();
        if (pTransform)
        {
            const DirectX::XMFLOAT3& pos = pTransform->GetPosition();
            DirectX::XMVECTOR lookVec = pTransform->GetLook();
            DirectX::XMFLOAT3 lookDir;
            DirectX::XMStoreFloat3(&lookDir, lookVec);

            pNetMgr->SendSkill(skillType, pos.x, pos.y, pos.z, lookDir.x, lookDir.y, lookDir.z);
        }
    }
}
