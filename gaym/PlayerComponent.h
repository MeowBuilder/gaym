#pragma once
#include "Component.h"

class InputSystem; // Forward declaration for InputSystem
class CCamera;     // Forward declaration for CCamera

enum class PlayerAnimState { Idle, Walk, Attack };

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

    // Call when player is teleported to a new map so the new floor Y is re-captured
    void ResetGroundY() { m_fGroundY = -FLT_MAX; }

private:
    float m_fMaxHP = 100.0f;
    float m_fCurrentHP = 100.0f;
    float m_fGroundY = -FLT_MAX;  // Captured on first update, used to lock Y

    // Animation state machine
    PlayerAnimState m_eAnimState = PlayerAnimState::Idle;
    float m_fAttackTimer = 0.0f;
    static constexpr float kAttackAnimDuration = 0.92f;  // Attack1 clip duration

    void UpdateAnimation(float deltaTime, bool bMoving, bool bAttackTriggered);
};
