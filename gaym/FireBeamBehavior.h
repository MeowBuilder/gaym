#pragma once

#include "ISkillBehavior.h"
#include "SkillData.h"

class FluidSkillVFXManager;

// E 슬롯 - 화염 빔 (Channel 방식, 키 누르는 동안 지속)
class FireBeamBehavior : public ISkillBehavior
{
public:
    FireBeamBehavior();
    FireBeamBehavior(const SkillData& customData);
    virtual ~FireBeamBehavior() = default;

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
    bool m_bIsActive = false;      // 빔이 현재 활성 중인지
    FluidSkillVFXManager* m_pVFXManager = nullptr;
    int m_vfxId = -1;
    GameObject* m_pCaster = nullptr;  // 빔 추적용 caster 캐시
    XMFLOAT3 m_lastTargetPos = { 0.f, 0.f, 0.f }; // 마지막 타겟 위치 캐시
};
