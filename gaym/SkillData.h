#pragma once

#include "SkillTypes.h"
#include <string>

// Base data structure for skill configuration
struct SkillData
{
    std::string name;
    ElementType element = ElementType::None;
    ActivationType activationType = ActivationType::Instant;

    float damage = 0.0f;
    float cooldown = 1.0f;          // Seconds
    float castTime = 0.0f;          // Instant if 0
    float range = 10.0f;
    float radius = 0.0f;            // For AoE skills

    // Resource costs (for future use)
    float manaCost = 0.0f;
    float staminaCost = 0.0f;
};

// Preset skill data for fire element skills
namespace FireSkillPresets
{
    inline SkillData Fireball()
    {
        SkillData data;
        data.name = "Fireball";
        data.element = ElementType::Fire;
        data.activationType = ActivationType::Instant;
        data.damage = 30.0f;
        data.cooldown = 2.0f;
        data.castTime = 0.0f;
        data.range = 50.0f;
        data.radius = 3.0f;
        data.manaCost = 10.0f;
        return data;
    }

    inline SkillData FlameWave()
    {
        SkillData data;
        data.name = "Flame Wave";
        data.element = ElementType::Fire;
        data.activationType = ActivationType::Instant;
        data.damage = 20.0f;
        data.cooldown = 5.0f;
        data.castTime = 0.0f;
        data.range = 15.0f;
        data.radius = 8.0f;
        data.manaCost = 25.0f;
        return data;
    }

    inline SkillData Meteor()
    {
        SkillData data;
        data.name = "Meteor";
        data.element = ElementType::Fire;
        data.activationType = ActivationType::Instant;
        data.damage = 100.0f;
        data.cooldown = 30.0f;
        data.castTime = 1.5f;
        data.range = 40.0f;
        data.radius = 10.0f;
        data.manaCost = 50.0f;
        return data;
    }
}
