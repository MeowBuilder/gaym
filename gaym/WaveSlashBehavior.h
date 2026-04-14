#pragma once

#include "ISkillBehavior.h"
#include "SkillData.h"

class FluidSkillVFXManager;
class Scene;

// Q 슬롯 - 웨이브 슬래시 (박스 확장 VFX)
class WaveSlashBehavior : public ISkillBehavior
{
public:
    WaveSlashBehavior();
    WaveSlashBehavior(const SkillData& customData);
    virtual ~WaveSlashBehavior() = default;

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
    void     HitEnemiesInWave(float damage);

    SkillData  m_SkillData;
    bool       m_bIsFinished = true;
    bool       m_bWaveActive = false;
    FluidSkillVFXManager* m_pVFXManager = nullptr;
    Scene*     m_pScene     = nullptr;
    int        m_vfxId      = -1;

    // 히트 판정용
    XMFLOAT3   m_waveOrigin  = {};
    XMFLOAT3   m_waveDir     = {0,0,1};
    float      m_damageMult  = 1.f;
    float      m_hitTimer    = 0.f;
    static constexpr float HIT_INTERVAL  = 0.4f;  // 다단히트 간격 (초)
    static constexpr float WAVE_HALF_W   = 5.0f;  // VFXLibrary waveHalfW 와 일치
    static constexpr float WAVE_HALF_H   = 3.0f;  // 수직 판정 범위 (약간 여유)
    // 파티클이 실제로 집중된 파도 선두 두께 (선두에서 뒤로 HIT_DEPTH만큼만 판정)
    static constexpr float WAVE_HIT_DEPTH = 5.0f;
};
