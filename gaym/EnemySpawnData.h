#pragma once
#include "stdafx.h"
#include "EnemyComponent.h"
#include "IAttackBehavior.h"
#include <string>
#include <functional>
#include <memory>

class BossPhaseConfig;

struct EnemySpawnData
{
    // Visual
    std::string m_strMeshPath;          // Empty = use CubeMesh
    std::string m_strAnimationPath;     // Empty = no animation
    std::string m_strTexturePath;       // Empty = no texture override
    XMFLOAT3 m_xmf3Scale = { 1.0f, 1.0f, 1.0f };
    XMFLOAT4 m_xmf4Color = { 1.0f, 0.0f, 0.0f, 1.0f }; // Red by default

    // Stats
    EnemyStats m_Stats;

    // Attack behavior factory
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnCreateAttack;

    // Special attack behavior factory (for bosses)
    std::function<std::unique_ptr<IAttackBehavior>()> m_fnCreateSpecialAttack;

    // Animation config
    EnemyAnimationConfig m_AnimConfig;

    // Attack indicator config
    AttackIndicatorConfig m_IndicatorConfig;

    // Flying mode
    bool m_bIsFlying = false;
    float m_fFlyHeight = 15.0f;

    // Stationary mode — 고정형 보스 (이동/회전 모두 봉쇄, 방사형 패턴 전용)
    bool m_bStationary = false;
    float m_fRotationSpeed = 180.0f;   // 기본 180 deg/s (Stationary 에선 무시됨)

    // 애니메이션 재생속도 — 1.0 기본, 낮추면 무거운 느낌 (골렘 등), 높이면 빠른 느낌
    float m_fAnimationPlaybackSpeed = 1.0f;

    // Collider override (0이면 scale 기반 자동 계산)
    float m_fColliderXZMultiplier = 0.0f;

    // 공격 원점 forward 오프셋 (크라켄처럼 몸 앞쪽 촉수가 실제 공격 원점인 경우)
    float m_fAttackOriginForwardOffset = 0.0f;

    // Boss settings
    bool m_bIsBoss = false;
    float m_fSpecialAttackCooldown = 10.0f;
    float m_fFlyingAttackCooldown = 12.0f;
    int m_nSpecialAttackChance = 30;  // Percentage (0-100)
    int m_nFlyingAttackChance = 30;   // Percentage (0-100)

    // Boss phase config factory (for multi-phase bosses)
    std::function<std::unique_ptr<BossPhaseConfig>()> m_fnCreateBossPhaseConfig;

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
