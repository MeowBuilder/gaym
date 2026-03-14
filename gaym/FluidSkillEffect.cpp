#include "stdafx.h"
#include "FluidSkillEffect.h"
#include "FluidParticleSystem.h"
#include "SkillComponent.h"
#include "ISkillBehavior.h"
#include "SkillData.h"
#include <algorithm>

// ============================================================================
// Init
// ============================================================================
void FluidSkillEffect::Init(FluidParticleSystem* pFluid, SkillComponent* pSkill)
{
    m_pFluid = pFluid;
    m_pSkill = pSkill;
}

// ============================================================================
// 헬퍼: 제어점 개수
// 장착 룬 합산 + 1 (기본 1개, 최대 5개)
// ============================================================================
int FluidSkillEffect::ComputeCPCount() const
{
    if (!m_pSkill) return 1;

    int total = 0;
    for (int s = 0; s < static_cast<int>(SkillSlot::Count); ++s)
        total += m_pSkill->GetEquippedRuneCount(static_cast<SkillSlot>(s));

    return (std::min)(total + 1, 5);
}

// ============================================================================
// 헬퍼: 원소 타입 (Q 슬롯 스킬 기준)
// ============================================================================
ElementType FluidSkillEffect::ComputeElement() const
{
    if (!m_pSkill) return ElementType::None;

    const ISkillBehavior* pBehavior = m_pSkill->GetSkill(SkillSlot::Q);
    if (pBehavior)
        return pBehavior->GetSkillData().element;

    return ElementType::None;
}

// ============================================================================
// 제어점 계산 & 시스템에 적용
// ============================================================================
void FluidSkillEffect::PushControlPoints(const XMFLOAT3& center)
{
    if (!m_pFluid || !m_pSkill) return;

    const bool  isChanneling  = m_pSkill->IsChanneling();
    const bool  isCharging    = m_pSkill->IsCharging();
    const bool  isEnhanced    = m_pSkill->IsEnhanced();
    const float chargeRatio   = m_pSkill->GetChargeProgress();  // 0..1

    const int cpCount = ComputeCPCount();

    // 궤도 반지름: Enhance 시 확대, Charge 시 파티클이 가운데로 모임
    float orbitRadius  = isEnhanced ? 4.2f : 2.8f;
    if (isCharging)
        orbitRadius *= (1.0f - chargeRatio * 0.45f);  // 최대 55% 까지 축소

    // 인력 강도: Charge 시 점점 강해져 파티클이 빠르게 수렴
    float strength = 12.0f;
    if (isCharging)   strength += 22.0f * chargeRatio;
    if (isEnhanced)   strength += 6.0f;

    // 구 경계 반지름
    float sphereRadius = isEnhanced ? 4.8f : 3.2f;

    std::vector<FluidControlPoint> cps;
    cps.reserve(cpCount);

    const float step = (cpCount > 1)
        ? (2.0f * XM_PI / static_cast<float>(cpCount))
        : 0.0f;

    for (int i = 0; i < cpCount; ++i)
    {
        float angle = m_fOrbitAngle + step * static_cast<float>(i);

        // 개별 제어점에 살짝 다른 수직 위상을 주어 자연스러운 흔들림
        float heightOffset = 1.5f + sinf(angle * 0.6f + static_cast<float>(i) * 1.1f) * 0.5f;

        FluidControlPoint cp;
        cp.position = {
            center.x + cosf(angle) * orbitRadius,
            center.y + heightOffset,
            center.z + sinf(angle) * orbitRadius
        };
        cp.attractionStrength = strength;
        cp.sphereRadius       = sphereRadius;
        cps.push_back(cp);
    }

    m_pFluid->SetControlPoints(cps);
}

// ============================================================================
// Update — 매 프레임 호출
// ============================================================================
void FluidSkillEffect::Update(float deltaTime, const XMFLOAT3& playerPos)
{
    if (!m_pFluid || !m_pSkill) return;

    // ── 첫 프레임: 파티클 스폰 ───────────────────────────────────────────────
    if (!m_bInitialized)
    {
        ElementType elem = ComputeElement();

        FluidParticleConfig cfg;
        cfg.element       = elem;
        cfg.particleCount = 200;
        cfg.spawnRadius   = 2.2f;

        PushControlPoints(playerPos);
        m_pFluid->Spawn(playerPos, cfg);

        m_bInitialized = true;
        m_eLastElement = elem;
        m_nLastCPCount = ComputeCPCount();
        return;
    }

    // ── 원소 변경 감지 → 재스폰 (색상 전환) ─────────────────────────────────
    ElementType curElem = ComputeElement();
    if (curElem != m_eLastElement)
    {
        FluidParticleConfig cfg;
        cfg.element       = curElem;
        cfg.particleCount = 200;
        cfg.spawnRadius   = 2.2f;

        PushControlPoints(playerPos);
        m_pFluid->Spawn(playerPos, cfg);

        m_eLastElement = curElem;
        m_nLastCPCount = ComputeCPCount();
        return;
    }

    // ── 궤도 속도 결정 ────────────────────────────────────────────────────────
    // 기본 0.7 rad/s → Channel 중 3× 가속, Charge 중 0.25× 감속
    float orbitSpeed = 0.7f;
    if (m_pSkill->IsChanneling()) orbitSpeed = 2.2f;
    if (m_pSkill->IsCharging())   orbitSpeed = 0.18f;

    m_fOrbitAngle += orbitSpeed * deltaTime;

    m_nLastCPCount = ComputeCPCount();
    PushControlPoints(playerPos);
}
