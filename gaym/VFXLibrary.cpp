#include "stdafx.h"
#include "VFXLibrary.h"
#include <cmath>

VFXLibrary& VFXLibrary::Get() {
    static VFXLibrary instance;
    return instance;
}

void VFXLibrary::Initialize() {
    // ──────────────────────────────────────────────────────────
    // Q - 웨이브 슬래시 (일정 폭으로 앞으로 나아가는 파도)
    // ──────────────────────────────────────────────────────────
    {
        VFXSequenceDef def;
        def.name          = "Q_WaveSlash";
        def.element       = ElementType::Fire;
        def.particleCount = 600;
        def.spawnRadius   = 1.0f;  // 플레이어 앞 1m에 스폰 (back wall은 플레이어 위치)

        // Wave 모드: 뒤가 막힌 ConfinementBox + SPH 압력으로 파티클이 앞으로 흘러나감
        // waveSpeed: waveDist 타이머 기준 (실제 이동은 SPH + wavePushForce가 담당)
        def.isWave        = true;
        def.waveSpeed     = 10.f;   // 타이머 기준 속도 (실제 파티클 이동속도와 다름)
        def.wavePushForce = 50.f;   // 매 프레임 앞방향 가속도 (m/s²) — SPH와 함께 앞으로 흐름
        def.waveMaxDist   = 20.f;   // 10 m/s * 2s = 20m → 2초 후 fade-out 시작
        def.waveHalfW     = 5.0f;
        def.waveHalfH     = 2.5f;   // 수직 진동 공간 확보 (1.5→2.5)

        // 파도 수직 진동: 전진 위치에 따라 다른 위상으로 위아래 요동
        // 파장 = 2π/k ≈ 9m (파도 진행 중 ~2개 마루가 보임)
        // 위상 속도 = ω/k = 5/0.7 ≈ 7 m/s (파티클 이동과 비슷한 속도로 패턴 이동)
        def.waveOscAmplitude  = 12.f;  // 수직 가속력 (m/s²)
        def.waveOscFrequency  = 5.f;   // ω = 5 rad/s
        def.waveOscWaveNumber = 0.7f;  // k = 0.7 rad/m

        // maxParticleSpeed: 높게 설정해 SPH 초기 블래스트가 앞으로 빠르게 퍼지도록
        def.maxParticleSpeed = 20.f;

        // SSF 블러: 파도가 하나의 유체 덩어리로 보이도록 bilateral blur 활성화
        def.useSSFBlur = true;

        // SPH: stiffness 높여 초기 압력으로 파티클을 앞방향으로 강하게 분출
        def.overridePhysics     = true;
        def.sphStiffness        = 20.f;
        def.sphNearPressureMult = 0.5f;
        def.sphRestDensity      = 0.0f;   // 항상 양압 (인력 없음)
        def.sphViscosity        = 0.4f;
        def.sphSmoothingRadius  = 1.8f;

        RegisterBase(SkillSlot::Q, def);

        // Enhance 룬: 더 넓고 큰 파도
        VFXModifier enhMod;
        enhMod.particleCountMult = 1.5f;
        enhMod.sizeScaleMult     = 1.3f;
        RegisterRuneMod(SkillSlot::Q, RUNE_ENHANCE, enhMod);

        // Charge 룬: 파티클 밀도 증가
        VFXModifier chgMod;
        chgMod.particleCountMult = 1.4f;
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
        def.spawnRadius   = 12.f;  // 가장 큰 궤도(18f) - sphereRadius(6f)*2.5(15f) = 3f 여유

        // Phase 0: 낙하 (OrbitalCP)
        VFXPhase p0;
        p0.startTime  = 0.f;
        p0.duration   = 3.f; // 낙하 시간 (충돌 전까지)
        p0.motionMode = ParticleMotionMode::OrbitalCP;
        def.phases.push_back(p0);

        // Phase 1: 충돌 후 폭발 (Gravity) — 엄청난 방사형 폭발 + 즉시 페이드
        VFXPhase p1;
        p1.startTime  = 3.f;
        p1.duration   = 0.7f;   // 짧고 강렬하게 (1.2s → 0.7s)
        p1.motionMode = ParticleMotionMode::Gravity;
        p1.gravityDesc.gravity         = { 0.f, -25.f, 0.f };  // 강한 중력으로 빠른 낙하
        p1.gravityDesc.initialSpeedMin = 45.f;  // 강력한 방사 (8 → 45)
        p1.gravityDesc.initialSpeedMax = 90.f;  // 최대 방사 속도 (20 → 90)
        p1.phaseMaxSpeed               = 90.f;  // 속도 상한 확장 (orbital 35 → 90)
        p1.triggerExplodeFadeOnEnter   = true;  // 폭발 즉시 파티클 축소+소멸
        def.phases.push_back(p1);

        // 원자 궤도 위성 CP: 3개 평면(수평/+60°/-60°) × 4개 전자 = 12개
        constexpr float TWO_PI = 2.f * 3.14159265358979f;
        // ring 0: 수평(xz), ring 1: +60° 기울기, ring 2: -60° 기울기
        const float ringRadii[3]  = { 14.f, 18.f, 16.f };
        const float ringSpeeds[3] = { 2.0f, 1.4f, 2.8f };
        const float ringTilts[3]  = { 0.f, 3.14159265358979f / 3.f, -3.14159265358979f / 3.f };

        for (int ring = 0; ring < 3; ++ring)
        {
            for (int e = 0; e < 4; ++e)  // 링당 전자 4개
            {
                SatelliteCPDesc sat;
                sat.orbitRadius        = ringRadii[ring];
                sat.orbitSpeed         = ringSpeeds[ring];
                sat.orbitPhase         = e * (TWO_PI / 4.f);  // 90° 간격
                sat.verticalOffset     = 0.f;
                sat.attractionStrength = 30.f;
                sat.sphereRadius       = 6.f;  // 인력권 15f → 12f 스폰에서 모든 링(최대 18f) 포착
                sat.orbitTiltX         = ringTilts[ring];
                def.satelliteCPs.push_back(sat);
            }
        }

        // 마스터 CP (VFXManager가 낙하 위치로 설정)
        // masterCPStrength/SphereRadius는 VFXSequenceDef 기본값 사용

        // 핵(밝은 주황-노랑) vs 궤도 링(어두운 불꽃)으로 시각적 구분
        def.overrideColors    = true;
        def.overrideCoreColor = { 1.0f, 0.70f, 0.10f, 1.0f }; // 밝은 주황-노랑 (메테오 코어)
        def.overrideEdgeColor = { 0.9f, 0.12f, 0.0f,  0.35f }; // 어두운 불꽃 (궤도 링)
        // 파티클 15%를 메테오 중심 근처에 집중 스폰 → 밝은 코어 형성
        def.nucleusSpawnFraction = 0.15f;
        def.nucleusSpawnRadius   = 2.5f;
        // masterCPSphereRadius: 위성이 최대 18f + sphereRadius(6f) = 24f 커버 필요
        // 경계 영역 = 10f * 2.5f = 25f → 모든 궤도 파티클 간섭 없음
        def.masterCPStrength     = 35.f;
        def.masterCPSphereRadius = 10.f;

        RegisterBase(SkillSlot::R, def);

        // Enhance 룬: 더 크고 강한 폭발
        VFXModifier enhMod;
        enhMod.particleCountMult = 1.6f;
        enhMod.strengthMult      = 1.3f;
        enhMod.sizeScaleMult     = 1.4f;
        RegisterRuneMod(SkillSlot::R, RUNE_ENHANCE, enhMod);
    }

    // ──────────────────────────────────────────────────────────
    // RightClick - 화염구 투사체
    // 페이즈 없이 단일 비행 — 사방에서 스폰된 파티클이 투사체를 쫓아
    // 이동하면서 서서히 뭉치는 혜성 효과
    // ──────────────────────────────────────────────────────────
    {
        VFXSequenceDef def;
        def.name             = "RC_Fireball";
        def.element          = ElementType::Fire;
        def.particleCount    = 420;
        def.spawnRadius      = 0.8f;   // 중심 스폰은 최소화 (cardinalSpawnRadius가 메인)
        def.maxParticleSpeed = 6.0f;   // 낮은 최대속도로 오버슈트 방지 (45→6)

        // 사방 집결 스폰: ±fwd/±right/±up 6방향에서 4유닛 거리에 파티클 스폰
        def.cardinalSpawnRadius = 4.0f;
        def.cardinalInwardSpeed = 0.0f;  // 초기 내향 속도 제거: CP 인력으로만 수렴 (15→0)

        // GPU SPH 물리 오버라이드: 높은 점성으로 수렴 시 속도 감쇄 강화
        def.overridePhysics     = true;
        def.sphStiffness        = 30.0f;
        def.sphNearPressureMult = 0.5f;
        def.sphRestDensity      = 5.0f;
        def.sphViscosity        = 1.5f;  // 기본값(0.25)의 6배 — 진동 억제 핵심
        def.sphSmoothingRadius  = 1.3f;

        // 색상: 핵은 밝은 흰-노랑, 집결은 불꽃 주황-빨강
        def.overrideColors    = true;
        def.overrideCoreColor = { 1.0f, 0.95f, 0.60f, 1.0f };  // 흰-노랑 (핵)
        def.overrideEdgeColor = { 1.0f, 0.28f, 0.0f,  0.55f };  // 불꽃 주황

        // CP A - 핵: 낮은 attractionStrength로 GPU 진동력(±strength*4) 억제
        // sphereRadius를 작게 설정해 경계력(450*overshoot)이 스폰 거리에서 작동
        // 경계: sphereRadius*2.5 = 3.0 < cardinalSpawnRadius 4.0 → 스폰 시점부터 수렴력 발생
        {
            FluidCPDesc cpHead;
            cpHead.forwardBias        = 0.0f;  // 투사체 중심에 위치 (비대칭 수렴 방지)
            cpHead.attractionStrength = 8.0f;  // GPU oscForce = ±32 (기존 ±360 → 억제됨)
            cpHead.sphereRadius       = 1.2f;  // 목표 구체 반경, boundary at 3.0 유닛
            def.cpDescs.push_back(cpHead);
        }
        // CP B(꼬리 앵커) 제거: 음수 oscForce가 반대 방향으로 파티클을 방출하는 주범

        // 단일 페이즈: offsetParticlesWithOrigin=true → 파티클이 투사체와 함께
        // 전진하면서 동시에 CP 인력으로 수렴 ("날아가며 뭉치는" 효과)
        VFXPhase p0;
        p0.startTime  = 0.f;
        p0.duration   = 99.f;
        p0.motionMode = ParticleMotionMode::ControlPoint;
        p0.offsetParticlesWithOrigin = true;
        def.phases.push_back(p0);

        RegisterBase(SkillSlot::RightClick, def);

        // Charge 룬: 파티클 증가 + 꼬리 길어짐
        VFXModifier chgMod;
        chgMod.particleCountMult = 1.5f;
        chgMod.sizeScaleMult     = 1.3f;
        RegisterRuneMod(SkillSlot::RightClick, RUNE_CHARGE, chgMod);

        // Enhance 룬: 파티클 증가 + 인력 강화
        VFXModifier enhMod;
        enhMod.particleCountMult = 1.3f;
        enhMod.strengthMult      = 1.2f;
        RegisterRuneMod(SkillSlot::RightClick, RUNE_ENHANCE, enhMod);
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
