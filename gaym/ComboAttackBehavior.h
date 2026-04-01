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
    };

    ComboAttackBehavior(const std::vector<ComboHit>& hits);
    virtual ~ComboAttackBehavior() = default;

    virtual void Execute(EnemyComponent* pEnemy) override;
    virtual void Update(float dt, EnemyComponent* pEnemy) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;

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
