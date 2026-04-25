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
        config.emissionRate = 15.0f;  // Reduced from 30 for performance
        config.minLifetime = 0.15f;
        config.maxLifetime = 0.3f;
        config.minStartSize = 0.25f;
        config.maxStartSize = 0.4f;
        config.minEndSize = 0.0f;
        config.maxEndSize = 0.08f;
        config.minVelocity = { -0.4f, -0.4f, -0.4f };
        config.maxVelocity = { 0.4f, 0.4f, 0.4f };
        config.startColor = { 1.0f, 0.6f, 0.1f, 1.0f };  // Bright orange
        config.endColor = { 1.0f, 0.2f, 0.0f, 0.0f };    // Dark red, fade out
        config.gravity = { 0.0f, 1.0f, 0.0f };           // Slight upward drift
        config.spawnRadius = 0.15f;
        return config;
    }

    inline ParticleEmitterConfig FireballExplosion()
    {
        ParticleEmitterConfig config;
        config.emissionRate = 0.0f;  // Burst only
        config.burstCount = 12;      // Reduced from 30 for performance
        config.minLifetime = 0.2f;
        config.maxLifetime = 0.4f;
        config.minStartSize = 0.35f;
        config.maxStartSize = 0.6f;
        config.minEndSize = 0.0f;
        config.maxEndSize = 0.15f;
        config.minVelocity = { -4.0f, -4.0f, -4.0f };
        config.maxVelocity = { 4.0f, 4.0f, 4.0f };
        config.startColor = { 1.0f, 0.8f, 0.3f, 1.0f };  // Yellow-orange
        config.endColor = { 0.8f, 0.1f, 0.0f, 0.0f };    // Dark red, fade out
        config.gravity = { 0.0f, -2.0f, 0.0f };          // Fall down
        config.spawnRadius = 0.4f;
        return config;
    }

    // Floating embers for volcanic atmosphere
    inline ParticleEmitterConfig FloatingEmbers()
    {
        ParticleEmitterConfig config;
        config.emissionRate = 4.0f;  // Reduced from 8 for performance
        config.minLifetime = 1.5f;
        config.maxLifetime = 3.0f;
        config.minStartSize = 0.05f;
        config.maxStartSize = 0.12f;
        config.minEndSize = 0.0f;
        config.maxEndSize = 0.02f;
        config.minVelocity = { -1.0f, 1.5f, -1.0f };   // Drift upward
        config.maxVelocity = { 1.0f, 3.0f, 1.0f };
        config.startColor = { 1.0f, 0.5f, 0.1f, 1.0f };  // Bright orange
        config.endColor = { 1.0f, 0.2f, 0.0f, 0.0f };    // Fade to dark red
        config.gravity = { 0.0f, 0.3f, 0.0f };           // Slight upward drift
        config.spawnRadius = 15.0f;  // Wide spawn area around emitter
        return config;
    }

    // 평상시 깔리는 먼지 — Earth 스테이지 ambient (가시성 확보).
    inline ParticleEmitterConfig FloatingDust()
    {
        ParticleEmitterConfig config;
        config.emissionRate = 35.0f;
        config.minLifetime = 2.2f;
        config.maxLifetime = 3.8f;
        config.minStartSize = 0.20f;
        config.maxStartSize = 0.38f;
        config.minEndSize = 0.22f;
        config.maxEndSize = 0.45f;
        config.minVelocity = { 0.8f, -0.1f, -0.4f };   // 약한 +X 미풍 (방향성 유지)
        config.maxVelocity = { 2.2f,  0.3f,  0.4f };
        config.startColor = { 0.82f, 0.70f, 0.52f, 0.55f };  // 밝기·알파 ↑
        config.endColor   = { 0.68f, 0.56f, 0.40f, 0.0f };
        config.gravity    = { 0.4f, -0.05f, 0.0f };
        config.spawnRadius = 12.0f;                          // 좁혀 밀도 ↑
        return config;
    }

    // 주기적 모래폭풍 burst — Earth 스테이지에서 Active 상태 동안 쓸려옴.
    // FloatingDust 대비 방출량/속도/입자 크기 모두 증가.
    inline ParticleEmitterConfig Sandstorm()
    {
        ParticleEmitterConfig config;
        config.emissionRate = 95.0f;
        config.minLifetime = 1.4f;
        config.maxLifetime = 2.6f;
        config.minStartSize = 0.22f;
        config.maxStartSize = 0.48f;
        config.minEndSize = 0.30f;
        config.maxEndSize = 0.60f;                     // 끝까지 streak 유지
        config.minVelocity = { 5.0f, -0.3f, -0.8f };
        config.maxVelocity = { 9.5f,  0.5f,  0.8f };
        config.startColor = { 0.85f, 0.70f, 0.50f, 0.65f };
        config.endColor   = { 0.70f, 0.55f, 0.38f, 0.0f };
        config.gravity    = { 2.5f, -0.05f, 0.0f };    // 가속 풍압
        config.spawnRadius = 18.0f;
        return config;
    }

    // Mega breath effect - continuous fire stream from dragon's mouth
    inline ParticleEmitterConfig DragonBreathStream()
    {
        ParticleEmitterConfig config;
        config.emissionRate = 20.0f;  // Reduced from 40 for performance
        config.burstCount = 0;
        config.minLifetime = 0.4f;
        config.maxLifetime = 0.8f;
        config.minStartSize = 1.5f;
        config.maxStartSize = 2.5f;
        config.minEndSize = 3.0f;
        config.maxEndSize = 5.0f;
        config.minVelocity = { -2.0f, -1.0f, -2.0f };
        config.maxVelocity = { 2.0f, 2.0f, 2.0f };
        config.startColor = { 1.0f, 0.9f, 0.3f, 1.0f };  // Bright yellow-white core
        config.endColor = { 1.0f, 0.2f, 0.0f, 0.0f };    // Fade to red
        config.gravity = { 0.0f, 0.5f, 0.0f };           // Slight upward
        config.spawnRadius = 0.5f;
        return config;
    }
}
