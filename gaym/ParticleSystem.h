#pragma once

#include "Particle.h"
#include <vector>
#include <memory>
#include <random>

class CDescriptorHeap;
class Mesh;

// Particle emitter - spawns and manages particles
class ParticleEmitter
{
public:
    ParticleEmitter(const ParticleEmitterConfig& config, size_t maxParticles = 100);
    ~ParticleEmitter() = default;

    // Update emitter and all particles
    void Update(float deltaTime);

    // Set emitter position (for following projectiles)
    void SetPosition(const XMFLOAT3& position) { m_Position = position; }
    const XMFLOAT3& GetPosition() const { return m_Position; }

    // Control emission
    void Start() { m_bIsEmitting = true; }
    void Stop() { m_bIsEmitting = false; }
    bool IsEmitting() const { return m_bIsEmitting; }

    // Trigger a burst of particles
    void Burst(int count = -1);  // -1 uses config's burstCount

    // Check if emitter has any active particles
    bool HasActiveParticles() const;

    // Access particles for rendering
    const std::vector<Particle>& GetParticles() const { return m_Particles; }

    // Get active particle count
    size_t GetActiveParticleCount() const;

private:
    void SpawnParticle();
    float RandomFloat(float min, float max);
    XMFLOAT3 RandomVector(const XMFLOAT3& min, const XMFLOAT3& max);

private:
    ParticleEmitterConfig m_Config;
    std::vector<Particle> m_Particles;
    XMFLOAT3 m_Position = { 0, 0, 0 };
    bool m_bIsEmitting = true;
    float m_fEmissionAccumulator = 0.0f;

    std::mt19937 m_RandomEngine;
};

// Particle system manager - handles all emitters and rendering
class ParticleSystem
{
public:
    ParticleSystem();
    ~ParticleSystem();

    // Initialize rendering resources
    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
              CDescriptorHeap* pDescriptorHeap, UINT nStartDescriptorIndex);

    // Create a new emitter and return its ID
    int CreateEmitter(const ParticleEmitterConfig& config, const XMFLOAT3& position);

    // Get emitter by ID
    ParticleEmitter* GetEmitter(int emitterId);

    // Remove emitter
    void RemoveEmitter(int emitterId);

    // Update all emitters
    void Update(float deltaTime);

    // Render all particles
    void Render(ID3D12GraphicsCommandList* pCommandList);

    // Clear all emitters
    void Clear();

    // Get total active particle count
    size_t GetTotalParticleCount() const;

private:
    struct EmitterEntry
    {
        std::unique_ptr<ParticleEmitter> emitter;
        bool active = false;
    };

    std::vector<EmitterEntry> m_Emitters;
    int m_nNextEmitterId = 0;

    // Rendering resources
    std::unique_ptr<Mesh> m_pParticleMesh;
    ComPtr<ID3D12Resource> m_pd3dcbParticles;
    void* m_pcbMappedParticles = nullptr;
    UINT m_nDescriptorStartIndex = 0;
    CDescriptorHeap* m_pDescriptorHeap = nullptr;

    static constexpr size_t MAX_RENDERED_PARTICLES = 512;
};
