#include "stdafx.h"
#include "RuneRegistry.h"
#include "EnemyComponent.h"
#include "PlayerComponent.h"
#include "SkillComponent.h"
#include "Scene.h"
#include "Room.h"
#include "TransformComponent.h"

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

    // Element override — 원소가 지정된 룬은 스킬 속성을 해당 원소로 전환
    if (element != ElementType::None)
        stats.elementOverride = element;

    // Behavioral flags
    stats.extraProjectiles += extraProjectiles * stackCount;
    if (piercing)       stats.piercing       = true;
    if (homing)         stats.homing         = true;
    if (doublecast)     stats.doublecast     = true;
    if (echoOnCast)     stats.echoOnCast     = true;
    stats.lifestealRatio   += lifestealRatio   * static_cast<float>(stackCount);
    stats.execDamageBonus  += execDamageBonus  * static_cast<float>(stackCount);
    stats.cdResetChance    += cdResetChance    * static_cast<float>(stackCount);
    stats.orbitalCount          += orbitalCount     * stackCount;
    stats.spawnOnHitCount       += spawnOnHitCount  * stackCount;
    if (randomElementOnCast)     stats.randomElementOnCast = true;

    // 서브 파티클 VFX (중복 방지)
    if (!subVFXId.empty())
    {
        bool already = false;
        for (const auto& id : stats.subVFXIds)
            if (id == subVFXId) { already = true; break; }
        if (!already) stats.subVFXIds.push_back(subVFXId);
    }

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

    // 원소 강화 (Normal)
    Register({ .id="W01", .name="수속성",       .grade=RuneGrade::Normal,  .element=ElementType::Water, .damageMult=1.10f, .subVFXId="sub_water" });
    Register({ .id="F01", .name="화속성",       .grade=RuneGrade::Normal,  .element=ElementType::Fire,  .damageMult=1.10f, .subVFXId="sub_fire"  });
    Register({ .id="E01", .name="토속성",       .grade=RuneGrade::Normal,  .element=ElementType::Earth, .damageMult=1.10f, .subVFXId="sub_earth" });
    Register({ .id="A01", .name="풍속성",       .grade=RuneGrade::Normal,  .element=ElementType::Wind,  .damageMult=1.10f, .subVFXId="sub_wind"  });

    // 발동 방식 (Normal)
    Register({ .id="I01", .name="신속",         .grade=RuneGrade::Normal,  .castTimeMult=0.85f });
    Register({ .id="I02", .name="연사",         .grade=RuneGrade::Normal,  .damageMult=0.85f,  .extraProjectiles=1 });
    Register({ .id="C01", .name="차징 파괴",    .grade=RuneGrade::Normal,  .damageMult=1.30f,  .activationOverride=ActivationType::Charge });
    Register({ .id="C02", .name="차징 신속",    .grade=RuneGrade::Normal,  .castTimeMult=0.80f, .activationOverride=ActivationType::Charge });
    Register({ .id="T01", .name="채널 파괴",    .grade=RuneGrade::Normal,  .damageMult=1.15f,  .activationOverride=ActivationType::Channel });
    Register({ .id="T02", .name="채널 신속",    .grade=RuneGrade::Normal,  .castTimeMult=0.85f, .activationOverride=ActivationType::Channel });
    Register({ .id="S01", .name="설치 연장",    .grade=RuneGrade::Normal,  .durationMult=1.25f, .activationOverride=ActivationType::Place });
    Register({ .id="S02", .name="설치 파괴",    .grade=RuneGrade::Normal,  .damageMult=1.30f,  .activationOverride=ActivationType::Place });
    Register({ .id="B01", .name="증강 파괴",    .grade=RuneGrade::Normal,  .damageMult=1.15f,  .activationOverride=ActivationType::Enhance });
    Register({ .id="B02", .name="증강 연장",    .grade=RuneGrade::Normal,  .durationMult=1.30f, .activationOverride=ActivationType::Enhance });
    Register({ .id="X01", .name="강격",         .grade=RuneGrade::Normal,  .damageMult=1.05f });

    // ─── 레어 등급 (12종) ────────────────────────────────────────────────────

    // 원소 강화 (Rare)
    Register({ .id="W02", .name="냉속성",       .grade=RuneGrade::Rare,    .element=ElementType::Water, .damageMult=1.15f, .statusDurationMult=1.20f, .subVFXId="sub_water" });
    Register({ .id="W03", .name="빙속성",       .grade=RuneGrade::Rare,    .element=ElementType::Water, .statusChanceMult=1.40f,                        .subVFXId="sub_water" });
    Register({ .id="F02", .name="점화",         .grade=RuneGrade::Rare,    .element=ElementType::Fire,  .damageMult=1.15f, .statusDurationMult=1.15f,  .subVFXId="sub_fire"  });
    Register({ .id="F03", .name="작열",         .grade=RuneGrade::Rare,    .element=ElementType::Fire,  .damageMult=1.10f, .statusChanceMult=1.30f,    .subVFXId="sub_fire"  });
    Register({ .id="E02", .name="암석",         .grade=RuneGrade::Rare,    .element=ElementType::Earth, .damageMult=1.15f,                              .subVFXId="sub_earth" });
    Register({ .id="E03", .name="진동",         .grade=RuneGrade::Rare,    .element=ElementType::Earth, .statusDurationMult=1.30f,                      .subVFXId="sub_earth" });
    Register({ .id="A02", .name="질풍",         .grade=RuneGrade::Rare,    .element=ElementType::Wind,  .damageMult=1.15f,                              .subVFXId="sub_wind"  });
    Register({ .id="A03", .name="냉각풍",       .grade=RuneGrade::Rare,    .element=ElementType::Wind,  .cooldownMult=0.90f,                            .subVFXId="sub_wind"  });

    // 발동 방식 (Rare)
    Register({ .id="I03", .name="관통",         .grade=RuneGrade::Rare,    .piercing=true });
    Register({ .id="C03", .name="차징 범위",    .grade=RuneGrade::Rare,    .radiusMult=1.30f,  .activationOverride=ActivationType::Charge });
    Register({ .id="T03", .name="채널 냉각",    .grade=RuneGrade::Rare,    .cooldownMult=0.90f, .activationOverride=ActivationType::Channel });
    Register({ .id="S03", .name="설치 연사",    .grade=RuneGrade::Rare,    .activationOverride=ActivationType::Place, .extraProjectiles=1 });

    // ─── 에픽 등급 (8종) ─────────────────────────────────────────────────────

    // 원소 강화 (Epic)
    Register({ .id="W04", .name="급류",         .grade=RuneGrade::Epic,    .element=ElementType::Water, .damageMult=1.25f,                              .subVFXId="sub_water" });
    Register({ .id="F04", .name="폭염",         .grade=RuneGrade::Epic,    .element=ElementType::Fire,  .damageMult=1.25f, .radiusMult=1.20f,           .subVFXId="sub_fire"  });
    Register({ .id="E04", .name="대지",         .grade=RuneGrade::Epic,    .element=ElementType::Earth, .damageMult=1.25f,                              .subVFXId="sub_earth" });
    Register({ .id="A04", .name="회오리",       .grade=RuneGrade::Epic,    .element=ElementType::Wind,  .damageMult=1.25f, .knockbackMult=1.40f,        .subVFXId="sub_wind"  });

    // 발동 방식 (Epic)
    Register({ .id="I04", .name="분열",         .grade=RuneGrade::Epic,    .extraProjectiles=3 });
    Register({ .id="C04", .name="차징 냉각",    .grade=RuneGrade::Epic,    .cooldownMult=0.75f, .activationOverride=ActivationType::Charge });
    Register({ .id="T04", .name="채널 범위",    .grade=RuneGrade::Epic,    .radiusMult=1.40f,  .activationOverride=ActivationType::Channel });
    // 증강 강화: 증강 모드 + 데미지 1.15배 + 범위 1.20배
    Register({ .id="B03", .name="증강 강화",    .grade=RuneGrade::Epic,    .damageMult=1.15f, .radiusMult=1.20f, .activationOverride=ActivationType::Enhance });

    // ─── 유니크 등급 (5종) ───────────────────────────────────────────────────
    // 흡혈류: 수속성 + 데미지 1.35배 + 흡혈 8%
    Register({ .id="W05", .name="흡혈류",       .grade=RuneGrade::Unique,  .element=ElementType::Water, .damageMult=1.35f, .lifestealRatio=0.08f, .subVFXId="sub_water" });
    // 방어 분쇄: 화속성 + 데미지 1.35배 + 방어력 25% 감소 디버프 (3초)
    Register({ .id="F05", .name="방어 분쇄",    .grade=RuneGrade::Unique,  .element=ElementType::Fire,  .damageMult=1.35f, .subVFXId="sub_fire",
               .onHit=[](SkillContext& ctx){
                   if (!ctx.hitEnemy) return;
                   auto* pEnemy = static_cast<EnemyComponent*>(ctx.hitEnemy);
                   pEnemy->ApplyDefenseDebuff(0.75f, 3.0f);
               } });
    Register({ .id="E05", .name="광물",         .grade=RuneGrade::Unique,  .element=ElementType::Earth, .damageMult=1.35f, .subVFXId="sub_earth" });
    Register({ .id="A05", .name="폭풍",         .grade=RuneGrade::Unique,  .element=ElementType::Wind,  .damageMult=1.35f, .subVFXId="sub_wind"  });
    // 연쇄 번개: 설치 모드 + 적중 시 가장 가까운 다른 적에게 50% 피해 연쇄
    Register({ .id="S04", .name="연쇄 번개",    .grade=RuneGrade::Unique,  .activationOverride=ActivationType::Place,
               .onHit=[](SkillContext& ctx){
                   if (!ctx.scene || !ctx.hitEnemy) return;
                   auto* pScene   = static_cast<Scene*>(ctx.scene);
                   auto* pHitEnemy = static_cast<EnemyComponent*>(ctx.hitEnemy);
                   CRoom* pRoom   = pScene->GetCurrentRoom();
                   if (!pRoom) return;
                   EnemyComponent* pNearest = nullptr;
                   float nearestDist = 12.f;
                   XMVECTOR origin = XMLoadFloat3(&ctx.hitEnemyPos);
                   for (const auto& obj : pRoom->GetGameObjects()) {
                       if (!obj) continue;
                       EnemyComponent* e = obj->GetComponent<EnemyComponent>();
                       if (!e || e->IsDead() || e == pHitEnemy) continue;
                       TransformComponent* t = obj->GetTransform();
                       if (!t) continue;
                       XMFLOAT3 ep = t->GetPosition();
                       float d = XMVectorGetX(XMVector3Length(XMLoadFloat3(&ep) - origin));
                       if (d < nearestDist) { nearestDist = d; pNearest = e; }
                   }
                   if (pNearest) pNearest->TakeDamage(ctx.damageDealt * 0.5f, false);
               } });

    // ─── 추가타 계열 (4종) ───────────────────────────────────────────────────
    Register({ .id="O01", .name="궤도",         .grade=RuneGrade::Rare,   .orbitalCount=2 });
    Register({ .id="O02", .name="중궤도",       .grade=RuneGrade::Epic,   .orbitalCount=4 });
    Register({ .id="H01", .name="반향",         .grade=RuneGrade::Rare,   .spawnOnHitCount=1 });
    Register({ .id="H02", .name="연쇄 반향",    .grade=RuneGrade::Epic,   .damageMult=0.85f, .spawnOnHitCount=2 });

    // ─── 범용 크기/데미지 계열 (4종) ─────────────────────────────────────────
    Register({ .id="G01", .name="범위 확대",    .grade=RuneGrade::Normal, .radiusMult=1.20f });
    Register({ .id="G02", .name="광역",         .grade=RuneGrade::Rare,   .damageMult=0.92f, .radiusMult=1.35f });
    Register({ .id="X02", .name="강타",         .grade=RuneGrade::Normal, .damageMult=1.15f });
    Register({ .id="X03", .name="파괴",         .grade=RuneGrade::Rare,   .damageMult=1.25f, .cooldownMult=1.15f });

    // ─── 레전더리 등급 (10종) ────────────────────────────────────────────────
    // 이중 발사: 즉시 같은 방향으로 한 번 더 발사 (50% 데미지)
    Register({ .id="L01", .name="이중 발사",    .grade=RuneGrade::Legendary, .doublecast=true });
    // 시간 역행: 적중 시 해당 스킬 쿨다운 1초 감소
    Register({ .id="L02", .name="시간 역행",    .grade=RuneGrade::Legendary,
               .onHit=[](SkillContext& ctx){
                   if (!ctx.caster || ctx.skillSlot == SkillSlot::Count) return;
                   SkillComponent* pSkill = ctx.caster->GetComponent<SkillComponent>();
                   if (pSkill) pSkill->ReduceCooldown(ctx.skillSlot, 1.0f);
               } });
    // 원소 증폭: BuildSkillStats에서 2개 이상 원소 장착 감지 시 +30% (onCast 훅 불필요)
    Register({ .id="L03", .name="원소 증폭",    .grade=RuneGrade::Legendary });
    // 원소 변환: 시전 시마다 원소를 무작위로 변경 + 데미지 1.20배
    Register({ .id="L04", .name="원소 변환",    .grade=RuneGrade::Legendary, .damageMult=1.20f, .randomElementOnCast=true });
    // 흡혈: 모든 스킬 피해의 15% 회복
    Register({ .id="L05", .name="흡혈",         .grade=RuneGrade::Legendary, .lifestealRatio=0.15f });
    Register({ .id="L06", .name="처형자",       .grade=RuneGrade::Legendary, .execDamageBonus=0.50f });
    // 보호막: 스킬 시전 시 기본 데미지의 30% 만큼 보호막 생성
    Register({ .id="L07", .name="보호막",       .grade=RuneGrade::Legendary,
               .onCast=[](SkillContext& ctx){
                   if (!ctx.caster) return;
                   PlayerComponent* pPlayer = ctx.caster->GetComponent<PlayerComponent>();
                   if (pPlayer) pPlayer->AddShield(ctx.baseDamage * 0.30f);
               } });
    // 유도: 투사체가 가장 가까운 적을 자동 추적
    Register({ .id="L08", .name="유도",         .grade=RuneGrade::Legendary, .homing=true });
    // 메아리: 스킬 시전 시 50% 데미지로 즉시 재시전
    Register({ .id="L09", .name="메아리",       .grade=RuneGrade::Legendary, .echoOnCast=true });
    Register({ .id="L10", .name="무한",         .grade=RuneGrade::Legendary, .cdResetChance=0.10f });
}
