#pragma once
#include "IAttackBehavior.h"
#include <DirectXMath.h>
#include <vector>
#include <string>

using namespace DirectX;

// Combo attack: boss performs a series of melee attacks in sequence
class ComboAttackBehavior : public IAttackBehavior
{
public:
    struct ComboHit
    {
        float fDamage = 10.0f;
        float fWindupTime = 0.15f;
        float fHitTime = 0.1f;
        float fRecoveryTime = 0.1f;
        float fHitRange = 4.0f;
        float fConeAngle = 90.0f;
        std::string strAnimation = "Attack 1";
        bool bTrackTarget = true;  // Re-face target before this hit

        // 전방 사각형 판정 모드 — 둘 다 >0 이면 cone 대신 사각형 사용
        float fRectWidthHalf = 0.0f;
        float fRectLength    = 0.0f;
    };

    ComboAttackBehavior(const std::vector<ComboHit>& hits);
    virtual ~ComboAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;
    // 첫 hit의 애니메이션을 반환 — EnemyComponent state 머신이 Attack 진입 시 올바른 클립 재생
    virtual const char* GetAnimClipName() const override {
        return m_vHits.empty() ? "" : m_vHits[0].strAnimation.c_str();
    }
    // 첫 hit의 windup 시간을 fill 기준으로 (이후 hit는 같은 애니 연속 재생이라 동일 텔레그래프 사용)
    virtual float GetTimeToHit() const override {
        return m_vHits.empty() ? 0.0f : m_vHits[0].fWindupTime;
    }

    // Builder helper for creating combos
    static ComboAttackBehavior* CreateLightCombo();   // Fast 3-hit combo
    static ComboAttackBehavior* CreateHeavyCombo();   // Slow 2-hit high damage
    static ComboAttackBehavior* CreateFuryCombo();    // Fast 5-hit flurry

private:
    void DealConeDamage(EnemyComponent* pEnemy, const ComboHit& hit);

private:
    std::vector<ComboHit> m_vHits;

    // Runtime state
    enum class HitPhase { Windup, Hit, Recovery };
    HitPhase m_eHitPhase = HitPhase::Windup;
    int m_nCurrentHit = 0;
    float m_fTimer = 0.0f;
    bool m_bHitDealt = false;
    bool m_bFinished = false;
};
