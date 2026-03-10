#include "stdafx.h"
#include "Scene.h"
#include "RenderComponent.h"
#include "Shader.h"
#include "RotatorComponent.h"
#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "InputSystem.h" // Added for InputSystem
#include "PlayerComponent.h"
#include "AnimationComponent.h"
#include "CollisionManager.h"
#include "CollisionLayer.h"
#include "SkillComponent.h"
#include "FireballBehavior.h"
#include "ProjectileManager.h"
#include "DropItemComponent.h"
#include "InteractableComponent.h"
#include "MathUtils.h"
#include <functional> // Added for std::function

Scene::Scene()
{
    m_pCamera = std::make_unique<CCamera>();
    m_pCollisionManager = std::make_unique<CollisionManager>();
    m_pEnemySpawner = std::make_unique<EnemySpawner>();
    m_pProjectileManager = std::make_unique<ProjectileManager>();
    m_pParticleSystem = std::make_unique<ParticleSystem>();
    m_pDebugRenderer = std::make_unique<DebugRenderer>();
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
    m_pDescriptorHeap->Create(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 2048, true);

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

        // Equip Fireball to Q slot
        auto fireballQ = std::make_unique<FireballBehavior>();
        fireballQ->SetProjectileManager(m_pProjectileManager.get());
        pSkillComponent->EquipSkill(SkillSlot::Q, std::move(fireballQ));

        // Also equip Fireball to RightClick slot for testing
        auto fireballRClick = std::make_unique<FireballBehavior>();
        fireballRClick->SetProjectileManager(m_pProjectileManager.get());
        pSkillComponent->EquipSkill(SkillSlot::RightClick, std::move(fireballRClick));

        OutputDebugString(L"[Scene] Skill system initialized - Fireball equipped to Q and RightClick\n");

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
    // 영속 디스크립터 워터마크 기록
    // 이 시점 이후의 슬롯(맵 오브젝트·적·포탈 등)은 맵 전환 시 재활용됩니다.
    // --------------------------------------------------------------------------
    m_nPersistentDescriptorEnd = m_nNextDescriptorIndex;

    // --------------------------------------------------------------------------
    // Map pool – 여기에 맵 JSON 경로를 추가하세요
    // --------------------------------------------------------------------------
    m_vMapPool.push_back("Assets/MapData/map.json");
    // m_vMapPool.push_back("Assets/MapData/map2.json");
    // m_vMapPool.push_back("Assets/MapData/map3.json");

    // --------------------------------------------------------------------------
    // Load map from JSON (recyclable slots from m_nPersistentDescriptorEnd onward)
    // --------------------------------------------------------------------------
    m_strCurrentMap = m_vMapPool[0];
    bool bMapLoaded = MapLoader::LoadIntoScene(
        m_strCurrentMap.c_str(), this, pDevice, pCommandList, m_vShaders[0].get());

    if (!bMapLoaded) {
        OutputDebugString(L"[Scene] Map load failed – using default test room\n");
        RoomSpawnConfig spawnConfig;
        spawnConfig.AddSpawn("RushAoEEnemy",  10.0f, 0.0f,  5.0f);
        spawnConfig.AddSpawn("RushFrontEnemy",-10.0f, 0.0f,  5.0f);
        spawnConfig.AddSpawn("RangedEnemy",    0.0f, 0.0f, 20.0f);
        m_pCurrentRoom->SetSpawnConfig(spawnConfig);
        m_pCurrentRoom->SetEnemySpawner(m_pEnemySpawner.get());
        m_pCurrentRoom->SetPlayerTarget(m_pPlayerGameObject);
        m_pCurrentRoom->SetScene(this);
    }

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
    // Toggle debug collider visualization with F1
    if (pInputSystem && pInputSystem->IsKeyPressed(VK_F1))
    {
        m_pDebugRenderer->Toggle();
        OutputDebugString(m_pDebugRenderer->IsEnabled() ? L"[Debug] Colliders ON\n" : L"[Debug] Colliders OFF\n");
    }

    // Update camera
    if (m_pCamera && pInputSystem)
    {
        m_pCamera->Update(pInputSystem->GetMouseDeltaX(), pInputSystem->GetMouseDeltaY(), pInputSystem->GetMouseWheelDelta());
    }

    // Update Pass Constants
    XMMATRIX mView = XMLoadFloat4x4(&m_pCamera->GetViewMatrix());
    XMMATRIX mProjection = XMLoadFloat4x4(&m_pCamera->GetProjectionMatrix());
    XMMATRIX mViewProj = mView * mProjection;
    XMStoreFloat4x4(&m_pcbMappedPass->m_xmf4x4ViewProj, XMMatrixTranspose(mViewProj));

    // Set lighting parameters for a more realistic look
    m_pcbMappedPass->m_xmf4LightColor = XMFLOAT4(0.9f, 0.85f, 0.75f, 1.0f); // Slightly warm white directional light (sun)
    XMVECTOR lightDir = XMVector3Normalize(XMVectorSet(-0.6f, -0.7f, 0.3f, 0.0f)); // 비스듬한 조명 (옆으로 긴 그림자)
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

    m_pcbMappedPass->m_xmf4AmbientLight = XMFLOAT4(0.05f, 0.07f, 0.1f, 1.0f); // Darker, slightly bluish ambient (indirect sky light)

    // Set Camera Position for Specular Calculation
    XMFLOAT3 cameraPosition = m_pCamera->GetPosition();
    m_pcbMappedPass->m_xmf3CameraPosition = cameraPosition;
    m_pcbMappedPass->m_fPadCam = 0.0f; // Padding

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
    }

    // Update player specific logic
    if (m_pPlayerGameObject && m_pPlayerGameObject->GetComponent<PlayerComponent>())
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
        m_pParticleSystem->Update(deltaTime);
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

void Scene::Render(ID3D12GraphicsCommandList* pCommandList, D3D12_GPU_DESCRIPTOR_HANDLE shadowSrvHandle)
{
    // Set the descriptor heap
    ID3D12DescriptorHeap* ppHeaps[] = { m_pDescriptorHeap->GetHeap() };
    pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // Note: UpdateRenderList() is already called in RenderShadowPass() before this

    // Iterate through shaders (groups) and render
    for (auto& shader : m_vShaders)
    {
        shader->Render(pCommandList, GetPassCBVAddress(), shadowSrvHandle);
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

GameObject* Scene::CreateGameObject(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    // Note: We use raw pointer here, but ownership is transferred to unique_ptr below
    GameObject* newGameObject = new GameObject();

    UINT slot = m_nNextDescriptorIndex;
    bool bRecyclable = (slot >= m_nPersistentDescriptorEnd);

    auto cacheIt = bRecyclable ? m_vCBCache.find(slot) : m_vCBCache.end();
    if (cacheIt != m_vCBCache.end())
    {
        // 같은 슬롯 번호에 이미 생성된 리소스 재사용
        // CBV는 힙에 그대로 → CreateConstantBufferView 불필요
        ObjectConstants* pMapped = nullptr;
        cacheIt->second->Map(0, nullptr, (void**)&pMapped);
        newGameObject->ReuseConstantBuffer(cacheIt->second, pMapped);
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

    const wchar_t* typeNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance" };
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
            const wchar_t* typeNames[] = { L"None", L"Instant", L"Charge", L"Channel", L"Place", L"Enhance" };
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