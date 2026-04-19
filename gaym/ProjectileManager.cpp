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
#include "SkillComponent.h"
#include "TransformComponent.h"
#include "Mesh.h"
#include "DescriptorHeap.h"
#include <DirectXCollision.h>
#include <random>

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
    m_pFluidVFXManager      = pScene->GetFluidVFXManager();
    m_pEnemyFluidVFXManager = pScene->GetEnemyFluidVFXManager();

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
    {
        auto& proj = m_Projectiles.back();

        if (proj.isPlayerProjectile && m_pFluidVFXManager)
        {
            // 플레이어 투사체: 플레이어 전용 매니저 → SSF 파이프라인
            uint32_t runeFlags = ToRuneFlags(proj.runeCombo);
            VFXSequenceDef seqDef = VFXLibrary::Get().GetDef(
                SkillSlot::RightClick, runeFlags, proj.element);
            proj.fluidVFXId = m_pFluidVFXManager->SpawnSequenceEffect(
                proj.position, proj.direction, seqDef);
        }
        else if (!proj.isPlayerProjectile && m_pEnemyFluidVFXManager)
        {
            // 적 투사체: 레거시 경로 유지, 파티클 수 축소
            FluidSkillVFXDef vfxDef = FluidSkillVFXManager::GetVFXDef(
                proj.element, proj.runeCombo, proj.chargeRatio);
            vfxDef.particleCount = static_cast<int>(vfxDef.particleCount * 0.3f);
            if (vfxDef.particleCount < 100) vfxDef.particleCount = 100;

            // 적 투사체 기본 파티클 크기 (0.35→0.75) — 존재감 강화
            vfxDef.particleSize = 0.75f;

            // proj.scale을 VFX 크기에 반영 (기본 scale=1.5 기준 배율)
            // 개별 파티클 크기 캡 기준값: 레드 드래곤 기준 s (원하는 "좋은 느낌")
            constexpr float PARTICLE_SIZE_S_CAP = 2.0f;      // Red scale=3.0 기준
            constexpr float SMOOTHING_S_CAP     = 2.5f;      // SPH 안정성 — 너무 크면 진동
            if (proj.scale > 1.5f)
            {
                float s = proj.scale / 1.5f;
                float orbitMult = sqrtf(s);  // 궤도는 sqrt로 덜 확대 → 꽉 뭉친 느낌
                for (auto& cpd : vfxDef.cpDescs)
                {
                    bool isNucleus = (cpd.orbitRadius < 0.001f);
                    if (isNucleus)
                    {
                        // 핵: 큰 반경 + 강한 인력 → 중심으로 빨아들임
                        cpd.sphereRadius       *= s * 2.5f;
                        cpd.attractionStrength *= s * 1.4f;
                    }
                    else
                    {
                        // 위성: 궤도는 sqrt(s)로 덜 확대, 인력은 원값 유지 → 타이트
                        cpd.orbitRadius        *= orbitMult;
                        cpd.sphereRadius       *= s;
                    }
                }
                vfxDef.spawnRadius    *= orbitMult;
                // smoothingRadius는 캡으로 고정 → 큰 s에서도 SPH 안정적
                vfxDef.smoothingRadius = 1.0f * (std::min)(s, SMOOTHING_S_CAP);
                vfxDef.restDensity     = 11.0f;             // 꽉 뭉치게
                // 개별 파티클 크기 캡 → 레드 기준 유지, 블루도 동일 크기
                vfxDef.particleSize    = 0.75f * (std::min)(s, PARTICLE_SIZE_S_CAP);
                // 파티클 수: s에 따른 성장 완만히 (sqrt 기반) — 너무 많으면 움직임 산만
                vfxDef.particleCount   = static_cast<int>(vfxDef.particleCount * (std::min)(sqrtf(s) * 1.5f, 6.0f));
            }
            else
            {
                // 기본 scale 투사체도 더 뭉치게
                vfxDef.restDensity = 11.0f;
            }

            proj.fluidVFXId = m_pEnemyFluidVFXManager->SpawnEffect(
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
    float chargeRatio,
    float maxDistance,
    bool isPiercing,
    bool isHoming,
    float lifestealRatio,
    float execDamageBonus,
    float cdResetChance,
    SkillSlot skillSlot)
{
    Projectile proj;
    proj.position         = startPos;
    proj.damage           = damage;
    proj.speed            = speed;
    proj.radius           = radius;
    proj.explosionRadius  = explosionRadius;
    proj.element          = element;
    proj.owner            = owner;
    proj.isPlayerProjectile = isPlayerProjectile;
    proj.maxDistance      = maxDistance;
    proj.distanceTraveled = 0.0f;
    proj.isActive         = true;
    proj.scale            = scale;
    proj.runeCombo        = runeCombo;
    proj.chargeRatio      = chargeRatio;
    proj.isPiercing       = isPiercing;
    proj.isHoming         = isHoming;
    proj.lifestealRatio   = lifestealRatio;
    proj.execDamageBonus  = execDamageBonus;
    proj.cdResetChance    = cdResetChance;
    proj.skillSlot        = skillSlot;

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

        // 유도: 가장 가까운 적 방향으로 방향 벡터를 서서히 회전
        if (projectile.isHoming && projectile.isPlayerProjectile && m_pScene)
        {
            CRoom* pRoom = m_pScene->GetCurrentRoom();
            if (pRoom)
            {
                float bestDist = FLT_MAX;
                XMFLOAT3 bestPos = projectile.position;
                for (const auto& obj : pRoom->GetGameObjects())
                {
                    if (!obj) continue;
                    EnemyComponent* pEnemy = obj->GetComponent<EnemyComponent>();
                    if (!pEnemy || pEnemy->IsDead()) continue;
                    XMFLOAT3 ePos = obj->GetTransform()->GetPosition();
                    XMVECTOR diff = XMLoadFloat3(&ePos) - XMLoadFloat3(&projectile.position);
                    float d = XMVectorGetX(XMVector3Length(diff));
                    if (d < bestDist) { bestDist = d; bestPos = ePos; }
                }
                if (bestDist < FLT_MAX)
                {
                    XMVECTOR cur = XMLoadFloat3(&projectile.direction);
                    XMVECTOR toTarget = XMVector3Normalize(
                        XMLoadFloat3(&bestPos) - XMLoadFloat3(&projectile.position));
                    constexpr float TURN_SPEED = 3.5f; // rad/s
                    XMVECTOR newDir = XMVector3Normalize(
                        cur + toTarget * (TURN_SPEED * deltaTime));
                    XMStoreFloat3(&projectile.direction, newDir);
                }
            }
        }

        // Update position
        projectile.Update(deltaTime);

        // 투사체 소유 매니저 선택 (플레이어↔적 완전 분리)
        FluidSkillVFXManager* pVFX = projectile.isPlayerProjectile
                                   ? m_pFluidVFXManager
                                   : m_pEnemyFluidVFXManager;

        // Update fluid VFX position
        if (pVFX && projectile.fluidVFXId >= 0)
            pVFX->TrackEffect(projectile.fluidVFXId, projectile.position, projectile.direction);

        // Check collisions
        if (projectile.isActive)
        {
            CheckProjectileCollisions(projectile);
        }

        // If projectile became inactive (hit something or expired), handle fluid VFX and spawn explosion
        if (!projectile.isActive)
        {
            if (pVFX && projectile.fluidVFXId >= 0)
            {
                if (projectile.wasHit)
                {
                    if (projectile.isPlayerProjectile)
                        pVFX->ExplodeEffect(projectile.fluidVFXId, projectile.position);
                    else
                        pVFX->ImpactEffect(projectile.fluidVFXId, projectile.position);
                }
                else
                    pVFX->StopEffect(projectile.fluidVFXId);
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

            // Scale collision sphere based on enemy size (1.2 → 1.5 — 뚱뚱한 메시도 커버)
            float maxScale = max(enemyScale.x, max(enemyScale.y, enemyScale.z));
            float radius = max(1.5f, maxScale * 1.5f);

            BoundingSphere enemySphere(enemyPos, radius);
            enemySphere.Center.y += radius * 0.7f;

            if (projSphere.Intersects(enemySphere))
            {
                if (projectile.explosionRadius > 0.0f)
                    ApplyAoEDamage(projectile, projectile.position);
                else
                    ApplyDamage(projectile, pEnemy);

                projectile.wasHit = true;
                if (!projectile.isPiercing)
                {
                    projectile.isActive = false;
                    return;
                }
                // 관통: 같은 적에 연속 히트 방지를 위해 잠깐 무적 처리 대신
                // 단순히 충돌 구체를 통과한 뒤 다음 적을 노림 — 루프 계속
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

    float dmg = projectile.damage;

    // 처형자: 대상 HP 30% 이하일 때 추가 피해
    if (projectile.execDamageBonus > 0.f && pEnemy->GetHpRatio() < 0.3f)
        dmg *= (1.f + projectile.execDamageBonus);

    pEnemy->TakeDamage(dmg);

    // 흡수: 피해량 * ratio 만큼 시전자 HP 회복
    if (projectile.lifestealRatio > 0.f && projectile.owner)
    {
        PlayerComponent* pPlayer = projectile.owner->GetComponent<PlayerComponent>();
        if (pPlayer) pPlayer->Heal(dmg * projectile.lifestealRatio);
    }

    // 무한: 확률로 시전자 스킬 쿨다운 즉시 초기화
    if (projectile.cdResetChance > 0.f && projectile.skillSlot != SkillSlot::Count && projectile.owner)
    {
        static std::mt19937 rng{ std::random_device{}() };
        static std::uniform_real_distribution<float> dist(0.f, 1.f);
        if (dist(rng) < projectile.cdResetChance)
        {
            SkillComponent* pSkill = projectile.owner->GetComponent<SkillComponent>();
            if (pSkill) pSkill->ResetCooldown(projectile.skillSlot);
        }
    }

    // onHit 훅 호출
    if (projectile.skillSlot != SkillSlot::Count && projectile.owner)
    {
        SkillComponent* pSkillComp = projectile.owner->GetComponent<SkillComponent>();
        if (pSkillComp)
        {
            ActivationType defaultType = ActivationType::Instant;
            SkillStats stats = pSkillComp->BuildSkillStats(projectile.skillSlot, defaultType);
            if (!stats.onHitHooks.empty())
            {
                SkillContext ctx;
                ctx.caster     = projectile.owner;
                ctx.element    = projectile.element;
                ctx.baseDamage = projectile.damage;
                ctx.damageDealt = dmg;
                for (auto& hook : stats.onHitHooks) hook(ctx);
            }
        }
    }
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

        // Scale collision sphere based on enemy size (1.2 → 1.5)
        float maxScale = max(enemyScale.x, max(enemyScale.y, enemyScale.z));
        float radius = max(1.5f, maxScale * 1.5f);

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
        if (renderIndex >= MAX_RENDERED_PROJECTILES) break;
        // 플레이어/적 모두 fluid VFX로 표현 — 큐브 메시는 렌더하지 않음
        continue;

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
