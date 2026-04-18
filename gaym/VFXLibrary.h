#pragma once
#include "VFXTypes.h"
#include "FluidParticle.h"  // FluidCPDesc, FluidSkillVFXDef 등
#include "SkillTypes.h"     // SkillSlot, ElementType (이미 정의됨)
#include <array>
#include <unordered_map>
#include <string>

// 룬 타입 (RuneCombo를 비트마스크로 표현)
enum RuneFlag : uint32_t {
    RUNE_NONE    = 0,
    RUNE_INSTANT = 1 << 0,
    RUNE_CHARGE  = 1 << 1,
    RUNE_CHANNEL = 1 << 2,
    RUNE_PLACE   = 1 << 3,
    RUNE_ENHANCE = 1 << 4,
    RUNE_SPLIT   = 1 << 5,
};

// RuneCombo -> RuneFlag 비트마스크 변환 유틸리티
inline uint32_t ToRuneFlags(const RuneCombo& combo) {
    uint32_t flags = 0;
    if (combo.hasInstant) flags |= RUNE_INSTANT;
    if (combo.hasCharge)  flags |= RUNE_CHARGE;
    if (combo.hasChannel) flags |= RUNE_CHANNEL;
    if (combo.hasPlace)   flags |= RUNE_PLACE;
    if (combo.hasEnhance) flags |= RUNE_ENHANCE;
    if (combo.hasSplit)   flags |= RUNE_SPLIT;
    return flags;
}

struct VFXSequenceDef {
    std::string name;
    std::vector<VFXPhase> phases;
    std::vector<FluidCPDesc> cpDescs;  // ControlPoint 모드용 공통 CP
    std::vector<SatelliteCPDesc> satelliteCPs; // OrbitalCP 모드용
    int particleCount  = 200;
    float spawnRadius  = 1.5f;
    ElementType element = ElementType::Fire;

    // OrbitalCP 모드 마스터 CP 설정
    float masterCPFallSpeed    = 15.f;   // 낙하 속도. 0이면 낙하 대신 slot.origin 추적 (투사체용)
    float masterCPStrength     = 25.f;   // 마스터 CP 인력
    float masterCPSphereRadius = 5.f;    // 마스터 CP 구체 반경

    // 핵-궤도 색상 오버라이드 (원자 궤도 이펙트용)
    bool     overrideColors    = false;
    XMFLOAT4 overrideCoreColor = {};   // 고밀도/핵 색상 (흰-노랑 등)
    XMFLOAT4 overrideEdgeColor = {};   // 저밀도/궤도 색상 (원소색)

    // 핵 전용 파티클 스폰 비율 (0=전체 구에 스폰, 0.25=25%를 핵 근처에 집중 스폰)
    float nucleusSpawnFraction = 0.0f;
    float nucleusSpawnRadius   = 0.4f;

    // 파티클 최대 속도 오버라이드 (0이면 자동: OrbitalCP=35, 나머지=12)
    float maxParticleSpeed = 0.f;

    // SPH 물리 오버라이드 (true 시 기본값 대신 적용)
    bool  overridePhysics    = false;
    float sphStiffness       = 50.f;
    float sphNearPressureMult = 2.0f;   // 근압력 배율 (이중 밀도 완화)
    float sphRestDensity     = 7.f;
    float sphViscosity       = 0.25f;
    float sphSmoothingRadius = 1.2f;

    // 사방 집결 스폰 (0이면 비활성 — 기본 구 스폰 사용)
    // SpawnSequenceEffect에서 ±X/±Y/±Z 방향에 각각 SpawnGroup을 생성
    float cardinalSpawnRadius  = 0.f;   // 집결 스폰 거리 (유닛)
    float cardinalInwardSpeed  = 12.f;  // 파티클 초기 내향 속도 (m/s)

    float particleSize = 0.f;    // 0 = 기본값(0.35f) 사용

    // SSF bilateral blur 활성화 (파도처럼 덩어리로 보여야 하는 스킬에만 true)
    bool  useSSFBlur   = false;

    // Wave 모드 (isWave=true면 phases 무시, 처음부터 일정 폭으로 전진)
    bool  isWave       = false;
    float waveSpeed    = 20.f;  // ConfinementBox 전진 속도 (units/s)
    float wavePushForce = 60.f; // 파티클에 매 프레임 가하는 전진 가속도 (units/s²)
    float waveMaxDist  = 22.f;  // 최대 이동 거리
    float waveHalfW    = 4.0f;  // 파도 좌우 반폭 (X)
    float waveHalfH    = 2.5f;  // 파도 높이 반폭 (Y)
    float waveDepth    = 2.5f;  // 파도 전후 두께 반폭 (Z) — 클수록 경계 반사 완화

    // Traveling wave 수직 진동 (F_y = A * sin(k * fwdPos - ω * t))
    // amplitude > 0 이면 자동 활성화
    float waveOscAmplitude  = 0.f;   // 힘 진폭 (m/s²)
    float waveOscFrequency  = 4.f;   // 시간 주파수 ω (rad/s)
    float waveOscWaveNumber = 0.8f;  // 공간 파수 k (rad/m), 파장 = 2π/k
};

class VFXLibrary {
public:
    static VFXLibrary& Get();

    void Initialize();  // 모든 스킬 VFX 등록

    void RegisterBase(SkillSlot slot, VFXSequenceDef def);
    void RegisterRuneMod(SkillSlot slot, uint32_t runeFlag, VFXModifier mod);
    void RegisterExactCombo(SkillSlot slot, uint32_t runeFlags, VFXSequenceDef def);

    // 런타임 조회: base에 룬 modifier 누적 적용 (element를 지정하면 def.element override)
    VFXSequenceDef GetDef(SkillSlot slot, uint32_t runeFlags,
                          ElementType element = ElementType::Fire) const;

private:
    VFXLibrary() = default;

    std::array<VFXSequenceDef, static_cast<int>(SkillSlot::Count)> m_BaseDefs;
    std::array<std::unordered_map<uint32_t, VFXModifier>, static_cast<int>(SkillSlot::Count)> m_RuneMods;
    std::unordered_map<uint64_t, VFXSequenceDef> m_ExactCombos; // (slot<<32)|runes -> def

    static uint64_t MakeKey(SkillSlot slot, uint32_t runes) {
        return ((uint64_t)static_cast<int>(slot) << 32) | runes;
    }

    VFXSequenceDef ApplyModifier(VFXSequenceDef def, const VFXModifier& mod) const;
};
