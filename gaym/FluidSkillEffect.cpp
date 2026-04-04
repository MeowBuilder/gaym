#include "stdafx.h"
#include "FluidSkillEffect.h"
#include "FluidParticleSystem.h"
#include "SkillComponent.h"

void FluidSkillEffect::Init(FluidParticleSystem* pFluid, SkillComponent* pSkill)
{
    m_pFluid = pFluid;
    m_pSkill = pSkill;
}

void FluidSkillEffect::PushControlPoints(const XMFLOAT3& center)
{
    if (!m_pFluid) return;

    constexpr int   RING_COUNT  = 8;
    constexpr float RING_RADIUS = 4.5f;

    std::vector<FluidControlPoint> cps;
    cps.reserve(RING_COUNT);

    for (int i = 0; i < RING_COUNT; ++i)
    {
        float angle = m_fOrbitAngle + (2.0f * XM_PI / RING_COUNT) * i;
        FluidControlPoint cp;
        cp.position = {
            center.x + cosf(angle) * RING_RADIUS,
            center.y + 0.8f,
            center.z + sinf(angle) * RING_RADIUS
        };
        cp.attractionStrength = 38.0f;
        cp.sphereRadius       = 1.2f;
        cps.push_back(cp);
    }

    m_pFluid->SetControlPoints(cps);
}

void FluidSkillEffect::Update(float deltaTime, const XMFLOAT3& playerPos)
{
    if (!m_pFluid || !m_pSkill) return;

    // Check if any slot has Enhance rune
    bool hasEnhance = false;
    for (int s = 0; s < static_cast<int>(SkillSlot::Count); ++s)
    {
        if (m_pSkill->GetRuneCombo(static_cast<SkillSlot>(s)).hasEnhance)
        {
            hasEnhance = true;
            break;
        }
    }

    if (!hasEnhance)
    {
        if (m_bInitialized)
        {
            m_pFluid->Clear();
            m_bInitialized = false;
        }
        return;
    }

    if (!m_bInitialized)
    {
        FluidParticleConfig cfg;
        cfg.element           = ElementType::Fire;
        cfg.particleCount     = 120;
        cfg.spawnRadius       = 0.4f;   // 링 근처에 밀집 스폰 → 위로 퍼지는 이펙트 방지
        cfg.smoothingRadius   = 1.2f;
        cfg.restDensity       = 7.0f;
        cfg.stiffness         = 50.0f;
        cfg.viscosity         = 0.25f;
        cfg.boundaryStiffness = 150.0f;
        cfg.particleSize      = 0.2f;

        // 링 평면 높이에서 스폰 → 파티클이 위로 퍼지지 않고 링으로 수렴
        XMFLOAT3 ringPlanePos = {
            playerPos.x,
            playerPos.y + 0.8f,
            playerPos.z
        };
        m_vLastPos = playerPos;
        PushControlPoints(playerPos);
        m_pFluid->Spawn(ringPlanePos, cfg);
        m_bInitialized = true;
        return;
    }

    // Co-move with player
    XMFLOAT3 delta = {
        playerPos.x - m_vLastPos.x,
        playerPos.y - m_vLastPos.y,
        playerPos.z - m_vLastPos.z
    };
    m_vLastPos = playerPos;
    if (fabsf(delta.x) + fabsf(delta.y) + fabsf(delta.z) > 0.0001f)
        m_pFluid->OffsetParticles(delta);

    m_fOrbitAngle += 1.2f * deltaTime;
    PushControlPoints(playerPos);
}
