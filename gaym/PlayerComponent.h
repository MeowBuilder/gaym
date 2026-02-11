#pragma once
#include "Component.h"

class InputSystem; // Forward declaration for InputSystem
class CCamera;     // Forward declaration for CCamera

class PlayerComponent : public Component
{
public:
    PlayerComponent(GameObject* pOwner);

    void PlayerUpdate(float deltaTime, InputSystem* pInputSystem, CCamera* pCamera);

    // HP System
    void TakeDamage(float fDamage);
    void Heal(float fAmount);
    float GetHPRatio() const { return m_fCurrentHP / m_fMaxHP; }
    float GetCurrentHP() const { return m_fCurrentHP; }
    float GetMaxHP() const { return m_fMaxHP; }
    bool IsDead() const { return m_fCurrentHP <= 0.0f; }

private:
    float m_fMaxHP = 100.0f;
    float m_fCurrentHP = 100.0f;
};
