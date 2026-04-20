#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>

using namespace DirectX;

// 측면 사이드스매시 — 플레이어가 있는 쪽(보스 기준 좌/우)으로 45° 회전하며 내려찍기
//   · 전방 사각형 판정 (windup 끝에서 보스 최종 yaw 기준으로 고정)
//   · 애니: turn_45_left_Smash_Attack / turn_45_Right_Smash_Attack
//   · windup 내내 부드럽게 몸을 틀며 인디케이터도 함께 기울어짐 → 회피 방향 텔레그래프
//   · 플레이어가 거의 중앙일 땐 50/50 랜덤 → 사이드스텝 예측 불가 압박
class SideSmashAttackBehavior : public IAttackBehavior
{
public:
    SideSmashAttackBehavior(float fDamage              = 60.0f,
                            float fTiltAngle           = 45.0f,   // 보스 yaw 가 얼마나 틀어질지 (deg)
                            float fRectWidthHalf       = 12.0f,   // 사각형 반폭
                            float fRectLength          = 28.0f,   // 전방 길이
                            float fWindupTime          = 1.2f,
                            float fSlamTime            = 0.3f,
                            float fRecoveryTime        = 0.8f,
                            float fCameraShakeIntensity = 0.25f,
                            float fCameraShakeDuration  = 0.35f,
                            float fAnimPlaybackSpeed    = 0.0f);
    virtual ~SideSmashAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;

    virtual const char* GetAnimClipName()  const override { return m_strClipName; }
    virtual float       GetTimeToHit()     const override { return m_fWindupTime; }
    virtual float       GetIndicatorRadius() const override { return m_fRectWidthHalf; }
    virtual float       GetIndicatorLength() const override { return m_fRectLength; }
    virtual int         GetIndicatorTypeOverride() const override { return 4; }   // ForwardBox
    virtual bool        ShouldLoopAnim()   const override { return false; }

private:
    enum class Side  { Left, Right };
    enum class Phase { Windup, Slam, Recovery };

    Side PickSide(EnemyComponent* pEnemy) const;
    void DealSmashDamage(EnemyComponent* pEnemy);

    // Parameters
    float m_fDamage;
    float m_fTiltAngle;
    float m_fRectWidthHalf;
    float m_fRectLength;
    float m_fWindupTime;
    float m_fSlamTime;
    float m_fRecoveryTime;
    float m_fCameraShakeIntensity;
    float m_fCameraShakeDuration;
    float m_fAnimPlaybackSpeed;

    // Runtime
    Side  m_eSide      = Side::Right;
    Phase m_ePhase     = Phase::Windup;
    float m_fTimer     = 0.0f;
    float m_fStartYaw  = 0.0f;
    float m_fTargetYaw = 0.0f;
    bool  m_bHitDealt  = false;
    bool  m_bFinished  = false;
    const char* m_strClipName = "turn_45_Right_Smash_Attack";
};
