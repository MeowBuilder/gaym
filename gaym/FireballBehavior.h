#pragma once

#include "ISkillBehavior.h"
#include "SkillData.h"

class ProjectileManager;

// Fireball skill implementation
class FireballBehavior : public ISkillBehavior
{
public:
    FireballBehavior();
    FireballBehavior(const SkillData& customData);
    virtual ~FireballBehavior() = default;

    // Set projectile manager reference (required for projectile spawning)
    void SetProjectileManager(ProjectileManager* pManager) { m_pProjectileManager = pManager; }

    // ISkillBehavior interface
    virtual void Execute(GameObject* caster, const DirectX::XMFLOAT3& targetPosition) override;
    virtual void Update(float deltaTime) override;
    virtual bool IsFinished() const override;
    virtual void Reset() override;
    virtual const SkillData& GetSkillData() const override { return m_SkillData; }

private:
    SkillData m_SkillData;
    bool m_bIsFinished = true;
    ProjectileManager* m_pProjectileManager = nullptr;

    // Projectile properties
    DirectX::XMFLOAT3 m_StartPosition;
    DirectX::XMFLOAT3 m_TargetPosition;
    static constexpr float PROJECTILE_SPEED = 30.0f;
};
