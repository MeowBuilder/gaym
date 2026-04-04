#pragma once

#include "ISkillBehavior.h"
#include "SkillData.h"

class FluidSkillVFXManager;

// Q 슬롯 - 웨이브 슬래시 (박스 확장 VFX)
class WaveSlashBehavior : public ISkillBehavior
{
public:
    WaveSlashBehavior();
    WaveSlashBehavior(const SkillData& customData);
    virtual ~WaveSlashBehavior() = default;

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
};
