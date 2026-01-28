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

// How the skill is activated
enum class ActivationType : uint8_t
{
    Instant,        // Immediate effect on key press
    Hold,           // Effect while key is held
    Channel,        // Channeled over time
    Charge          // Charge up then release
};

// Current state of a skill
enum class SkillState : uint8_t
{
    Ready,          // Available to use
    Casting,        // Currently being cast/executed
    Cooldown,       // On cooldown, cannot use
    Disabled        // Temporarily unavailable
};
