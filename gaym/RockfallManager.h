#pragma once

#include "stdafx.h"
#include <array>
#include <memory>

class CRoom;
class Scene;
class GameObject;
class Mesh;
class Shader;
class CDescriptorHeap;

// Earth 스테이지 전용 기믹: 천장에서 낙석.
//   1) Warning   — 바닥에 갈색 링이 점점 차오름 (1.5s)
//   2) Falling   — 하늘에서 작은 바위가 빠르게 낙하 (0.4s)
//   3) Impact    — 데미지 + 카메라 쉐이크
// LavaGeyserManager 패턴을 단순화: 별도 Component 없이 풀 내부 구조체로 관리.
class RockfallManager
{
public:
    RockfallManager();
    ~RockfallManager();

    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
              CRoom* pRoom, Shader* pShader);

    void Update(float deltaTime);

    void SetActive(bool bActive);
    bool IsActive() const { return m_bActive; }

    void SetSpawnInterval(float interval) { m_fSpawnInterval = interval; }

private:
    enum class State { Idle, Warning, Falling };

    struct Rockfall
    {
        State state = State::Idle;
        float timer = 0.0f;
        XMFLOAT3 target = { 0.0f, 0.0f, 0.0f };
        GameObject* warningRing = nullptr;
        GameObject* rock = nullptr;
        bool damageDealt = false;
    };

    GameObject* CreateWarningRing(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, Shader* pShader);
    GameObject* CreateRock(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, Shader* pShader);

    void TrySpawn();
    void Activate(Rockfall& rf, const XMFLOAT3& pos);
    void OnImpact(Rockfall& rf);
    void Hide(Rockfall& rf);

    XMFLOAT3 GetPlayerPosition() const;
    float RandFloat();

    static constexpr int POOL_SIZE = 4;
    std::array<Rockfall, POOL_SIZE> m_pool;

    CRoom*  m_pRoom   = nullptr;
    Scene*  m_pScene  = nullptr;
    Shader* m_pShader = nullptr;

    Mesh* m_pRingMesh = nullptr;
    Mesh* m_pRockMesh = nullptr;

    // 스폰 설정
    float m_fSpawnTimer       = 0.0f;
    float m_fSpawnInterval    = 4.0f;

    // 낙석 설정
    float m_fWarningDuration  = 1.5f;
    float m_fFallDuration     = 0.4f;
    float m_fFallStartHeight  = 35.0f;
    float m_fImpactRadius     = 4.0f;
    float m_fDamage           = 25.0f;

    // 카메라 쉐이크
    float m_fShakeIntensity   = 1.2f;
    float m_fShakeDuration    = 0.35f;

    bool m_bActive      = false;
    bool m_bInitialized = false;

    ID3D12Device*              m_pDevice      = nullptr;
    ID3D12GraphicsCommandList* m_pCommandList = nullptr;

    uint32_t m_nSeed = 54321;
};
