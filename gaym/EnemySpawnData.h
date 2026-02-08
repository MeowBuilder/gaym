#pragma once
#include "stdafx.h"
#include "EnemyComponent.h"
#include "IAttackBehavior.h"
#include <string>
#include <functional>
#include <memory>

struct EnemySpawnData
{
    // Visual
    std::string m_strMeshPath;          // Empty = use CubeMesh
    std::string m_strAnimationPath;     // Empty = no animation
    XMFLOAT3 m_xmf3Scale = { 1.0f, 1.0f, 1.0f };
    XMFLOAT4 m_xmf4Color = { 1.0f, 0.0f, 0.0f, 1.0f }; // Red by default

    // Stats
    EnemyStats m_Stats;

    // Attack behavior factory
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnCreateAttack;

    // Animation config
    EnemyAnimationConfig m_AnimConfig;

    // Attack indicator config
    AttackIndicatorConfig m_IndicatorConfig;

    // Constructor with defaults for test enemy
    EnemySpawnData()
    {
        m_Stats.m_fMaxHP = 100.0f;
        m_Stats.m_fCurrentHP = 100.0f;
        m_Stats.m_fMoveSpeed = 5.0f;
        m_Stats.m_fAttackRange = 3.0f;
        m_Stats.m_fAttackCooldown = 2.0f;
        m_Stats.m_fDetectionRange = 50.0f;
    }
};

// Configuration for spawning enemies in a room
struct RoomSpawnConfig
{
    // List of (preset name, position) pairs for enemies to spawn
    std::vector<std::pair<std::string, XMFLOAT3>> m_vEnemySpawns;

    void AddSpawn(const std::string& presetName, const XMFLOAT3& position)
    {
        m_vEnemySpawns.push_back({ presetName, position });
    }

    void AddSpawn(const std::string& presetName, float x, float y, float z)
    {
        m_vEnemySpawns.push_back({ presetName, XMFLOAT3(x, y, z) });
    }

    void Clear()
    {
        m_vEnemySpawns.clear();
    }
};
