#include "stdafx.h"
#include "ParticleSystem.h"
#include "Mesh.h"
#include "DescriptorHeap.h"
#include "GameObject.h"  // For ObjectConstants layout

// ============================================================================
// ParticleEmitter Implementation
// ============================================================================

ParticleEmitter::ParticleEmitter(const ParticleEmitterConfig& config, size_t maxParticles)
    : m_Config(config)
    , m_RandomEngine(std::random_device{}())
{
    m_Particles.resize(maxParticles);
}

void ParticleEmitter::Update(float deltaTime)
{
    // Update existing particles
    for (auto& particle : m_Particles)
    {
        particle.Update(deltaTime, m_Config.gravity);
    }

    // Emit new particles if emitting
    if (m_bIsEmitting && m_Config.emissionRate > 0.0f)
    {
        m_fEmissionAccumulator += deltaTime * m_Config.emissionRate;

        while (m_fEmissionAccumulator >= 1.0f)
        {
            SpawnParticle();
            m_fEmissionAccumulator -= 1.0f;
        }
    }
}

void ParticleEmitter::Burst(int count)
{
    int burstCount = (count < 0) ? m_Config.burstCount : count;
    for (int i = 0; i < burstCount; ++i)
    {
        SpawnParticle();
    }
}

bool ParticleEmitter::HasActiveParticles() const
{
    for (const auto& particle : m_Particles)
    {
        if (particle.isActive) return true;
    }
    return false;
}

size_t ParticleEmitter::GetActiveParticleCount() const
{
    size_t count = 0;
    for (const auto& particle : m_Particles)
    {
        if (particle.isActive) count++;
    }
    return count;
}

void ParticleEmitter::SpawnParticle()
{
    // Find an inactive particle slot
    for (auto& particle : m_Particles)
    {
        if (!particle.isActive)
        {
            // Initialize the particle
            particle.isActive = true;
            particle.age = 0.0f;
            particle.lifetime = RandomFloat(m_Config.minLifetime, m_Config.maxLifetime);

            // Random position within spawn radius
            float angle = RandomFloat(0.0f, XM_2PI);
            float radius = RandomFloat(0.0f, m_Config.spawnRadius);
            particle.position.x = m_Position.x + cosf(angle) * radius;
            particle.position.y = m_Position.y + RandomFloat(-m_Config.spawnRadius, m_Config.spawnRadius);
            particle.position.z = m_Position.z + sinf(angle) * radius;

            // Random velocity
            particle.velocity = RandomVector(m_Config.minVelocity, m_Config.maxVelocity);

            // Size
            particle.startSize = RandomFloat(m_Config.minStartSize, m_Config.maxStartSize);
            particle.endSize = RandomFloat(m_Config.minEndSize, m_Config.maxEndSize);
            particle.size = particle.startSize;

            // Color
            particle.startColor = m_Config.startColor;
            particle.endColor = m_Config.endColor;
            particle.color = particle.startColor;

            return;
        }
    }
    // No free slot available
}

float ParticleEmitter::RandomFloat(float min, float max)
{
    std::uniform_real_distribution<float> dist(min, max);
    return dist(m_RandomEngine);
}

XMFLOAT3 ParticleEmitter::RandomVector(const XMFLOAT3& min, const XMFLOAT3& max)
{
    return XMFLOAT3(
        RandomFloat(min.x, max.x),
        RandomFloat(min.y, max.y),
        RandomFloat(min.z, max.z)
    );
}

// ============================================================================
// ParticleSystem Implementation
// ============================================================================

ParticleSystem::ParticleSystem()
{
}

ParticleSystem::~ParticleSystem()
{
    if (m_pd3dcbParticles && m_pcbMappedParticles)
    {
        m_pd3dcbParticles->Unmap(0, nullptr);
        m_pcbMappedParticles = nullptr;
    }
}

void ParticleSystem::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                          CDescriptorHeap* pDescriptorHeap, UINT nStartDescriptorIndex)
{
    m_pDescriptorHeap = pDescriptorHeap;
    m_nDescriptorStartIndex = nStartDescriptorIndex;

    // Create particle mesh (small quad/cube for billboard effect)
    m_pParticleMesh = std::make_unique<CubeMesh>(pDevice, pCommandList, 1.0f, 1.0f, 1.0f);

    // Create constant buffer for particles
    UINT nSingleCBSize = (sizeof(ObjectConstants) + 255) & ~255;
    UINT nTotalCBSize = nSingleCBSize * MAX_RENDERED_PARTICLES;
    m_pd3dcbParticles = CreateBufferResource(pDevice, pCommandList, nullptr, nTotalCBSize,
                                              D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

    m_pd3dcbParticles->Map(0, nullptr, &m_pcbMappedParticles);

    // Create CBV for each particle slot
    for (size_t i = 0; i < MAX_RENDERED_PARTICLES; ++i)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = m_pd3dcbParticles->GetGPUVirtualAddress() + (i * nSingleCBSize);
        cbvDesc.SizeInBytes = nSingleCBSize;

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = pDescriptorHeap->GetCPUHandle(nStartDescriptorIndex + static_cast<UINT>(i));
        pDevice->CreateConstantBufferView(&cbvDesc, cpuHandle);
    }

    OutputDebugString(L"[ParticleSystem] Initialized\n");
}

int ParticleSystem::CreateEmitter(const ParticleEmitterConfig& config, const XMFLOAT3& position)
{
    // Find an inactive slot or create a new one
    for (size_t i = 0; i < m_Emitters.size(); ++i)
    {
        if (!m_Emitters[i].active)
        {
            m_Emitters[i].emitter = std::make_unique<ParticleEmitter>(config);
            m_Emitters[i].emitter->SetPosition(position);
            m_Emitters[i].active = true;
            return static_cast<int>(i);
        }
    }

    // Create new slot
    EmitterEntry entry;
    entry.emitter = std::make_unique<ParticleEmitter>(config);
    entry.emitter->SetPosition(position);
    entry.active = true;
    m_Emitters.push_back(std::move(entry));
    return static_cast<int>(m_Emitters.size() - 1);
}

ParticleEmitter* ParticleSystem::GetEmitter(int emitterId)
{
    if (emitterId >= 0 && emitterId < static_cast<int>(m_Emitters.size()) && m_Emitters[emitterId].active)
    {
        return m_Emitters[emitterId].emitter.get();
    }
    return nullptr;
}

void ParticleSystem::RemoveEmitter(int emitterId)
{
    if (emitterId >= 0 && emitterId < static_cast<int>(m_Emitters.size()))
    {
        m_Emitters[emitterId].active = false;
        m_Emitters[emitterId].emitter.reset();
    }
}

void ParticleSystem::Update(float deltaTime)
{
    for (auto& entry : m_Emitters)
    {
        if (entry.active && entry.emitter)
        {
            entry.emitter->Update(deltaTime);

            // Auto-remove emitters that have stopped and have no particles
            if (!entry.emitter->IsEmitting() && !entry.emitter->HasActiveParticles())
            {
                entry.active = false;
                entry.emitter.reset();
            }
        }
    }
}

void ParticleSystem::Render(ID3D12GraphicsCommandList* pCommandList)
{
    if (!m_pParticleMesh || !m_pcbMappedParticles || !m_pDescriptorHeap) return;

    UINT nSingleCBSize = (sizeof(ObjectConstants) + 255) & ~255;
    BYTE* pMappedBytes = reinterpret_cast<BYTE*>(m_pcbMappedParticles);

    size_t renderIndex = 0;

    for (const auto& entry : m_Emitters)
    {
        if (!entry.active || !entry.emitter) continue;

        for (const auto& particle : entry.emitter->GetParticles())
        {
            if (!particle.isActive) continue;
            if (renderIndex >= MAX_RENDERED_PARTICLES) break;

            // Update constant buffer for this particle
            ObjectConstants* pCB = reinterpret_cast<ObjectConstants*>(pMappedBytes + renderIndex * nSingleCBSize);

            // Scale and position
            XMMATRIX worldMatrix = XMMatrixScaling(particle.size, particle.size, particle.size) *
                                   XMMatrixTranslation(particle.position.x, particle.position.y, particle.position.z);

            XMStoreFloat4x4(&pCB->m_xmf4x4World, XMMatrixTranspose(worldMatrix));

            // Material properties
            pCB->m_nMaterialIndex = 0;
            pCB->m_bIsSkinned = 0;
            pCB->m_bHasTexture = 0;

            // Use particle color with emissive for glow effect
            pCB->mMaterial.m_cAmbient = particle.color;
            pCB->mMaterial.m_cDiffuse = particle.color;
            pCB->mMaterial.m_cSpecular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);  // No specular
            pCB->mMaterial.m_cEmissive = XMFLOAT4(
                particle.color.x * 0.8f,
                particle.color.y * 0.8f,
                particle.color.z * 0.8f,
                particle.color.w
            );  // Strong emissive for glow

            // Set descriptor table
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_pDescriptorHeap->GetGPUHandle(
                m_nDescriptorStartIndex + static_cast<UINT>(renderIndex));
            pCommandList->SetGraphicsRootDescriptorTable(0, gpuHandle);

            // Render
            m_pParticleMesh->Render(pCommandList, 0);

            renderIndex++;
        }

        if (renderIndex >= MAX_RENDERED_PARTICLES) break;
    }
}

void ParticleSystem::Clear()
{
    m_Emitters.clear();
}

size_t ParticleSystem::GetTotalParticleCount() const
{
    size_t count = 0;
    for (const auto& entry : m_Emitters)
    {
        if (entry.active && entry.emitter)
        {
            count += entry.emitter->GetActiveParticleCount();
        }
    }
    return count;
}
