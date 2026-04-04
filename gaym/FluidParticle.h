#pragma once

#include <DirectXMath.h>
#include <vector>
#include "SkillTypes.h"

using namespace DirectX;

// GPU-side render data per particle (StructuredBuffer element)
struct FluidParticleRenderData
{
    XMFLOAT3 position;
    float    size;
    XMFLOAT4 color;
};  // 32 bytes, naturally aligned

// CPU-side simulation particle
struct FluidParticle
{
    XMFLOAT3 position   = { 0, 0, 0 };
    XMFLOAT3 velocity   = { 0, 0, 0 };
    XMFLOAT3 force      = { 0, 0, 0 };
    float    density    = 0.0f;
    float    pressure   = 0.0f;
    float    mass       = 1.0f;
    bool     active     = true;
    // Beam 모드 전용: 빔-로컬 좌표 (방향 변경 시 즉시 스냅)
    float    beamT      = 0.f;  // 빔 축 방향 진행 거리
    float    beamRx     = 0.f;  // 빔 수직 평면 X 오프셋
    float    beamRy     = 0.f;  // 빔 수직 평면 Y 오프셋
    float    beamSpeed  = 0.f;  // 빔 진행 속도
};

// Attraction control point (skill/rune places these)
struct FluidControlPoint
{
    XMFLOAT3 position           = { 0, 0, 0 };
    float    attractionStrength = 20.0f;  // N/kg
    float    sphereRadius       = 3.0f;   // soft sphere boundary radius
};

// Per-element visual colors
struct FluidElementColor
{
    XMFLOAT4 coreColor;   // high-density / center
    XMFLOAT4 edgeColor;   // low-density / periphery
};

// SPH + visual configuration
struct FluidParticleConfig
{
    // SPH physics
    // restDensity must match the actual kernel-computed density at equilibrium spacing.
    // For h=1.5, mass=1.0, ~200 particles in radius 2.5: expected density ~5-8.
    float smoothingRadius    = 1.5f;
    float restDensity        = 7.0f;
    float stiffness              = 60.0f;
    float nearPressureMultiplier = 2.0f;   // 근압력 배율 (Sebastian Lague 이중 밀도 완화)
    float viscosity              = 0.30f;
    float boundaryStiffness      = 200.0f;

    // Particle visual
    float particleSize       = 0.35f;

    // Spawn
    int   particleCount      = 300;
    float spawnRadius        = 2.5f;   // initial scatter radius

    // Element (determines color)
    ElementType element      = ElementType::None;
};

namespace FluidElementColors
{
    inline FluidElementColor Get(ElementType e)
    {
        switch (e)
        {
        case ElementType::Fire:
            return { {1.0f, 0.45f, 0.05f, 0.95f}, {1.0f, 0.1f, 0.0f, 0.25f} };
        case ElementType::Water:
            return { {0.15f, 0.55f, 1.0f, 0.90f}, {0.0f, 0.25f, 0.75f, 0.25f} };
        case ElementType::Wind:
            return { {0.75f, 1.0f, 0.75f, 0.75f}, {0.35f, 0.85f, 0.35f, 0.20f} };
        case ElementType::Earth:
            return { {0.60f, 0.38f, 0.18f, 0.90f}, {0.38f, 0.20f, 0.08f, 0.25f} };
        default:
            return { {0.55f, 0.55f, 0.85f, 0.85f}, {0.30f, 0.30f, 0.60f, 0.25f} };
        }
    }
}

// 스킬 기반 유체 VFX의 제어점 동작 설명자
struct FluidCPDesc {
    float orbitRadius        = 0.8f;   // 투사체 forward 축 주위 궤도 반지름
    float orbitSpeed         = 4.0f;   // rad/s
    float orbitPhase         = 0.0f;   // 초기 각도 오프셋 (라디안)
    float forwardBias        = 0.0f;   // 진행 방향을 따른 오프셋 (+앞, -뒤)
    float attractionStrength = 15.0f;
    float sphereRadius       = 1.8f;
};

// 스킬 시전 하나의 전체 VFX 정의
struct FluidSkillVFXDef {
    std::vector<FluidCPDesc> cpDescs;
    int         particleCount = 80;
    float       spawnRadius   = 0.8f;
    ElementType element       = ElementType::Fire;
};
