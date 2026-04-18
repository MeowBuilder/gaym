#include "stdafx.h"
#include "RuneRegistry.h"

// ─────────────────────────────────────────────────────────────────────────────
// RuneDef::ApplyTo  — accumulate one rune into SkillStats
// Stack formula: effective = 1 + (baseMult - 1) * stackCount
// ─────────────────────────────────────────────────────────────────────────────
void RuneDef::ApplyTo(SkillStats& stats, int stackCount) const
{
    if (stackCount <= 0) stackCount = 1;

    auto scaledMult = [&](float base) {
        return 1.f + (base - 1.f) * static_cast<float>(stackCount);
    };

    stats.damageMult         *= scaledMult(damageMult);
    stats.cooldownMult       *= scaledMult(cooldownMult);
    stats.rangeMult          *= scaledMult(rangeMult);
    stats.radiusMult         *= scaledMult(radiusMult);
    stats.castTimeMult       *= scaledMult(castTimeMult);
    stats.durationMult       *= scaledMult(durationMult);
    stats.manaCostMult       *= scaledMult(manaCostMult);
    stats.statusDurationMult *= scaledMult(statusDurationMult);
    stats.statusChanceMult   *= scaledMult(statusChanceMult);
    stats.knockbackMult      *= scaledMult(knockbackMult);

    if (activationOverride.has_value())
        stats.activationType = activationOverride.value();

    // VFX mods accumulate multiplicatively
    stats.vfxMod.particleCountMult *= vfxMod.particleCountMult;
    stats.vfxMod.strengthMult      *= vfxMod.strengthMult;
    stats.vfxMod.sizeScaleMult     *= vfxMod.sizeScaleMult;
    stats.vfxMod.speedMult         *= vfxMod.speedMult;

    // Behavioral flags
    stats.extraProjectiles += extraProjectiles * stackCount;
    if (piercing)       stats.piercing       = true;
    if (homing)         stats.homing         = true;
    if (doublecast)     stats.doublecast     = true;
    if (echoOnCast)     stats.echoOnCast     = true;
    stats.lifestealRatio   += lifestealRatio   * static_cast<float>(stackCount);
    stats.execDamageBonus  += execDamageBonus  * static_cast<float>(stackCount);
    stats.cdResetChance    += cdResetChance    * static_cast<float>(stackCount);

    // Hooks
    if (onCast) stats.onCastHooks.push_back(onCast);
    if (onHit)  stats.onHitHooks.push_back(onHit);
}

float RuneDef::GetStackBonus(RuneGrade grade)
{
    switch (grade)
    {
    case RuneGrade::Normal:    return 0.10f;
    case RuneGrade::Rare:      return 0.08f;
    case RuneGrade::Epic:      return 0.06f;
    case RuneGrade::Unique:    return 0.05f;
    case RuneGrade::Legendary: return 0.03f;
    default:                   return 0.f;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// RuneRegistry
// ─────────────────────────────────────────────────────────────────────────────
RuneRegistry& RuneRegistry::Get()
{
    static RuneRegistry instance;
    return instance;
}

const RuneDef* RuneRegistry::Find(const std::string& id) const
{
    auto it = m_defs.find(id);
    return (it != m_defs.end()) ? &it->second : nullptr;
}

std::vector<std::string> RuneRegistry::GetIdsByGrade(RuneGrade grade) const
{
    std::vector<std::string> result;
    for (const auto& [id, def] : m_defs)
        if (def.grade == grade) result.push_back(id);
    return result;
}

void RuneRegistry::Register(RuneDef def)
{
    m_defs.emplace(def.id, std::move(def));
}

// ═════════════════════════════════════════════════════════════════════════════
//  모든 룬 정의 (RuneList.md 기반, 50종)
//  추가/수정은 이 파일 한 곳에서만 작업
// ═════════════════════════════════════════════════════════════════════════════
RuneRegistry::RuneRegistry()
{
    // ─── 노멀 등급 (15종) ────────────────────────────────────────────────────

    // 원소 강화
    Register({ .id="W01", .name="물방울",   .grade=RuneGrade::Normal,  .element=ElementType::Water, .damageMult=1.10f });
    Register({ .id="F01", .name="불꽃",     .grade=RuneGrade::Normal,  .element=ElementType::Fire,  .damageMult=1.10f });
    Register({ .id="E01", .name="자갈",     .grade=RuneGrade::Normal,  .element=ElementType::Earth, .damageMult=1.10f });
    Register({ .id="A01", .name="미풍",     .grade=RuneGrade::Normal,  .element=ElementType::Wind,  .damageMult=1.10f });

    // 발동 방식
    Register({ .id="I01", .name="신속",     .grade=RuneGrade::Normal,  .castTimeMult=0.85f });
    Register({ .id="I02", .name="연사",     .grade=RuneGrade::Normal,  .damageMult=0.85f,  .extraProjectiles=1 });
    Register({ .id="C01", .name="집중",     .grade=RuneGrade::Normal,  .damageMult=1.30f,  .activationOverride=ActivationType::Charge });
    Register({ .id="C02", .name="축적",     .grade=RuneGrade::Normal,  .castTimeMult=0.80f, .activationOverride=ActivationType::Charge });
    Register({ .id="T01", .name="주문",     .grade=RuneGrade::Normal,  .damageMult=1.15f,  .activationOverride=ActivationType::Channel });
    Register({ .id="T02", .name="가속",     .grade=RuneGrade::Normal,  .castTimeMult=0.85f, .activationOverride=ActivationType::Channel });
    Register({ .id="S01", .name="지속",     .grade=RuneGrade::Normal,  .durationMult=1.25f, .activationOverride=ActivationType::Place });
    Register({ .id="S02", .name="견고",     .grade=RuneGrade::Normal,  .damageMult=1.30f,  .activationOverride=ActivationType::Place });
    Register({ .id="B01", .name="증폭",     .grade=RuneGrade::Normal,  .damageMult=1.15f,  .activationOverride=ActivationType::Enhance });
    Register({ .id="B02", .name="연장",     .grade=RuneGrade::Normal,  .durationMult=1.30f, .activationOverride=ActivationType::Enhance });
    Register({ .id="X01", .name="강인",     .grade=RuneGrade::Normal,  .damageMult=1.05f });

    // ─── 레어 등급 (12종) ────────────────────────────────────────────────────

    // 원소 강화
    Register({ .id="W02", .name="냉기",     .grade=RuneGrade::Rare,    .element=ElementType::Water, .damageMult=1.15f, .statusDurationMult=1.20f });
    Register({ .id="W03", .name="서리",     .grade=RuneGrade::Rare,    .element=ElementType::Water, .statusChanceMult=1.40f });
    Register({ .id="F02", .name="점화",     .grade=RuneGrade::Rare,    .element=ElementType::Fire,  .damageMult=1.15f, .statusDurationMult=1.15f });
    Register({ .id="F03", .name="작열",     .grade=RuneGrade::Rare,    .element=ElementType::Fire,  .damageMult=1.10f, .statusChanceMult=1.30f });
    Register({ .id="E02", .name="암석",     .grade=RuneGrade::Rare,    .element=ElementType::Earth, .damageMult=1.15f });
    Register({ .id="E03", .name="진동",     .grade=RuneGrade::Rare,    .element=ElementType::Earth, .statusDurationMult=1.30f });
    Register({ .id="A02", .name="질풍",     .grade=RuneGrade::Rare,    .element=ElementType::Wind,  .damageMult=1.15f });
    Register({ .id="A03", .name="선풍",     .grade=RuneGrade::Rare,    .element=ElementType::Wind,  .cooldownMult=0.90f });

    // 발동 방식
    Register({ .id="I03", .name="관통",     .grade=RuneGrade::Rare,    .piercing=true });
    Register({ .id="C03", .name="과충전",   .grade=RuneGrade::Rare,    .radiusMult=1.30f,  .activationOverride=ActivationType::Charge });
    Register({ .id="T03", .name="의식",     .grade=RuneGrade::Rare,    .activationOverride=ActivationType::Channel });
    Register({ .id="S03", .name="증식",     .grade=RuneGrade::Rare,    .activationOverride=ActivationType::Place, .extraProjectiles=1 });

    // ─── 에픽 등급 (8종) ─────────────────────────────────────────────────────

    // 원소 강화
    Register({ .id="W04", .name="급류",     .grade=RuneGrade::Epic,    .element=ElementType::Water, .damageMult=1.25f });
    Register({ .id="F04", .name="폭염",     .grade=RuneGrade::Epic,    .element=ElementType::Fire,  .damageMult=1.25f, .radiusMult=1.20f });
    Register({ .id="E04", .name="대지",     .grade=RuneGrade::Epic,    .element=ElementType::Earth, .damageMult=1.25f });
    Register({ .id="A04", .name="회오리",   .grade=RuneGrade::Epic,    .element=ElementType::Wind,  .damageMult=1.25f, .knockbackMult=1.40f });

    // 발동 방식
    Register({ .id="I04", .name="분열",     .grade=RuneGrade::Epic,    .extraProjectiles=3,
               .onHit=[](SkillContext& ctx){ (void)ctx; /* TODO: 작은 투사체 3개 분열 */ } });
    Register({ .id="C04", .name="해방",     .grade=RuneGrade::Epic,    .cooldownMult=0.75f, .activationOverride=ActivationType::Charge });
    Register({ .id="T04", .name="확장",     .grade=RuneGrade::Epic,    .radiusMult=1.40f,  .activationOverride=ActivationType::Channel });
    Register({ .id="B03", .name="전파",     .grade=RuneGrade::Epic,    .activationOverride=ActivationType::Enhance,
               .onCast=[](SkillContext& ctx){ (void)ctx; /* TODO: 주변 아군 50% 효과 전파 */ } });

    // ─── 유니크 등급 (5종) ───────────────────────────────────────────────────
    Register({ .id="W05", .name="해류",     .grade=RuneGrade::Unique,  .element=ElementType::Water, .damageMult=1.35f, .lifestealRatio=0.08f });
    Register({ .id="F05", .name="용암",     .grade=RuneGrade::Unique,  .element=ElementType::Fire,  .damageMult=1.35f,
               .onHit=[](SkillContext& ctx){ (void)ctx; /* TODO: 방어력 25% 감소 디버프 */ } });
    Register({ .id="E05", .name="광물",     .grade=RuneGrade::Unique,  .element=ElementType::Earth, .damageMult=1.35f });
    Register({ .id="A05", .name="폭풍",     .grade=RuneGrade::Unique,  .element=ElementType::Wind,  .damageMult=1.35f });
    Register({ .id="S04", .name="연쇄",     .grade=RuneGrade::Unique,  .activationOverride=ActivationType::Place,
               .onHit=[](SkillContext& ctx){ (void)ctx; /* TODO: 설치물 간 연쇄 번개 */ } });

    // ─── 레전더리 등급 (10종) ────────────────────────────────────────────────
    Register({ .id="L01", .name="쌍둥이별", .grade=RuneGrade::Legendary, .doublecast=true });
    Register({ .id="L02", .name="시간왜곡", .grade=RuneGrade::Legendary,
               .onHit=[](SkillContext& ctx){ (void)ctx; /* TODO: 해당 스킬 쿨다운 1초 감소 */ } });
    Register({ .id="L03", .name="원소폭주", .grade=RuneGrade::Legendary,
               .onCast=[](SkillContext& ctx){ (void)ctx; /* TODO: 2개 이상 원소 장착 시 +30% */ } });
    Register({ .id="L04", .name="연금술사", .grade=RuneGrade::Legendary, .damageMult=1.20f,
               .onCast=[](SkillContext& ctx){ (void)ctx; /* TODO: 시전마다 원소 랜덤 변경 */ } });
    Register({ .id="L05", .name="흡수",     .grade=RuneGrade::Legendary, .lifestealRatio=0.15f });
    Register({ .id="L06", .name="처형자",   .grade=RuneGrade::Legendary, .execDamageBonus=0.50f });
    Register({ .id="L07", .name="수호자",   .grade=RuneGrade::Legendary,
               .onCast=[](SkillContext& ctx){ (void)ctx; /* TODO: 스킬 사용 시 보호막 생성 */ } });
    Register({ .id="L08", .name="궤도",     .grade=RuneGrade::Legendary, .homing=true });
    Register({ .id="L09", .name="잔상",     .grade=RuneGrade::Legendary, .echoOnCast=true });
    Register({ .id="L10", .name="무한",     .grade=RuneGrade::Legendary, .cdResetChance=0.10f });
}
