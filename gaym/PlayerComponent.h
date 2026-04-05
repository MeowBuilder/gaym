#pragma once
#include "Component.h"

class InputSystem; // Forward declaration for InputSystem
class CCamera;     // Forward declaration for CCamera
class SkillComponent; // Forward declaration for SkillComponent

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

    // Reset velocity when teleported
    void ResetGroundY() { m_fVelocityY = 0.0f; m_bOnGround = false; }

private:
    float m_fMaxHP = 100.0f;
    float m_fCurrentHP = 100.0f;

    // Gravity system
    float m_fVelocityY = 0.0f;
    bool m_bOnGround = false;
    static constexpr float GRAVITY = 50.0f;
    static constexpr float GROUND_Y = 0.0f;  // Tile surface height

    // Animation state machine
    PlayerAnimState m_eAnimState = PlayerAnimState::Idle;
    float m_fAttackTimer = 0.0f;
    static constexpr float kAttackAnimDuration = 0.92f;  // Attack1 clip duration

    // 네트워크 회전 동기화용 (이전 프레임 Y 회전값)
    float m_fPrevYaw = 0.0f;
    static constexpr float YAW_SYNC_THRESHOLD = 1.0f;  // 1도 이상 변화 시 동기화

    void UpdateAnimation(float deltaTime, bool bMoving, bool bAttackTriggered);
};
