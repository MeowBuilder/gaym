#pragma once

#include "ISkillBehavior.h"
#include "SkillData.h"

class FluidSkillVFXManager;
class Scene;

// R 슬롯 - 메테오 (상공에서 낙하, OrbitalCP -> Gravity 폭발)
class MeteorBehavior : public ISkillBehavior
{
public:
    MeteorBehavior();
    MeteorBehavior(const SkillData& customData);
    virtual ~MeteorBehavior() = default;

    void SetVFXManager(FluidSkillVFXManager* mgr) { m_pVFXManager = mgr; }
    void SetScene(Scene* pScene)                  { m_pScene = pScene; }

    // ISkillBehavior 인터페이스
    virtual void Execute(GameObject* caster, const DirectX::XMFLOAT3& targetPosition, float damageMultiplier = 1.0f) override;
    virtual void Update(float deltaTime) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;
    virtual const SkillData& GetSkillData() const override { return m_SkillData; }

private:
    uint32_t GetRuneFlags(GameObject* caster) const;
    void     ApplyExplosionDamage(float damage, float radius, bool bTriggerStagger);

    SkillData m_SkillData;
    bool m_bIsFinished = true;
    FluidSkillVFXManager* m_pVFXManager = nullptr;
    Scene*    m_pScene  = nullptr;
    int m_vfxId = -1;

    // 히트 판정용
    XMFLOAT3  m_targetPos    = {};
    float     m_damageMult   = 1.f;
    float     m_elapsed      = 0.f;
    bool      m_bExploded    = false;   // 초기 폭발 AoE 처리 완료
    float     m_hitTimer     = 0.f;

    static constexpr float METEOR_SPAWN_HEIGHT = 50.f;
    static constexpr float METEOR_FORWARD_DIST = 15.f;
    static constexpr float FALL_DURATION       = 3.0f;  // VFXLibrary Phase0 duration
    static constexpr float EXPLODE_DURATION    = 1.2f;  // VFXLibrary Phase1 duration
    static constexpr float MULTI_HIT_INTERVAL  = 0.3f;  // 폭발 중 다단히트 간격 (초)
    static constexpr float EXPLODE_RADIUS      = 10.0f; // 초기 폭발 반경 (SkillData.radius)
    static constexpr float MULTI_HIT_RADIUS    = 8.0f;  // 다단히트 반경
};
