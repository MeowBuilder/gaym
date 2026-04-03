#pragma once
#include "Component.h"
#include <DirectXMath.h>
#include <memory>
#include <functional>
#include <string>
#include <vector>
#include "ThreatSystem.h"

using namespace DirectX;

class IAttackBehavior;
class CRoom;
class AnimationComponent;
class BossPhaseController;
class BossPhaseConfig;

struct EnemyAnimationConfig
{
    std::string m_strIdleClip = "idle";
    std::string m_strChaseClip = "Run_Forward";
    std::string m_strAttackClip = "Combat_Unarmed_Attack";
    std::string m_strStaggerClip = "Combat_Stun";
    std::string m_strDeathClip = "Death";

    bool m_bLoopIdle = true;
    bool m_bLoopChase = true;
    bool m_bLoopAttack = false;
    bool m_bLoopStagger = false;
    bool m_bLoopDeath = false;
};

enum class IndicatorType
{
    None,
    Circle,      // Melee: circle around enemy
    RushCircle,  // Rush + 360 AoE: line + circle at destination
    RushCone     // Rush + cone: line + fan at destination
};

struct AttackIndicatorConfig
{
    IndicatorType m_eType = IndicatorType::None;
    float m_fRushDistance = 0.0f;    // Length of rush path (speed * duration)
    float m_fHitRadius = 0.0f;      // Radius for circle/AoE hit zone
    float m_fConeAngle = 0.0f;      // Cone angle in degrees (for RushCone)
};

enum class EnemyState
{
    Idle,
    Chase,
    Attack,
    Stagger,
    Dead
};

// Boss intro cutscene phases
enum class BossIntroPhase
{
    None,           // No intro (regular enemy)
    FlyingIn,       // Flying down from sky
    Landing,        // Landing animation
    Roaring,        // Scream/roar animation
    Done            // Intro complete, start combat
};

struct EnemyStats
{
    float m_fMaxHP = 100.0f;
    float m_fCurrentHP = 100.0f;
    float m_fMoveSpeed = 5.0f;
    float m_fAttackRange = 2.0f;
    float m_fAttackCooldown = 1.0f;
    float m_fDetectionRange = 50.0f;

    // Boss distance-based attack ranges
    float m_fLongRangeThreshold = 30.0f;   // 이 거리 이상: 비행/브레스 공격
    float m_fMidRangeThreshold = 15.0f;    // 이 거리 이상: 특수 공격
    // 이 거리 미만: 근접 공격
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

    // Threat (Aggro) System
    void RegisterAllPlayers(const std::vector<GameObject*>& players);
    void AddThreat(GameObject* pPlayer, float fAmount);
    void ReduceThreat(GameObject* pPlayer, float fAmount);
    ThreatTable& GetThreatTable() { return m_ThreatTable; }

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

    // Special attack behavior (for bosses)
    void SetSpecialAttackBehavior(std::unique_ptr<IAttackBehavior> pBehavior);
    IAttackBehavior* GetSpecialAttackBehavior() const { return m_pSpecialAttackBehavior.get(); }
    bool IsUsingSpecialAttack() const { return m_bUsingSpecialAttack; }

    // Flying attack behavior (for bosses with flight capability)
    void SetFlyingAttackBehavior(std::unique_ptr<IAttackBehavior> pBehavior);
    IAttackBehavior* GetFlyingAttackBehavior() const { return m_pFlyingAttackBehavior.get(); }
    bool IsUsingFlyingAttack() const { return m_bUsingFlyingAttack; }
    void SetFlyingAttackCooldown(float fCooldown) { m_fFlyingAttackCooldown = fCooldown; }
    void SetFlyingAttackChance(int nChance) { m_nFlyingAttackChance = nChance; }

    // Boss settings
    void SetBoss(bool bIsBoss) { m_bIsBoss = bIsBoss; }
    bool IsBoss() const { return m_bIsBoss; }

    // Boss Phase System
    void SetBossPhaseController(std::unique_ptr<BossPhaseController> pController);
    BossPhaseController* GetPhaseController() const { return m_pPhaseController.get(); }
    void SetSpeedMultiplier(float fMultiplier) { m_fSpeedMultiplier = fMultiplier; }
    float GetSpeedMultiplier() const { return m_fSpeedMultiplier; }
    bool CanUseFlyingAttack() const;  // 현재 페이즈에서 비행 공격 가능 여부

    // Invincibility (used during special attacks)
    void SetInvincible(bool bInvincible) { m_bInvincible = bInvincible; }
    bool IsInvincible() const { return m_bInvincible; }

    // Special attack cooldown
    void SetSpecialAttackCooldown(float fCooldown) { m_fSpecialAttackCooldown = fCooldown; }
    void SetSpecialAttackChance(int nChance) { m_nSpecialAttackChance = nChance; }  // 0-100

    // Room reference for death callback
    void SetRoom(CRoom* pRoom) { m_pRoom = pRoom; }
    CRoom* GetRoom() const { return m_pRoom; }

    // Death callback
    using DeathCallback = std::function<void(EnemyComponent*)>;
    void SetOnDeathCallback(DeathCallback callback) { m_OnDeathCallback = callback; }

    // AI utility
    float GetDistanceToTarget() const;
    void FaceTarget(float dt = 0.0f, bool bInstant = false);  // Smooth rotation towards target (dt=0 means instant)
    void MoveTowardsTarget(float dt);

    // Separation (avoid stacking with other enemies)
    void SetSeparationRadius(float fRadius) { m_fSeparationRadius = fRadius; }
    void SetSeparationStrength(float fStrength) { m_fSeparationStrength = fStrength; }

    // Animation
    void SetAnimationComponent(AnimationComponent* pAnimComp) { m_pAnimationComp = pAnimComp; }
    AnimationComponent* GetAnimationComponent() const { return m_pAnimationComp; }
    void SetAnimationConfig(const EnemyAnimationConfig& config) { m_AnimConfig = config; }

    // Flying mode
    void SetFlying(bool bFlying, float fHeight = 15.0f) { m_bIsFlying = bFlying; m_fFlyHeight = fHeight; }
    bool IsFlying() const { return m_bIsFlying; }

    // Boss intro cutscene
    void StartBossIntro(float fStartHeight = 30.0f);
    bool IsInIntro() const { return m_eIntroPhase != BossIntroPhase::None && m_eIntroPhase != BossIntroPhase::Done; }

    // Attack indicators
    void SetIndicatorConfig(const AttackIndicatorConfig& config) { m_IndicatorConfig = config; }
    void SetRushLineIndicator(GameObject* pIndicator) { m_pRushLineIndicator = pIndicator; }
    void SetHitZoneIndicator(GameObject* pIndicator) { m_pHitZoneIndicator = pIndicator; }

private:
    // State update functions
    void UpdateIdle(float dt);
    void UpdateChase(float dt);
    void UpdateAttack(float dt);
    void UpdateStagger(float dt);
    void UpdateDead(float dt);

    void Die();
    void ShowIndicators();
    void HideIndicators();

private:
    // Threat System
    ThreatTable m_ThreatTable;
    float m_fTargetReevaluationTimer = 0.0f;
    void ReevaluateTarget();

    EnemyState m_eCurrentState = EnemyState::Idle;
    EnemyStats m_Stats;
    std::unique_ptr<IAttackBehavior> m_pAttackBehavior;
    std::unique_ptr<IAttackBehavior> m_pSpecialAttackBehavior;  // Special pattern for bosses
    std::unique_ptr<IAttackBehavior> m_pFlyingAttackBehavior;   // Flying pattern for bosses
    GameObject* m_pTarget = nullptr;
    CRoom* m_pRoom = nullptr;

    // Boss flags
    bool m_bIsBoss = false;
    bool m_bInvincible = false;
    bool m_bUsingSpecialAttack = false;
    bool m_bUsingFlyingAttack = false;

    // Boss Phase System
    std::unique_ptr<BossPhaseController> m_pPhaseController;
    float m_fSpeedMultiplier = 1.0f;

    // Special attack parameters
    float m_fSpecialAttackCooldown = 10.0f;
    float m_fSpecialCooldownTimer = 0.0f;
    int m_nSpecialAttackChance = 30;  // 30% chance when cooldown ready

    // Flying attack parameters
    float m_fFlyingAttackCooldown = 15.0f;
    float m_fFlyingCooldownTimer = 0.0f;
    int m_nFlyingAttackChance = 30;

    // Animation
    AnimationComponent* m_pAnimationComp = nullptr;
    EnemyAnimationConfig m_AnimConfig;

    // Attack indicators
    AttackIndicatorConfig m_IndicatorConfig;
    GameObject* m_pRushLineIndicator = nullptr;
    GameObject* m_pHitZoneIndicator = nullptr;

    // Timers
    float m_fAttackCooldownTimer = 0.0f;
    float m_fStaggerTimer = 0.0f;
    float m_fDeadTimer = 0.0f;

    // Constants
    static constexpr float STAGGER_DURATION = 0.5f;
    static constexpr float DEAD_LINGER_TIME = 2.0f;

    // Separation (avoid stacking)
    float m_fSeparationRadius = 5.0f;    // Distance to start avoiding other enemies
    float m_fSeparationStrength = 10.0f; // Strength of the separation force

    // Smooth rotation
    float m_fRotationSpeed = 180.0f;     // Degrees per second

    // Callbacks
    DeathCallback m_OnDeathCallback;

    // Gravity system
    float m_fVelocityY = 0.0f;
    bool m_bOnGround = false;
    static constexpr float GRAVITY = 50.0f;
    static constexpr float GROUND_Y = 0.0f;  // Tile surface height

    // Flying mode
    bool m_bIsFlying = false;
    float m_fFlyHeight = 15.0f;

    // Boss intro cutscene
    BossIntroPhase m_eIntroPhase = BossIntroPhase::None;
    float m_fIntroTimer = 0.0f;
    float m_fIntroStartHeight = 30.0f;
    float m_fIntroTargetHeight = 0.0f;
    void UpdateBossIntro(float dt);
};
