#include "stdafx.h"
#include "FireballBehavior.h"
#include "ProjectileManager.h"
#include "GameObject.h"
#include "TransformComponent.h"

FireballBehavior::FireballBehavior()
    : m_SkillData(FireSkillPresets::Fireball())
{
}

FireballBehavior::FireballBehavior(const SkillData& customData)
    : m_SkillData(customData)
{
}

void FireballBehavior::Execute(GameObject* caster, const DirectX::XMFLOAT3& targetPosition)
{
    m_bIsFinished = false;
    m_TargetPosition = targetPosition;

    // Get caster position for start point
    if (caster && caster->GetTransform())
    {
        m_StartPosition = caster->GetTransform()->GetPosition();
        // Raise spawn point to chest height
        m_StartPosition.y += 1.5f;
    }

    // Debug output
    wchar_t buffer[256];
    swprintf_s(buffer, 256,
        L"[Skill] %hs executed! Target: (%.1f, %.1f, %.1f), Damage: %.0f\n",
        m_SkillData.name.c_str(),
        targetPosition.x, targetPosition.y, targetPosition.z,
        m_SkillData.damage);
    OutputDebugString(buffer);

    // Spawn actual projectile if manager is available
    if (m_pProjectileManager)
    {
        m_pProjectileManager->SpawnProjectile(
            m_StartPosition,
            m_TargetPosition,
            m_SkillData.damage,
            PROJECTILE_SPEED,
            0.5f,                       // collision radius
            m_SkillData.radius,         // explosion radius (AoE)
            m_SkillData.element,
            caster,
            true                        // is player projectile
        );
    }
    else
    {
        OutputDebugString(L"[Skill] Warning: No ProjectileManager set!\n");
    }

    // Skill execution is instant - projectile handles the rest
    m_bIsFinished = true;
}

void FireballBehavior::Update(float deltaTime)
{
    // Projectile movement and collision is handled by ProjectileManager
    // This function is now mostly unused for instant-cast skills
}

bool FireballBehavior::IsFinished() const
{
    return m_bIsFinished;
}

void FireballBehavior::Reset()
{
    m_bIsFinished = true;
    m_StartPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_TargetPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
}
