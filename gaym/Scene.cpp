#include "stdafx.h"
#include "Scene.h"
#include "RenderComponent.h"
#include "Shader.h"
#include "RotatorComponent.h"
#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "InputSystem.h" // Added for InputSystem
#include "EnemyComponent.h"
#include "MegaBreathAttackBehavior.h"
#include "Room.h"
#include "PlayerComponent.h"
#include "AnimationComponent.h"
#include "CollisionManager.h"
#include "CollisionLayer.h"
#include "SkillComponent.h"
#include "FireballBehavior.h"
#include "WaveSlashBehavior.h"
#include "FireBeamBehavior.h"
#include "MeteorBehavior.h"
#include "ProjectileManager.h"
#include "DropItemComponent.h"
#include "InteractableComponent.h"
#include "EnemyComponent.h"
#include "MathUtils.h"
#include "LavaGeyserManager.h"
#include "VFXLibrary.h"
#include <functional> // Added for std::function
#include "MapLoader.h"
#include "WICTextureLoader12.h"
#include "D3dx12.h"

Scene::Scene()
{
    m_pCamera = std::make_unique<CCamera>();
    m_pCollisionManager = std::make_unique<CollisionManager>();
    m_pEnemySpawner = std::make_unique<EnemySpawner>();
    m_pProjectileManager = std::make_unique<ProjectileManager>();
    m_pParticleSystem = std::make_unique<ParticleSystem>();
    m_pFluidParticleSystem = std::make_unique<FluidParticleSystem>();
    m_pFluidSkillEffect    = std::make_unique<FluidSkillEffect>();
    m_pFluidVFXManager     = std::make_unique<FluidSkillVFXManager>();
    m_pSSF                 = std::make_unique<ScreenSpaceFluid>();
    m_pDebugRenderer = std::make_unique<DebugRenderer>();
    m_pTorchSystem = std::make_unique<TorchSystem>();
}

Scene::~Scene()
{
    if (m_pd3dcbPass) m_pd3dcbPass->Unmap(0, NULL);
}

#include "MeshLoader.h"
#include "MapLoader.h"
#include "Dx12App.h"

void Scene::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    // Create Descriptor Heap
    m_pDescriptorHeap = std::make_unique<CDescriptorHeap>();
    m_pDescriptorHeap->Create(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 4096, true);

    // Create Pass Constant Buffer
    UINT nConstantBufferSize = (sizeof(PassConstants) + 255) & ~255;
    m_pd3dcbPass = CreateBufferResource(pDevice, pCommandList, NULL, nConstantBufferSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, NULL);
    m_pd3dcbPass->Map(0, NULL, (void**)&m_pcbMappedPass);

    // Set up camera projection
    m_pCamera->SetLens(XMConvertToRadians(60.0f), (float)kWindowWidth / (float)kWindowHeight, 0.1f, 500.0f);

    // Create Shader
    auto pShader = std::make_unique<Shader>();
    pShader->Build(pDevice);

    // --------------------------------------------------------------------------
    // 1. Create default room (will be overwritten by MapLoader if map.json exists)
    // --------------------------------------------------------------------------
    auto pRoom = std::make_unique<CRoom>();
    pRoom->SetState(RoomState::Inactive);
    pRoom->SetBoundingBox(BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(100.0f, 100.0f, 100.0f)));
    m_vRooms.push_back(std::move(pRoom));


    // --------------------------------------------------------------------------
    // 2. Load Global Objects (Player)
    // --------------------------------------------------------------------------
    m_pCurrentRoom = nullptr; 

    GameObject* pPlayer = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Assets/Player/MageBlue.bin");
    if (pPlayer)
    {
        OutputDebugString(L"Player model loaded successfully!\n");
        pPlayer->GetTransform()->SetPosition(0.0f, 0.0f, 0.0f);
        pPlayer->GetTransform()->SetScale(5.0f, 5.0f, 5.0f);
        pPlayer->AddComponent<PlayerComponent>();
        m_pPlayerGameObject = pPlayer;

        auto* pAnim = pPlayer->AddComponent<AnimationComponent>();
        pAnim->LoadAnimation("Assets/Player/MageBlue_Anim.bin");
        pAnim->Play("Idle", true);

        // Add Collider Component for Player
        auto* pPlayerCollider = pPlayer->AddComponent<ColliderComponent>();
        pPlayerCollider->SetExtents(1.0f, 2.0f, 1.0f);  // Player-sized box
        pPlayerCollider->SetCenter(0.0f, 2.0f, 0.0f);   // Center at player's midsection
        pPlayerCollider->SetLayer(CollisionLayer::Player);
        pPlayerCollider->SetCollisionMask(CollisionMask::Player);
        pPlayerCollider->SetOnCollisionEnter([](ColliderComponent* pOther) {
            OutputDebugString(L"[Collision] Player ENTER collision!\n");
        });
        pPlayerCollider->SetOnCollisionStay([](ColliderComponent* pOther) {
            OutputDebugString(L"[Collision] Player STAY collision...\n");
        });
        pPlayerCollider->SetOnCollisionExit([](ColliderComponent* pOther) {
            OutputDebugString(L"[Collision] Player EXIT collision!\n");
        });

        // Add Skill Component
        auto* pSkillComponent = pPlayer->AddComponent<SkillComponent>();

        // Q - WaveSlash (웨이브 슬래시)
        auto waveSlash = std::make_unique<WaveSlashBehavior>();
        waveSlash->SetVFXManager(m_pFluidVFXManager.get());
        waveSlash->SetScene(this);
        pSkillComponent->EquipSkill(SkillSlot::Q, std::move(waveSlash));

        // E - FireBeam (화염 빔)
        auto fireBeam = std::make_unique<FireBeamBehavior>();
        fireBeam->SetVFXManager(m_pFluidVFXManager.get());
        fireBeam->SetScene(this);
        pSkillComponent->EquipSkill(SkillSlot::E, std::move(fireBeam));

        // R - Meteor (메테오)
        auto meteor = std::make_unique<MeteorBehavior>();
        meteor->SetVFXManager(m_pFluidVFXManager.get());
        meteor->SetScene(this);
        pSkillComponent->EquipSkill(SkillSlot::R, std::move(meteor));

        // RightClick - Fireball (기존 투사체 유지)
        auto fireballRClick = std::make_unique<FireballBehavior>();
        fireballRClick->SetProjectileManager(m_pProjectileManager.get());
        pSkillComponent->EquipSkill(SkillSlot::RightClick, std::move(fireballRClick));

        OutputDebugString(L"[Scene] Skill system initialized - Q:WaveSlash, E:FireBeam, R:Meteor, RClick:Fireball\n");

        AddRenderComponentsToHierarchy(pDevice, pCommandList, pPlayer, pShader.get(), true);  // Player casts shadow
    }
    else
    { // Fallback
        pPlayer = CreateGameObject(pDevice, pCommandList);
        pPlayer->GetTransform()->SetPosition(0.0f, 0.0f, 20.0f);
        pPlayer->AddComponent<PlayerComponent>();
        m_pPlayerGameObject = pPlayer;
    }
    m_pCamera->SetTarget(m_pPlayerGameObject);

    // Set current room
    m_pCurrentRoom = m_vRooms[0].get();

    // --------------------------------------------------------------------------
    // Initialize Enemy Spawner
    // --------------------------------------------------------------------------
    m_pEnemySpawner->Init(pDevice, pCommandList, this, pShader.get());

    // --------------------------------------------------------------------------
    // 영속 리소스(Particles, Projectiles, Interaction Cube)를 먼저 초기화하여
    // 디스크립터 힙의 앞부분에 고정 배치합니다.
    // 이 이후에 맵 오브젝트(MapLoader)가 오도록 순서를 맞춤으로써,
    // 맵 전환 시 m_nPersistentDescriptorEnd 이후 슬롯만 재활용할 수 있습니다.
    // --------------------------------------------------------------------------

    // Particle System (512 reserved slots)
    UINT nParticleDescriptorStart = m_nNextDescriptorIndex;
    m_nNextDescriptorIndex += 512;
    m_pParticleSystem->Init(pDevice, pCommandList, m_pDescriptorHeap.get(), nParticleDescriptorStart);
    OutputDebugString(L"[Scene] Particle system initialized\n");

    // Floating embers for volcanic atmosphere
    m_nEmberEmitterId = m_pParticleSystem->CreateEmitter(
        FireParticlePresets::FloatingEmbers(),
        XMFLOAT3(0.0f, 0.0f, 0.0f)
    );
    OutputDebugString(L"[Scene] Floating embers emitter created\n");

    // Fluid Particle System (SRV 디스크립터 슬롯 1개)
    UINT nFluidParticleDescriptorStart = m_nNextDescriptorIndex;
    m_nNextDescriptorIndex += 1;
    m_pFluidParticleSystem->Init(pDevice, pCommandList, m_pDescriptorHeap.get(), nFluidParticleDescriptorStart);
    OutputDebugString(L"[Scene] Fluid particle system initialized\n");

    // FluidSkillVFXManager (최대 8개 동시 이펙트)
    UINT nFluidVFXDescStart = m_nNextDescriptorIndex;
    m_nNextDescriptorIndex += FluidSkillVFXManager::MAX_EFFECTS;
    m_pFluidVFXManager->Init(pDevice, pCommandList, m_pDescriptorHeap.get(), nFluidVFXDescStart);
    OutputDebugString(L"[Scene] FluidSkillVFXManager initialized\n");

    // TorchSystem (횃불 조명 및 불꽃 빌보드)
    UINT nTorchDescStart = m_nNextDescriptorIndex;
    m_nNextDescriptorIndex += 2;  // 1 for flame texture SRV, 1 for instance buffer SRV
    m_pTorchSystem->Init(pDevice, pCommandList, this, pShader.get(), m_pDescriptorHeap.get(), nTorchDescStart);
    OutputDebugString(L"[Scene] TorchSystem initialized\n");

    // VFXLibrary 초기화 (모든 스킬 VFX 정의 등록)
    VFXLibrary::Get().Initialize();
    OutputDebugString(L"[Scene] VFXLibrary initialized\n");

    // Screen-Space Fluid Renderer 초기화
    if (auto* pApp = Dx12App::GetInstance())
    {
        m_pSSF->Init(pDevice, pApp->GetWindowWidth(), pApp->GetWindowHeight());
        OutputDebugString(L"[Scene] ScreenSpaceFluid initialized\n");
    }

    // FluidSkillEffect: SkillComponent 연결 (플레이어 설정 후)
    if (m_pPlayerGameObject)
    {
        auto* pSkill = m_pPlayerGameObject->GetComponent<SkillComponent>();
        if (pSkill)
        {
            m_pFluidSkillEffect->Init(m_pFluidParticleSystem.get(), pSkill);
            OutputDebugString(L"[Scene] FluidSkillEffect initialized\n");
        }
    }

    // Projectile Manager (64 reserved slots)
    UINT nProjectileDescriptorStart = m_nNextDescriptorIndex;
    m_nNextDescriptorIndex += 64;
    m_pProjectileManager->Init(this, pDevice, pCommandList, m_pDescriptorHeap.get(), nProjectileDescriptorStart);
    OutputDebugString(L"[Scene] Projectile system initialized\n");

    // Debug Renderer (no descriptors)
    m_pDebugRenderer->Init(pDevice, pCommandList);
    OutputDebugString(L"[Scene] Debug renderer initialized (F1 to toggle)\n");

    // SpotLight parameters
    m_pcbMappedPass->m_SpotLight.m_xmf4SpotLightColor = XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f);
    m_pcbMappedPass->m_SpotLight.m_fSpotLightRange = 100.0f;
    m_pcbMappedPass->m_SpotLight.m_fSpotLightInnerCone = cosf(XMConvertToRadians(20.0f));
    m_pcbMappedPass->m_SpotLight.m_fSpotLightOuterCone = cosf(XMConvertToRadians(30.0f));
    m_pcbMappedPass->m_SpotLight.m_fPad5 = 0.0f;
    m_pcbMappedPass->m_SpotLight.m_fPad6 = 0.0f;

    // Store the shader (needed before Interaction Cube creation)
    m_vShaders.push_back(std::move(pShader));

    // Initialize global GameObjects (player hierarchy)
    for (auto& gameObject : m_vGameObjects)
        gameObject->Init(pDevice, pCommandList);

    // --------------------------------------------------------------------------
    // Create Interaction Cube (Blue Cube) – global object, permanent slot
    // --------------------------------------------------------------------------
    {
        CRoom* pTempRoom = m_pCurrentRoom;
        m_pCurrentRoom = nullptr;  // global object, not in any room
        m_pInteractionCube = CreateGameObject(pDevice, pCommandList);
        m_pCurrentRoom = pTempRoom;
    }

    m_pInteractionCube->GetTransform()->SetPosition(0.0f, 0.0f, 0.0f);  // repositioned after MapLoader
    m_pInteractionCube->GetTransform()->SetScale(2.0f, 2.0f, 2.0f);

    {
        CubeMesh* pCubeMesh = new CubeMesh(pDevice, pCommandList, 1.0f, 1.0f, 1.0f);
        m_pInteractionCube->SetMesh(pCubeMesh);

        MATERIAL blueMaterial;
        blueMaterial.m_cAmbient  = XMFLOAT4(0.0f, 0.0f, 0.2f, 1.0f);
        blueMaterial.m_cDiffuse  = XMFLOAT4(0.2f, 0.4f, 1.0f, 1.0f);
        blueMaterial.m_cSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 32.0f);
        blueMaterial.m_cEmissive = XMFLOAT4(0.0f, 0.1f, 0.3f, 1.0f);
        m_pInteractionCube->SetMaterial(blueMaterial);

        m_pInteractionCube->AddComponent<RenderComponent>()->SetMesh(pCubeMesh);
        m_vShaders[0]->AddRenderComponent(m_pInteractionCube->GetComponent<RenderComponent>());

        auto* pInteractable = m_pInteractionCube->AddComponent<InteractableComponent>();
        pInteractable->SetPromptText(L"[F] Interact");
        pInteractable->SetInteractionDistance(m_fInteractionDistance);
        pInteractable->SetOnInteract([this](InteractableComponent* pComp) {
            if (m_pCurrentRoom && m_pCurrentRoom->GetState() == RoomState::Inactive)
            {
                m_pCurrentRoom->SetState(RoomState::Active);
                OutputDebugString(L"[Scene] Room activated via InteractableComponent!\n");
            }
            pComp->Hide();
        });
    }
    m_bInteractionCubeActive = true;
    m_bEnemiesSpawned = false;

    // --------------------------------------------------------------------------
    // 용암 바닥 배치 (타일 아래에 큰 평면 하나) — 워터마크 이전에 생성해 영속 슬롯 확보
    // --------------------------------------------------------------------------
    {
        CRoom* pTempRoom = m_pCurrentRoom;
        m_pCurrentRoom = nullptr;  // m_vGameObjects에 등록 (룸에 속하지 않음)
        m_pLavaPlane = CreateGameObject(pDevice, pCommandList);
        m_pCurrentRoom = pTempRoom;

        if (m_pLavaPlane)
        {
            // 하나의 큰 평면 메쉬 (타일 아래 전체를 덮음)
            CubeMesh* pPlaneMesh = new CubeMesh(pDevice, pCommandList, 1.0f, 0.1f, 1.0f);
            m_pLavaPlane->SetMesh(pPlaneMesh);

            // 타일보다 약간 아래에 배치, 맵 + 화산 외곽까지 충분히 덮음
            m_pLavaPlane->GetTransform()->SetPosition(0.0f, -3.5f, -200.0f);
            m_pLavaPlane->GetTransform()->SetScale(2000.0f, 1.0f, 2000.0f);

            m_pLavaPlane->SetLava(true);

            // 용암 머티리얼 (텍스쳐 원본 색상 유지)
            MATERIAL lavaMat;
            lavaMat.m_cAmbient  = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
            lavaMat.m_cDiffuse  = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            lavaMat.m_cSpecular = XMFLOAT4(0.85f, 0.85f, 0.85f, 8.0f);  // smoothness 0.85
            lavaMat.m_cEmissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            m_pLavaPlane->SetMaterial(lavaMat);

            // 텍스쳐 로드
            m_pLavaPlane->SetTextureName("Assets/MapData/meshes/textures/lava-texture.png");
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
            AllocateDescriptor(&cpuHandle, &gpuHandle);
            m_pLavaPlane->LoadTexture(pDevice, pCommandList, cpuHandle);
            m_pLavaPlane->SetSrvGpuDescriptorHandle(gpuHandle);

            auto* pRC = m_pLavaPlane->AddComponent<RenderComponent>();
            pRC->SetMesh(pPlaneMesh);
            pRC->SetCastsShadow(false);
            m_vShaders[0]->AddRenderComponent(pRC);
        }
        OutputDebugString(L"[Scene] Lava floor plane placed under tiles\n");
    }

    // --------------------------------------------------------------------------
    // Volcano 장식 메쉬 배치 (맵 외곽 배경용) — 워터마크 이전에 생성해 영속 슬롯 확보
    // --------------------------------------------------------------------------
    {
        struct VolcanoPlacement {
            float x, y, z;
            float scale;
            float rotY;
        };
        // 대형 화산 3개 (맵 외곽 배경용 - 충분히 먼 거리로 배치)
        VolcanoPlacement placements[] = {
            { -600.0f, -8.0f, -800.0f, 3000.0f, 20.0f },   // 서북쪽 먼 외곽
            { 600.0f, -8.0f, -800.0f, 2800.0f, -15.0f },   // 동북쪽 먼 외곽
            { 0.0f, -10.0f, 400.0f, 5000.0f, 10.0f },      // 남쪽 외곽 (더 큰 화산)
        };

        XMFLOAT4X4 identity;
        XMStoreFloat4x4(&identity, XMMatrixIdentity());

        for (const auto& placement : placements)
        {
            CRoom* pTempRoom = m_pCurrentRoom;
            m_pCurrentRoom = nullptr;  // m_vGameObjects에 등록 (룸에 속하지 않음)

            GameObject* pVolcano = MeshLoader::LoadGeometryFromFile(
                this, pDevice, pCommandList, NULL,
                "Assets/MapData/meshes/volcano/volcano.bin");

            m_pCurrentRoom = pTempRoom;

            if (pVolcano)
            {
                pVolcano->GetTransform()->SetLocalMatrix(identity);
                pVolcano->GetTransform()->SetPosition(placement.x, placement.y, placement.z);
                pVolcano->GetTransform()->SetScale(placement.scale, placement.scale, placement.scale);
                pVolcano->GetTransform()->SetRotation(-90.0f, placement.rotY, 0.0f);

                AddRenderComponentsToHierarchy(pDevice, pCommandList, pVolcano, m_vShaders[0].get(), false);
            }
        }
        OutputDebugString(L"[Scene] Volcano decorations placed (4 large volcanoes)\n");
    }

    // --------------------------------------------------------------------------
    // 영속 디스크립터 워터마크 기록
    // 이 시점 이후의 슬롯(맵 오브젝트·적·포탈 등)은 맵 전환 시 재활용됩니다.
    // --------------------------------------------------------------------------
    m_nPersistentDescriptorEnd = m_nNextDescriptorIndex;

    // --------------------------------------------------------------------------
    // Map pool – 여기에 맵 JSON 경로를 추가하세요
    // --------------------------------------------------------------------------
    // rooms.json manifest가 있으면 그 목록을 pool로 사용, 없으면 map.json 폴백
    {
        JsonVal manifest = JsonVal::parseFile("Assets/MapData/rooms.json");
        if (!manifest.isNull() && manifest.has("rooms"))
        {
            const JsonVal& roomFiles = manifest["rooms"];
            for (size_t i = 0; i < roomFiles.size(); i++)
                m_vMapPool.push_back(roomFiles[i].str);
        }
        if (!manifest.isNull() && manifest.has("bossRoom"))
            m_strBossMap = manifest["bossRoom"].str;
        if (m_vMapPool.empty())
            m_vMapPool.push_back("Assets/MapData/map.json");
        if (m_strBossMap.empty())
            m_strBossMap = "Assets/MapData/map.json";
    }

    // --------------------------------------------------------------------------
    // Load map from JSON (recyclable slots from m_nPersistentDescriptorEnd onward)
    // --------------------------------------------------------------------------
    m_strCurrentMap = m_vMapPool[0];
    bool bMapLoaded = MapLoader::LoadIntoScene(
        m_strCurrentMap.c_str(), this, pDevice, pCommandList, m_vShaders[0].get());

    if (!bMapLoaded) {
        OutputDebugString(L"[Scene] Map load failed – using default test room\n");
        m_pCurrentRoom->SetEnemySpawner(m_pEnemySpawner.get());
        m_pCurrentRoom->SetPlayerTarget(m_pPlayerGameObject);
        m_pCurrentRoom->SetScene(this);
    }

    // 일반 맵: 적 스폰은 인터랙션 큐브 활성화 후 Room::SetState(Active)에서 처리됨
    OutputDebugString(L"[Scene] Normal map loaded - enemies will spawn on room activation\n");

    // 맵 정적 오브젝트의 상수 버퍼를 한 번 초기화
    // (CRoom::Update는 Inactive 상태에서 스킵하므로, 맵 로드 직후 딱 한 번 강제 갱신)
    if (m_pCurrentRoom)
    {
        for (const auto& pGO : m_pCurrentRoom->GetGameObjects())
            pGO->Update(0.0f);
    }

    // 인터랙션 큐브를 플레이어 스폰 근처로 이동 (MapLoader가 플레이어 위치를 결정한 후)
    if (m_pPlayerGameObject)
    {
        XMFLOAT3 playerSpawn = m_pPlayerGameObject->GetTransform()->GetPosition();
        m_pInteractionCube->GetTransform()->SetPosition(
            playerSpawn.x + 5.0f, playerSpawn.y, playerSpawn.z);
    }

    // --------------------------------------------------------------------------
    // 8. Initialize LavaGeyser Manager for current room (화염 맵 기믹)
    // --------------------------------------------------------------------------
    if (m_pCurrentRoom)
    {
        UINT nGeyserDescStart = m_nNextDescriptorIndex;
        m_nNextDescriptorIndex += 1;  // FluidParticleSystem uses 1 descriptor slot

        m_pCurrentRoom->InitLavaGeyserManager(
            pDevice, pCommandList, m_vShaders[0].get(),
            m_pDescriptorHeap.get(), nGeyserDescStart);

        OutputDebugString(L"[Scene] LavaGeyserManager initialized for current room\n");
    }

    OutputDebugString(L"[Scene] Enemy spawn system initialized\n");

    OutputDebugString(L"[Scene] Interaction cube created\n");
}

void Scene::LoadSceneFromFile(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, const char* pstrFileName)
{
}

CRoom* Scene::CreateRoomFromBounds(const XMFLOAT3& center, const XMFLOAT3& extents)
{
    auto pRoom = std::make_unique<CRoom>();
    pRoom->SetState(RoomState::Inactive);
    pRoom->SetBoundingBox(BoundingBox(center, extents));
    CRoom* pRaw = pRoom.get();
    m_vRooms.push_back(std::move(pRoom));
    // First room from map becomes the current room
    if (!m_pCurrentRoom)
        m_pCurrentRoom = pRaw;
    return pRaw;
}

void Scene::AddRenderComponentsToHierarchy(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
	GameObject* pGameObject, Shader* pShader, bool bCastsShadow)
{
	if (!pGameObject)
	{
		OutputDebugString(L"AddRenderComponentsToHierarchy called with a NULL game object!\n");
		return;
	}

	if (pGameObject->GetMesh())
	{
		auto* pRenderComp = pGameObject->AddComponent<RenderComponent>();
		pRenderComp->SetMesh(pGameObject->GetMesh());
		pRenderComp->SetCastsShadow(bCastsShadow);
		pShader->AddRenderComponent(pRenderComp);
	}

	if (pGameObject->m_pChild)
	{
		AddRenderComponentsToHierarchy(pDevice, pCommandList, pGameObject->m_pChild, pShader, bCastsShadow);
	}
	if (pGameObject->m_pSibling)
	{
		AddRenderComponentsToHierarchy(pDevice, pCommandList, pGameObject->m_pSibling, pShader, bCastsShadow);
	}
}

void Scene::Update(float deltaTime, InputSystem* pInputSystem)
{
    m_fLastDeltaTime = deltaTime;

    // ── Kraken emergence cinematic ──────────────────────────────────────────
    // Trigger: death callback sets m_bPendingKrakenSpawn
    if (m_bPendingKrakenSpawn && m_pPreloadedKraken)
    {
        m_bPendingKrakenSpawn = false;

        GameObject* pKrakenObj = m_pPreloadedKraken->GetOwner();
        if (pKrakenObj)
        {
            XMFLOAT3 emergePos = m_xmf3PendingKrakenPos;
            emergePos.y = -5.0f;
            pKrakenObj->GetTransform()->SetPosition(emergePos);
            pKrakenObj->GetTransform()->SetScale(0.05f, 0.05f, 0.05f);
        }

        m_eKrakenStage = KrakenCutsceneStage::Rumble;
        m_fKrakenEmergeTimer = 0.0f;

        // Lock camera on emergence point + rumble shake
        XMFLOAT3 camFocus = m_xmf3PendingKrakenPos;
        camFocus.y = 0.0f;
        m_pCamera->StartCinematic(camFocus, 45.0f, 25.0f, m_pCamera->IsFreeCam() ? 45.0f : 200.0f);
        m_pCamera->StartShake(1.5f, KRAKEN_T_RUMBLE);

        OutputDebugString(L"[Scene] Kraken cutscene: RUMBLE\n");
    }

    if (m_eKrakenStage != KrakenCutsceneStage::None && m_pPreloadedKraken)
    {
        m_fKrakenEmergeTimer += deltaTime;
        float T = m_fKrakenEmergeTimer;

        GameObject* pKrakenObj = m_pPreloadedKraken->GetOwner();

        auto easeOutCubic = [](float t) { return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t); };
        auto easeInOutQuad = [](float t) { return t < 0.5f ? 2*t*t : 1 - (-2*t+2)*(-2*t+2)/2; };
        auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };

        // ── Stage: Rumble (0 ~ KRAKEN_T_RUMBLE) ─────────────────────────────
        if (m_eKrakenStage == KrakenCutsceneStage::Rumble)
        {
            // Kraken barely stirs underground
            if (pKrakenObj)
            {
                float t = T / KRAKEN_T_RUMBLE;
                float s = lerp(0.05f, 0.12f, easeOutCubic(t));
                pKrakenObj->GetTransform()->SetScale(s, s, s);
                XMFLOAT3 pos = m_xmf3PendingKrakenPos; pos.y = -5.0f;
                pKrakenObj->GetTransform()->SetPosition(pos);
            }

            if (T >= KRAKEN_T_RUMBLE)
            {
                m_eKrakenStage = KrakenCutsceneStage::Rise;
                // Zoom in closer for the rise
                XMFLOAT3 camFocus = m_xmf3PendingKrakenPos; camFocus.y = 1.0f;
                m_pCamera->StartCinematic(camFocus, 30.0f, 20.0f, 210.0f);
                m_pCamera->StartShake(0.4f, KRAKEN_T_RISE - KRAKEN_T_RUMBLE);
                OutputDebugString(L"[Scene] Kraken cutscene: RISE\n");
            }
        }
        // ── Stage: Rise (KRAKEN_T_RUMBLE ~ KRAKEN_T_RISE) ───────────────────
        else if (m_eKrakenStage == KrakenCutsceneStage::Rise)
        {
            float t = (T - KRAKEN_T_RUMBLE) / (KRAKEN_T_RISE - KRAKEN_T_RUMBLE);
            if (t > 1.0f) t = 1.0f;
            float e = easeInOutQuad(t);

            if (pKrakenObj)
            {
                float s = lerp(0.12f, 0.55f, e);
                pKrakenObj->GetTransform()->SetScale(s, s, s);
                XMFLOAT3 pos = m_xmf3PendingKrakenPos;
                pos.y = lerp(-5.0f, -1.0f, e);
                pKrakenObj->GetTransform()->SetPosition(pos);
            }

            if (T >= KRAKEN_T_RISE)
            {
                m_eKrakenStage = KrakenCutsceneStage::Burst;
                // Pull back dramatically for the burst
                XMFLOAT3 camFocus = m_xmf3PendingKrakenPos; camFocus.y = 2.0f;
                m_pCamera->StartCinematic(camFocus, 50.0f, 30.0f, 210.0f);
                m_pCamera->StartShake(2.5f, KRAKEN_T_BURST - KRAKEN_T_RISE);
                OutputDebugString(L"[Scene] Kraken cutscene: BURST\n");
            }
        }
        // ── Stage: Burst (KRAKEN_T_RISE ~ KRAKEN_T_BURST) ───────────────────
        else if (m_eKrakenStage == KrakenCutsceneStage::Burst)
        {
            float t = (T - KRAKEN_T_RISE) / (KRAKEN_T_BURST - KRAKEN_T_RISE);
            if (t > 1.0f) t = 1.0f;
            float e = easeOutCubic(t);

            if (pKrakenObj)
            {
                float s = lerp(0.55f, KRAKEN_SCALE, e);
                pKrakenObj->GetTransform()->SetScale(s, s, s);
                XMFLOAT3 pos = m_xmf3PendingKrakenPos;
                pos.y = lerp(-1.0f, 0.0f, e);
                pKrakenObj->GetTransform()->SetPosition(pos);
            }

            if (T >= KRAKEN_T_BURST)
            {
                m_eKrakenStage = KrakenCutsceneStage::Reveal;
                // Wide reveal shot
                XMFLOAT3 camFocus = m_xmf3PendingKrakenPos; camFocus.y = 3.0f;
                m_pCamera->StartCinematic(camFocus, 65.0f, 35.0f, 210.0f);
                OutputDebugString(L"[Scene] Kraken cutscene: REVEAL\n");
            }
        }
        // ── Stage: Reveal (KRAKEN_T_BURST ~ KRAKEN_T_REVEAL) ────────────────
        else if (m_eKrakenStage == KrakenCutsceneStage::Reveal)
        {
            float t = (T - KRAKEN_T_BURST) / (KRAKEN_T_REVEAL - KRAKEN_T_BURST);
            if (t > 1.0f) t = 1.0f;

            // Slowly zoom out further during reveal
            float dist = lerp(65.0f, 75.0f, t);
            m_pCamera->SetCinematicOrbit(dist, 35.0f, 210.0f);

            if (T >= KRAKEN_T_REVEAL)
            {
                // Cutscene over: return camera to player, start combat
                m_eKrakenStage = KrakenCutsceneStage::None;
                m_pCamera->StopCinematic();

                m_pPreloadedKraken->SetTarget(m_pPlayerGameObject);
                m_pPreloadedKraken = nullptr;
                OutputDebugString(L"[Scene] Kraken fully emerged - combat begins\n");
            }
        }
    }

    // F2: FreeCam 토글 (테스트용 자유 시점)
    if (pInputSystem && pInputSystem->IsKeyPressed(VK_F2))
    {
        m_pCamera->ToggleFreeCam();
    }

    // Toggle debug collider visualization with F1
    if (pInputSystem && pInputSystem->IsKeyPressed(VK_F1))
    {
        m_pDebugRenderer->Toggle();
        OutputDebugString(m_pDebugRenderer->IsEnabled() ? L"[Debug] Colliders ON\n" : L"[Debug] Colliders OFF\n");
    }

    // F3: Toggle static bind pose (no skinning) — plane stays = mesh/material bug, disappears = skinning bug
    if (pInputSystem && pInputSystem->IsKeyPressed(VK_F3))
    {
        AnimationComponent::s_bDebugStaticPose = !AnimationComponent::s_bDebugStaticPose;
        OutputDebugString(AnimationComponent::s_bDebugStaticPose
            ? L"[Debug] Static pose ON (skinning disabled)\n"
            : L"[Debug] Static pose OFF (skinning enabled)\n");
    }

    // F4: Toggle no-texture mode — shows raw geometry with material color only
    if (pInputSystem && pInputSystem->IsKeyPressed(VK_F4))
    {
        GameObject::s_bDebugNoTexture = !GameObject::s_bDebugNoTexture;
        OutputDebugString(GameObject::s_bDebugNoTexture
            ? L"[Debug] No-texture ON (solid color)\n"
            : L"[Debug] No-texture OFF (textures enabled)\n");
    }

    // B 키: 현재 테마에 맞는 보스전
    if (pInputSystem && pInputSystem->IsKeyPressed('B'))
    {
        switch (m_eCurrentTheme)
        {
        case StageTheme::Water:
            OutputDebugString(L"[Scene] B key - Water boss (Kraken)\n");
            TransitionToWaterBossRoom(); break;
        case StageTheme::Earth:
            OutputDebugString(L"[Scene] B key - Earth boss (Golem)\n");
            TransitionToEarthBossRoom(); break;
        case StageTheme::Grass:
            OutputDebugString(L"[Scene] B key - Grass boss (Demon)\n");
            TransitionToGrassBossRoom(); break;
        default:
            OutputDebugString(L"[Scene] B key - Fire boss (Dragon)\n");
            TransitionToBossRoom(); break;
        }
    }

    // N 키: 다음 스테이지로 전환 (불→물→땅→풀)
    if (pInputSystem && pInputSystem->IsKeyPressed('N'))
    {
        switch (m_eCurrentTheme)
        {
        case StageTheme::Fire:
            OutputDebugString(L"[Scene] N key - Fire → Water\n");
            TransitionToWaterStage(); break;
        case StageTheme::Water:
            OutputDebugString(L"[Scene] N key - Water → Earth\n");
            TransitionToEarthStage(); break;
        case StageTheme::Earth:
            OutputDebugString(L"[Scene] N key - Earth → Grass\n");
            TransitionToGrassStage(); break;
        case StageTheme::Grass:
            OutputDebugString(L"[Scene] N key - Grass → Fire\n");
            TransitionToBossRoom(); break;  // 풀 이후는 처음으로
        }
    }

    // L 키: 보스 메가 브레스 강제 실행 (테스트용)
    if (pInputSystem && pInputSystem->IsKeyPressed('L'))
    {
        if (m_pCurrentRoom)
        {
            const auto& gameObjects = m_pCurrentRoom->GetGameObjects();
            for (const auto& pObj : gameObjects)
            {
                if (!pObj) continue;
                EnemyComponent* pEnemy = pObj->GetComponent<EnemyComponent>();
                if (pEnemy && pEnemy->IsBoss() && !pEnemy->IsDead())
                {
                    OutputDebugString(L"[Scene] Debug key 'L' pressed - Forcing Mega Breath!\n");
                    // 즉시 메가 브레스 주입 및 실행
                    auto megaBreath = std::make_unique<MegaBreathAttackBehavior>();
                    megaBreath->Execute(pEnemy); // 즉시 초기화 및 애니메이션 시작
                    pEnemy->SetAttackBehavior(std::move(megaBreath));
                    pEnemy->ChangeState(EnemyState::Attack); // 공격 상태로 강제 전이
                }
            }
        }
    }

    // 0 키: 다음 방 / 9 키: 이전 방 (개발용 직접 이동)
    if (pInputSystem && !m_vMapPool.empty())
    {
        int poolSize = (int)m_vMapPool.size();
        if (pInputSystem->IsKeyPressed('0'))
            TransitionToRoomByIndex((m_nCurrentPoolIndex + 1) % poolSize);
        else if (pInputSystem->IsKeyPressed('9'))
            TransitionToRoomByIndex((m_nCurrentPoolIndex - 1 + poolSize) % poolSize);
    }

    // Update camera
    if (m_pCamera && pInputSystem)
    {
        bool bFreeCam = m_pCamera->IsFreeCam();
        m_pCamera->Update(
            pInputSystem->GetMouseDeltaX(),
            pInputSystem->GetMouseDeltaY(),
            pInputSystem->GetMouseWheelDelta(),
            deltaTime,
            bFreeCam && pInputSystem->IsKeyDown('W'),
            bFreeCam && pInputSystem->IsKeyDown('S'),
            bFreeCam && pInputSystem->IsKeyDown('A'),
            bFreeCam && pInputSystem->IsKeyDown('D'),
            bFreeCam && pInputSystem->IsKeyDown('E'),
            bFreeCam && pInputSystem->IsKeyDown('Q')
        );
    }

    // Update Pass Constants
    XMMATRIX mView = XMLoadFloat4x4(&m_pCamera->GetViewMatrix());
    XMMATRIX mProjection = XMLoadFloat4x4(&m_pCamera->GetProjectionMatrix());
    XMMATRIX mViewProj = mView * mProjection;
    XMStoreFloat4x4(&m_pcbMappedPass->m_xmf4x4ViewProj, XMMatrixTranspose(mViewProj));

    // Set lighting parameters based on current theme
    XMVECTOR lightDir;
    switch (m_eCurrentTheme)
    {
    case StageTheme::Water:
        m_pcbMappedPass->m_xmf4LightColor = XMFLOAT4(2.0f, 1.9f, 1.7f, 1.0f);
        lightDir = XMVector3Normalize(XMVectorSet(-0.8f, -0.3f, 0.5f, 0.0f));
        break;
    case StageTheme::Earth:
        // 따뜻한 황토빛 태양
        m_pcbMappedPass->m_xmf4LightColor = XMFLOAT4(1.9f, 1.7f, 1.2f, 1.0f);
        lightDir = XMVector3Normalize(XMVectorSet(-0.5f, -0.6f, 0.4f, 0.0f));
        break;
    case StageTheme::Grass:
        // 밝은 낮 햇빛 (청량한 하늘)
        m_pcbMappedPass->m_xmf4LightColor = XMFLOAT4(2.1f, 2.0f, 1.8f, 1.0f);
        lightDir = XMVector3Normalize(XMVectorSet(-0.4f, -0.8f, 0.3f, 0.0f));
        break;
    default: // Fire
        m_pcbMappedPass->m_xmf4LightColor = XMFLOAT4(2.0f, 1.3f, 0.8f, 1.0f);
        lightDir = XMVector3Normalize(XMVectorSet(-0.6f, -0.7f, 0.3f, 0.0f));
        break;
    }
    XMStoreFloat3(&m_pcbMappedPass->m_xmf3LightDirection, lightDir);

    // Calculate Light View-Projection for Shadow Mapping
    {
        // Get player position as shadow focus center
        XMFLOAT3 shadowCenter = XMFLOAT3(0.0f, 0.0f, 0.0f);
        if (m_pPlayerGameObject)
        {
            shadowCenter = m_pPlayerGameObject->GetTransform()->GetPosition();
        }

        // Light position: center + opposite of light direction * distance
        float lightDistance = 50.0f;  // 가까워짐
        XMVECTOR vShadowCenter = XMLoadFloat3(&shadowCenter);
        XMVECTOR vLightPos = vShadowCenter - lightDir * lightDistance;

        // Light View Matrix (look at shadow center from light position)
        XMVECTOR vUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
        // If light is nearly vertical, use different up vector
        if (fabsf(XMVectorGetY(lightDir)) > 0.99f)
        {
            vUp = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
        }
        XMMATRIX mLightView = XMMatrixLookAtLH(vLightPos, vShadowCenter, vUp);

        // Orthographic Projection for directional light shadow
        float shadowOrthoSize = 160.0f;  // 넓은 그림자 영역
        float nearZ = 0.1f;
        float farZ = 150.0f;
        XMMATRIX mLightProj = XMMatrixOrthographicLH(shadowOrthoSize, shadowOrthoSize, nearZ, farZ);

        XMMATRIX mLightViewProj = mLightView * mLightProj;
        XMStoreFloat4x4(&m_pcbMappedPass->m_xmf4x4LightViewProj, XMMatrixTranspose(mLightViewProj));
    }
    m_pcbMappedPass->m_fPad0 = 0.0f; // Padding for directional light

    m_pcbMappedPass->m_xmf4PointLightColor = XMFLOAT4(0.7f, 0.5f, 0.3f, 1.0f); // Subtle warm point light
    m_pcbMappedPass->m_xmf3PointLightPosition = XMFLOAT3(10.0f, 5.0f, -10.0f); // Off to the side
    m_pcbMappedPass->m_fPad1 = 0.0f; // Padding for point light position

    m_pcbMappedPass->m_fPointLightRange = 50.0f; // Smaller range
    m_pcbMappedPass->m_fPad2 = 0.0f; // Padding
    m_pcbMappedPass->m_fPad3 = 0.0f; // Padding
    m_pcbMappedPass->m_fPad4 = 0.0f; // Padding

    switch (m_eCurrentTheme)
    {
    case StageTheme::Water:
        m_pcbMappedPass->m_xmf4AmbientLight = XMFLOAT4(0.6f, 0.65f, 0.75f, 1.0f);
        break;
    case StageTheme::Earth:
        m_pcbMappedPass->m_xmf4AmbientLight = XMFLOAT4(0.5f, 0.42f, 0.3f, 1.0f);
        break;
    case StageTheme::Grass:
        m_pcbMappedPass->m_xmf4AmbientLight = XMFLOAT4(0.5f, 0.6f, 0.4f, 1.0f);
        break;
    default: // Fire
        m_pcbMappedPass->m_xmf4AmbientLight = XMFLOAT4(0.35f, 0.2f, 0.1f, 1.0f);
        break;
    }

    // Set Camera Position for Specular Calculation
    XMFLOAT3 cameraPosition = m_pCamera->GetPosition();
    m_pcbMappedPass->m_xmf3CameraPosition = cameraPosition;
    m_pcbMappedPass->m_fPadCam = 0.0f; // Padding

    // Update time for lava animation
    m_fTotalTime += deltaTime;
    m_pcbMappedPass->m_fTime = m_fTotalTime;

    // Update SpotLight parameters based on player position
    if (m_pPlayerGameObject)
    {
        XMFLOAT3 playerPosition = m_pPlayerGameObject->GetTransform()->GetPosition();
        XMVECTOR playerForward = m_pPlayerGameObject->GetTransform()->GetLook();
        XMVECTOR spotlightOffset = XMVectorScale(playerForward, 5.0f); // 5 units in front of the player
        XMVECTOR spotlightPosition = XMLoadFloat3(&playerPosition) + spotlightOffset;
        XMStoreFloat3(&m_pcbMappedPass->m_SpotLight.m_xmf3SpotLightPosition, spotlightPosition);
    }
    else
    {
        // Fallback to camera position if player not available
        m_pcbMappedPass->m_SpotLight.m_xmf3SpotLightPosition = cameraPosition;
    }
    XMVECTOR look = m_pCamera->GetLookDirection();
    XMStoreFloat3(&m_pcbMappedPass->m_SpotLight.m_xmf3SpotLightDirection, look);


    // 1. Update Global Components (Player, etc.)
    for (auto& gameObject : m_vGameObjects)
    {
        gameObject->Update(deltaTime);
    }

    // 2. Update Current Room
    if (m_pCurrentRoom)
    {
        m_pCurrentRoom->Update(deltaTime);

        // 보스 클리어 감지 → 다음 스테이지 자동 전환
        if (m_bInBossRoom && m_pCurrentRoom->GetState() == RoomState::Cleared)
        {
            m_bInBossRoom = false;
            OutputDebugString(L"[Scene] Boss defeated! Transitioning to next stage...\n");
            switch (m_eCurrentTheme)
            {
            case StageTheme::Fire:  TransitionToWaterStage();  break;
            case StageTheme::Water: TransitionToEarthStage();  break;
            case StageTheme::Earth: TransitionToGrassStage();  break;
            case StageTheme::Grass: TransitionToWaterStage();  break;  // 풀 보스 이후는 처음부터
            }
        }
    }

    // ── Dragon boss intro cutscene ──────────────────────────────────────────
    if (m_pDragonIntroEnemy)
    {
        if (m_pDragonIntroEnemy->IsInIntro())
        {
            BossIntroPhase phase = m_pDragonIntroEnemy->GetIntroPhase();
            GameObject* pDragonObj = m_pDragonIntroEnemy->GetOwner();
            XMFLOAT3 dragonPos = pDragonObj ? pDragonObj->GetTransform()->GetPosition() : XMFLOAT3(0,0,0);

            if (phase != m_eLastDragonPhase)
            {
                m_eLastDragonPhase = phase;
                switch (phase)
                {
                case BossIntroPhase::Landing:
                    // Mid shot — watch the dragon touch down
                    m_pCamera->StartCinematic({ dragonPos.x, 2.0f, dragonPos.z }, 60.0f, 25.0f, 200.0f);
                    m_pCamera->StartShake(0.4f, 1.5f);
                    break;
                case BossIntroPhase::Roaring:
                    // Dramatic side angle — strong shake, dragon fills frame
                    m_pCamera->StartCinematic({ dragonPos.x, 4.0f, dragonPos.z }, 42.0f, 30.0f, 230.0f);
                    m_pCamera->StartShake(2.2f, 2.2f);
                    break;
                default: break;
                }
            }

            // FlyingIn: wide overhead shot tracking the dragon as it descends
            // Player can move freely during this phase
            if (phase == BossIntroPhase::FlyingIn && pDragonObj)
            {
                float focusY = dragonPos.y * 0.45f + 3.0f;
                m_pCamera->StartCinematic({ dragonPos.x, focusY, dragonPos.z }, 95.0f, 22.0f, 185.0f);
            }
        }
        else
        {
            // Intro done — return camera to player
            m_pCamera->StopCinematic();
            m_eLastDragonPhase = BossIntroPhase::None;
            m_pDragonIntroEnemy = nullptr;
            OutputDebugString(L"[Scene] Dragon intro complete - combat begins\n");
        }
    }

    // Player input: allowed during FlyingIn so player can walk in,
    // blocked only during Landing/Roaring and Kraken cutscene
    bool bBlockInput = (m_eKrakenStage != KrakenCutsceneStage::None)
        || (m_pDragonIntroEnemy != nullptr
            && m_eLastDragonPhase != BossIntroPhase::FlyingIn
            && m_eLastDragonPhase != BossIntroPhase::None);
    if (m_pPlayerGameObject && m_pPlayerGameObject->GetComponent<PlayerComponent>()
        && !bBlockInput)
    {
        m_pPlayerGameObject->GetComponent<PlayerComponent>()->PlayerUpdate(deltaTime, pInputSystem, m_pCamera.get());
    }

    // Update Projectile System
    if (m_pProjectileManager)
    {
        m_pProjectileManager->Update(deltaTime);
    }

    // Update Particle System
    if (m_pParticleSystem)
    {
        // Update floating embers to follow player
        if (m_nEmberEmitterId >= 0 && m_pPlayerGameObject)
        {
            auto* pEmitter = m_pParticleSystem->GetEmitter(m_nEmberEmitterId);
            if (pEmitter)
            {
                XMFLOAT3 playerPos = m_pPlayerGameObject->GetTransform()->GetPosition();
                pEmitter->SetPosition(playerPos);
            }
        }
        m_pParticleSystem->Update(deltaTime);
    }

    // Update Fluid Particle System
    if (m_pFluidParticleSystem)
    {
        m_pFluidParticleSystem->Update(deltaTime);
    }

    // Update Fluid Skill VFX Manager
    if (m_pFluidVFXManager)
    {
        m_pFluidVFXManager->Update(deltaTime);
    }

    // Update Fluid Skill Effect (제어점을 플레이어 위치에 맞게 갱신)
    if (m_pFluidSkillEffect && m_pPlayerGameObject)
    {
        m_pFluidSkillEffect->Update(deltaTime,
            m_pPlayerGameObject->GetTransform()->GetPosition());
    }

    // Update Torch System (flickering effect)
    if (m_pTorchSystem)
    {
        m_pTorchSystem->Update(deltaTime);
        m_pTorchSystem->FillLightData(m_pcbMappedPass);
    }

    // 2. Check for collisions
    if (m_pCollisionManager)
    {
        // Collect colliders from global objects
        std::vector<ColliderComponent*> globalColliders;
        for (auto& gameObject : m_vGameObjects)
        {
            CollectColliders(gameObject.get(), globalColliders);
        }

        // Collect colliders from current room
        std::vector<ColliderComponent*> roomColliders;
        if (m_pCurrentRoom)
        {
            const auto& roomObjects = m_pCurrentRoom->GetGameObjects();
            for (const auto& obj : roomObjects)
            {
                CollectColliders(obj.get(), roomColliders);
            }
        }

        // Run collision detection
        m_pCollisionManager->Update(globalColliders, roomColliders);
    }

    // Process pending deletions at end of frame
    ProcessPendingDeletions();
}

void Scene::UpdateRenderList()
{
    // 1. Clear previous frame's render list from all shaders
    for (auto& shader : m_vShaders)
    {
        shader->ClearRenderComponents();
    }

    // 2. Register Global Objects (Player, etc.)
    Shader* pMainShader = m_vShaders[0].get();

    // Helper vector for traversal to avoid recursion
    std::vector<GameObject*> stack;
    stack.reserve(64);

    // Process Global Objects
    for (auto& gameObject : m_vGameObjects)
    {
        stack.push_back(gameObject.get());
        while (!stack.empty())
        {
            GameObject* pObj = stack.back();
            stack.pop_back();

            if (pObj->GetComponent<RenderComponent>())
            {
                pMainShader->AddRenderComponent(pObj->GetComponent<RenderComponent>());
            }

            if (pObj->m_pSibling) stack.push_back(pObj->m_pSibling);
            if (pObj->m_pChild) stack.push_back(pObj->m_pChild);
        }
    }

    // 3. Register Current Room Objects
    if (m_pCurrentRoom)
    {
        const auto& roomObjects = m_pCurrentRoom->GetGameObjects();
        for (const auto& obj : roomObjects)
        {
            stack.push_back(obj.get());
            while (!stack.empty())
            {
                GameObject* pObj = stack.back();
                stack.pop_back();

                if (pObj->GetComponent<RenderComponent>())
                {
                    pMainShader->AddRenderComponent(pObj->GetComponent<RenderComponent>());
                }

                if (pObj->m_pSibling) stack.push_back(pObj->m_pSibling);
                if (pObj->m_pChild) stack.push_back(pObj->m_pChild);
            }
        }
    }
}

void Scene::RenderShadowPass(ID3D12GraphicsCommandList* pCommandList)
{
    // Update render list before shadow pass (ensures correct objects are rendered)
    UpdateRenderList();

    // Set the descriptor heap
    ID3D12DescriptorHeap* ppHeaps[] = { m_pDescriptorHeap->GetHeap() };
    pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Render shadow casters
    for (auto& shader : m_vShaders)
    {
        shader->RenderShadowPass(pCommandList, GetPassCBVAddress());
    }
}

void Scene::Render(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle,
                   D3D12_CPU_DESCRIPTOR_HANDLE mainRTV, D3D12_CPU_DESCRIPTOR_HANDLE mainDSV,
                   ID3D12Resource* pMainRTBuffer)
{
    // Set the descriptor heap
    ID3D12DescriptorHeap* ppHeaps[] = { m_pDescriptorHeap->GetHeap() };
    pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Note: UpdateRenderList() is already called in RenderShadowPass() before this

    // Iterate through shaders (groups) and render
    for (auto& shader : m_vShaders)
    {
        shader->Render(pCommandList, GetPassCBVAddress(), shadowSrvHandle,
                       m_d3dWaterNormal2GpuHandle, m_d3dWaterHeight2GpuHandle,
                       m_d3dFoamOpacityGpuHandle, m_d3dFoamDiffuseGpuHandle);
    }

    // Terrain 렌더 (불투명, 전용 힙 사용)
    if (m_pTerrain && m_pTerrain->IsLoaded())
    {
        m_pTerrain->Render(pCommandList, GetPassCBVAddress());

        // Terrain 전용 힙 사용 후 → 메인 힙 복구
        ID3D12DescriptorHeap* mainHeaps[] = { m_pDescriptorHeap->GetHeap() };
        pCommandList->SetDescriptorHeaps(1, mainHeaps);
    }

    // Render projectiles (after main rendering, pipeline state is already set)
    if (m_pProjectileManager)
    {
        m_pProjectileManager->Render(pCommandList);
    }

    // Render particles
    if (m_pParticleSystem)
    {
        m_pParticleSystem->Render(pCommandList);
    }

    // ---------- Screen-Space Fluid 렌더링 ----------
    bool bHasFluid = (m_pFluidParticleSystem && m_pFluidParticleSystem->IsActive());
    if (!bHasFluid && m_pFluidVFXManager)
    {
        // VFX 매니저에 활성 슬롯이 있는지 확인 (Render에서 내부적으로도 체크하므로 간단히 true)
        bHasFluid = true;
    }

    if (m_pSSF && m_pSSF->IsInitialized() && bHasFluid)
    {
        // 행렬 준비
        XMMATRIX mView = XMLoadFloat4x4(&m_pCamera->GetViewMatrix());
        XMMATRIX mProj = XMLoadFloat4x4(&m_pCamera->GetProjectionMatrix());
        // 뷰 행렬 r[i] = (xaxis.i, yaxis.i, zaxis.i) 구조이므로 열 방향으로 추출
        XMFLOAT3 camRight = { XMVectorGetX(mView.r[0]), XMVectorGetX(mView.r[1]), XMVectorGetX(mView.r[2]) };
        XMFLOAT3 camUp    = { XMVectorGetY(mView.r[0]), XMVectorGetY(mView.r[1]), XMVectorGetY(mView.r[2]) };

        XMFLOAT4X4 viewProjT, viewT;
        XMStoreFloat4x4(&viewProjT, XMMatrixTranspose(mView * mProj));
        XMStoreFloat4x4(&viewT, XMMatrixTranspose(mView));

        XMFLOAT4X4 projRaw;
        XMStoreFloat4x4(&projRaw, mProj);  // 비전치 (projA/projB 추출용)

        float projA = projRaw._33;
        float projB = projRaw._43;

        // GPU SPH dispatch (BeginDepthPass 전에)
        if (m_pFluidVFXManager)
            m_pFluidVFXManager->DispatchSPH(pCommandList, m_fLastDeltaTime);
        if (m_pFluidParticleSystem && m_pFluidParticleSystem->IsActive())
            m_pFluidParticleSystem->DispatchSPH(pCommandList, m_fLastDeltaTime);

        // 장면 캡처: 유체 렌더 전 메인 RT를 복사 (굴절 배경용)
        if (pMainRTBuffer)
        {
            m_pSSF->CaptureSceneColor(pCommandList, pMainRTBuffer);
        }

        // Pass 1a: Sphere depth (깊이 테스트 있음 - 가장 가까운 구체 표면 Z 캡처)
        m_pSSF->BeginDepthPass(pCommandList);

        if (m_pFluidParticleSystem && m_pFluidParticleSystem->IsActive())
            m_pFluidParticleSystem->RenderDepth(pCommandList, viewProjT, viewT, camRight, camUp, projA, projB, m_pSSF.get());

        if (m_pFluidVFXManager)
            m_pFluidVFXManager->RenderDepth(pCommandList, viewProjT, viewT, camRight, camUp, projA, projB, m_pSSF.get());

        // Pass 1b: Thickness (깊이 테스트 없음 - 모든 파티클이 두께에 기여)
        // 카메라 각도에 무관하게 좌우 대칭 두께 보장
        m_pSSF->BeginThicknessPass(pCommandList);

        if (m_pFluidParticleSystem && m_pFluidParticleSystem->IsActive())
            m_pFluidParticleSystem->RenderThicknessOnly(pCommandList, m_pSSF.get());

        if (m_pFluidVFXManager)
            m_pFluidVFXManager->RenderThicknessOnly(pCommandList, m_pSSF.get());

        m_pSSF->EndDepthPass(pCommandList);

        // Pass 2+3: Smooth + Composite (메인 RT에 합성)
        // 조명 방향을 뷰 공간으로 변환
        XMFLOAT3 lightDirWorld = { -0.5f, -0.8f, -0.3f };
        XMVECTOR lightV = XMVector3TransformNormal(XMLoadFloat3(&lightDirWorld), mView);
        XMFLOAT3 lightDirVS;
        XMStoreFloat3(&lightDirVS, XMVector3Normalize(lightV));

        // 태양 이펙트 색상: outer=코로나(붉은 주황), inner=코어(밝은 노란-흰색)
        XMFLOAT4 fluidColorOuter = { 0.95f, 0.15f, 0.0f,  0.9f };  // 기본: 진한 붉은 주황
        XMFLOAT4 fluidColorInner = { 1.0f,  0.88f, 0.25f, 1.0f };  // 기본: 밝은 노란-흰색
        if (m_pFluidVFXManager)
        {
            FluidElementColor colors = m_pFluidVFXManager->GetDominantFluidColors();
            // outer = 얇은 외곽 (edgeColor), inner = 두꺼운 코어 (coreColor)
            fluidColorOuter = colors.edgeColor;
            fluidColorOuter.w = (std::max)(fluidColorOuter.w, 0.6f);  // emit strength 최소 보장
            fluidColorInner = colors.coreColor;
        }

        // 뷰포트/시저렉트를 메인 크기로 복원
        if (auto* pApp = Dx12App::GetInstance())
        {
            D3D12_VIEWPORT vp = { 0, 0, (FLOAT)pApp->GetWindowWidth(), (FLOAT)pApp->GetWindowHeight(), 0.0f, 1.0f };
            pCommandList->RSSetViewports(1, &vp);
            D3D12_RECT sr = { 0, 0, (LONG)pApp->GetWindowWidth(), (LONG)pApp->GetWindowHeight() };
            pCommandList->RSSetScissorRects(1, &sr);
        }

        m_pSSF->SmoothAndComposite(pCommandList, mainRTV, mainDSV, projRaw, lightDirVS, fluidColorOuter, fluidColorInner);

        // SSF 후 메인 RT 복원 (디스크립터 힙도 Scene의 마스터 힙으로 복원)
        pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        pCommandList->OMSetRenderTargets(1, &mainRTV, FALSE, &mainDSV);
    }
    else
    {
        // Fallback: 기존 빌보드 렌더링
        if (m_pFluidParticleSystem && m_pFluidParticleSystem->IsActive())
        {
            XMMATRIX mView = XMLoadFloat4x4(&m_pCamera->GetViewMatrix());
            XMFLOAT3 camRight = { XMVectorGetX(mView.r[0]), XMVectorGetX(mView.r[1]), XMVectorGetX(mView.r[2]) };
            XMFLOAT3 camUp    = { XMVectorGetY(mView.r[0]), XMVectorGetY(mView.r[1]), XMVectorGetY(mView.r[2]) };

            XMMATRIX mViewProj = mView * XMLoadFloat4x4(&m_pCamera->GetProjectionMatrix());
            XMFLOAT4X4 viewProj;
            XMStoreFloat4x4(&viewProj, XMMatrixTranspose(mViewProj));

            m_pFluidParticleSystem->Render(pCommandList, viewProj, camRight, camUp);
        }

        if (m_pFluidVFXManager)
        {
            XMMATRIX mView2 = XMLoadFloat4x4(&m_pCamera->GetViewMatrix());
            XMFLOAT3 camRight2 = { XMVectorGetX(mView2.r[0]), XMVectorGetX(mView2.r[1]), XMVectorGetX(mView2.r[2]) };
            XMFLOAT3 camUp2    = { XMVectorGetY(mView2.r[0]), XMVectorGetY(mView2.r[1]), XMVectorGetY(mView2.r[2]) };

            XMMATRIX mViewProj2 = mView2 * XMLoadFloat4x4(&m_pCamera->GetProjectionMatrix());
            XMFLOAT4X4 viewProj2;
            XMStoreFloat4x4(&viewProj2, XMMatrixTranspose(mViewProj2));

            m_pFluidVFXManager->Render(pCommandList, viewProj2, camRight2, camUp2);
        }
    }

    // Render lava geyser particles (Room 기반 맵 기믹)
    if (m_pCurrentRoom)
    {
        LavaGeyserManager* pGeyserManager = m_pCurrentRoom->GetLavaGeyserManager();
        if (pGeyserManager && pGeyserManager->IsActive())
        {
            XMMATRIX mView3 = XMLoadFloat4x4(&m_pCamera->GetViewMatrix());
            XMFLOAT3 camRight3 = { XMVectorGetX(mView3.r[0]), XMVectorGetX(mView3.r[1]), XMVectorGetX(mView3.r[2]) };
            XMFLOAT3 camUp3    = { XMVectorGetY(mView3.r[0]), XMVectorGetY(mView3.r[1]), XMVectorGetY(mView3.r[2]) };

            XMMATRIX mViewProj3 = mView3 * XMLoadFloat4x4(&m_pCamera->GetProjectionMatrix());
            XMFLOAT4X4 viewProj3;
            XMStoreFloat4x4(&viewProj3, XMMatrixTranspose(mViewProj3));

            pGeyserManager->Render(pCommandList, viewProj3, camRight3, camUp3);
        }
    }

    // Render torch flame billboards
    if (m_pTorchSystem && m_pTorchSystem->GetTorchCount() > 0)
    {
        XMMATRIX mView4 = XMLoadFloat4x4(&m_pCamera->GetViewMatrix());
        XMFLOAT3 camRight4 = { XMVectorGetX(mView4.r[0]), XMVectorGetY(mView4.r[0]), XMVectorGetZ(mView4.r[0]) };
        XMFLOAT3 camUp4    = { XMVectorGetX(mView4.r[1]), XMVectorGetY(mView4.r[1]), XMVectorGetZ(mView4.r[1]) };

        XMMATRIX mViewProj4 = mView4 * XMLoadFloat4x4(&m_pCamera->GetProjectionMatrix());
        XMFLOAT4X4 viewProj4;
        XMStoreFloat4x4(&viewProj4, XMMatrixTranspose(mViewProj4));

        m_pTorchSystem->Render(pCommandList, viewProj4, camRight4, camUp4);
    }

    // Render debug colliders (F1 to toggle)
    if (m_pDebugRenderer && m_pDebugRenderer->IsEnabled())
    {
        std::vector<ColliderComponent*> allColliders;

        // Collect from global objects
        for (auto& gameObject : m_vGameObjects)
        {
            CollectColliders(gameObject.get(), allColliders);
        }

        // Collect from current room
        if (m_pCurrentRoom)
        {
            const auto& roomObjects = m_pCurrentRoom->GetGameObjects();
            for (const auto& obj : roomObjects)
            {
                CollectColliders(obj.get(), allColliders);
            }
        }

        m_pDebugRenderer->Render(pCommandList, GetPassCBVAddress(), allColliders);
    }
}

void Scene::OnResizeSSF(UINT width, UINT height)
{
    if (m_pSSF && m_pSSF->IsInitialized())
    {
        if (auto* pApp = Dx12App::GetInstance())
        {
            m_pSSF->OnResize(pApp->GetDevice(), width, height);
        }
    }
}

GameObject* Scene::CreateGameObject(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    // Note: We use raw pointer here, but ownership is transferred to unique_ptr below
    GameObject* newGameObject = new GameObject();

    UINT slot = m_nNextDescriptorIndex;
    bool bRecyclable = (slot >= m_nPersistentDescriptorEnd);

    auto cacheIt = bRecyclable ? m_vCBCache.find(slot) : m_vCBCache.end();
    bool bReused = (cacheIt != m_vCBCache.end());

    // 슬롯 충돌 진단 로그
    wchar_t dbgBuf[256];
    swprintf_s(dbgBuf, L"[Scene] CreateObject: Slot=%u, Reused=%s, PersistentEnd=%u\n", 
              slot, (bReused ? L"TRUE" : L"FALSE"), m_nPersistentDescriptorEnd);
    OutputDebugString(dbgBuf);

    if (bReused)
    {
        // 같은 슬롯 번호에 이미 생성된 리소스 재사용
        ObjectConstants* pMapped = nullptr;
        cacheIt->second->Map(0, nullptr, (void**)&pMapped);
        newGameObject->ReuseConstantBuffer(cacheIt->second, pMapped);

        // 다른 방 방문 시 AllocateDescriptor(SRV)가 이 슬롯을 덮어썼을 수 있으므로
        // CBV 뷰를 항상 재생성하여 슬롯 타입 충돌 버그를 방지
        UINT nCBSize = (sizeof(ObjectConstants) + 255) & ~255;
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
        cbvDesc.BufferLocation = cacheIt->second->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes    = nCBSize;
        pDevice->CreateConstantBufferView(&cbvDesc, m_pDescriptorHeap->GetCPUHandle(slot));
    }
    else
    {
        // 처음 사용하는 슬롯 — 리소스 + CBV 생성
        D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_pDescriptorHeap->GetCPUHandle(slot);
        newGameObject->CreateConstantBuffer(pDevice, pCommandList, sizeof(ObjectConstants), cpuHandle);
        if (bRecyclable)
            m_vCBCache[slot] = newGameObject->GetConstantBufferResource();
    }

    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_pDescriptorHeap->GetGPUHandle(slot);
    newGameObject->SetGpuDescriptorHandle(gpuHandle);

    m_nNextDescriptorIndex++;

    // Add to Room or Scene
    if (m_pCurrentRoom)
    {
        m_pCurrentRoom->AddGameObject(std::unique_ptr<GameObject>(newGameObject));
    }
    else
    {
        m_vGameObjects.push_back(std::unique_ptr<GameObject>(newGameObject));
    }

    return newGameObject;
}

void Scene::PrintHierarchy(GameObject* pGameObject, int nDepth)
{
	if (!pGameObject) return;

	// Indent for hierarchy visualization
	std::wstring indent(nDepth * 2, ' ');

	// Prepare debug string
	wchar_t buffer[256];
	swprintf_s(buffer, 256, L"%sFrame: %hs, Has Mesh: %s, Has RenderComponent: %s\n",
		indent.c_str(),
		pGameObject->m_pstrFrameName,
		pGameObject->GetMesh() ? L"Yes" : L"No",
		pGameObject->GetComponent<RenderComponent>() ? L"Yes" : L"No"
	);

	OutputDebugString(buffer);

	// Recurse for children and siblings
	if (pGameObject->m_pChild)
	{
		PrintHierarchy(pGameObject->m_pChild, nDepth + 1);
	}
	if (pGameObject->m_pSibling)
	{
		PrintHierarchy(pGameObject->m_pSibling, nDepth);
	}
}

void Scene::CollectColliders(GameObject* pGameObject, std::vector<ColliderComponent*>& outColliders)
{
    if (!pGameObject) return;

    // Check if this object has a ColliderComponent
    ColliderComponent* pCollider = pGameObject->GetComponent<ColliderComponent>();
    if (pCollider && pCollider->IsEnabled())
    {
        outColliders.push_back(pCollider);
    }

    // Recurse for children and siblings
    if (pGameObject->m_pChild)
    {
        CollectColliders(pGameObject->m_pChild, outColliders);
    }
    if (pGameObject->m_pSibling)
    {
        CollectColliders(pGameObject->m_pSibling, outColliders);
    }
}

bool Scene::IsNearInteractionCube() const
{
    if (!m_pInteractionCube || !m_pPlayerGameObject)
        return false;

    auto* pInteractable = m_pInteractionCube->GetComponent<InteractableComponent>();
    if (!pInteractable || !pInteractable->IsActive())
        return false;

    return pInteractable->IsPlayerInRange(m_pPlayerGameObject);
}

void Scene::TriggerInteraction()
{
    if (!IsNearInteractionCube())
        return;

    auto* pInteractable = m_pInteractionCube->GetComponent<InteractableComponent>();
    if (pInteractable)
    {
        pInteractable->Interact();
        m_bInteractionCubeActive = false;
        m_bEnemiesSpawned = true;
    }
}

bool Scene::IsNearDropItem() const
{
    if (!m_pCurrentRoom || !m_pPlayerGameObject)
        return false;

    GameObject* pDropItem = m_pCurrentRoom->GetDropItem();
    if (!pDropItem)
        return false;

    DropItemComponent* pDropComp = pDropItem->GetComponent<DropItemComponent>();
    if (!pDropComp || !pDropComp->IsActive())
        return false;

    XMFLOAT3 playerPos = m_pPlayerGameObject->GetTransform()->GetPosition();
    XMFLOAT3 dropPos = pDropItem->GetTransform()->GetPosition();

    return MathUtils::Distance3D(playerPos, dropPos) <= m_fDropInteractionDistance;
}

void Scene::StartDropInteraction()
{
    if (m_eDropState != DropInteractionState::None && m_eDropState != DropInteractionState::NearDrop)
        return;

    if (!IsNearDropItem())
        return;

    m_pCurrentDropItem = m_pCurrentRoom->GetDropItem();
    m_eDropState = DropInteractionState::SelectingRune;

    OutputDebugString(L"[Scene] Started drop interaction - selecting rune\n");
}

void Scene::SelectRune(int choice)
{
    if (m_eDropState != DropInteractionState::SelectingRune)
        return;

    if (choice < 0 || choice >= 3)
        return;

    if (!m_pCurrentDropItem)
    {
        CancelDropInteraction();
        return;
    }

    DropItemComponent* pDropComp = m_pCurrentDropItem->GetComponent<DropItemComponent>();
    if (!pDropComp)
    {
        CancelDropInteraction();
        return;
    }

    // Get the selected rune type
    ActivationType selectedRune = pDropComp->GetRuneOption(choice);

    // Apply to player's skill component
    if (m_pPlayerGameObject)
    {
        SkillComponent* pSkill = m_pPlayerGameObject->GetComponent<SkillComponent>();
        if (pSkill)
        {
            pSkill->SetActivationType(selectedRune);

            const wchar_t* typeNames[] = { L"Instant", L"Charge", L"Channel", L"Place", L"Enhance" };
            wchar_t buffer[128];
            swprintf_s(buffer, L"[Scene] Rune selected: %s\n", typeNames[static_cast<int>(selectedRune)]);
            OutputDebugString(buffer);
        }
    }

    // Deactivate and hide the drop item
    pDropComp->SetActive(false);
    m_pCurrentDropItem->GetTransform()->SetPosition(0.0f, -1000.0f, 0.0f);

    // Clear room's drop reference
    if (m_pCurrentRoom)
    {
        m_pCurrentRoom->ClearDropItem();
    }

    // Reset state
    m_pCurrentDropItem = nullptr;
    m_eDropState = DropInteractionState::None;
}

void Scene::CancelDropInteraction()
{
    m_eDropState = DropInteractionState::None;
    m_pCurrentDropItem = nullptr;
    m_eSelectedRune = ActivationType::None;
    OutputDebugString(L"[Scene] Drop interaction cancelled\n");
}

void Scene::SelectRuneByClick(int runeIndex)
{
    if (m_eDropState != DropInteractionState::SelectingRune)
        return;

    if (runeIndex < 0 || runeIndex >= 3)
        return;

    if (!m_pCurrentDropItem)
    {
        CancelDropInteraction();
        return;
    }

    DropItemComponent* pDropComp = m_pCurrentDropItem->GetComponent<DropItemComponent>();
    if (!pDropComp)
    {
        CancelDropInteraction();
        return;
    }

    // Store selected rune and move to skill selection state
    m_eSelectedRune = pDropComp->GetRuneOption(runeIndex);
    m_eDropState = DropInteractionState::SelectingSkill;

    const wchar_t* typeNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance", L"Split" };
    wchar_t buffer[128];
    swprintf_s(buffer, L"[Scene] Rune clicked: %s - Now select skill slot\n", typeNames[static_cast<int>(m_eSelectedRune)]);
    OutputDebugString(buffer);
}

void Scene::SelectSkillSlot(SkillSlot slot, int runeSlotIndex)
{
    if (m_eDropState != DropInteractionState::SelectingSkill)
        return;

    if (m_eSelectedRune == ActivationType::None)
    {
        CancelDropInteraction();
        return;
    }

    // Apply rune to player's skill slot
    if (m_pPlayerGameObject)
    {
        SkillComponent* pSkill = m_pPlayerGameObject->GetComponent<SkillComponent>();
        if (pSkill)
        {
            pSkill->SetRuneSlot(slot, runeSlotIndex, m_eSelectedRune);

            // Also update legacy activation type to the first skill's first rune
            pSkill->SetActivationType(pSkill->GetSkillActivationType(SkillSlot::Q));

            const wchar_t* slotNames[] = { L"Q", L"E", L"R", L"RMB" };
            const wchar_t* typeNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance", L"Split" };
            wchar_t buffer[128];
            swprintf_s(buffer, L"[Scene] Rune %s assigned to %s slot %d\n",
                typeNames[static_cast<int>(m_eSelectedRune)], slotNames[static_cast<int>(slot)], runeSlotIndex + 1);
            OutputDebugString(buffer);
        }
    }

    // Deactivate and hide the drop item
    if (m_pCurrentDropItem)
    {
        DropItemComponent* pDropComp = m_pCurrentDropItem->GetComponent<DropItemComponent>();
        if (pDropComp)
            pDropComp->SetActive(false);
        m_pCurrentDropItem->GetTransform()->SetPosition(0.0f, -1000.0f, 0.0f);
    }

    // Clear room's drop reference
    if (m_pCurrentRoom)
    {
        m_pCurrentRoom->ClearDropItem();
    }

    // Reset state
    m_pCurrentDropItem = nullptr;
    m_eSelectedRune = ActivationType::None;
    m_eDropState = DropInteractionState::None;
}

bool Scene::IsNearPortalCube() const
{
    if (!m_pCurrentRoom || !m_pPlayerGameObject)
        return false;

    GameObject* pPortal = m_pCurrentRoom->GetPortalCube();
    if (!pPortal)
        return false;

    auto* pInteractable = pPortal->GetComponent<InteractableComponent>();
    if (!pInteractable || !pInteractable->IsActive())
        return false;

    return pInteractable->IsPlayerInRange(m_pPlayerGameObject);
}

void Scene::TriggerPortalInteraction()
{
    if (!IsNearPortalCube())
        return;

    GameObject* pPortal = m_pCurrentRoom->GetPortalCube();
    if (!pPortal)
        return;

    auto* pInteractable = pPortal->GetComponent<InteractableComponent>();
    if (pInteractable)
    {
        pInteractable->Interact();
    }
}

void Scene::ReAddRenderComponentsToShader(GameObject* pGO)
{
    if (!pGO) return;
    auto* pRC = pGO->GetComponent<RenderComponent>();
    if (pRC) m_vShaders[0]->AddRenderComponent(pRC);
    ReAddRenderComponentsToShader(pGO->m_pChild);
    ReAddRenderComponentsToShader(pGO->m_pSibling);
}

void Scene::TransitionToNextRoom()
{
    OutputDebugString(L"[Scene] Transitioning to next room...\n");

    m_nRoomCount++;

    // 5스테이지 클리어 후 보스전 진입
    if (m_nRoomCount >= 5)
    {
        OutputDebugString(L"[Scene] 5 stages cleared - entering boss room!\n");
        TransitionToBossRoom();
        return;
    }

    if (m_vMapPool.empty())
    {
        OutputDebugString(L"[Scene] Map pool is empty – cannot transition\n");
        return;
    }

    // ── 1. 셰이더 RC 목록 전체 클리어 (룸 오브젝트 RC가 댕글링 포인터가 되기 전에)
    m_vShaders[0]->ClearRenderComponents();

    // ── 1b. 이번 프레임에 처리 대기 중인 삭제 요청을 미리 처리
    //        (적 사망 등이 방 파기와 같은 프레임에 발생하면, m_vRooms.clear() 이후
    //         ProcessPendingDeletions 에서 해제된 메모리에 접근 → 새 타일 오브젝트를
    //         잘못 삭제하는 버그 방지)
    ProcessPendingDeletions();

    // ── 2. 기존 룸 전체 파기 (룸 오브젝트, 적, 맵 메시 등)
    m_vRooms.clear();
    m_pCurrentRoom = nullptr;

    // ── 2b. 디스크립터 인덱스를 워터마크로 리셋 (맵 슬롯 재활용)
    m_nNextDescriptorIndex = m_nPersistentDescriptorEnd;

    // ── 2c. 횃불 시스템 클리어 (새 맵에서 다시 배치)
    if (m_pTorchSystem) m_pTorchSystem->Clear();

    // ── 3. 인터랙션 큐브 숨기기 (다음 맵에서 다시 보여야 하는 시작 큐브)
    if (m_pInteractionCube)
    {
        auto* pInteractable = m_pInteractionCube->GetComponent<InteractableComponent>();
        if (pInteractable) pInteractable->SetActive(true);
        m_bInteractionCubeActive = true;
    }

    // ── 4. 영속 오브젝트의 RC를 셰이더에 다시 등록
    //       (플레이어 계층 전체, 인터랙션 큐브)
    for (auto& pGO : m_vGameObjects)
        ReAddRenderComponentsToShader(pGO.get());

    // ── 5. 풀에서 랜덤 맵 선택 (현재 맵과 다른 것 우선)
    std::string nextMap = m_strCurrentMap;
    if (m_vMapPool.size() > 1)
    {
        // 현재 맵이 아닌 것 중에서 랜덤 선택
        std::vector<std::string> candidates;
        for (const auto& path : m_vMapPool)
            if (path != m_strCurrentMap) candidates.push_back(path);
        nextMap = candidates[rand() % candidates.size()];
    }
    // (풀에 맵이 1개뿐이면 같은 맵 재로드)
    m_strCurrentMap = nextMap;

    // ── 6. 새 맵 로드
    ID3D12Device*               pDevice      = Dx12App::GetInstance()->GetDevice();
    ID3D12GraphicsCommandList*  pCommandList = Dx12App::GetInstance()->GetCommandList();

    bool bLoaded = MapLoader::LoadIntoScene(
        m_strCurrentMap.c_str(), this, pDevice, pCommandList, m_vShaders[0].get());

    if (!bLoaded)
    {
        OutputDebugString(L"[Scene] Map load failed during transition – keeping current state\n");
        return;
    }

    // ── 7. 맵 정적 오브젝트 상수 버퍼 초기화 (Inactive 상태에서는 Update가 스킵되므로)
    if (m_pCurrentRoom)
    {
        for (const auto& pGO : m_pCurrentRoom->GetGameObjects())
            pGO->Update(0.0f);
    }

    // ── 7b. LavaGeyser Manager 초기화 (화염 맵 기믹)
    if (m_pCurrentRoom)
    {
        UINT nGeyserDescStart = m_nNextDescriptorIndex;
        m_nNextDescriptorIndex += 1;

        m_pCurrentRoom->InitLavaGeyserManager(
            pDevice, pCommandList, m_vShaders[0].get(),
            m_pDescriptorHeap.get(), nGeyserDescStart);

        OutputDebugString(L"[Scene] LavaGeyserManager initialized for new room\n");
    }

    // ── 8. 포탈을 통해 입장하면 즉시 몬스터 스폰 (인터랙션 큐브 단계 없이)
    if (m_pCurrentRoom)
        m_pCurrentRoom->SetState(RoomState::Active);

    // ── 9. 인터랙션 큐브는 이후 맵에서 숨김 (첫 맵 전용)
    if (m_pInteractionCube)
    {
        auto* pInteractable = m_pInteractionCube->GetComponent<InteractableComponent>();
        if (pInteractable) pInteractable->Hide();
        m_bInteractionCubeActive = false;
    }

    // ── 10. 플레이어 groundY 리셋 (새 맵 바닥 높이 재캡처)
    if (m_pPlayerGameObject)
    {
        auto* pPC = m_pPlayerGameObject->GetComponent<PlayerComponent>();
        if (pPC) pPC->ResetGroundY();
    }

    wchar_t buffer[128];
    swprintf_s(buffer, L"[Scene] Transitioned to map: %hs (room #%d)\n",
               m_strCurrentMap.c_str(), m_nRoomCount + 1);
    OutputDebugString(buffer);
}

void Scene::TransitionToRoomByIndex(int index)
{
    if (m_vMapPool.empty()) return;

    int poolSize = (int)m_vMapPool.size();
    index = ((index % poolSize) + poolSize) % poolSize; // 안전한 wrapping
    m_nCurrentPoolIndex = index;

    wchar_t dbg[128];
    swprintf_s(dbg, L"[Scene] Dev nav → room index %d: %hs\n", index, m_vMapPool[index].c_str());
    OutputDebugString(dbg);

    // ── 공통 정리 단계 (TransitionToNextRoom과 동일)
    m_vShaders[0]->ClearRenderComponents();
    ProcessPendingDeletions();

    m_vRooms.clear();
    m_pCurrentRoom = nullptr;

    m_nNextDescriptorIndex = m_nPersistentDescriptorEnd;

    // 횃불 시스템 클리어
    if (m_pTorchSystem) m_pTorchSystem->Clear();

    if (m_pInteractionCube)
    {
        auto* pInteractable = m_pInteractionCube->GetComponent<InteractableComponent>();
        if (pInteractable) pInteractable->SetActive(true);
        m_bInteractionCubeActive = true;
    }

    for (auto& pGO : m_vGameObjects)
        ReAddRenderComponentsToShader(pGO.get());

    // ── 지정 인덱스 맵 로드
    m_strCurrentMap = m_vMapPool[index];

    ID3D12Device*              pDevice      = Dx12App::GetInstance()->GetDevice();
    ID3D12GraphicsCommandList* pCommandList = Dx12App::GetInstance()->GetCommandList();

    bool bLoaded = MapLoader::LoadIntoScene(
        m_strCurrentMap.c_str(), this, pDevice, pCommandList, m_vShaders[0].get());

    if (!bLoaded)
    {
        OutputDebugString(L"[Scene] TransitionToRoomByIndex: map load failed\n");
        return;
    }

    if (m_pCurrentRoom)
    {
        for (const auto& pGO : m_pCurrentRoom->GetGameObjects())
            pGO->Update(0.0f);

        UINT nGeyserDescStart = m_nNextDescriptorIndex;
        m_nNextDescriptorIndex += 1;
        m_pCurrentRoom->InitLavaGeyserManager(
            pDevice, pCommandList, m_vShaders[0].get(),
            m_pDescriptorHeap.get(), nGeyserDescStart);

        m_pCurrentRoom->SetState(RoomState::Active);
    }

    if (m_pInteractionCube)
    {
        auto* pInteractable = m_pInteractionCube->GetComponent<InteractableComponent>();
        if (pInteractable) pInteractable->Hide();
        m_bInteractionCubeActive = false;
    }

    if (m_pPlayerGameObject)
    {
        auto* pPC = m_pPlayerGameObject->GetComponent<PlayerComponent>();
        if (pPC) pPC->ResetGroundY();
    }
}

void Scene::TransitionToBossRoom()
{
    OutputDebugString(L"[Scene] ========== BOSS ROOM ==========\n");

    // ── 1. 셰이더 RC 목록 전체 클리어
    m_vShaders[0]->ClearRenderComponents();
    ProcessPendingDeletions();

    // ── 2. 기존 룸 전체 파기
    m_vRooms.clear();
    m_pCurrentRoom = nullptr;

    // ── 3. 디스크립터 인덱스를 워터마크로 리셋
    m_nNextDescriptorIndex = m_nPersistentDescriptorEnd;

    // ── 3b. 횃불 시스템 클리어
    if (m_pTorchSystem) m_pTorchSystem->Clear();

    // ── 4. 영속 오브젝트의 RC를 셰이더에 다시 등록
    for (auto& pGO : m_vGameObjects)
        ReAddRenderComponentsToShader(pGO.get());

    // ── 5. 보스 맵 로드 (일반 맵과 동일)
    ID3D12Device*               pDevice      = Dx12App::GetInstance()->GetDevice();
    ID3D12GraphicsCommandList*  pCommandList = Dx12App::GetInstance()->GetCommandList();

    m_strCurrentMap = m_strBossMap;

    bool bLoaded = MapLoader::LoadIntoScene(
        m_strCurrentMap.c_str(), this, pDevice, pCommandList, m_vShaders[0].get());

    if (!bLoaded)
    {
        OutputDebugString(L"[Scene] Boss map load failed!\n");
        return;
    }

    // ── 6. 맵 정적 오브젝트 상수 버퍼 초기화
    if (m_pCurrentRoom)
    {
        for (const auto& pGO : m_pCurrentRoom->GetGameObjects())
            pGO->Update(0.0f);
    }

    // ── 7. LavaGeyser Manager 초기화 (보스전에서도 기믹 적용)
    if (m_pCurrentRoom)
    {
        UINT nGeyserDescStart = m_nNextDescriptorIndex;
        m_nNextDescriptorIndex += 1;

        m_pCurrentRoom->InitLavaGeyserManager(
            pDevice, pCommandList, m_vShaders[0].get(),
            m_pDescriptorHeap.get(), nGeyserDescStart);
    }

    // ── 8. 보스(드래곤) 스폰 - 일반 적 스폰 설정 제거
    if (m_pCurrentRoom && m_pEnemySpawner)
    {
        // 일반 적 스폰 설정 제거
        RoomSpawnConfig emptyConfig;
        m_pCurrentRoom->SetSpawnConfig(emptyConfig);

        // 드래곤 스폰
        OutputDebugString(L"[Scene] Spawning Dragon boss\n");
        XMFLOAT3 dragonPos = XMFLOAT3(0.0f, 0.0f, 20.0f);
        if (m_pPlayerGameObject)
        {
            XMFLOAT3 playerPos = m_pPlayerGameObject->GetTransform()->GetPosition();
            dragonPos = XMFLOAT3(playerPos.x, playerPos.y, playerPos.z + 15.0f);
        }
        GameObject* pDragon = m_pEnemySpawner->SpawnEnemy(m_pCurrentRoom, "Dragon", dragonPos, m_pPlayerGameObject);

        // 보스 인트로 컷씬 시작
        if (pDragon)
        {
            EnemyComponent* pEnemy = pDragon->GetComponent<EnemyComponent>();
            if (pEnemy)
            {
                pEnemy->StartBossIntro(25.0f);  // 25유닛 위에서 착지

                // Camera cinematic: wide-angle shot looking up at the sky where dragon appears
                XMFLOAT3 landPos = dragonPos;
                landPos.y = 0.0f;
                m_pCamera->StartCinematic(landPos, 55.0f, 15.0f, 180.0f);
                m_pDragonIntroEnemy = pEnemy;
                m_eLastDragonPhase  = BossIntroPhase::None;
            }
        }

        // 방 활성화
        m_pCurrentRoom->SetState(RoomState::Active);
    }

    // ── 9. 인터랙션 큐브 숨김
    if (m_pInteractionCube)
    {
        auto* pInteractable = m_pInteractionCube->GetComponent<InteractableComponent>();
        if (pInteractable) pInteractable->Hide();
        m_bInteractionCubeActive = false;
    }

    // ── 10. 플레이어 지면 리셋 (공중에 뜨지 않도록 y=0 강제)
    if (m_pPlayerGameObject)
    {
        XMFLOAT3 pPos = m_pPlayerGameObject->GetTransform()->GetPosition();
        pPos.y = 0.0f;
        m_pPlayerGameObject->GetTransform()->SetPosition(pPos);
        auto* pPC = m_pPlayerGameObject->GetComponent<PlayerComponent>();
        if (pPC) pPC->ResetGroundY();
    }

    m_bInBossRoom = true;
    OutputDebugString(L"[Scene] Boss room ready - Dragon spawned!\n");
}

void Scene::MarkForDeletion(GameObject* pGameObject)
{
    if (!pGameObject) return;

    // Avoid duplicates
    for (auto* pObj : m_vPendingDeletions)
    {
        if (pObj == pGameObject) return;
    }

    m_vPendingDeletions.push_back(pGameObject);
    OutputDebugString(L"[Scene] GameObject marked for deletion\n");
}

// Helper function to recursively unregister colliders from hierarchy
static void UnregisterCollidersRecursive(GameObject* pObj, CollisionManager* pCollisionMgr)
{
    if (!pObj || !pCollisionMgr) return;

    auto* pCollider = pObj->GetComponent<ColliderComponent>();
    if (pCollider)
    {
        pCollisionMgr->UnregisterCollider(pCollider);
    }

    if (pObj->m_pChild) UnregisterCollidersRecursive(pObj->m_pChild, pCollisionMgr);
    if (pObj->m_pSibling) UnregisterCollidersRecursive(pObj->m_pSibling, pCollisionMgr);
}

// Helper function to collect all child/sibling GameObjects for deletion
static void CollectHierarchyForDeletion(GameObject* pObj, std::vector<GameObject*>& outList)
{
    if (!pObj) return;
    outList.push_back(pObj);
    if (pObj->m_pChild) CollectHierarchyForDeletion(pObj->m_pChild, outList);
    if (pObj->m_pSibling) CollectHierarchyForDeletion(pObj->m_pSibling, outList);
}

void Scene::ProcessPendingDeletions()
{
    if (m_vPendingDeletions.empty()) return;

    // Collect all objects to delete (including children/siblings)
    std::vector<GameObject*> allToDelete;
    for (GameObject* pObj : m_vPendingDeletions)
    {
        if (!pObj) continue;

        // Collect this object and all its children/siblings
        // (MeshLoader creates each Frame as separate GameObject in Room)
        if (pObj->m_pChild) CollectHierarchyForDeletion(pObj->m_pChild, allToDelete);
        // Note: Don't add siblings of root - they are separate entities
        allToDelete.push_back(pObj);
    }

    for (GameObject* pObj : allToDelete)
    {
        if (!pObj) continue;

        // Unregister colliders from this object
        auto* pCollider = pObj->GetComponent<ColliderComponent>();
        if (pCollider && m_pCollisionManager)
        {
            m_pCollisionManager->UnregisterCollider(pCollider);
        }

        // Try to remove from Scene's global objects
        bool bFound = false;
        for (auto it = m_vGameObjects.begin(); it != m_vGameObjects.end(); ++it)
        {
            if (it->get() == pObj)
            {
                m_vGameObjects.erase(it);
                bFound = true;
                OutputDebugString(L"[Scene] Deleted GameObject from Scene\n");
                break;
            }
        }

        // If not found in Scene, try current room
        if (!bFound && m_pCurrentRoom)
        {
            m_pCurrentRoom->RemoveGameObject(pObj);
        }
    }

    m_vPendingDeletions.clear();
}

// 모든 플레이어 목록 반환 (로컬 + 원격)
std::vector<GameObject*> Scene::GetAllPlayers() const
{
    std::vector<GameObject*> players;

    // 로컬 플레이어 추가
    if (m_pPlayerGameObject)
    {
        players.push_back(m_pPlayerGameObject);
    }

    // 원격 플레이어 추가 (NetworkManager에서 가져옴)
    // TODO: 멀티플레이어 구현 시 NetworkManager::GetRemotePlayers() 연동
    // NetworkManager* pNetMgr = NetworkManager::GetInstance();
    // if (pNetMgr)
    // {
    //     for (const auto& pair : pNetMgr->GetRemotePlayers())
    //     {
    //         if (pair.second) players.push_back(pair.second);
    //     }
    // }

    return players;
}

// ================================================================
// Terrain 로드
// ================================================================
void Scene::LoadTerrain(const char* configJsonPath, int subdivisionStep)
{
    ID3D12Device*              pDevice      = Dx12App::GetInstance()->GetDevice();
    ID3D12GraphicsCommandList* pCommandList = Dx12App::GetInstance()->GetCommandList();

    m_pTerrain = std::make_unique<Terrain>();
    if (!m_pTerrain->Load(pDevice, pCommandList, configJsonPath, subdivisionStep))
    {
        OutputDebugString(L"[Scene] Terrain load failed!\n");
        m_pTerrain.reset();
    }
    else
    {
        wchar_t msg[256];
        swprintf_s(msg, L"[Scene] Terrain loaded: %hs\n", configJsonPath);
        OutputDebugString(msg);
    }
}

// 적에게 모든 플레이어 등록 (어그로 시스템용)
void Scene::RegisterPlayersToEnemy(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    std::vector<GameObject*> players = GetAllPlayers();
    pEnemy->RegisterAllPlayers(players);
}

void Scene::TransitionToWaterStage()
{
    OutputDebugString(L"[Scene] ========== WATER STAGE ==========\n");

    // ── 1. 테마 변경 (조명 색상에 영향)
    m_eCurrentTheme = StageTheme::Water;

    // ── 2. 셰이더 RC 목록 전체 클리어
    m_vShaders[0]->ClearRenderComponents();
    ProcessPendingDeletions();

    // ── 3. 기존 룸 전체 파기
    m_vRooms.clear();
    m_pCurrentRoom = nullptr;

    // ── 4. 디스크립터 인덱스를 워터마크로 리셋
    m_nNextDescriptorIndex = m_nPersistentDescriptorEnd;

    // ── 5. 횃불 시스템 클리어
    if (m_pTorchSystem) m_pTorchSystem->Clear();

    // ── 6. 용암 평면 숨기기 (렌더링에서 제외)
    if (m_pLavaPlane)
    {
        m_pLavaPlane->GetTransform()->SetPosition(0.0f, -10000.0f, 0.0f);  // 화면 밖으로 이동
    }

    // ── 7. 영속 오브젝트의 RC를 셰이더에 다시 등록 (용암 제외)
    for (auto& pGO : m_vGameObjects)
    {
        if (pGO.get() != m_pLavaPlane)  // 용암은 제외
            ReAddRenderComponentsToShader(pGO.get());
    }

    ID3D12Device*              pDevice      = Dx12App::GetInstance()->GetDevice();
    ID3D12GraphicsCommandList* pCommandList = Dx12App::GetInstance()->GetCommandList();

    // ── 7. 기존 맵 로드 (일반 맵 사용)
    m_strCurrentMap = m_vMapPool[0];
    bool bLoaded = MapLoader::LoadIntoScene(
        m_strCurrentMap.c_str(), this, pDevice, pCommandList, m_vShaders[0].get());

    if (!bLoaded)
    {
        OutputDebugString(L"[Scene] Water stage map load failed!\n");
        return;
    }

    // ── 8. 물 바닥 평면 생성 (용암 대신 물)
    {
        CRoom* pTempRoom = m_pCurrentRoom;
        m_pCurrentRoom = nullptr;

        m_pWaterPlane = CreateGameObject(pDevice, pCommandList);
        m_pCurrentRoom = pTempRoom;

        if (m_pWaterPlane)
        {
            // 세분화된 평면 메쉬 (256x256 그리드 = 66049 정점, 정점 변위용)
            GridPlaneMesh* pPlaneMesh = new GridPlaneMesh(pDevice, pCommandList, 1.0f, 1.0f, 256, 256);
            m_pWaterPlane->SetMesh(pPlaneMesh);

            // 맵이 살짝 잠기도록 높이 조정
            m_pWaterPlane->GetTransform()->SetPosition(0.0f, -1.0f, -200.0f);
            m_pWaterPlane->GetTransform()->SetScale(2000.0f, 1.0f, 2000.0f);

            m_pWaterPlane->SetWater(true);

            // 물 머티리얼 (파란색, 높은 광택)
            MATERIAL waterMat;
            waterMat.m_cAmbient  = XMFLOAT4(0.1f, 0.15f, 0.25f, 1.0f);
            waterMat.m_cDiffuse  = XMFLOAT4(0.8f, 0.9f, 1.0f, 1.0f);
            waterMat.m_cSpecular = XMFLOAT4(0.95f, 0.95f, 0.95f, 64.0f);
            waterMat.m_cEmissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            m_pWaterPlane->SetMaterial(waterMat);

            // 물 텍스쳐 로드 (Base Color)
            m_pWaterPlane->SetTextureName("Assets/Stylize Water Texture/Textures/Vol_36_5_Base_Color.png");
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
            AllocateDescriptor(&cpuHandle, &gpuHandle);
            m_pWaterPlane->LoadTexture(pDevice, pCommandList, cpuHandle);
            m_pWaterPlane->SetSrvGpuDescriptorHandle(gpuHandle);

            // 물 노말맵 로드
            m_pWaterPlane->SetNormalMapName("Assets/Stylize Water Texture/Textures/Vol_36_5_Normal.png");
            D3D12_CPU_DESCRIPTOR_HANDLE normalCpuHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE normalGpuHandle;
            AllocateDescriptor(&normalCpuHandle, &normalGpuHandle);
            m_pWaterPlane->LoadNormalMap(pDevice, pCommandList, normalCpuHandle);
            m_pWaterPlane->SetNormalMapSrvGpuHandle(normalGpuHandle);

            // 물 높이맵 로드
            m_pWaterPlane->SetHeightMapName("Assets/Stylize Water Texture/Textures/Vol_36_5_Height.png");
            D3D12_CPU_DESCRIPTOR_HANDLE heightCpuHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE heightGpuHandle;
            AllocateDescriptor(&heightCpuHandle, &heightGpuHandle);
            m_pWaterPlane->LoadHeightMap(pDevice, pCommandList, heightCpuHandle);
            m_pWaterPlane->SetHeightMapSrvGpuHandle(heightGpuHandle);

            // 물 AO 맵 로드 (스타일라이즈드 패턴 깊이감)
            m_pWaterPlane->SetAOMapName("Assets/Stylize Water Texture/Textures/Vol_36_5_Ambient_Occlusion.png");
            D3D12_CPU_DESCRIPTOR_HANDLE aoCpuHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE aoGpuHandle;
            AllocateDescriptor(&aoCpuHandle, &aoGpuHandle);
            m_pWaterPlane->LoadAOMap(pDevice, pCommandList, aoCpuHandle);
            m_pWaterPlane->SetAOMapSrvGpuHandle(aoGpuHandle);

            // 물 Roughness 맵 로드 (날카로운 스펙큘러)
            m_pWaterPlane->SetRoughnessMapName("Assets/Stylize Water Texture/Textures/Vol_36_5_Roughness.png");
            D3D12_CPU_DESCRIPTOR_HANDLE roughCpuHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE roughGpuHandle;
            AllocateDescriptor(&roughCpuHandle, &roughGpuHandle);
            m_pWaterPlane->LoadRoughnessMap(pDevice, pCommandList, roughCpuHandle);
            m_pWaterPlane->SetRoughnessMapSrvGpuHandle(roughGpuHandle);

            // 물 Emissive 맵 로드 (빛나는 물결 효과)
            m_pWaterPlane->SetEmissiveTextureName("Assets/Stylize Water Texture/Textures/Vol_36_5_Emissive.png");
            D3D12_CPU_DESCRIPTOR_HANDLE emissiveCpuHandle;
            D3D12_GPU_DESCRIPTOR_HANDLE emissiveGpuHandle;
            AllocateDescriptor(&emissiveCpuHandle, &emissiveGpuHandle);
            m_pWaterPlane->LoadEmissiveTexture(pDevice, pCommandList, emissiveCpuHandle);
            m_pWaterPlane->SetEmissiveSrvGpuDescriptorHandle(emissiveGpuHandle);
            m_pWaterPlane->SetHasEmissiveTexture(true);

            // ── 추가 물 텍스처 로드 (Water_6 + foam4) ──

            // water_normal_01.png (t7)
            {
                std::wstring normalPath2 = L"Assets/Stylize Water Texture/water_normal_01.png";
                std::unique_ptr<uint8_t[]> decodedData;
                D3D12_SUBRESOURCE_DATA subresource;

                HRESULT hr = DirectX::LoadWICTextureFromFile(pDevice, normalPath2.c_str(), m_pd3dWaterNormal2.GetAddressOf(), decodedData, subresource);
                if (SUCCEEDED(hr))
                {
                    UINT64 nBytes = GetRequiredIntermediateSize(m_pd3dWaterNormal2.Get(), 0, 1);
                    m_pd3dWaterNormal2Upload = CreateBufferResource(pDevice, pCommandList, NULL, nBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, NULL);
                    UpdateSubresources(pCommandList, m_pd3dWaterNormal2.Get(), m_pd3dWaterNormal2Upload.Get(), 0, 0, 1, &subresource);

                    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pd3dWaterNormal2.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    pCommandList->ResourceBarrier(1, &barrier);

                    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
                    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
                    AllocateDescriptor(&cpuHandle, &gpuHandle);

                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    srvDesc.Format = m_pd3dWaterNormal2->GetDesc().Format;
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels = 1;
                    pDevice->CreateShaderResourceView(m_pd3dWaterNormal2.Get(), &srvDesc, cpuHandle);

                    m_d3dWaterNormal2GpuHandle = gpuHandle;  // Store GPU handle
                    OutputDebugString(L"[Scene] water_normal_01.png loaded (t7)\n");
                }
                else
                {
                    OutputDebugString(L"[Scene] Failed to load water_normal_01.png\n");
                }
            }

            // water_height_01.png (t8)
            {
                std::wstring heightPath2 = L"Assets/Stylize Water Texture/water_height_01.png";
                std::unique_ptr<uint8_t[]> decodedData;
                D3D12_SUBRESOURCE_DATA subresource;

                HRESULT hr = DirectX::LoadWICTextureFromFile(pDevice, heightPath2.c_str(), m_pd3dWaterHeight2.GetAddressOf(), decodedData, subresource);
                if (SUCCEEDED(hr))
                {
                    UINT64 nBytes = GetRequiredIntermediateSize(m_pd3dWaterHeight2.Get(), 0, 1);
                    m_pd3dWaterHeight2Upload = CreateBufferResource(pDevice, pCommandList, NULL, nBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, NULL);
                    UpdateSubresources(pCommandList, m_pd3dWaterHeight2.Get(), m_pd3dWaterHeight2Upload.Get(), 0, 0, 1, &subresource);

                    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pd3dWaterHeight2.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    pCommandList->ResourceBarrier(1, &barrier);

                    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
                    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
                    AllocateDescriptor(&cpuHandle, &gpuHandle);

                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    srvDesc.Format = m_pd3dWaterHeight2->GetDesc().Format;
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels = 1;
                    pDevice->CreateShaderResourceView(m_pd3dWaterHeight2.Get(), &srvDesc, cpuHandle);

                    m_d3dWaterHeight2GpuHandle = gpuHandle;  // Store GPU handle
                    OutputDebugString(L"[Scene] water_height_01.png loaded (t8)\n");
                }
                else
                {
                    OutputDebugString(L"[Scene] Failed to load water_height_01.png\n");
                }
            }

            // water_normal_02.png (t9) - detail normal layer
            {
                std::wstring foamOpacityPath = L"Assets/Stylize Water Texture/water_normal_02.png";
                std::unique_ptr<uint8_t[]> decodedData;
                D3D12_SUBRESOURCE_DATA subresource;

                HRESULT hr = DirectX::LoadWICTextureFromFile(pDevice, foamOpacityPath.c_str(), m_pd3dFoamOpacity.GetAddressOf(), decodedData, subresource);
                if (SUCCEEDED(hr))
                {
                    UINT64 nBytes = GetRequiredIntermediateSize(m_pd3dFoamOpacity.Get(), 0, 1);
                    m_pd3dFoamOpacityUpload = CreateBufferResource(pDevice, pCommandList, NULL, nBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, NULL);
                    UpdateSubresources(pCommandList, m_pd3dFoamOpacity.Get(), m_pd3dFoamOpacityUpload.Get(), 0, 0, 1, &subresource);

                    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pd3dFoamOpacity.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    pCommandList->ResourceBarrier(1, &barrier);

                    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
                    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
                    AllocateDescriptor(&cpuHandle, &gpuHandle);

                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    srvDesc.Format = m_pd3dFoamOpacity->GetDesc().Format;
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels = 1;
                    pDevice->CreateShaderResourceView(m_pd3dFoamOpacity.Get(), &srvDesc, cpuHandle);

                    m_d3dFoamOpacityGpuHandle = gpuHandle;  // Store GPU handle
                    OutputDebugString(L"[Scene] water_normal_02.png loaded (t9)\n");
                }
                else
                {
                    OutputDebugString(L"[Scene] Failed to load water_normal_02.png\n");
                }
            }

            // water_height_02.png (t10) - detail height layer
            {
                std::wstring foamDiffusePath = L"Assets/Stylize Water Texture/water_height_02.png";
                std::unique_ptr<uint8_t[]> decodedData;
                D3D12_SUBRESOURCE_DATA subresource;

                HRESULT hr = DirectX::LoadWICTextureFromFile(pDevice, foamDiffusePath.c_str(), m_pd3dFoamDiffuse.GetAddressOf(), decodedData, subresource);
                if (SUCCEEDED(hr))
                {
                    UINT64 nBytes = GetRequiredIntermediateSize(m_pd3dFoamDiffuse.Get(), 0, 1);
                    m_pd3dFoamDiffuseUpload = CreateBufferResource(pDevice, pCommandList, NULL, nBytes, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, NULL);
                    UpdateSubresources(pCommandList, m_pd3dFoamDiffuse.Get(), m_pd3dFoamDiffuseUpload.Get(), 0, 0, 1, &subresource);

                    D3D12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_pd3dFoamDiffuse.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                    pCommandList->ResourceBarrier(1, &barrier);

                    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle;
                    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
                    AllocateDescriptor(&cpuHandle, &gpuHandle);

                    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    srvDesc.Format = m_pd3dFoamDiffuse->GetDesc().Format;
                    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels = 1;
                    pDevice->CreateShaderResourceView(m_pd3dFoamDiffuse.Get(), &srvDesc, cpuHandle);

                    m_d3dFoamDiffuseGpuHandle = gpuHandle;  // Store GPU handle
                    OutputDebugString(L"[Scene] water_height_02.png loaded (t10)\n");
                }
                else
                {
                    OutputDebugString(L"[Scene] Failed to load water_height_02.png\n");
                }
            }

            auto* pRC = m_pWaterPlane->AddComponent<RenderComponent>();
            pRC->SetMesh(pPlaneMesh);
            pRC->SetCastsShadow(false);
            pRC->SetTransparent(true);  // 물 투명 렌더링 활성화
            m_vShaders[0]->AddRenderComponent(pRC);
        }
        OutputDebugString(L"[Scene] Water floor plane placed (with water_normal_01/02 + water_height_01/02)\n");
    }

    // ── 9. 맵 정적 오브젝트 상수 버퍼 초기화
    if (m_pCurrentRoom)
    {
        for (const auto& pGO : m_pCurrentRoom->GetGameObjects())
            pGO->Update(0.0f);

        // 룸 활성화 (적 스폰)
        m_pCurrentRoom->SetState(RoomState::Active);
    }

    // ── 10. 인터랙션 큐브 숨김
    if (m_pInteractionCube)
    {
        auto* pInteractable = m_pInteractionCube->GetComponent<InteractableComponent>();
        if (pInteractable) pInteractable->Hide();
        m_bInteractionCubeActive = false;
    }

    // ── 11. 플레이어 groundY 리셋
    if (m_pPlayerGameObject)
    {
        auto* pPC = m_pPlayerGameObject->GetComponent<PlayerComponent>();
        if (pPC) pPC->ResetGroundY();
    }

    // ── 12. Gerstner Waves (균형: 뾰족하지 않으면서 명확한 파도)
    if (m_pcbMappedPass)
    {
        // Wave 1: 메인 파동 (큰 파도)
        m_pcbMappedPass->m_Waves[0].m_fWavelength = 70.0f;
        m_pcbMappedPass->m_Waves[0].m_fAmplitude = 10.0f;      // 6 → 10
        m_pcbMappedPass->m_Waves[0].m_fSteepness = 0.35f;
        m_pcbMappedPass->m_Waves[0].m_fSpeed = 4.5f;
        m_pcbMappedPass->m_Waves[0].m_xmf2Direction = XMFLOAT2(1.0f, 0.3f);
        m_pcbMappedPass->m_Waves[0].m_fFadeSpeed = 0.1f;

        // Wave 2: 부 파동 (교차)
        m_pcbMappedPass->m_Waves[1].m_fWavelength = 45.0f;
        m_pcbMappedPass->m_Waves[1].m_fAmplitude = 6.5f;       // 4 → 6.5
        m_pcbMappedPass->m_Waves[1].m_fSteepness = 0.3f;
        m_pcbMappedPass->m_Waves[1].m_fSpeed = 6.0f;
        m_pcbMappedPass->m_Waves[1].m_xmf2Direction = XMFLOAT2(-0.7f, 0.7f);
        m_pcbMappedPass->m_Waves[1].m_fFadeSpeed = 0.12f;

        // Wave 3: 중간 파동 (복잡도 추가)
        m_pcbMappedPass->m_Waves[2].m_fWavelength = 28.0f;
        m_pcbMappedPass->m_Waves[2].m_fAmplitude = 4.0f;       // 2.5 → 4
        m_pcbMappedPass->m_Waves[2].m_fSteepness = 0.28f;
        m_pcbMappedPass->m_Waves[2].m_fSpeed = 8.0f;
        m_pcbMappedPass->m_Waves[2].m_xmf2Direction = XMFLOAT2(0.6f, -0.8f);
        m_pcbMappedPass->m_Waves[2].m_fFadeSpeed = 0.15f;

        // Wave 4-5: 작은 디테일
        m_pcbMappedPass->m_Waves[3].m_fWavelength = 30.0f;
        m_pcbMappedPass->m_Waves[3].m_fAmplitude = 1.8f;       // 1.0 → 1.8
        m_pcbMappedPass->m_Waves[3].m_fSteepness = 0.2f;
        m_pcbMappedPass->m_Waves[3].m_fSpeed = 9.0f;
        m_pcbMappedPass->m_Waves[3].m_xmf2Direction = XMFLOAT2(0.5f, 0.9f);
        m_pcbMappedPass->m_Waves[3].m_fFadeSpeed = 0.0f;

        m_pcbMappedPass->m_Waves[4].m_fWavelength = 22.0f;
        m_pcbMappedPass->m_Waves[4].m_fAmplitude = 1.0f;       // 0.6 → 1.0
        m_pcbMappedPass->m_Waves[4].m_fSteepness = 0.18f;
        m_pcbMappedPass->m_Waves[4].m_fSpeed = 11.0f;
        m_pcbMappedPass->m_Waves[4].m_xmf2Direction = XMFLOAT2(-0.9f, 0.4f);
        m_pcbMappedPass->m_Waves[4].m_fFadeSpeed = 0.0f;

        OutputDebugString(L"[Scene] Balanced ocean waves (visible + natural)\n");
    }

    // ── 13. 장식용 터레인 로드 (파일이 있으면)
    {
        const char* terrainConfig = "Assets/Terrain/terrain_config.json";
        // 파일 존재 여부 확인 후 로드
        FILE* f = nullptr;
        fopen_s(&f, terrainConfig, "r");
        if (f)
        {
            fclose(f);
            LoadTerrain(terrainConfig, 4);
        }
        else
        {
            OutputDebugString(L"[Scene] terrain_config.json not found – skipping terrain load\n");
        }
    }

    OutputDebugString(L"[Scene] Water stage ready!\n");
}

void Scene::TransitionToWaterBossRoom()
{
    OutputDebugString(L"[Scene] ========== WATER BOSS ROOM (KRAKEN) ==========\n");

    // ── 1. 테마를 Water로 유지
    m_eCurrentTheme = StageTheme::Water;

    // ── 2. 셰이더 RC 목록 전체 클리어
    m_vShaders[0]->ClearRenderComponents();
    ProcessPendingDeletions();

    // ── 3. 기존 룸 전체 파기
    m_vRooms.clear();
    m_pCurrentRoom = nullptr;

    // ── 4. 디스크립터 인덱스를 워터마크로 리셋
    m_nNextDescriptorIndex = m_nPersistentDescriptorEnd;

    // ── 5. 횃불 시스템 클리어
    if (m_pTorchSystem) m_pTorchSystem->Clear();

    // ── 6. 용암 평면 숨기기
    if (m_pLavaPlane)
        m_pLavaPlane->GetTransform()->SetPosition(0.0f, -10000.0f, 0.0f);

    // ── 7. 물 평면 제거 (이전 물 평면을 m_vGameObjects에서 삭제)
    GameObject* pOldWaterPlane = m_pWaterPlane;
    m_pWaterPlane = nullptr;

    // ── 8. 영속 오브젝트 RC 재등록 (용암, 이전 물 평면 제외)
    for (auto& pGO : m_vGameObjects)
    {
        if (pGO.get() != m_pLavaPlane && pGO.get() != pOldWaterPlane)
            ReAddRenderComponentsToShader(pGO.get());
    }

    // 이전 물 평면을 m_vGameObjects에서 제거 (고아 RC 방지)
    if (pOldWaterPlane)
    {
        m_vGameObjects.erase(
            std::remove_if(m_vGameObjects.begin(), m_vGameObjects.end(),
                [pOldWaterPlane](const std::unique_ptr<GameObject>& p) { return p.get() == pOldWaterPlane; }),
            m_vGameObjects.end());
    }

    ID3D12Device*              pDevice      = Dx12App::GetInstance()->GetDevice();
    ID3D12GraphicsCommandList* pCommandList = Dx12App::GetInstance()->GetCommandList();

    // ── 9. 물 맵 로드 (첫 번째 맵 풀 사용)
    if (!m_vMapPool.empty())
        m_strCurrentMap = m_vMapPool[0];
    bool bLoaded = MapLoader::LoadIntoScene(
        m_strCurrentMap.c_str(), this, pDevice, pCommandList, m_vShaders[0].get());
    if (!bLoaded)
    {
        OutputDebugString(L"[Scene] Water boss map load failed!\n");
        return;
    }

    // ── 10. 물 바닥 평면 생성 (TransitionToWaterStage와 동일)
    {
        CRoom* pTempRoom = m_pCurrentRoom;
        m_pCurrentRoom = nullptr;

        m_pWaterPlane = CreateGameObject(pDevice, pCommandList);
        m_pCurrentRoom = pTempRoom;

        if (m_pWaterPlane)
        {
            GridPlaneMesh* pPlaneMesh = new GridPlaneMesh(pDevice, pCommandList, 1.0f, 1.0f, 256, 256);
            m_pWaterPlane->SetMesh(pPlaneMesh);
            m_pWaterPlane->GetTransform()->SetPosition(0.0f, -1.0f, -200.0f);
            m_pWaterPlane->GetTransform()->SetScale(2000.0f, 1.0f, 2000.0f);
            m_pWaterPlane->SetWater(true);

            MATERIAL waterMat;
            waterMat.m_cAmbient  = XMFLOAT4(0.1f, 0.15f, 0.25f, 1.0f);
            waterMat.m_cDiffuse  = XMFLOAT4(0.8f, 0.9f, 1.0f, 1.0f);
            waterMat.m_cSpecular = XMFLOAT4(0.95f, 0.95f, 0.95f, 64.0f);
            waterMat.m_cEmissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
            m_pWaterPlane->SetMaterial(waterMat);

            m_pWaterPlane->SetTextureName("Assets/Stylize Water Texture/Textures/Vol_36_5_Base_Color.png");
            D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle; D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle;
            AllocateDescriptor(&cpuHandle, &gpuHandle);
            m_pWaterPlane->LoadTexture(pDevice, pCommandList, cpuHandle);
            m_pWaterPlane->SetSrvGpuDescriptorHandle(gpuHandle);

            m_pWaterPlane->SetNormalMapName("Assets/Stylize Water Texture/Textures/Vol_36_5_Normal.png");
            D3D12_CPU_DESCRIPTOR_HANDLE normalCpu; D3D12_GPU_DESCRIPTOR_HANDLE normalGpu;
            AllocateDescriptor(&normalCpu, &normalGpu);
            m_pWaterPlane->LoadNormalMap(pDevice, pCommandList, normalCpu);
            m_pWaterPlane->SetNormalMapSrvGpuHandle(normalGpu);

            m_pWaterPlane->SetHeightMapName("Assets/Stylize Water Texture/Textures/Vol_36_5_Height.png");
            D3D12_CPU_DESCRIPTOR_HANDLE heightCpu; D3D12_GPU_DESCRIPTOR_HANDLE heightGpu;
            AllocateDescriptor(&heightCpu, &heightGpu);
            m_pWaterPlane->LoadHeightMap(pDevice, pCommandList, heightCpu);
            m_pWaterPlane->SetHeightMapSrvGpuHandle(heightGpu);

            auto* pRC = m_pWaterPlane->AddComponent<RenderComponent>();
            pRC->SetMesh(pPlaneMesh);
            pRC->SetCastsShadow(false);
            pRC->SetTransparent(true);
            m_vShaders[0]->AddRenderComponent(pRC);
        }
        OutputDebugString(L"[Scene] Water boss room floor placed\n");
    }

    // ── 11. 맵 정적 오브젝트 상수 버퍼 초기화
    if (m_pCurrentRoom)
    {
        for (const auto& pGO : m_pCurrentRoom->GetGameObjects())
            pGO->Update(0.0f);
    }

    // ── 12. 2페이즈 보스 스폰: Phase 1 = Blue Dragon, Phase 2 = Kraken (pre-loaded hidden)
    m_pPreloadedKraken = nullptr;
    m_bPendingKrakenSpawn = false;
    m_bKrakenEmerging = false;

    if (m_pCurrentRoom && m_pEnemySpawner)
    {
        RoomSpawnConfig emptyConfig;
        m_pCurrentRoom->SetSpawnConfig(emptyConfig);

        XMFLOAT3 bossPos = XMFLOAT3(0.0f, 0.0f, 20.0f);
        if (m_pPlayerGameObject)
        {
            XMFLOAT3 playerPos = m_pPlayerGameObject->GetTransform()->GetPosition();
            bossPos = XMFLOAT3(playerPos.x, playerPos.y, playerPos.z + 20.0f);
        }

        // Pre-spawn Kraken hidden underground (no target) to avoid mid-combat GPU upload lag
        XMFLOAT3 hidePos = XMFLOAT3(bossPos.x, -500.0f, bossPos.z);
        GameObject* pKraken = m_pEnemySpawner->SpawnEnemy(m_pCurrentRoom, "Kraken", hidePos, nullptr);
        if (pKraken)
        {
            pKraken->GetTransform()->SetScale(0.05f, 0.05f, 0.05f);
            m_pPreloadedKraken = pKraken->GetComponent<EnemyComponent>();
        }

        OutputDebugString(L"[Scene] Spawning Blue Dragon (Phase 1)\n");
        GameObject* pDragon = m_pEnemySpawner->SpawnEnemy(m_pCurrentRoom, "BlueDragon", bossPos, m_pPlayerGameObject);

        if (pDragon)
        {
            EnemyComponent* pDragonEnemy = pDragon->GetComponent<EnemyComponent>();
            if (pDragonEnemy)
            {
                pDragonEnemy->StartBossIntro(5.0f);

                CRoom* pRoom = m_pCurrentRoom;
                pDragonEnemy->SetOnDeathCallback([this, pRoom](EnemyComponent* pDeadEnemy)
                {
                    if (pRoom)
                        pRoom->OnEnemyDeath(pDeadEnemy);

                    OutputDebugString(L"[Scene] Blue Dragon defeated - Kraken emerges! (Phase 2)\n");
                    m_xmf3PendingKrakenPos = { 0.0f, 0.0f, 0.0f };
                    if (pDeadEnemy && pDeadEnemy->GetOwner())
                        m_xmf3PendingKrakenPos = pDeadEnemy->GetOwner()->GetTransform()->GetPosition();
                    m_bPendingKrakenSpawn = true;
                });
            }
        }

        m_pCurrentRoom->SetState(RoomState::Active);
    }

    // ── 13. 인터랙션 큐브 숨김
    if (m_pInteractionCube)
    {
        auto* pInteractable = m_pInteractionCube->GetComponent<InteractableComponent>();
        if (pInteractable) pInteractable->Hide();
        m_bInteractionCubeActive = false;
    }

    // ── 14. 플레이어 groundY 리셋
    if (m_pPlayerGameObject)
    {
        auto* pPC = m_pPlayerGameObject->GetComponent<PlayerComponent>();
        if (pPC) pPC->ResetGroundY();
    }

    // ── 15. Gerstner Waves (보스전용 거친 파도)
    if (m_pcbMappedPass)
    {
        m_pcbMappedPass->m_Waves[0].m_fWavelength = 70.0f;
        m_pcbMappedPass->m_Waves[0].m_fAmplitude  = 12.0f;
        m_pcbMappedPass->m_Waves[0].m_fSteepness  = 0.4f;
        m_pcbMappedPass->m_Waves[0].m_fSpeed      = 5.0f;
        m_pcbMappedPass->m_Waves[0].m_xmf2Direction = XMFLOAT2(1.0f, 0.3f);
        m_pcbMappedPass->m_Waves[0].m_fFadeSpeed  = 0.1f;

        m_pcbMappedPass->m_Waves[1].m_fWavelength = 45.0f;
        m_pcbMappedPass->m_Waves[1].m_fAmplitude  = 8.0f;
        m_pcbMappedPass->m_Waves[1].m_fSteepness  = 0.35f;
        m_pcbMappedPass->m_Waves[1].m_fSpeed      = 7.0f;
        m_pcbMappedPass->m_Waves[1].m_xmf2Direction = XMFLOAT2(-0.7f, 0.7f);
        m_pcbMappedPass->m_Waves[1].m_fFadeSpeed  = 0.12f;

        m_pcbMappedPass->m_Waves[2].m_fWavelength = 28.0f;
        m_pcbMappedPass->m_Waves[2].m_fAmplitude  = 5.0f;
        m_pcbMappedPass->m_Waves[2].m_fSteepness  = 0.3f;
        m_pcbMappedPass->m_Waves[2].m_fSpeed      = 9.0f;
        m_pcbMappedPass->m_Waves[2].m_xmf2Direction = XMFLOAT2(0.6f, -0.8f);
        m_pcbMappedPass->m_Waves[2].m_fFadeSpeed  = 0.15f;

        m_pcbMappedPass->m_Waves[3].m_fWavelength = 30.0f;
        m_pcbMappedPass->m_Waves[3].m_fAmplitude  = 2.5f;
        m_pcbMappedPass->m_Waves[3].m_fSteepness  = 0.25f;
        m_pcbMappedPass->m_Waves[3].m_fSpeed      = 10.0f;
        m_pcbMappedPass->m_Waves[3].m_xmf2Direction = XMFLOAT2(0.5f, 0.9f);
        m_pcbMappedPass->m_Waves[3].m_fFadeSpeed  = 0.0f;

        m_pcbMappedPass->m_Waves[4].m_fWavelength = 22.0f;
        m_pcbMappedPass->m_Waves[4].m_fAmplitude  = 1.5f;
        m_pcbMappedPass->m_Waves[4].m_fSteepness  = 0.2f;
        m_pcbMappedPass->m_Waves[4].m_fSpeed      = 12.0f;
        m_pcbMappedPass->m_Waves[4].m_xmf2Direction = XMFLOAT2(-0.9f, 0.4f);
        m_pcbMappedPass->m_Waves[4].m_fFadeSpeed  = 0.0f;
    }

    m_bInBossRoom = true;
    OutputDebugString(L"[Scene] Water boss room ready - Kraken spawned!\n");
}

void Scene::TransitionToEarthStage()
{
    OutputDebugString(L"[Scene] ========== EARTH STAGE ==========\n");

    m_eCurrentTheme = StageTheme::Earth;
    m_bInBossRoom = false;

    m_vShaders[0]->ClearRenderComponents();
    ProcessPendingDeletions();
    m_vRooms.clear();
    m_pCurrentRoom = nullptr;
    m_nNextDescriptorIndex = m_nPersistentDescriptorEnd;
    if (m_pTorchSystem) m_pTorchSystem->Clear();

    // 용암·물 바닥 숨기기
    if (m_pLavaPlane)  m_pLavaPlane->GetTransform()->SetPosition(0.0f, -10000.0f, 0.0f);
    if (m_pWaterPlane) m_pWaterPlane->GetTransform()->SetPosition(0.0f, -10000.0f, 0.0f);

    for (auto& pGO : m_vGameObjects)
    {
        if (pGO.get() != m_pLavaPlane && pGO.get() != m_pWaterPlane)
            ReAddRenderComponentsToShader(pGO.get());
    }

    ID3D12Device*              pDevice      = Dx12App::GetInstance()->GetDevice();
    ID3D12GraphicsCommandList* pCommandList = Dx12App::GetInstance()->GetCommandList();

    if (!m_vMapPool.empty()) m_strCurrentMap = m_vMapPool[0];
    bool bLoaded = MapLoader::LoadIntoScene(
        m_strCurrentMap.c_str(), this, pDevice, pCommandList, m_vShaders[0].get());
    if (!bLoaded) { OutputDebugString(L"[Scene] Earth stage map load failed!\n"); return; }

    if (m_pCurrentRoom) {
        for (const auto& pGO : m_pCurrentRoom->GetGameObjects()) pGO->Update(0.0f);
        m_pCurrentRoom->SetState(RoomState::Active);
    }

    if (m_pInteractionCube) {
        auto* pI = m_pInteractionCube->GetComponent<InteractableComponent>();
        if (pI) pI->Hide();
        m_bInteractionCubeActive = false;
    }
    if (m_pPlayerGameObject) {
        auto* pPC = m_pPlayerGameObject->GetComponent<PlayerComponent>();
        if (pPC) pPC->ResetGroundY();
    }

    // Gerstner Wave를 꺼줌 (땅 맵엔 물 없음)
    if (m_pcbMappedPass)
        for (int i = 0; i < 5; i++) m_pcbMappedPass->m_Waves[i].m_fAmplitude = 0.0f;

    OutputDebugString(L"[Scene] Earth stage ready!\n");
}

void Scene::TransitionToGrassStage()
{
    OutputDebugString(L"[Scene] ========== GRASS STAGE ==========\n");

    m_eCurrentTheme = StageTheme::Grass;
    m_bInBossRoom = false;

    m_vShaders[0]->ClearRenderComponents();
    ProcessPendingDeletions();
    m_vRooms.clear();
    m_pCurrentRoom = nullptr;
    m_nNextDescriptorIndex = m_nPersistentDescriptorEnd;
    if (m_pTorchSystem) m_pTorchSystem->Clear();

    if (m_pLavaPlane)  m_pLavaPlane->GetTransform()->SetPosition(0.0f, -10000.0f, 0.0f);
    if (m_pWaterPlane) m_pWaterPlane->GetTransform()->SetPosition(0.0f, -10000.0f, 0.0f);

    for (auto& pGO : m_vGameObjects)
    {
        if (pGO.get() != m_pLavaPlane && pGO.get() != m_pWaterPlane)
            ReAddRenderComponentsToShader(pGO.get());
    }

    ID3D12Device*              pDevice      = Dx12App::GetInstance()->GetDevice();
    ID3D12GraphicsCommandList* pCommandList = Dx12App::GetInstance()->GetCommandList();

    if (!m_vMapPool.empty()) m_strCurrentMap = m_vMapPool[0];
    bool bLoaded = MapLoader::LoadIntoScene(
        m_strCurrentMap.c_str(), this, pDevice, pCommandList, m_vShaders[0].get());
    if (!bLoaded) { OutputDebugString(L"[Scene] Grass stage map load failed!\n"); return; }

    if (m_pCurrentRoom) {
        for (const auto& pGO : m_pCurrentRoom->GetGameObjects()) pGO->Update(0.0f);
        m_pCurrentRoom->SetState(RoomState::Active);
    }

    if (m_pInteractionCube) {
        auto* pI = m_pInteractionCube->GetComponent<InteractableComponent>();
        if (pI) pI->Hide();
        m_bInteractionCubeActive = false;
    }
    if (m_pPlayerGameObject) {
        auto* pPC = m_pPlayerGameObject->GetComponent<PlayerComponent>();
        if (pPC) pPC->ResetGroundY();
    }

    if (m_pcbMappedPass)
        for (int i = 0; i < 5; i++) m_pcbMappedPass->m_Waves[i].m_fAmplitude = 0.0f;

    OutputDebugString(L"[Scene] Grass stage ready!\n");
}

void Scene::TransitionToEarthBossRoom()
{
    OutputDebugString(L"[Scene] ========== EARTH BOSS ROOM (GOLEM) ==========\n");

    m_eCurrentTheme = StageTheme::Earth;

    m_vShaders[0]->ClearRenderComponents();
    ProcessPendingDeletions();
    m_vRooms.clear();
    m_pCurrentRoom = nullptr;
    m_nNextDescriptorIndex = m_nPersistentDescriptorEnd;
    if (m_pTorchSystem) m_pTorchSystem->Clear();

    if (m_pLavaPlane)  m_pLavaPlane->GetTransform()->SetPosition(0.0f, -10000.0f, 0.0f);
    if (m_pWaterPlane) m_pWaterPlane->GetTransform()->SetPosition(0.0f, -10000.0f, 0.0f);

    for (auto& pGO : m_vGameObjects)
    {
        if (pGO.get() != m_pLavaPlane && pGO.get() != m_pWaterPlane)
            ReAddRenderComponentsToShader(pGO.get());
    }

    ID3D12Device*              pDevice      = Dx12App::GetInstance()->GetDevice();
    ID3D12GraphicsCommandList* pCommandList = Dx12App::GetInstance()->GetCommandList();

    if (!m_vMapPool.empty()) m_strCurrentMap = m_vMapPool[0];
    bool bLoaded = MapLoader::LoadIntoScene(
        m_strCurrentMap.c_str(), this, pDevice, pCommandList, m_vShaders[0].get());
    if (!bLoaded) { OutputDebugString(L"[Scene] Earth boss map load failed!\n"); return; }

    if (m_pCurrentRoom)
        for (const auto& pGO : m_pCurrentRoom->GetGameObjects()) pGO->Update(0.0f);

    if (m_pCurrentRoom && m_pEnemySpawner)
    {
        RoomSpawnConfig emptyConfig;
        m_pCurrentRoom->SetSpawnConfig(emptyConfig);

        OutputDebugString(L"[Scene] Spawning Golem boss\n");
        XMFLOAT3 golemPos = XMFLOAT3(0.0f, 0.0f, 25.0f);
        if (m_pPlayerGameObject)
        {
            XMFLOAT3 p = m_pPlayerGameObject->GetTransform()->GetPosition();
            golemPos = XMFLOAT3(p.x, p.y, p.z + 25.0f);
        }

        GameObject* pGolem = m_pEnemySpawner->SpawnEnemy(m_pCurrentRoom, "Golem", golemPos, m_pPlayerGameObject);
        if (pGolem)
        {
            EnemyComponent* pEnemy = pGolem->GetComponent<EnemyComponent>();
            if (pEnemy) pEnemy->StartBossIntro(3.0f);
        }

        m_pCurrentRoom->SetState(RoomState::Active);
    }

    if (m_pInteractionCube) {
        auto* pI = m_pInteractionCube->GetComponent<InteractableComponent>();
        if (pI) pI->Hide();
        m_bInteractionCubeActive = false;
    }
    if (m_pPlayerGameObject) {
        auto* pPC = m_pPlayerGameObject->GetComponent<PlayerComponent>();
        if (pPC) pPC->ResetGroundY();
    }
    if (m_pcbMappedPass)
        for (int i = 0; i < 5; i++) m_pcbMappedPass->m_Waves[i].m_fAmplitude = 0.0f;

    m_bInBossRoom = true;
    OutputDebugString(L"[Scene] Earth boss room ready - Golem spawned!\n");
}

void Scene::TransitionToGrassBossRoom()
{
    OutputDebugString(L"[Scene] ========== GRASS BOSS ROOM (DEMON) ==========\n");

    m_eCurrentTheme = StageTheme::Grass;

    m_vShaders[0]->ClearRenderComponents();
    ProcessPendingDeletions();
    m_vRooms.clear();
    m_pCurrentRoom = nullptr;
    m_nNextDescriptorIndex = m_nPersistentDescriptorEnd;
    if (m_pTorchSystem) m_pTorchSystem->Clear();

    if (m_pLavaPlane)  m_pLavaPlane->GetTransform()->SetPosition(0.0f, -10000.0f, 0.0f);
    if (m_pWaterPlane) m_pWaterPlane->GetTransform()->SetPosition(0.0f, -10000.0f, 0.0f);

    for (auto& pGO : m_vGameObjects)
    {
        if (pGO.get() != m_pLavaPlane && pGO.get() != m_pWaterPlane)
            ReAddRenderComponentsToShader(pGO.get());
    }

    ID3D12Device*              pDevice      = Dx12App::GetInstance()->GetDevice();
    ID3D12GraphicsCommandList* pCommandList = Dx12App::GetInstance()->GetCommandList();

    if (!m_vMapPool.empty()) m_strCurrentMap = m_vMapPool[0];
    bool bLoaded = MapLoader::LoadIntoScene(
        m_strCurrentMap.c_str(), this, pDevice, pCommandList, m_vShaders[0].get());
    if (!bLoaded) { OutputDebugString(L"[Scene] Grass boss map load failed!\n"); return; }

    if (m_pCurrentRoom)
        for (const auto& pGO : m_pCurrentRoom->GetGameObjects()) pGO->Update(0.0f);

    if (m_pCurrentRoom && m_pEnemySpawner)
    {
        RoomSpawnConfig emptyConfig;
        m_pCurrentRoom->SetSpawnConfig(emptyConfig);

        OutputDebugString(L"[Scene] Spawning Demon boss\n");
        XMFLOAT3 demonPos = XMFLOAT3(0.0f, 0.0f, 20.0f);
        if (m_pPlayerGameObject)
        {
            XMFLOAT3 p = m_pPlayerGameObject->GetTransform()->GetPosition();
            demonPos = XMFLOAT3(p.x, p.y, p.z + 20.0f);
        }

        GameObject* pDemon = m_pEnemySpawner->SpawnEnemy(m_pCurrentRoom, "Demon", demonPos, m_pPlayerGameObject);
        if (pDemon)
        {
            EnemyComponent* pEnemy = pDemon->GetComponent<EnemyComponent>();
            if (pEnemy) pEnemy->StartBossIntro(4.0f);
        }

        m_pCurrentRoom->SetState(RoomState::Active);
    }

    if (m_pInteractionCube) {
        auto* pI = m_pInteractionCube->GetComponent<InteractableComponent>();
        if (pI) pI->Hide();
        m_bInteractionCubeActive = false;
    }
    if (m_pPlayerGameObject) {
        auto* pPC = m_pPlayerGameObject->GetComponent<PlayerComponent>();
        if (pPC) pPC->ResetGroundY();
    }
    if (m_pcbMappedPass)
        for (int i = 0; i < 5; i++) m_pcbMappedPass->m_Waves[i].m_fAmplitude = 0.0f;

    m_bInBossRoom = true;
    OutputDebugString(L"[Scene] Grass boss room ready - Demon spawned!\n");
}