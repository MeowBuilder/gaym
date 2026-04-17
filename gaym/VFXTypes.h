#pragma once
#include <vector>
#include <optional>
#include <DirectXMath.h>
#include "FluidParticle.h"  // FluidCPDesc
using namespace DirectX;

// 파티클 운동 모드
enum class ParticleMotionMode {
    ControlPoint,  // 기존: CP 인력 기반 SPH
    Gravity,       // 신규: 중력만 작용
    Beam,          // 신규: 시작->끝 선형 이동, 반복
    OrbitalCP,     // 신규: 마스터 CP + 공전 위성 CP군
};

// 중력 설정
struct GravityDesc {
    XMFLOAT3 gravity = { 0.f, -9.8f, 0.f };
    float initialSpeedMin = 2.f;
    float initialSpeedMax = 8.f;
};

// 빔 설정
struct BeamDesc {
    XMFLOAT3 startPos = {};
    XMFLOAT3 endPos   = {};
    float speedMin = 8.f;
    float speedMax = 16.f;
    float spreadRadius = 0.3f; // 빔 폭 (시작점 랜덤 오프셋)
    float verticalScale = 1.0f; // 수직 배율 (기본 1.0 = 원형, 보스 브레스 등은 낮춰서 납작하게 만듦)
    bool  enableFlow = false;   // 입자 흐름 활성화 (false = 정적 레이저, true = 흐르는 브레스)
    XMFLOAT3 prevDir = { 0.f, 0.f, 1.f }; // 이전 프레임 방향 (빔 전체 회전 계산용)
};

// 박스 경계
struct ConfinementBoxDesc {
    XMFLOAT3 center      = {};
    XMFLOAT3 halfExtents = { 1.f, 1.f, 1.f };
    // 월드 공간 기준 OBB를 위한 로컬 축 (기본: 축정렬)
    XMFLOAT3 axisX = { 1,0,0 };
    XMFLOAT3 axisY = { 0,1,0 };
    XMFLOAT3 axisZ = { 0,0,1 };
    bool active = false;
};

// 위성 CP 설명자 (OrbitalCP 모드용)
struct SatelliteCPDesc {
    float orbitRadius;
    float orbitSpeed;     // rad/s
    float orbitPhase;     // 초기 위상
    float verticalOffset; // 마스터 CP 기준 Y 오프셋
    float attractionStrength;
    float sphereRadius;
    // 궤도 기울기: X축 기준으로 xz면을 회전 (0=수평, π/2=수직 yz면)
    float orbitTiltX = 0.f;
    // 궤도면 세차운동 속도 (rad/s): 0이면 고정, 양수/음수로 방향 제어
    float precessionSpeed = 0.f;

    // 궤도 반지름 호흡 (줄었다 늘었다)
    // R_eff = orbitRadius * (1 + breatheAmplitude * sin(elapsed * breatheSpeed + breathePhase))
    float breatheAmplitude = 0.f;  // 진폭 (0~1): 1이면 완전히 0까지 수축
    float breatheSpeed     = 0.f;  // 속도 (rad/s)
    float breathePhase     = 0.f;  // 초기 위상
};

// VFX 페이즈
struct VFXPhase {
    float startTime = 0.f;
    float duration  = 1.f;

    ParticleMotionMode motionMode = ParticleMotionMode::ControlPoint;

    // ConfinementBox (모든 모드에서 선택적 활성화 가능)
    ConfinementBoxDesc boxDesc;

    // Gravity 모드용
    GravityDesc gravityDesc;

    // Beam 모드용
    BeamDesc beamDesc;

    // ControlPoint 모드용 CP 목록 (비어있으면 CP 없음)
    std::vector<FluidCPDesc> cpDescs;

    // 박스 확장 방향 힘 (OBB 로컬 좌표계: x=right, y=up, z=forward)
    XMFLOAT3 expansionForce = { 0.f, 0.f, 0.f };
    float expansionForceStrength = 0.f;

    // 전역 중력 강도 (모든 모드에서 적용 가능, 0이면 비활성)
    float globalGravityStrength = 0.f;

    // Phase 진입 시 forward(Z축) 방향 속도 제거
    bool cancelForwardVelocityOnEnter = false;

    // expansionForce를 양방향 분산으로 적용 (중심 기준으로 양쪽으로 밀기)
    // false: 모든 파티클에 동일 방향 (기존), true: 중심 기준 양방향
    bool useAxisSpreadForce = false;

    // Phase 진입 시 right 축 방향으로 랜덤 속도 부여 (양방향, 파도 확산용)
    // 0이면 비활성, 양수면 이 값 범위에서 랜덤 (-randomSidewaysImpulse ~ +randomSidewaysImpulse)
    float randomSidewaysImpulse = 0.f;

    // 투사체 이동 시 파티클도 같이 텔레포트할지 여부
    // true(기본): 파티클이 origin과 함께 이동 (뭉쳐서 이동)
    // false: CP만 이동, 파티클은 물리로 따라감 → 혜성 꼬리 효과
    bool offsetParticlesWithOrigin = true;

    // Phase 진입 시 maxParticleSpeed 오버라이드 (0=변경 없음)
    // 폭발 페이즈에서 속도 상한을 높여 빠른 방사 허용
    float phaseMaxSpeed = 0.f;

    // Phase 진입 시 ExplodeFade 모드 시작 (파티클이 작아지며 사라짐)
    // true이면 phase.duration 동안 fadeRatio 1→0으로 감소 후 슬롯 소멸
    bool triggerExplodeFadeOnEnter = false;
};

// VFX 룬 수식자
struct VFXModifier {
    float particleCountMult = 1.f;
    float strengthMult      = 1.f;
    float sizeScaleMult     = 1.f;
    float speedMult         = 1.f;
    std::optional<std::vector<VFXPhase>> phaseOverride;
};
