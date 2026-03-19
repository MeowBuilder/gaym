#pragma once

#include "stdafx.h"
#include "FluidParticle.h"
#include "SkillTypes.h"

class FluidParticleSystem;
class SkillComponent;

// Shows a fire ring around the player when Enhance rune is equipped.
// Clears automatically when no Enhance rune is present.
class FluidSkillEffect
{
public:
    void Init(FluidParticleSystem* pFluid, SkillComponent* pSkill);
    void Update(float deltaTime, const XMFLOAT3& playerPos);

private:
    void PushControlPoints(const XMFLOAT3& center);

    FluidParticleSystem* m_pFluid = nullptr;
    SkillComponent*      m_pSkill = nullptr;

    float    m_fOrbitAngle  = 0.0f;
    bool     m_bInitialized = false;
    XMFLOAT3 m_vLastPos     = { 0, 0, 0 };
};
