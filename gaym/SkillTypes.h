#pragma once

#include <cstdint>

// Skill slot enum - maps to input keys
enum class SkillSlot : uint8_t
{
    Q = 0,          // Q key skill
    E,              // E key skill
    R,              // R key skill (ultimate)
    RightClick,     // Right mouse button skill
    Count
};

// Element type for skills
enum class ElementType : uint8_t
{
    None = 0,
    Fire,       // 불
    Water,      // 물
    Wind,       // 바람
    Earth       // 대지
};

// How the skill is activated (determined by Activation Rune)
enum class ActivationType : uint8_t
{
    Instant = 0,    // 1: Immediate effect on key press
    Charge,         // 2: Charge up then release (more damage)
    Channel,        // 3: Channeled over time (continuous effect)
    Place,          // 4: Place trap/turret at location
    Enhance,        // 5: Self-buff, enhance next attack
    Count
};

// Rune types for skill modification
enum class RuneType : uint8_t
{
    // Activation Runes (changes how skill is cast)
    ActivationInstant = 0,
    ActivationCharge,
    ActivationChannel,
    ActivationPlace,
    ActivationEnhance,

    // Element Runes (changes element) - future
    // Effect Runes (adds effects) - future

    None = 255
};

// Current state of a skill
enum class SkillState : uint8_t
{
    Ready,          // Available to use
    Casting,        // Currently being cast/executed
    Cooldown,       // On cooldown, cannot use
    Disabled        // Temporarily unavailable
};
