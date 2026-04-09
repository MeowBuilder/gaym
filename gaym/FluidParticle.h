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
    int      cpGroup    = -1;  // 담당 CP 인덱스 (-1=전체, 0=핵, 1+=위성)
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

    // 파티클 최대 속도 (GPU DispatchSPH에서 clamp)
    float maxParticleSpeed   = 12.0f;

    // 핵 전용 스폰 (OrbitalCP 원자 궤도용)
    float nucleusFraction    = 0.0f;  // 전체 파티클 중 핵 근처에 스폰할 비율 (0~1)
    float nucleusRadius      = 0.4f;  // 핵 스폰 반경

    // 색상 오버라이드 (원소별 기본값 대신 직접 지정)
    bool     overrideColors    = false;
    XMFLOAT4 customCoreColor   = {};   // 고밀도/핵 색상
    XMFLOAT4 customEdgeColor   = {};   // 저밀도/궤도 색상

    // 추가 스폰 그룹 (위성 CP 위치 등)
    struct SpawnGroup {
        XMFLOAT3 center = {};
        int       count  = 0;
        float     radius = 0.5f;
    };
    std::vector<SpawnGroup> spawnGroups;
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
    // 궤도 기울기: 0=right/up 면(forward 수직), π/2=right/fwd 면(forward 수평)
    // 예) 60° 기울이면 궤도가 forward 방향으로 비스듬히 기울어짐
    float orbitTilt          = 0.0f;
};

// 스킬 시전 하나의 전체 VFX 정의
struct FluidSkillVFXDef {
    std::vector<FluidCPDesc> cpDescs;
    int         particleCount = 80;
    float       spawnRadius   = 0.8f;
    ElementType element       = ElementType::Fire;
};
