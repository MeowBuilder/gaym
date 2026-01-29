#pragma once

#include <DirectXMath.h>

class GameObject;
struct SkillData;

// Strategy pattern interface for skill execution
class ISkillBehavior
{
public:
    virtual ~ISkillBehavior() = default;

    // Execute the skill - called when skill is activated
    // caster: The GameObject using the skill
    // targetPosition: World position where skill is aimed
    // damageMultiplier: Multiplier for damage (default 1.0, can be modified by charge/enhance)
    //   Special values: -1.0 = placement mode, 0.0 = VFX only (no damage)
    virtual void Execute(GameObject* caster, const DirectX::XMFLOAT3& targetPosition, float damageMultiplier = 1.0f) = 0;

    // Update the skill each frame while active
    // Returns true if skill is still active, false if finished
    virtual void Update(float deltaTime) = 0;

    // Check if the skill execution is complete
    virtual bool IsFinished() const = 0;

    // Reset the skill state for reuse
    virtual void Reset() = 0;

    // Get the skill data
    virtual const SkillData& GetSkillData() const = 0;
};
