#pragma once

#include "stdafx.h"
#include <array>
#include <memory>

class CRoom;
class Scene;
class GameObject;
class LavaGeyserComponent;
class FluidParticleSystem;
class Mesh;
class Shader;
class CDescriptorHeap;

class LavaGeyserManager
{
public:
    LavaGeyserManager();
    ~LavaGeyserManager();

    // 초기화 (Room 생성 후 호출)
    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
              CRoom* pRoom, Shader* pShader, CDescriptorHeap* pDescriptorHeap, UINT nDescriptorIndex);

    // 매 프레임 업데이트
    void Update(float deltaTime);

    // 렌더링 (유체 파티클)
    void Render(ID3D12GraphicsCommandList* pCommandList,
                const XMFLOAT4X4& viewProj, const XMFLOAT3& cameraRight, const XMFLOAT3& cameraUp);

    // 활성화/비활성화 (Room 상태에 따라)
    void SetActive(bool bActive);
    bool IsActive() const { return m_bActive; }

    // 설정
    void SetSpawnInterval(float interval) { m_fSpawnInterval = interval; }
    void SetSpawnCount(int minCount, int maxCount) { m_nMinSpawnCount = minCount; m_nMaxSpawnCount = maxCount; }

private:
    // 인디케이터 GameObject 생성 (Scene::CreateGameObject 사용)
    GameObject* CreateIndicatorObject(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, Shader* pShader);

    // 장판 스폰
    void SpawnGeysers();

    // 플레이어 위치 가져오기
    XMFLOAT3 GetPlayerPosition() const;

    // 장판 풀
    static constexpr int POOL_SIZE = 5;
    std::array<GameObject*, POOL_SIZE> m_Indicators;       // Scene이 소유 (raw pointer)
    std::array<LavaGeyserComponent*, POOL_SIZE> m_Components;

    // Room/Scene 참조
    CRoom* m_pRoom = nullptr;
    Scene* m_pScene = nullptr;
    Shader* m_pShader = nullptr;

    // 공유 리소스
    Mesh* m_pRingMesh = nullptr;
    Mesh* m_pCircleMesh = nullptr;  // 채워진 원 (점점 차오르는 효과용)

    // 유체 파티클 시스템
    std::unique_ptr<FluidParticleSystem> m_pFluidSystem;

    // 스폰 설정
    float m_fSpawnTimer = 0.0f;
    float m_fSpawnInterval = 5.0f;
    int m_nMinSpawnCount = 2;
    int m_nMaxSpawnCount = 2;   // 2개 고정

    // 활성화 상태
    bool m_bActive = false;
    bool m_bInitialized = false;

    // D3D12 리소스
    ID3D12Device* m_pDevice = nullptr;
    ID3D12GraphicsCommandList* m_pCommandList = nullptr;

    // 랜덤 시드
    uint32_t m_nSeed = 12345;
    float RandFloat();  // 0.0 ~ 1.0
};
