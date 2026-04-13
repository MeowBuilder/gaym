#include "stdafx.h"
#include "ProjectileManager.h"
#include "ParticleSystem.h"
#include "FluidSkillVFXManager.h"
#include "VFXLibrary.h"
#include "Scene.h"
#include "Room.h"
#include "GameObject.h"
#include "EnemyComponent.h"
#include "PlayerComponent.h"
#include "TransformComponent.h"
#include "Mesh.h"
#include "DescriptorHeap.h"
#include <DirectXCollision.h>

ProjectileManager::ProjectileManager()
{
    m_Projectiles.reserve(MAX_PROJECTILES);
}

ProjectileManager::~ProjectileManager()
{
    if (m_pd3dcbProjectiles && m_pcbMappedProjectiles)
    {
        m_pd3dcbProjectiles->Unmap(0, nullptr);
        m_pcbMappedProjectiles = nullptr;
    }
}

void ProjectileManager::Init(Scene* pScene, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                              CDescriptorHeap* pDescriptorHeap, UINT nStartDescriptorIndex)
{
    m_pScene = pScene;
    m_pDescriptorHeap = pDescriptorHeap;
    m_nDescriptorStartIndex = nStartDescriptorIndex;

    // Get particle system from scene
    m_pParticleSystem = pScene->GetParticleSystem();
    m_pFluidVFXManager = pScene->GetFluidVFXManager();

    // Create projectile mesh (small cube)
    m_pProjectileMesh = std::make_unique<CubeMesh>(pDevice, pCommandList, 0.5f, 0.5f, 0.5f);

    // Create constant buffer for renderable projectiles
    UINT nSingleCBSize = (sizeof(ProjectileConstants) + 255) & ~255;
    UINT nTotalCBSize = nSingleCBSize * MAX_RENDERED_PROJECTILES;
    m_pd3dcbProjectiles = CreateBufferResource(pDevice, pCommandList, nullptr, nTotalCBSize,
                                                D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

    m_pd3dcbProjectiles->Map(0, nullptr, (void**)&m_pcbMappedProjectiles);

    // Create CBV for each projectile slot
    for (size_t i = 0; i < MAX_RENDERED_PROJECTILES; ++i)
    {
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = m_pd3dcbProjectiles->GetGPUVirtualAddress() + (i * nSingleCBSize);
        cbvDesc.SizeInBytes = nSingleCBSize;

        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = pDescriptorHeap->GetCPUHandle(nStartDescriptorIndex + static_cast<UINT>(i));
        pDevice->CreateConstantBufferView(&cbvDesc, cpuHandle);
    }

    OutputDebugString(L"[ProjectileManager] Rendering resources initialized\n");
}

void ProjectileManager::SpawnProjectile(const Projectile& projectile)
{
    if (m_Projectiles.size() >= MAX_PROJECTILES)
    {
        // Remove oldest inactive projectile or skip
        CleanupInactiveProjectiles();
        if (m_Projectiles.size() >= MAX_PROJECTILES)
        {
            OutputDebugString(L"[ProjectileManager] Max projectiles reached!\n");
            return;
        }
    }

    m_Projectiles.push_back(projectile);

    // Create fluid VFX trail for this projectile
    if (m_pFluidVFXManager)
    {
        auto& proj = m_Projectiles.back();

        if (proj.isPlayerProjectile)
        {
            // 플레이어 투사체: VFXLibrary 경로 (일관된 룬 조합 처리)
            uint32_t runeFlags = ToRuneFlags(proj.runeCombo);
            VFXSequenceDef seqDef = VFXLibrary::Get().GetDef(
                SkillSlot::RightClick, runeFlags, proj.element);
            proj.fluidVFXId = m_pFluidVFXManager->SpawnSequenceEffect(
                proj.position, proj.direction, seqDef);
        }
        else
        {
            // 적 투사체: 레거시 경로 유지, 파티클 수 축소
            FluidSkillVFXDef vfxDef = FluidSkillVFXManager::GetVFXDef(
                proj.element, proj.runeCombo, proj.chargeRatio);
            vfxDef.particleCount = static_cast<int>(vfxDef.particleCount * 0.3f);
            if (vfxDef.particleCount < 100) vfxDef.particleCount = 100;
            proj.fluidVFXId = m_pFluidVFXManager->SpawnEffect(
                proj.position, proj.direction, vfxDef);
        }
    }

    wchar_t buffer[256];
    swprintf_s(buffer, 256, L"[ProjectileManager] Spawned projectile at (%.1f, %.1f, %.1f) -> dir (%.2f, %.2f, %.2f)\n",
        projectile.position.x, projectile.position.y, projectile.position.z,
        projectile.direction.x, projectile.direction.y, projectile.direction.z);
    OutputDebugString(buffer);
}

void ProjectileManager::SpawnProjectile(
    const XMFLOAT3& startPos,
    const XMFLOAT3& targetPos,
    float damage,
    float speed,
    float radius,
    float explosionRadius,
    ElementType element,
    GameObject* owner,
    bool isPlayerProjectile,
    float scale,
    const RuneCombo& runeCombo,
    float chargeRatio)
{
    Projectile proj;
    proj.position = startPos;
    proj.damage = damage;
    proj.speed = speed;
    proj.radius = radius;
    proj.explosionRadius = explosionRadius;
    proj.element = element;
    proj.owner = owner;
    proj.isPlayerProjectile = isPlayerProjectile;
    proj.maxDistance = 100.0f;
    proj.distanceTraveled = 0.0f;
    proj.isActive = true;
    proj.scale = scale;
    proj.runeCombo = runeCombo;
    proj.chargeRatio = chargeRatio;

    // Calculate direction
    XMVECTOR start = XMLoadFloat3(&startPos);
    XMVECTOR target = XMLoadFloat3(&targetPos);
    XMVECTOR dir = XMVector3Normalize(target - start);
    XMStoreFloat3(&proj.direction, dir);

    SpawnProjectile(proj);
}

void ProjectileManager::Update(float deltaTime)
{
    size_t inactiveCount = 0;

    for (auto& projectile : m_Projectiles)
    {
        if (!projectile.isActive)
        {
            inactiveCount++;
            continue;
        }

        // Update position
        projectile.Update(deltaTime);

        // Update fluid VFX position
        if (m_pFluidVFXManager && projectile.fluidVFXId >= 0)
            m_pFluidVFXManager->TrackEffect(projectile.fluidVFXId, projectile.position, projectile.direction);

        // Check collisions
        if (projectile.isActive)
        {
            CheckProjectileCollisions(projectile);
        }

        // If projectile became inactive (hit something or expired), handle fluid VFX and spawn explosion
        if (!projectile.isActive)
        {
            if (m_pFluidVFXManager && projectile.fluidVFXId >= 0)
            {
                if (projectile.wasHit)
                {
                    if (projectile.isPlayerProjectile)
                        // 플레이어 투사체(R/우클릭): CP 제거 + 방사형 폭발
                        m_pFluidVFXManager->ExplodeEffect(projectile.fluidVFXId, projectile.position);
                    else
                        // 적 투사체: 기존 수렴 소멸
                        m_pFluidVFXManager->ImpactEffect(projectile.fluidVFXId, projectile.position);
                }
                else
                    // 사거리 초과: 즉시 소멸
                    m_pFluidVFXManager->StopEffect(projectile.fluidVFXId);
            }
            // Spawn explosion particles
            SpawnExplosionParticles(projectile.position, projectile.element);
            inactiveCount++;
        }
    }

    // Cleanup if too many inactive
    if (inactiveCount >= CLEANUP_THRESHOLD)
    {
        CleanupInactiveProjectiles();
    }
}

size_t ProjectileManager::GetActiveCount() const
{
    size_t count = 0;
    for (const auto& proj : m_Projectiles)
    {
        if (proj.isActive) count++;
    }
    return count;
}

void ProjectileManager::Clear()
{
    m_Projectiles.clear();
}

void ProjectileManager::CheckProjectileCollisions(Projectile& projectile)
{
    if (!m_pScene) return;

    BoundingSphere projSphere = projectile.GetBoundingSphere();

    if (projectile.isPlayerProjectile)
    {
        // Player projectile: check against enemies in the room
        CRoom* pRoom = m_pScene->GetCurrentRoom();
        if (!pRoom) return;

        const auto& gameObjects = pRoom->GetGameObjects();
        for (const auto& obj : gameObjects)
        {
            if (!obj) continue;

            EnemyComponent* pEnemy = obj->GetComponent<EnemyComponent>();
            if (!pEnemy || pEnemy->IsDead()) continue;

            TransformComponent* pTransform = obj->GetTransform();
            if (!pTransform) continue;

            XMFLOAT3 enemyPos = pTransform->GetPosition();
            XMFLOAT3 enemyScale = pTransform->GetScale();

            // Scale collision sphere based on enemy size
            float maxScale = max(enemyScale.x, max(enemyScale.y, enemyScale.z));
            float radius = max(1.5f, maxScale * 1.2f);

            BoundingSphere enemySphere(enemyPos, radius);
            enemySphere.Center.y += radius * 0.7f;

            if (projSphere.Intersects(enemySphere))
            {
                if (projectile.explosionRadius > 0.0f)
                {
                    ApplyAoEDamage(projectile, projectile.position);
                }
                else
                {
                    ApplyDamage(projectile, pEnemy);
                }

                projectile.wasHit = true;
                projectile.isActive = false;

                wchar_t buffer[128];
                swprintf_s(buffer, 128, L"[ProjectileManager] Hit enemy! Damage: %.0f\n", projectile.damage);
                OutputDebugString(buffer);
                return;
            }
        }
    }
    else
    {
        // Enemy projectile: check against player
        GameObject* pPlayer = m_pScene->GetPlayer();
        if (!pPlayer) return;

        TransformComponent* pPlayerTransform = pPlayer->GetTransform();
        if (!pPlayerTransform) return;

        XMFLOAT3 playerPos = pPlayerTransform->GetPosition();

        BoundingSphere playerSphere(playerPos, 1.5f);
        playerSphere.Center.y += 1.0f;

        if (projSphere.Intersects(playerSphere))
        {
            projectile.wasHit = true;
            projectile.isActive = false;

            // Deal damage to player
            PlayerComponent* pPlayerComp = pPlayer->GetComponent<PlayerComponent>();
            if (pPlayerComp)
            {
                pPlayerComp->TakeDamage(projectile.damage);

                wchar_t buffer[128];
                swprintf_s(buffer, 128, L"[ProjectileManager] Enemy projectile hit player! Dealt %.0f damage (HP: %.1f/%.1f)\n",
                    projectile.damage, pPlayerComp->GetCurrentHP(), pPlayerComp->GetMaxHP());
                OutputDebugString(buffer);
            }
            return;
        }
    }
}

void ProjectileManager::ApplyDamage(Projectile& projectile, EnemyComponent* pEnemy)
{
    if (!pEnemy) return;
    pEnemy->TakeDamage(projectile.damage);
}

void ProjectileManager::ApplyAoEDamage(Projectile& projectile, const XMFLOAT3& impactPoint)
{
    if (!m_pScene) return;

    CRoom* pRoom = m_pScene->GetCurrentRoom();
    if (!pRoom) return;

    BoundingSphere explosionSphere(impactPoint, projectile.explosionRadius);

    const auto& gameObjects = pRoom->GetGameObjects();
    for (const auto& obj : gameObjects)
    {
        if (!obj) continue;

        EnemyComponent* pEnemy = obj->GetComponent<EnemyComponent>();
        if (!pEnemy || pEnemy->IsDead()) continue;

        TransformComponent* pTransform = obj->GetTransform();
        if (!pTransform) continue;

        XMFLOAT3 enemyPos = pTransform->GetPosition();
        XMFLOAT3 enemyScale = pTransform->GetScale();

        // Scale collision sphere based on enemy size
        float maxScale = max(enemyScale.x, max(enemyScale.y, enemyScale.z));
        float radius = max(1.5f, maxScale * 1.2f);

        BoundingSphere enemySphere(enemyPos, radius);
        enemySphere.Center.y += radius * 0.7f;

        if (explosionSphere.Intersects(enemySphere))
        {
            // Calculate damage falloff based on distance (optional)
            XMVECTOR impact = XMLoadFloat3(&impactPoint);
            XMVECTOR enemy = XMLoadFloat3(&enemyPos);
            float distance = XMVectorGetX(XMVector3Length(enemy - impact));

            // Linear falloff: full damage at center, 50% at edge
            float falloff = 1.0f - (distance / projectile.explosionRadius) * 0.5f;
            falloff = max(0.5f, falloff);

            float finalDamage = projectile.damage * falloff;
            pEnemy->TakeDamage(finalDamage);

            wchar_t buffer[128];
            swprintf_s(buffer, 128, L"[ProjectileManager] AoE hit! Damage: %.0f (falloff: %.2f)\n", finalDamage, falloff);
            OutputDebugString(buffer);
        }
    }
}

void ProjectileManager::CleanupInactiveProjectiles()
{
    auto it = std::remove_if(m_Projectiles.begin(), m_Projectiles.end(),
        [](const Projectile& p) { return !p.isActive; });

    size_t removed = std::distance(it, m_Projectiles.end());
    m_Projectiles.erase(it, m_Projectiles.end());

    if (removed > 0)
    {
        wchar_t buffer[128];
        swprintf_s(buffer, 128, L"[ProjectileManager] Cleaned up %zu inactive projectiles\n", removed);
        OutputDebugString(buffer);
    }
}

void ProjectileManager::Render(ID3D12GraphicsCommandList* pCommandList)
{
    if (!m_pProjectileMesh || !m_pcbMappedProjectiles || !m_pDescriptorHeap) return;

    // Calculate stride for constant buffer indexing
    UINT nSingleCBSize = (sizeof(ProjectileConstants) + 255) & ~255;
    // Cast to byte pointer for proper pointer arithmetic
    BYTE* pMappedBytes = reinterpret_cast<BYTE*>(m_pcbMappedProjectiles);

    size_t renderIndex = 0;
    for (const auto& projectile : m_Projectiles)
    {
        if (!projectile.isActive) continue;
        if (projectile.isPlayerProjectile) continue;  // Uses fluid VFX only
        if (renderIndex >= MAX_RENDERED_PROJECTILES) break;

        // Update constant buffer for this projectile
        ProjectileConstants* pCB = reinterpret_cast<ProjectileConstants*>(pMappedBytes + renderIndex * nSingleCBSize);

        XMMATRIX worldMatrix = XMMatrixScaling(projectile.scale, projectile.scale, projectile.scale) *
                               XMMatrixTranslation(projectile.position.x, projectile.position.y, projectile.position.z);

        XMStoreFloat4x4(&pCB->m_xmf4x4World, XMMatrixTranspose(worldMatrix));

        // Set material properties
        pCB->m_nMaterialIndex = 0;
        pCB->m_bIsSkinned = 0;
        pCB->m_bHasTexture = 0;

        XMFLOAT4 color = GetElementColor(projectile.element);
        pCB->m_cAmbient = color;
        pCB->m_cDiffuse = color;
        pCB->m_cSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 32.0f);  // Moderate specular
        pCB->m_cEmissive = XMFLOAT4(color.x * 0.5f, color.y * 0.5f, color.z * 0.5f, 1.0f);  // Slight glow

        // Set the descriptor table for this projectile's CBV
        D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_pDescriptorHeap->GetGPUHandle(
            m_nDescriptorStartIndex + static_cast<UINT>(renderIndex));
        pCommandList->SetGraphicsRootDescriptorTable(0, gpuHandle);

        // Render the mesh
        m_pProjectileMesh->Render(pCommandList, 0);

        renderIndex++;
    }
}

XMFLOAT4 ProjectileManager::GetElementColor(ElementType element) const
{
    switch (element)
    {
    case ElementType::Fire:
        return XMFLOAT4(1.0f, 0.3f, 0.1f, 1.0f);  // Orange-red (불)
    case ElementType::Water:
        return XMFLOAT4(0.2f, 0.5f, 1.0f, 1.0f);  // Blue (물)
    case ElementType::Wind:
        return XMFLOAT4(0.7f, 1.0f, 0.7f, 1.0f);  // Light green (바람)
    case ElementType::Earth:
        return XMFLOAT4(0.6f, 0.4f, 0.2f, 1.0f);  // Brown (대지)
    default:
        return XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);  // White
    }
}

void ProjectileManager::SpawnExplosionParticles(const XMFLOAT3& position, ElementType element)
{
    if (!m_pParticleSystem) return;

    ParticleEmitterConfig config;

    // Reduced particle counts for better performance
    switch (element)
    {
    case ElementType::Fire:
        config = FireParticlePresets::FireballExplosion();
        break;
    case ElementType::Water:
        config.burstCount = 10;  // Reduced from 25
        config.emissionRate = 0.0f;
        config.minLifetime = 0.15f;
        config.maxLifetime = 0.35f;
        config.minStartSize = 0.25f;
        config.maxStartSize = 0.5f;
        config.minVelocity = { -3.0f, -1.5f, -3.0f };
        config.maxVelocity = { 3.0f, 3.0f, 3.0f };
        config.startColor = { 0.4f, 0.7f, 1.0f, 1.0f };
        config.endColor = { 0.2f, 0.4f, 0.8f, 0.0f };
        config.gravity = { 0.0f, -5.0f, 0.0f };
        break;
    case ElementType::Wind:
        config.burstCount = 8;  // Reduced from 20
        config.emissionRate = 0.0f;
        config.minLifetime = 0.15f;
        config.maxLifetime = 0.3f;
        config.minStartSize = 0.18f;
        config.maxStartSize = 0.4f;
        config.minVelocity = { -5.0f, -5.0f, -5.0f };
        config.maxVelocity = { 5.0f, 5.0f, 5.0f };
        config.startColor = { 0.9f, 1.0f, 0.9f, 0.9f };
        config.endColor = { 0.7f, 0.95f, 0.7f, 0.0f };
        break;
    case ElementType::Earth:
        config.burstCount = 8;  // Reduced from 20
        config.emissionRate = 0.0f;
        config.minLifetime = 0.3f;
        config.maxLifetime = 0.5f;
        config.minStartSize = 0.35f;
        config.maxStartSize = 0.6f;
        config.minVelocity = { -2.5f, 0.0f, -2.5f };
        config.maxVelocity = { 2.5f, 4.0f, 2.5f };
        config.startColor = { 0.8f, 0.6f, 0.4f, 1.0f };
        config.endColor = { 0.5f, 0.35f, 0.2f, 0.0f };
        config.gravity = { 0.0f, -8.0f, 0.0f };
        break;
    default:
        config.burstCount = 6;  // Reduced from 15
        config.emissionRate = 0.0f;
        config.startColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        config.endColor = { 0.5f, 0.5f, 0.5f, 0.0f };
        break;
    }

    int emitterId = m_pParticleSystem->CreateEmitter(config, position);
    ParticleEmitter* pEmitter = m_pParticleSystem->GetEmitter(emitterId);
    if (pEmitter)
    {
        pEmitter->Burst();
        pEmitter->Stop();  // Stop continuous emission, let burst particles fade
    }
}
