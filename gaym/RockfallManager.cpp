#include "stdafx.h"
#include "RockfallManager.h"
#include "Room.h"
#include "Scene.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "RenderComponent.h"
#include "Mesh.h"
#include "Shader.h"
#include "Camera.h"
#include "PlayerComponent.h"

RockfallManager::RockfallManager() = default;

RockfallManager::~RockfallManager()
{
    if (m_pRingMesh) { m_pRingMesh->Release(); m_pRingMesh = nullptr; }
    if (m_pRockMesh) { m_pRockMesh->Release(); m_pRockMesh = nullptr; }
}

void RockfallManager::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                            CRoom* pRoom, Shader* pShader)
{
    if (m_bInitialized) return;

    m_pRoom        = pRoom;
    m_pScene       = pRoom ? pRoom->GetScene() : nullptr;
    m_pShader      = pShader;
    m_pDevice      = pDevice;
    m_pCommandList = pCommandList;

    if (!m_pScene)
    {
        OutputDebugString(L"[RockfallManager] ERROR: Room has no Scene!\n");
        return;
    }

    // 공유 메쉬 — 두꺼운 링 (경고) + 작은 큐브 (떨어지는 바위)
    m_pRingMesh = new RingMesh(pDevice, pCommandList, 1.0f, 0.85f, 64);
    m_pRingMesh->AddRef();

    m_pRockMesh = new CubeMesh(pDevice, pCommandList, 1.0f, 1.0f, 1.0f);
    m_pRockMesh->AddRef();

    for (int i = 0; i < POOL_SIZE; ++i)
    {
        m_pool[i].warningRing = CreateWarningRing(pDevice, pCommandList, pShader);
        m_pool[i].rock        = CreateRock(pDevice, pCommandList, pShader);
    }

    m_bInitialized = true;
    OutputDebugString(L"[RockfallManager] Initialized\n");
}

GameObject* RockfallManager::CreateWarningRing(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, Shader* pShader)
{
    if (!m_pScene || !m_pRingMesh || !pShader) return nullptr;

    GameObject* pObj = m_pScene->CreateGameObject(pDevice, pCommandList);
    if (!pObj) return nullptr;

    pObj->GetTransform()->SetPosition(0.0f, -1000.0f, 0.0f);

    m_pRingMesh->AddRef();
    pObj->SetMesh(m_pRingMesh);

    // 흙먼지 갈색 / 균열 톤 — 적 공격 인디케이터(빨강)와 구분
    MATERIAL mat;
    mat.m_cAmbient  = XMFLOAT4(0.35f, 0.20f, 0.05f, 1.0f);
    mat.m_cDiffuse  = XMFLOAT4(0.85f, 0.55f, 0.20f, 1.0f);
    mat.m_cSpecular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    mat.m_cEmissive = XMFLOAT4(1.20f, 0.55f, 0.10f, 1.0f);
    pObj->SetMaterial(mat);

    auto* pRC = pObj->AddComponent<RenderComponent>();
    pRC->SetMesh(m_pRingMesh);
    pRC->SetCastsShadow(false);
    pShader->AddRenderComponent(pRC);

    return pObj;
}

GameObject* RockfallManager::CreateRock(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, Shader* pShader)
{
    if (!m_pScene || !m_pRockMesh || !pShader) return nullptr;

    GameObject* pObj = m_pScene->CreateGameObject(pDevice, pCommandList);
    if (!pObj) return nullptr;

    pObj->GetTransform()->SetPosition(0.0f, -1000.0f, 0.0f);
    pObj->GetTransform()->SetScale(1.6f, 1.6f, 1.6f);

    m_pRockMesh->AddRef();
    pObj->SetMesh(m_pRockMesh);

    MATERIAL mat;
    mat.m_cAmbient  = XMFLOAT4(0.18f, 0.16f, 0.14f, 1.0f);
    mat.m_cDiffuse  = XMFLOAT4(0.40f, 0.36f, 0.32f, 1.0f);
    mat.m_cSpecular = XMFLOAT4(0.10f, 0.10f, 0.10f, 4.0f);
    mat.m_cEmissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
    pObj->SetMaterial(mat);

    auto* pRC = pObj->AddComponent<RenderComponent>();
    pRC->SetMesh(m_pRockMesh);
    pRC->SetCastsShadow(false);
    pShader->AddRenderComponent(pRC);

    return pObj;
}

void RockfallManager::Update(float deltaTime)
{
    if (!m_bActive || !m_bInitialized) return;

    for (auto& rf : m_pool)
    {
        if (rf.state == State::Idle) continue;

        rf.timer += deltaTime;

        if (rf.state == State::Warning)
        {
            float progress = rf.timer / m_fWarningDuration;
            if (progress > 1.0f) progress = 1.0f;

            float scale = m_fImpactRadius * progress;
            if (rf.warningRing && rf.warningRing->GetTransform())
                rf.warningRing->GetTransform()->SetScale(scale, 1.0f, scale);

            if (rf.timer >= m_fWarningDuration)
            {
                rf.state = State::Falling;
                rf.timer = 0.0f;
                rf.damageDealt = false;

                if (rf.rock && rf.rock->GetTransform())
                {
                    rf.rock->GetTransform()->SetPosition(
                        rf.target.x, rf.target.y + m_fFallStartHeight, rf.target.z);
                }
            }
        }
        else if (rf.state == State::Falling)
        {
            float t = rf.timer / m_fFallDuration;
            if (t > 1.0f) t = 1.0f;
            // 가속 낙하 (t^2)
            float h = m_fFallStartHeight * (1.0f - t * t);

            if (rf.rock && rf.rock->GetTransform())
            {
                XMFLOAT3 p = rf.target;
                p.y += h;
                rf.rock->GetTransform()->SetPosition(p);
                // 살짝 회전 — 굴러떨어지는 느낌
                rf.rock->GetTransform()->Rotate(0.0f, 360.0f * deltaTime, 180.0f * deltaTime);
            }

            if (rf.timer >= m_fFallDuration)
            {
                OnImpact(rf);
                Hide(rf);
                rf.state = State::Idle;
                rf.timer = 0.0f;
            }
        }
    }

    m_fSpawnTimer += deltaTime;
    if (m_fSpawnTimer >= m_fSpawnInterval)
    {
        m_fSpawnTimer = 0.0f;
        TrySpawn();
    }
}

void RockfallManager::TrySpawn()
{
    XMFLOAT3 playerPos = GetPlayerPosition();

    // 한 번에 1~2개 스폰
    int count = 1 + (RandFloat() < 0.5f ? 1 : 0);
    for (int i = 0; i < count; ++i)
    {
        // Idle 슬롯 찾기
        Rockfall* pSlot = nullptr;
        for (auto& rf : m_pool) { if (rf.state == State::Idle) { pSlot = &rf; break; } }
        if (!pSlot) return;

        float angle = RandFloat() * XM_2PI;
        float dist  = 3.0f + RandFloat() * 6.0f;
        XMFLOAT3 pos = {
            playerPos.x + cosf(angle) * dist,
            playerPos.y,
            playerPos.z + sinf(angle) * dist
        };
        Activate(*pSlot, pos);
    }
}

void RockfallManager::Activate(Rockfall& rf, const XMFLOAT3& pos)
{
    rf.state = State::Warning;
    rf.timer = 0.0f;
    rf.target = pos;
    rf.damageDealt = false;

    if (rf.warningRing && rf.warningRing->GetTransform())
    {
        rf.warningRing->GetTransform()->SetPosition(pos.x, pos.y + 0.1f, pos.z);
        rf.warningRing->GetTransform()->SetScale(0.0f, 1.0f, 0.0f);
    }
    // 바위는 경고 단계에선 숨김
    if (rf.rock && rf.rock->GetTransform())
        rf.rock->GetTransform()->SetPosition(0.0f, -1000.0f, 0.0f);
}

void RockfallManager::OnImpact(Rockfall& rf)
{
    if (rf.damageDealt) return;
    rf.damageDealt = true;

    // 1) 데미지
    if (m_pScene)
    {
        if (auto* pPlayer = m_pScene->GetPlayer())
        {
            if (auto* pT = pPlayer->GetTransform())
            {
                XMFLOAT3 pp = pT->GetPosition();
                float dx = pp.x - rf.target.x;
                float dz = pp.z - rf.target.z;
                if (dx * dx + dz * dz <= m_fImpactRadius * m_fImpactRadius)
                {
                    if (auto* pPC = pPlayer->GetComponent<PlayerComponent>())
                        pPC->TakeDamage(m_fDamage);
                }
            }
        }

        // 2) 카메라 쉐이크
        if (auto* pCam = m_pScene->GetCamera())
            pCam->StartShake(m_fShakeIntensity, m_fShakeDuration);
    }
}

void RockfallManager::Hide(Rockfall& rf)
{
    if (rf.warningRing && rf.warningRing->GetTransform())
        rf.warningRing->GetTransform()->SetPosition(0.0f, -1000.0f, 0.0f);
    if (rf.rock && rf.rock->GetTransform())
        rf.rock->GetTransform()->SetPosition(0.0f, -1000.0f, 0.0f);
}

void RockfallManager::SetActive(bool bActive)
{
    m_bActive = bActive;
    m_fSpawnTimer = 0.0f;

    if (!bActive)
    {
        for (auto& rf : m_pool)
        {
            rf.state = State::Idle;
            rf.timer = 0.0f;
            Hide(rf);
        }
    }

    OutputDebugString(bActive ? L"[RockfallManager] Active\n" : L"[RockfallManager] Inactive\n");
}

XMFLOAT3 RockfallManager::GetPlayerPosition() const
{
    if (!m_pScene) return XMFLOAT3(0, 0, 0);
    GameObject* pPlayer = m_pScene->GetPlayer();
    if (!pPlayer || !pPlayer->GetTransform()) return XMFLOAT3(0, 0, 0);
    return pPlayer->GetTransform()->GetPosition();
}

float RockfallManager::RandFloat()
{
    m_nSeed ^= m_nSeed << 13;
    m_nSeed ^= m_nSeed >> 17;
    m_nSeed ^= m_nSeed << 5;
    return static_cast<float>(m_nSeed) / static_cast<float>(0xFFFFFFFF);
}
