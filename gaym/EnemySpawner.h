#pragma once
#include "stdafx.h"
#include "EnemySpawnData.h"
#include "MeshLoader.h"
#include <map>
#include <string>
#include <memory>

class GameObject;
class CRoom;
class Scene;
class Shader;
class EnemyComponent;
class Mesh;
class LineMesh;
class FanMesh;

class EnemySpawner
{
public:
    EnemySpawner();
    ~EnemySpawner();

    // Initialize with device and command list for creating resources
    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, Scene* pScene, Shader* pShader);

    // Register enemy presets
    void RegisterEnemyPreset(const std::string& name, const EnemySpawnData& data);

    // Spawn an enemy from a preset
    GameObject* SpawnEnemy(CRoom* pRoom, const std::string& preset, const XMFLOAT3& position, GameObject* pTarget);

    // Spawn a test enemy using CubeMesh (for testing without mesh files)
    GameObject* SpawnTestEnemy(CRoom* pRoom, const XMFLOAT3& position, GameObject* pTarget);

    // Spawn all enemies for a room based on its spawn config
    void SpawnRoomEnemies(CRoom* pRoom, const RoomSpawnConfig& config, GameObject* pTarget);

    // Get registered presets
    bool HasPreset(const std::string& name) const;

private:
    // Create a cube mesh enemy for testing
    GameObject* CreateCubeEnemy(CRoom* pRoom, const XMFLOAT3& position, const XMFLOAT3& scale, const XMFLOAT4& color);

    // Create an enemy with loaded mesh from file
    GameObject* CreateMeshEnemy(CRoom* pRoom, const XMFLOAT3& position, const EnemySpawnData& data);

    // Add render components recursively to game object hierarchy
    void AddRenderComponentsToHierarchy(GameObject* pGameObject);

    // Apply color tint to all meshes in game object hierarchy
    void ApplyColorToHierarchy(GameObject* pGameObject, const XMFLOAT4& color);

    // Setup common enemy components
    void SetupEnemyComponents(GameObject* pEnemy, const EnemySpawnData& data, CRoom* pRoom, GameObject* pTarget);

    // Create indicator game objects
    GameObject* CreateIndicatorObject(CRoom* pRoom, Mesh* pMesh);

    // Setup attack indicators for an enemy
    void SetupAttackIndicators(GameObject* pEnemy, EnemyComponent* pEnemyComp, const AttackIndicatorConfig& config, CRoom* pRoom);

private:
    std::map<std::string, EnemySpawnData> m_mapPresets;

    ID3D12Device* m_pDevice = nullptr;
    ID3D12GraphicsCommandList* m_pCommandList = nullptr;
    Scene* m_pScene = nullptr;
    Shader* m_pShader = nullptr;

    // Shared meshes for indicators
    RingMesh* m_pRingMesh = nullptr;
    LineMesh* m_pLineMesh = nullptr;
    FanMesh* m_pFanMesh = nullptr;
};
