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

    // Reset velocity when teleported — 중력이 플레이어를 바닥까지 끌어내리도록 onGround=false
    // (텔레포트 Y가 바닥보다 높으면 gravity가 자연스럽게 스냅; Y=0이면 즉시 ground 판정)
    void ResetGroundY() { m_fVelocityY = 0.0f; m_bOnGround = false; }

    // Fall zone: safe AABB(center±extents) 안 = 수면에 뜸(차오르는 물 따라 상승),
    // 밖 = 중력 낙하 → y<FALL_DEATH_Y 도달 시 즉사. 크라켄 WaterRise/전투에서만 활성화.
    void EnableFallZone(const XMFLOAT3& safeCenter, const XMFLOAT3& safeExtents);
    void DisableFallZone() { m_bFallZoneActive = false; m_fFallZoneWaterY = -1e9f; }
    void SetFallZoneWaterY(float waterY) { m_fFallZoneWaterY = waterY; }
    bool IsFallZoneActive() const { return m_bFallZoneActive; }

private:
    float m_fMaxHP = 100.0f;
    float m_fCurrentHP = 100.0f;

    // Gravity system
    float m_fVelocityY = 0.0f;
    bool m_bOnGround = false;
    static constexpr float GRAVITY = 50.0f;
    static constexpr float GROUND_Y = 0.0f;  // Tile surface height

    // Fall zone (water-boss)
    bool m_bFallZoneActive = false;
    XMFLOAT3 m_xmf3SafeCenter  = {};
    XMFLOAT3 m_xmf3SafeExtents = {};
    float m_fFallZoneWaterY = -1e9f;  // 현재 수면 Y (Scene이 매 프레임 갱신)
    static constexpr float FALL_DEATH_Y = -10.0f;  // 맵 바닥(-4) 훨씬 아래로 떨어지면 즉사

    // Animation state machine
    PlayerAnimState m_eAnimState = PlayerAnimState::Idle;
    float m_fAttackTimer = 0.0f;
    static constexpr float kAttackAnimDuration = 0.92f;  // Attack1 clip duration

    // 네트워크 회전 동기화용 (이전 프레임 Y 회전값)
    float m_fPrevYaw = 0.0f;
    static constexpr float YAW_SYNC_THRESHOLD = 1.0f;  // 1도 이상 변화 시 동기화

    void UpdateAnimation(float deltaTime, bool bMoving, bool bAttackTriggered);
};
