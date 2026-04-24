#pragma once

#include <DirectXMath.h>
#include <vector>
#include "SkillTypes.h"

using namespace DirectX;

// Projectile data structure
struct Projectile
{
    // Transform
    XMFLOAT3 position;
    XMFLOAT3 direction;
    float speed = 30.0f;

    // Combat
    float damage = 10.0f;
    float radius = 0.5f;          // Collision radius
    float explosionRadius = 0.0f; // AoE explosion radius (0 = single target)
    ElementType element = ElementType::None;

    // Ownership
    class GameObject* owner = nullptr;
    bool isPlayerProjectile = true;

    // Lifetime
    float maxDistance = 100.0f;
    float distanceTraveled = 0.0f;
    bool isActive = true;

    // Visual
    float scale = 1.0f;
    float chargeRatio = 0.0f;
    int fluidVFXId = -1;             // Associated fluid VFX effect (-1 = none)
    std::vector<int> extraVFXIds;    // Extra VFX slots for multi-element runes
    RuneCombo runeCombo;             // Rune combo for VFX customization
    std::vector<ElementType> elementSet; // Rune element set for multi-element VFX
    bool wasHit = false;         // True if deactivated by collision (not range)

    // Rune-driven runtime flags (set at spawn from SkillStats)
    bool  isPiercing      = false;  // 관통: 충돌 후에도 계속 진행
    bool  isHoming        = false;  // 유도: 매 프레임 가장 가까운 적 방향으로 회전
    float lifestealRatio  = 0.f;    // 흡수: 피해량 * ratio 만큼 시전자 HP 회복
    float execDamageBonus = 0.f;    // 처형자: 대상 HP 30% 이하 시 추가 배율
    float cdResetChance   = 0.f;    // 무한: 적중 시 쿨다운 초기화 확률
    SkillSlot skillSlot   = SkillSlot::Count; // 적중 시 onHit 훅 호출용 슬롯 정보

    // Helper to update position
    void Update(float deltaTime)
    {
        if (!isActive) return;

        float moveDistance = speed * deltaTime;
        position.x += direction.x * moveDistance;
        position.y += direction.y * moveDistance;
        position.z += direction.z * moveDistance;

        distanceTraveled += moveDistance;

        // Deactivate if max distance reached
        if (distanceTraveled >= maxDistance)
        {
            isActive = false;
        }
    }

    // Get bounding sphere for collision
    DirectX::BoundingSphere GetBoundingSphere() const
    {
        return DirectX::BoundingSphere(position, radius);
    }
};
