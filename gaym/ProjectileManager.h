#pragma once

#include "Projectile.h"
#include <vector>
#include <memory>

class Scene;
class GameObject;
class EnemyComponent;
class Mesh;
class CDescriptorHeap;
class ParticleSystem;

// Forward declare MATERIAL from GameObject.h
struct MATERIAL;

// Constant buffer for projectile rendering (must match ObjectConstants layout)
struct ProjectileConstants
{
    XMFLOAT4X4 m_xmf4x4World;
    UINT m_nMaterialIndex = 0;
    UINT m_bIsSkinned = 0;
    UINT m_bHasTexture = 0;
    float pad3 = 0.0f;
    // Material embedded directly (same layout as MATERIAL)
    XMFLOAT4 m_cAmbient;
    XMFLOAT4 m_cDiffuse;
    XMFLOAT4 m_cSpecular;
    XMFLOAT4 m_cEmissive;
    XMFLOAT4X4 m_xmf4x4BoneTransforms[96];  // Not used, but needed for layout match
};

class ProjectileManager
{
public:
    ProjectileManager();
    ~ProjectileManager();

    // Initialize with scene reference and graphics resources
    void Init(Scene* pScene, ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
              CDescriptorHeap* pDescriptorHeap, UINT nStartDescriptorIndex);

    // Spawn a new projectile
    void SpawnProjectile(const Projectile& projectile);

    // Convenience method for common projectile creation
    void SpawnProjectile(
        const XMFLOAT3& startPos,
        const XMFLOAT3& targetPos,
        float damage,
        float speed,
        float radius,
        float explosionRadius,
        ElementType element,
        GameObject* owner,
        bool isPlayerProjectile = true
    );

    // Update all projectiles (movement + collision)
    void Update(float deltaTime);

    // Render all active projectiles
    void Render(ID3D12GraphicsCommandList* pCommandList);

    // Get active projectile count
    size_t GetActiveCount() const;

    // Clear all projectiles
    void Clear();

private:
    // Check collisions for a single projectile
    void CheckProjectileCollisions(Projectile& projectile);

    // Apply damage to enemy
    void ApplyDamage(Projectile& projectile, EnemyComponent* pEnemy);

    // Apply AoE damage
    void ApplyAoEDamage(Projectile& projectile, const XMFLOAT3& impactPoint);

    // Remove inactive projectiles
    void CleanupInactiveProjectiles();

    // Get color based on element type
    XMFLOAT4 GetElementColor(ElementType element) const;

    // Create particle trail for projectile
    void CreateProjectileParticles(Projectile& projectile);

    // Spawn explosion particles at position
    void SpawnExplosionParticles(const XMFLOAT3& position, ElementType element);

private:
    std::vector<Projectile> m_Projectiles;
    Scene* m_pScene = nullptr;
    ParticleSystem* m_pParticleSystem = nullptr;

    // Rendering resources
    std::unique_ptr<Mesh> m_pProjectileMesh;
    ComPtr<ID3D12Resource> m_pd3dcbProjectiles;
    ProjectileConstants* m_pcbMappedProjectiles = nullptr;
    UINT m_nDescriptorStartIndex = 0;
    CDescriptorHeap* m_pDescriptorHeap = nullptr;

    // Pool settings
    static constexpr size_t MAX_PROJECTILES = 256;
    static constexpr size_t MAX_RENDERED_PROJECTILES = 64;  // Max projectiles to render at once
    static constexpr size_t CLEANUP_THRESHOLD = 32; // Cleanup when this many are inactive
};
