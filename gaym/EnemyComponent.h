#pragma once
#include "Component.h"
#include <DirectXMath.h>
#include <memory>
#include <functional>

using namespace DirectX;

class IAttackBehavior;
class CRoom;

enum class EnemyState
{
    Idle,
    Chase,
    Attack,
    Stagger,
    Dead
};

struct EnemyStats
{
    float m_fMaxHP = 100.0f;
    float m_fCurrentHP = 100.0f;
    float m_fMoveSpeed = 5.0f;
    float m_fAttackRange = 2.0f;
    float m_fAttackCooldown = 1.0f;
    float m_fDetectionRange = 50.0f;
};

class EnemyComponent : public Component
{
public:
    EnemyComponent(GameObject* pOwner);
    virtual ~EnemyComponent();

    virtual void Update(float deltaTime) override;

    // State management
    void ChangeState(EnemyState newState);
    EnemyState GetState() const { return m_eCurrentState; }

    // Target management
    void SetTarget(GameObject* pTarget) { m_pTarget = pTarget; }
    GameObject* GetTarget() const { return m_pTarget; }

    // Stats
    void SetStats(const EnemyStats& stats) { m_Stats = stats; }
    EnemyStats& GetStats() { return m_Stats; }
    const EnemyStats& GetStats() const { return m_Stats; }

    // Damage handling
    void TakeDamage(float fDamage);
    bool IsDead() const { return m_eCurrentState == EnemyState::Dead; }

    // Attack behavior
    void SetAttackBehavior(std::unique_ptr<IAttackBehavior> pBehavior);
    IAttackBehavior* GetAttackBehavior() const { return m_pAttackBehavior.get(); }

    // Room reference for death callback
    void SetRoom(CRoom* pRoom) { m_pRoom = pRoom; }

    // Death callback
    using DeathCallback = std::function<void(EnemyComponent*)>;
    void SetOnDeathCallback(DeathCallback callback) { m_OnDeathCallback = callback; }

    // AI utility
    float GetDistanceToTarget() const;
    void FaceTarget();
    void MoveTowardsTarget(float dt);

private:
    // State update functions
    void UpdateIdle(float dt);
    void UpdateChase(float dt);
    void UpdateAttack(float dt);
    void UpdateStagger(float dt);
    void UpdateDead(float dt);

    void Die();

private:
    EnemyState m_eCurrentState = EnemyState::Idle;
    EnemyStats m_Stats;
    std::unique_ptr<IAttackBehavior> m_pAttackBehavior;
    GameObject* m_pTarget = nullptr;
    CRoom* m_pRoom = nullptr;

    // Timers
    float m_fAttackCooldownTimer = 0.0f;
    float m_fStaggerTimer = 0.0f;
    float m_fDeadTimer = 0.0f;

    // Constants
    static constexpr float STAGGER_DURATION = 0.5f;
    static constexpr float DEAD_LINGER_TIME = 2.0f;

    // Callbacks
    DeathCallback m_OnDeathCallback;
};
