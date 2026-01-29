#pragma once

#include <DirectXMath.h>

using namespace DirectX;

// Individual particle data
struct Particle
{
    XMFLOAT3 position;
    XMFLOAT3 velocity;
    XMFLOAT4 color;         // Current color (RGBA)
    XMFLOAT4 startColor;    // Color at birth
    XMFLOAT4 endColor;      // Color at death
    float size;             // Current size
    float startSize;        // Size at birth
    float endSize;          // Size at death
    float lifetime;         // Total lifetime
    float age;              // Current age
    bool isActive = false;

    // Get normalized age (0 to 1)
    float GetNormalizedAge() const
    {
        return (lifetime > 0.0f) ? (age / lifetime) : 1.0f;
    }

    // Update particle
    void Update(float deltaTime, const XMFLOAT3& gravity = XMFLOAT3(0, 0, 0))
    {
        if (!isActive) return;

        age += deltaTime;
        if (age >= lifetime)
        {
            isActive = false;
            return;
        }

        // Update position
        position.x += velocity.x * deltaTime;
        position.y += velocity.y * deltaTime;
        position.z += velocity.z * deltaTime;

        // Apply gravity
        velocity.x += gravity.x * deltaTime;
        velocity.y += gravity.y * deltaTime;
        velocity.z += gravity.z * deltaTime;

        // Interpolate color and size based on age
        float t = GetNormalizedAge();
        color.x = startColor.x + (endColor.x - startColor.x) * t;
        color.y = startColor.y + (endColor.y - startColor.y) * t;
        color.z = startColor.z + (endColor.z - startColor.z) * t;
        color.w = startColor.w + (endColor.w - startColor.w) * t;

        size = startSize + (endSize - startSize) * t;
    }
};

// Particle emitter configuration
struct ParticleEmitterConfig
{
    // Emission settings
    float emissionRate = 10.0f;         // Particles per second
    int burstCount = 0;                 // Instant burst count (0 = continuous)

    // Lifetime
    float minLifetime = 0.5f;
    float maxLifetime = 1.0f;

    // Size
    float minStartSize = 0.2f;
    float maxStartSize = 0.4f;
    float minEndSize = 0.0f;
    float maxEndSize = 0.1f;

    // Velocity
    XMFLOAT3 minVelocity = { -1.0f, -1.0f, -1.0f };
    XMFLOAT3 maxVelocity = { 1.0f, 1.0f, 1.0f };

    // Color
    XMFLOAT4 startColor = { 1.0f, 0.5f, 0.0f, 1.0f };   // Orange
    XMFLOAT4 endColor = { 1.0f, 0.0f, 0.0f, 0.0f };     // Red, fading out

    // Physics
    XMFLOAT3 gravity = { 0.0f, 0.0f, 0.0f };

    // Spawn area
    float spawnRadius = 0.1f;
};

// Predefined emitter configs for fire effects
namespace FireParticlePresets
{
    inline ParticleEmitterConfig FireballTrail()
    {
        ParticleEmitterConfig config;
        config.emissionRate = 30.0f;
        config.minLifetime = 0.2f;
        config.maxLifetime = 0.4f;
        config.minStartSize = 0.3f;
        config.maxStartSize = 0.5f;
        config.minEndSize = 0.0f;
        config.maxEndSize = 0.1f;
        config.minVelocity = { -0.5f, -0.5f, -0.5f };
        config.maxVelocity = { 0.5f, 0.5f, 0.5f };
        config.startColor = { 1.0f, 0.6f, 0.1f, 1.0f };  // Bright orange
        config.endColor = { 1.0f, 0.2f, 0.0f, 0.0f };    // Dark red, fade out
        config.gravity = { 0.0f, 1.0f, 0.0f };           // Slight upward drift
        config.spawnRadius = 0.2f;
        return config;
    }

    inline ParticleEmitterConfig FireballExplosion()
    {
        ParticleEmitterConfig config;
        config.emissionRate = 0.0f;  // Burst only
        config.burstCount = 30;
        config.minLifetime = 0.3f;
        config.maxLifetime = 0.6f;
        config.minStartSize = 0.4f;
        config.maxStartSize = 0.8f;
        config.minEndSize = 0.0f;
        config.maxEndSize = 0.2f;
        config.minVelocity = { -5.0f, -5.0f, -5.0f };
        config.maxVelocity = { 5.0f, 5.0f, 5.0f };
        config.startColor = { 1.0f, 0.8f, 0.3f, 1.0f };  // Yellow-orange
        config.endColor = { 0.8f, 0.1f, 0.0f, 0.0f };    // Dark red, fade out
        config.gravity = { 0.0f, -2.0f, 0.0f };          // Fall down
        config.spawnRadius = 0.5f;
        return config;
    }
}
