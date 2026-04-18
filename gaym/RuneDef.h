#pragma once
#include <string>
#include <functional>
#include <optional>
#include <vector>
#include <DirectXMath.h>
#include "SkillTypes.h"
#include "VFXTypes.h"

using namespace DirectX;

class GameObject;

// ─────────────────────────────────────────────────────────────────────────────
// Context passed to rune hook callbacks (onCast, onHit)
// ─────────────────────────────────────────────────────────────────────────────
struct SkillContext
{
    GameObject* caster        = nullptr;
    XMFLOAT3    targetPos     = {};
    ElementType element       = ElementType::None;
    float       baseDamage    = 0.f;
    float       damageDealt   = 0.f;   // populated on hit
    int         projectileIdx = 0;     // for split/multi projectiles
};

// ─────────────────────────────────────────────────────────────────────────────
// Accumulated stats computed from all equipped runes on one skill slot.
// Skills and SkillComponent read only this — they never inspect RuneDef directly.
// ─────────────────────────────────────────────────────────────────────────────
struct SkillStats
{
    // Stat multipliers (all start at 1.0)
    float damageMult         = 1.f;
    float cooldownMult       = 1.f;
    float rangeMult          = 1.f;
    float radiusMult         = 1.f;
    float castTimeMult       = 1.f;
    float durationMult       = 1.f;
    float manaCostMult       = 1.f;
    float statusDurationMult = 1.f;
    float statusChanceMult   = 1.f;
    float knockbackMult      = 1.f;

    // Final activation type (skill default unless overridden by a rune)
    ActivationType activationType = ActivationType::Instant;

    // Element override (if any rune changes the element)
    std::optional<ElementType> elementOverride;

    // VFX modification
    VFXModifier vfxMod;

    // Behavioral flags
    int   extraProjectiles = 0;    // 연사 (I02): +1 per rune
    bool  piercing         = false; // 관통 (I03)
    bool  homing           = false; // 궤도 (L08)
    float lifestealRatio   = 0.f;  // 흡수 (L05, W05)
    float execDamageBonus  = 0.f;  // 처형자 (L06): extra mult when target < 30% HP
    bool  doublecast       = false; // 쌍둥이별 (L01)
    bool  echoOnCast       = false; // 잔상 (L09)
    float cdResetChance    = 0.f;  // 무한 (L10): % chance to reset cooldown

    // Hooks accumulated from all equipped runes
    std::vector<std::function<void(SkillContext&)>> onCastHooks;
    std::vector<std::function<void(SkillContext&)>> onHitHooks;

    // Activation type helpers
    bool IsCharge()  const { return activationType == ActivationType::Charge; }
    bool IsChannel() const { return activationType == ActivationType::Channel; }
    bool IsPlace()   const { return activationType == ActivationType::Place; }
    bool IsEnhance() const { return activationType == ActivationType::Enhance; }
    bool IsSplit()   const { return activationType == ActivationType::Split; }

    // Legacy RuneCombo interop — used by code not yet migrated to SkillStats
    RuneCombo ToRuneCombo() const
    {
        RuneCombo c;
        c.hasCharge  = IsCharge();
        c.hasChannel = IsChannel();
        c.hasPlace   = IsPlace();
        c.hasEnhance = IsEnhance();
        c.hasSplit   = IsSplit() || (extraProjectiles > 0);
        c.hasInstant = (activationType == ActivationType::Instant);
        c.count      = 0; // count not used by legacy code
        return c;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Rune grades
// ─────────────────────────────────────────────────────────────────────────────
enum class RuneGrade : uint8_t
{
    Normal,
    Rare,
    Epic,
    Unique,
    Legendary
};

// ─────────────────────────────────────────────────────────────────────────────
// Static definition of one rune type (registered in RuneRegistry)
// ─────────────────────────────────────────────────────────────────────────────
struct RuneDef
{
    std::string id;
    std::string name;
    RuneGrade   grade   = RuneGrade::Normal;
    ElementType element = ElementType::None;  // None = universal

    // Stat multipliers (base values for 1 stack)
    // Stack formula: effective = 1 + (baseMult - 1) * stackCount
    float damageMult         = 1.f;
    float cooldownMult       = 1.f;
    float rangeMult          = 1.f;
    float radiusMult         = 1.f;
    float castTimeMult       = 1.f;
    float durationMult       = 1.f;
    float manaCostMult       = 1.f;
    float statusDurationMult = 1.f;
    float statusChanceMult   = 1.f;
    float knockbackMult      = 1.f;

    // Activation type override (overrides the skill's default activation)
    std::optional<ActivationType> activationOverride;

    // VFX modification applied when this rune is equipped
    VFXModifier vfxMod;

    // Behavioral flags (additive / boolean)
    int   extraProjectiles = 0;
    bool  piercing         = false;
    bool  homing           = false;
    float lifestealRatio   = 0.f;
    float execDamageBonus  = 0.f;
    bool  doublecast       = false;
    bool  echoOnCast       = false;
    float cdResetChance    = 0.f;

    // Complex behavior hooks (nullptr for simple runes)
    std::function<void(SkillContext&)> onCast;
    std::function<void(SkillContext&)> onHit;

    // Accumulate this rune's contribution into stats
    void ApplyTo(SkillStats& stats, int stackCount = 1) const;

    // Per-stack bonus for each grade (see RuneList.md)
    static float GetStackBonus(RuneGrade grade);
};

// ─────────────────────────────────────────────────────────────────────────────
// An equipped rune instance (stored per slot in SkillComponent)
// ─────────────────────────────────────────────────────────────────────────────
struct EquippedRune
{
    std::string runeId;      // "" = empty slot
    int         stackCount = 1;

    bool IsEmpty() const { return runeId.empty(); }
};
