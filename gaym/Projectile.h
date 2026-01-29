#pragma once

#include <DirectXMath.h>
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
    int particleEmitterId = -1;  // Associated particle emitter (-1 = none)

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
