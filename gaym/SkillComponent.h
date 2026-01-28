#pragma once

#include "Component.h"
#include "SkillTypes.h"
#include <memory>
#include <array>

class ISkillBehavior;
class InputSystem;
class CCamera;

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
};
