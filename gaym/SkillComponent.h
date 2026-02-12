#pragma once

#include "Component.h"
#include "SkillTypes.h"
#include <memory>
#include <array>

class ISkillBehavior;
class InputSystem;
class CCamera;

// Number of rune slots per skill
constexpr int RUNES_PER_SKILL = 3;

// Combo flags from multiple runes equipped on a single skill
struct RuneCombo {
    bool hasInstant = false;
    bool hasCharge = false;
    bool hasChannel = false;
    bool hasPlace = false;
    bool hasEnhance = false;
    int count = 0;  // Total equipped rune count
};

// Component that manages skill slots and execution for a GameObject
class SkillComponent : public Component
{
public:
    SkillComponent(GameObject* pOwner);
    virtual ~SkillComponent();

    virtual void Update(float deltaTime) override;

    // Process skill input from the input system
    // Should be called in the player's update with access to input and camera
    void ProcessSkillInput(InputSystem* pInputSystem, CCamera* pCamera);

    // Equip a skill behavior to a specific slot
    void EquipSkill(SkillSlot slot, std::unique_ptr<ISkillBehavior> pBehavior);

    // Remove skill from slot
    void UnequipSkill(SkillSlot slot);

    // Get skill behavior from slot
    ISkillBehavior* GetSkill(SkillSlot slot) const;

    // Check if a skill is ready to use
    bool IsSkillReady(SkillSlot slot) const;

    // Get remaining cooldown for a skill
    float GetCooldownRemaining(SkillSlot slot) const;

    // Get cooldown progress (0.0 = just used, 1.0 = ready)
    float GetCooldownProgress(SkillSlot slot) const;

    // Rune/Activation type management (legacy - uses first equipped rune)
    void SetActivationType(ActivationType type);
    ActivationType GetActivationType() const { return m_CurrentActivationType; }

    // Per-skill rune slot management
    void SetRuneSlot(SkillSlot skill, int runeIndex, ActivationType type);
    ActivationType GetRuneSlot(SkillSlot skill, int runeIndex) const;
    void ClearRuneSlot(SkillSlot skill, int runeIndex);
    int GetEquippedRuneCount(SkillSlot skill) const;

    // Get combined activation type for a skill (based on equipped runes)
    ActivationType GetSkillActivationType(SkillSlot skill) const;

    // Get rune combo flags for a skill (all equipped rune types)
    RuneCombo GetRuneCombo(SkillSlot skill) const;

    // Block rune input (e.g., during drop rune selection)
    void SetRuneInputBlocked(bool blocked) { m_bRuneInputBlocked = blocked; }
    bool IsRuneInputBlocked() const { return m_bRuneInputBlocked; }

    // Charge state (for Charge activation type)
    bool IsCharging() const { return m_bIsCharging; }
    float GetChargeProgress() const;

    // Channel state
    bool IsChanneling() const { return m_bIsChanneling; }
    float GetChannelProgress() const { return (m_fChannelDuration > 0.0f) ? (m_fChannelTime / m_fChannelDuration) : 0.0f; }

    // Enhance state
    bool IsEnhanced() const { return m_bIsEnhanced; }
    float GetEnhanceTimeRemaining() const { return m_fEnhanceTimer; }

private:
    // Try to use a skill in the given slot
    bool TryUseSkill(SkillSlot slot, const DirectX::XMFLOAT3& targetPosition);

    // Calculate target position from mouse cursor (ray-plane intersection)
    DirectX::XMFLOAT3 CalculateTargetPosition(InputSystem* pInputSystem, CCamera* pCamera) const;

    // Check if the input key for a skill slot is pressed
    bool IsSkillKeyPressed(SkillSlot slot, InputSystem* pInputSystem) const;

private:
    // Skill slots
    std::array<std::unique_ptr<ISkillBehavior>, static_cast<size_t>(SkillSlot::Count)> m_Skills;

    // Cooldown timers for each slot
    std::array<float, static_cast<size_t>(SkillSlot::Count)> m_CooldownTimers;

    // Skill states
    std::array<SkillState, static_cast<size_t>(SkillSlot::Count)> m_SkillStates;

    // Currently active skill (if any)
    SkillSlot m_ActiveSkillSlot = SkillSlot::Count;  // Count means no active skill

    // Current activation type (set by rune) - legacy, for backwards compatibility
    ActivationType m_CurrentActivationType = ActivationType::Instant;

    // Per-skill rune slots: [skill][runeSlot] = ActivationType
    // None = empty slot
    std::array<std::array<ActivationType, RUNES_PER_SKILL>, static_cast<size_t>(SkillSlot::Count)> m_SkillRunes;

    // Charge system
    bool m_bIsCharging = false;
    float m_fChargeTime = 0.0f;
    float m_fMaxChargeTime = 1.5f;  // Time to full charge
    SkillSlot m_ChargingSlot = SkillSlot::Count;
    DirectX::XMFLOAT3 m_ChargeTargetPosition;

    // Channel system
    bool m_bIsChanneling = false;
    float m_fChannelTime = 0.0f;
    float m_fChannelDuration = 2.0f;  // Total channel duration
    float m_fChannelTickRate = 0.2f;  // Time between ticks
    float m_fChannelTickAccum = 0.0f;
    DirectX::XMFLOAT3 m_ChannelTargetPosition;  // Stored target for channeling

    // Enhance system
    bool m_bIsEnhanced = false;
    float m_fEnhanceTimer = 0.0f;
    float m_fEnhanceDuration = 5.0f;  // Enhancement lasts 5 seconds
    float m_fEnhanceMultiplier = 2.0f;  // Damage multiplier

    // Process rune input (1-5 keys)
    void ProcessRuneInput(InputSystem* pInputSystem);

    // Rune input blocking
    bool m_bRuneInputBlocked = false;

    // Execute skill based on current activation type
    void ExecuteWithActivationType(SkillSlot slot, const DirectX::XMFLOAT3& targetPosition);
};
