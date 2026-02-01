#pragma once

#include "Component.h"
#include "SkillTypes.h"
#include <array>

class DropItemComponent : public Component
{
public:
    DropItemComponent(GameObject* pOwner);
    virtual ~DropItemComponent();

    virtual void Update(float deltaTime) override;

    // Generate 3 random runes for selection
    void GenerateRandomRunes();

    // Get the rune options
    const std::array<ActivationType, 3>& GetRuneOptions() const { return m_RuneOptions; }
    ActivationType GetRuneOption(int index) const;

    // Check if drop is active (can be picked up)
    bool IsActive() const { return m_bIsActive; }
    void SetActive(bool active) { m_bIsActive = active; }

    // Animation helpers
    float GetBobOffset() const { return m_fBobOffset; }

private:
    std::array<ActivationType, 3> m_RuneOptions;
    bool m_bIsActive = true;

    // Floating animation
    float m_fBobTime = 0.0f;
    float m_fBobSpeed = 2.0f;
    float m_fBobAmplitude = 0.3f;
    float m_fBobOffset = 0.0f;
};
