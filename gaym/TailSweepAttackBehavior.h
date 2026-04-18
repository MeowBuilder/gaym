#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>

using namespace DirectX;

// Tail sweep attack: boss spins and hits in 180-degree arc behind/around it
class TailSweepAttackBehavior : public IAttackBehavior
{
public:
    TailSweepAttackBehavior(float fDamage = 18.0f,
                            float fWindupTime = 0.4f,
                            float fSweepTime = 0.3f,
                            float fRecoveryTime = 0.4f,
                            float fHitRange = 6.0f,
                            float fSweepArc = 180.0f,    // Degrees
                            bool bHitBehind = true,      // Start from behind
                            const char* pClipOverride = nullptr,  // nullptr → "Tail Attack"
                            // 전방 직사각형 판정 모드 — 값이 설정되면 fHitRange/fSweepArc 대신 사각형 사용
                            float fRectWidthHalf = 0.0f,  // 사각형 반폭 (보스 측면 ± 이 값)
                            float fRectLength    = 0.0f); // 사각형 전방 길이
    virtual ~TailSweepAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;
    virtual const char* GetAnimClipName() const override { return m_strClipName; }
    // 데미지는 windup 후 sweep 중간 지점에서 발생
    virtual float GetTimeToHit() const override { return m_fWindupTime + m_fSweepTime * 0.5f; }

private:
    void DealSweepDamage(EnemyComponent* pEnemy);

private:
    // Parameters
    float m_fDamage = 18.0f;
    float m_fWindupTime = 0.4f;
    float m_fSweepTime = 0.3f;
    float m_fRecoveryTime = 0.4f;
    float m_fHitRange = 6.0f;
    float m_fSweepArc = 180.0f;
    bool m_bHitBehind = true;
    const char* m_strClipName = "Tail Attack";  // 기본 클립 (오버라이드 가능)
    float m_fRectWidthHalf = 0.0f;   // >0 이면 전방 사각형 판정 모드
    float m_fRectLength    = 0.0f;

    // Runtime state
    enum class Phase { Windup, Sweep, Recovery };
    Phase m_ePhase = Phase::Windup;
    float m_fTimer = 0.0f;
    float m_fInitialRotation = 0.0f;
    bool m_bHitDealt = false;
    bool m_bFinished = false;
};
