#pragma once

#include "ISkillBehavior.h"
#include "SkillData.h"

class FluidSkillVFXManager;

// R 슬롯 - 메테오 (상공에서 낙하, OrbitalCP -> Gravity 폭발)
class MeteorBehavior : public ISkillBehavior
{
public:
    MeteorBehavior();
    MeteorBehavior(const SkillData& customData);
    virtual ~MeteorBehavior() = default;

    void SetVFXManager(FluidSkillVFXManager* mgr) { m_pVFXManager = mgr; }

    // ISkillBehavior 인터페이스
    virtual void Execute(GameObject* caster, const DirectX::XMFLOAT3& targetPosition, float damageMultiplier = 1.0f) override;
    virtual void Update(float deltaTime) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;
    virtual const SkillData& GetSkillData() const override { return m_SkillData; }

private:
    uint32_t GetRuneFlags(GameObject* caster) const;

    SkillData m_SkillData;
    bool m_bIsFinished = true;
    FluidSkillVFXManager* m_pVFXManager = nullptr;
    int m_vfxId = -1;

    static constexpr float METEOR_SPAWN_HEIGHT = 50.f;    // 타겟 상공 높이
    static constexpr float METEOR_FORWARD_DIST = 15.f;    // 전방 기본 거리
};
