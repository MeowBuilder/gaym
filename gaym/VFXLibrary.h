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

    // SPH 물리 오버라이드 (true 시 기본값 대신 적용)
    bool  overridePhysics    = false;
    float sphStiffness       = 50.f;
    float sphNearPressureMult = 2.0f;   // 근압력 배율 (이중 밀도 완화)
    float sphRestDensity     = 7.f;
    float sphViscosity       = 0.25f;
    float sphSmoothingRadius = 1.2f;
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
