#include "stdafx.h"
#include "VFXLibrary.h"
#include <cmath>

VFXLibrary& VFXLibrary::Get() {
    static VFXLibrary instance;
    return instance;
}

void VFXLibrary::Initialize() {
    // ──────────────────────────────────────────────────────────
    // Q - 웨이브 슬래시 (박스 확장 + ConfinementBox)
    // ──────────────────────────────────────────────────────────
    {
        VFXSequenceDef def;
        def.name          = "Q_WaveSlash";
        def.element       = ElementType::Fire;
        def.particleCount = 256;
        def.spawnRadius   = 0.4f;

        // Phase 0: 플레이어 앞 작은 박스에 뭉침 (0~0.3s)
        VFXPhase p0;
        p0.startTime  = 0.f;
        p0.duration   = 0.3f;
        p0.motionMode = ParticleMotionMode::ControlPoint;
        p0.boxDesc.active      = true;
        p0.boxDesc.halfExtents = { 0.8f, 0.8f, 0.8f };
        // Phase 0: CP 1개 (박스 center에 파티클 집결)
        {
            FluidCPDesc cp0;
            cp0.orbitRadius        = 0.0f;
            cp0.orbitSpeed         = 0.0f;
            cp0.orbitPhase         = 0.0f;
            cp0.forwardBias        = 0.5f;  // 플레이어 앞 0.5 (박스 center)
            cp0.attractionStrength = 25.f;   // 진동 기반, softAttr과 oscForce 비율 고려
            cp0.sphereRadius       = 0.8f;
            p0.cpDescs = { cp0 };
        }
        def.phases.push_back(p0);

        // Phase 1: 앞으로 박스 확장 (0.3~1.1s)
        VFXPhase p1;
        p1.startTime  = 0.3f;
        p1.duration   = 0.8f;           // 0.5 -> 0.8 (앞으로 완전히 퍼지는 시간 늘림)
        p1.motionMode = ParticleMotionMode::ControlPoint;
        p1.boxDesc.active      = true;
        p1.boxDesc.halfExtents = { 1.5f, 1.5f, 5.0f }; // Y도 넓게: 측면에서 보여도 두꺼운 덩어리
        // Phase 1: CP 없음 + 앞(Z)방향 힘
        p1.cpDescs = {};
        p1.expansionForce = { 0.f, 0.f, 1.f };  // 로컬 forward(Z) 방향
        p1.expansionForceStrength = 22.5f;
        def.phases.push_back(p1);

        // Phase 2: 구름 유지 (1.1~5.1s) - 3D 공간에 고르게 분포
        // SebLague 방식: 중력 최소화 + 상하 산란 → 어떤 각도에서 봐도 두껍게 보임
        VFXPhase p2;
        p2.startTime  = 1.1f;
        p2.duration   = 4.0f;
        p2.motionMode = ParticleMotionMode::ControlPoint;
        p2.cpDescs    = {};
        // ConfinementBox: X/Z 넓게, Y를 충분히 높여 3D 분포 허용
        p2.boxDesc.active      = true;
        p2.boxDesc.halfExtents = { 10.0f, 3.5f, 8.0f };  // Y 3.5: 상하 산란 공간 확보
        p2.cancelForwardVelocityOnEnter = true;
        p2.randomSidewaysImpulse  = 14.f;  // 좌우 산란 (+ UpdatePhase에서 상하 산란도 추가)
        p2.expansionForce         = { 0.f, 0.f, 0.f };
        p2.expansionForceStrength = 0.f;
        p2.useAxisSpreadForce     = false;
        // 중력 대폭 감소: 파티클이 납작해지지 않고 3D 볼륨 유지
        p2.globalGravityStrength  = 1.5f;  // 4.0 → 1.5 (SPH 압력이 중력 이기도록)
        def.phases.push_back(p2);

        // SPH 물리 오버라이드: 강한 반발력 + 낮은 목표밀도 → 바닥에 넓게 깔림
        def.overridePhysics           = true;
        def.sphStiffness              = 80.f;    // 압력 배율 (낮출수록 파티클이 퍼짐)
        def.sphNearPressureMult       = 3.0f;    // 근압력 (클러스터링 방지)
        def.sphRestDensity            = 1.5f;    // 목표 밀도 (낮을수록 더 퍼짐)
        def.sphViscosity              = 0.05f;   // 점성 (낮을수록 빠르게 퍼짐)
        def.sphSmoothingRadius        = 2.0f;    // 상호작용 반경

        RegisterBase(SkillSlot::Q, def);

        // Enhance 룬: 더 크고 많은 파티클
        VFXModifier enhMod;
        enhMod.particleCountMult = 1.5f;
        enhMod.sizeScaleMult     = 1.3f;
        enhMod.strengthMult      = 1.2f;
        RegisterRuneMod(SkillSlot::Q, RUNE_ENHANCE, enhMod);

        // Charge 룬: 더 강하고 빠르게 확장
        VFXModifier chgMod;
        chgMod.particleCountMult = 1.3f;
        chgMod.speedMult         = 1.5f;
        chgMod.strengthMult      = 1.4f;
        RegisterRuneMod(SkillSlot::Q, RUNE_CHARGE, chgMod);
    }

    // ──────────────────────────────────────────────────────────
    // E - 화염 빔 (Beam 모드)
    // ──────────────────────────────────────────────────────────
    {
        VFXSequenceDef def;
        def.name          = "E_FireBeam";
        def.element       = ElementType::Fire;
        def.particleCount = 1024;
        def.spawnRadius   = 0.2f;

        VFXPhase p0;
        p0.startTime  = 0.f;
        p0.duration   = 99.f; // 스킬이 끝날 때까지 지속
        p0.motionMode = ParticleMotionMode::Beam;
        // startPos/endPos는 VFXManager가 플레이어 위치/방향으로 설정
        p0.beamDesc.speedMin     = 10.f;
        p0.beamDesc.speedMax     = 18.f;
        p0.beamDesc.spreadRadius = 0.25f;
        def.phases.push_back(p0);

        RegisterBase(SkillSlot::E, def);

        // Channel 룬: 지속시간 증가 (VFXManager가 처리), 더 많은 파티클
        VFXModifier chanMod;
        chanMod.particleCountMult = 1.5f;
        chanMod.speedMult         = 1.2f;
        RegisterRuneMod(SkillSlot::E, RUNE_CHANNEL, chanMod);

        // Enhance 룬: 빔 폭 넓어짐 (spreadRadius x 2.5)
        VFXModifier enhMod;
        enhMod.particleCountMult = 1.4f;
        enhMod.sizeScaleMult     = 1.5f; // spread 배율로도 활용
        RegisterRuneMod(SkillSlot::E, RUNE_ENHANCE, enhMod);
    }

    // ──────────────────────────────────────────────────────────
    // R - 메테오 (OrbitalCP -> Gravity 전환)
    // ──────────────────────────────────────────────────────────
    {
        VFXSequenceDef def;
        def.name          = "R_Meteor";
        def.element       = ElementType::Fire;
        def.particleCount = 1024;
        def.spawnRadius   = 6.f;

        // Phase 0: 낙하 (OrbitalCP)
        VFXPhase p0;
        p0.startTime  = 0.f;
        p0.duration   = 3.f; // 낙하 시간 (충돌 전까지)
        p0.motionMode = ParticleMotionMode::OrbitalCP;
        def.phases.push_back(p0);

        // Phase 1: 충돌 후 폭발 (Gravity)
        VFXPhase p1;
        p1.startTime  = 3.f;
        p1.duration   = 1.2f;
        p1.motionMode = ParticleMotionMode::Gravity;
        p1.gravityDesc.gravity       = { 0.f, -15.f, 0.f };
        p1.gravityDesc.initialSpeedMin = 8.f;
        p1.gravityDesc.initialSpeedMax = 20.f;
        def.phases.push_back(p1);

        // 위성 CP 12개
        constexpr float TWO_PI = 2.f * 3.14159265358979f;
        for (int i = 0; i < 12; i++) {
            SatelliteCPDesc sat;
            sat.orbitRadius        = 7.f + (i % 3) * 2.f; // 7, 9, 11
            sat.orbitSpeed         = 4.0f + i * 0.4f;      // 1.2+i*0.15 → 4.0+i*0.4 (빠른 회전)
            sat.orbitPhase         = i * (TWO_PI / 12.f);
            sat.verticalOffset     = (i % 4 - 1.5f) * 2.f; // -3 ~ +3
            sat.attractionStrength = 18.f;
            sat.sphereRadius       = 4.f;
            def.satelliteCPs.push_back(sat);
        }

        // 마스터 CP (VFXManager가 낙하 위치로 설정)
        // cpDescs[0] = 마스터 CP

        RegisterBase(SkillSlot::R, def);

        // Enhance 룬: 더 크고 강한 폭발
        VFXModifier enhMod;
        enhMod.particleCountMult = 1.6f;
        enhMod.strengthMult      = 1.3f;
        enhMod.sizeScaleMult     = 1.4f;
        RegisterRuneMod(SkillSlot::R, RUNE_ENHANCE, enhMod);
    }

    // ──────────────────────────────────────────────────────────
    // RightClick - 화염구 투사체 (기존 ControlPoint 유지)
    // ──────────────────────────────────────────────────────────
    {
        VFXSequenceDef def;
        def.name          = "RC_Fireball";
        def.element       = ElementType::Fire;
        def.particleCount = 5;
        def.spawnRadius   = 2.5f;

        VFXPhase p0;
        p0.startTime  = 0.f;
        p0.duration   = 99.f;
        p0.motionMode = ParticleMotionMode::ControlPoint;
        def.phases.push_back(p0);

        // cpDescs는 기존 Fire GetVFXDef()의 3-CP 혜성 꼬리 패턴 유지
        // VFXManager가 기존 PushControlPoints로 처리

        RegisterBase(SkillSlot::RightClick, def);
    }

    // ──────────────────────────────────────────────────────────
    // Boss - 드래곤 메가 브레스 (Beam 모드, 대규모 파티클)
    // ──────────────────────────────────────────────────────────
    {
        VFXSequenceDef def;
        def.name          = "Dragon_MegaBreath";
        def.element       = ElementType::Fire;
        def.particleCount = 4096; // 시스템 한계치 적용
        def.spawnRadius   = 8.0f; // 더 거대한 발사구

        VFXPhase p0;
        p0.startTime  = 0.f;
        p0.duration   = 99.f; 
        p0.motionMode = ParticleMotionMode::Beam;
        
        // 보스 브레스: 가로로 압도적인 스케일 + 끊임없는 흐름
        p0.beamDesc.speedMin      = 55.f; 
        p0.beamDesc.speedMax      = 110.f;
        p0.beamDesc.spreadRadius  = 35.0f; // 가로 폭 최적화
        p0.beamDesc.verticalScale = 0.15f; // 보스 브레스만 납작하게 설정 (중요)
        
        // 가로 확산력 및 중력 유지
        p0.randomSidewaysImpulse = 25.0f; 
        p0.globalGravityStrength = 45.0f; 
        def.phases.push_back(p0);

        // SkillSlot::None(0)과 999번 룬 플래그를 조합하여 보스 전용 키로 등록
        RegisterExactCombo((SkillSlot)0, 999, std::move(def));
    }
}

void VFXLibrary::RegisterBase(SkillSlot slot, VFXSequenceDef def) {
    m_BaseDefs[static_cast<int>(slot)] = std::move(def);
}

void VFXLibrary::RegisterRuneMod(SkillSlot slot, uint32_t runeFlag, VFXModifier mod) {
    m_RuneMods[static_cast<int>(slot)][runeFlag] = mod;
}

void VFXLibrary::RegisterExactCombo(SkillSlot slot, uint32_t runeFlags, VFXSequenceDef def) {
    m_ExactCombos[MakeKey(slot, runeFlags)] = std::move(def);
}

VFXSequenceDef VFXLibrary::GetDef(SkillSlot slot, uint32_t runeFlags, ElementType element) const {
    VFXSequenceDef result;

    // 정확한 조합 먼저
    auto exactIt = m_ExactCombos.find(MakeKey(slot, runeFlags));
    if (exactIt != m_ExactCombos.end()) {
        result = exactIt->second;
    } else {
        // 기본 정의에 룬 modifier 누적 적용
        result = m_BaseDefs[static_cast<int>(slot)];
        const auto& mods = m_RuneMods[static_cast<int>(slot)];
        for (const auto& [flag, mod] : mods) {
            if (runeFlags & flag) {
                result = ApplyModifier(result, mod);
            }
        }
    }

    // 호출 측에서 element를 명시하면 def의 element를 override (속성색 적용)
    result.element = element;
    return result;
}

VFXSequenceDef VFXLibrary::ApplyModifier(VFXSequenceDef def, const VFXModifier& mod) const {
    if (mod.phaseOverride.has_value()) {
        def.phases = mod.phaseOverride.value();
        return def;
    }
    def.particleCount = static_cast<int>(def.particleCount * mod.particleCountMult);
    // spreadRadius, beamDesc 등도 배율 적용
    for (auto& phase : def.phases) {
        if (phase.motionMode == ParticleMotionMode::Beam) {
            phase.beamDesc.speedMin     *= mod.speedMult;
            phase.beamDesc.speedMax     *= mod.speedMult;
            phase.beamDesc.spreadRadius *= mod.sizeScaleMult;
        }
        if (phase.motionMode == ParticleMotionMode::Gravity) {
            phase.gravityDesc.initialSpeedMin *= mod.speedMult;
            phase.gravityDesc.initialSpeedMax *= mod.speedMult;
        }
    }
    for (auto& sat : def.satelliteCPs) {
        sat.attractionStrength *= mod.strengthMult;
        sat.orbitRadius        *= mod.sizeScaleMult;
    }
    return def;
}
