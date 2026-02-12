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
}

Scene::~Scene()
{
    if (m_pd3dcbPass) m_pd3dcbPass->Unmap(0, NULL);
}

#include "MeshLoader.h"

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
    // 1. Create Room
    // --------------------------------------------------------------------------
    auto pRoom = std::make_unique<CRoom>();
    pRoom->SetState(RoomState::Inactive);  // Start inactive, activate on interaction
    pRoom->SetBoundingBox(BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(100.0f, 100.0f, 100.0f)));
    m_vRooms.push_back(std::move(pRoom));


    // --------------------------------------------------------------------------
    // 2. Load Global Objects (Player)
    // --------------------------------------------------------------------------
    m_pCurrentRoom = nullptr; 

    GameObject* pPlayer = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Assets/Player/Models/Vampire A Lusth.bin");
    if (pPlayer)
    {
        OutputDebugString(L"Player model loaded successfully!\n");
        pPlayer->GetTransform()->SetPosition(0.0f, 0.0f, 0.0f);
        pPlayer->GetTransform()->SetScale(5.0f, 5.0f, 5.0f);
        pPlayer->AddComponent<PlayerComponent>();
        m_pPlayerGameObject = pPlayer;

        // Add Animation Component
        pPlayer->AddComponent<AnimationComponent>();
        pPlayer->GetComponent<AnimationComponent>()->LoadAnimation("Assets/Player/Animations/Walking_Anim.bin");
        pPlayer->GetComponent<AnimationComponent>()->Play("mixamo.com");

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

        AddRenderComponentsToHierarchy(pDevice, pCommandList, pPlayer, pShader.get());
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
    // Initialize Enemy Spawner and configure room spawns
    // --------------------------------------------------------------------------
    m_pEnemySpawner->Init(pDevice, pCommandList, this, pShader.get());

    // Configure spawn points for the room (diverse enemy types)
    RoomSpawnConfig spawnConfig;
    spawnConfig.AddSpawn("RushAoEEnemy", 10.0f, 0.0f, 5.0f);     // Red - right side (melee rush)
    spawnConfig.AddSpawn("RushFrontEnemy", -10.0f, 0.0f, 5.0f);  // Green - left side (melee rush)
    spawnConfig.AddSpawn("RangedEnemy", 0.0f, 0.0f, 20.0f);      // Blue - far back (ranged)

    // Apply configuration to room
    m_pCurrentRoom->SetSpawnConfig(spawnConfig);
    m_pCurrentRoom->SetEnemySpawner(m_pEnemySpawner.get());
    m_pCurrentRoom->SetPlayerTarget(m_pPlayerGameObject);
    m_pCurrentRoom->SetScene(this);

    OutputDebugString(L"[Scene] Enemy spawn system initialized\n");

    // Initialize Particle System (before Projectile Manager so it can use particles)
    UINT nParticleDescriptorStart = m_nNextDescriptorIndex;
    m_nNextDescriptorIndex += 512;  // Reserve 512 descriptors for particles
    m_pParticleSystem->Init(pDevice, pCommandList, m_pDescriptorHeap.get(), nParticleDescriptorStart);
    OutputDebugString(L"[Scene] Particle system initialized\n");

    // Initialize Projectile Manager with rendering resources
    // Reserve descriptor indices for projectiles (64 max rendered projectiles)
    UINT nProjectileDescriptorStart = m_nNextDescriptorIndex;
    m_nNextDescriptorIndex += 64;  // Reserve 64 descriptors for projectiles
    m_pProjectileManager->Init(this, pDevice, pCommandList, m_pDescriptorHeap.get(), nProjectileDescriptorStart);
    OutputDebugString(L"[Scene] Projectile system initialized\n");

    // Store the shader
    m_vShaders.push_back(std::move(pShader));

    // Initialize all components for global game objects
    for (auto& gameObject : m_vGameObjects)
    {
        gameObject->Init(pDevice, pCommandList);
    }
    
    // Init Room Objects
    for (auto& room : m_vRooms)
    {
       // Room objects initialization (implicit via CreateGameObject or MeshLoader)
    }

    // Initialize SpotLight parameters
    m_pcbMappedPass->m_SpotLight.m_xmf4SpotLightColor = XMFLOAT4(0.5f, 0.0f, 0.0f, 1.0f); // Red spotlight
    m_pcbMappedPass->m_SpotLight.m_fSpotLightRange = 100.0f;
    m_pcbMappedPass->m_SpotLight.m_fSpotLightInnerCone = cosf(XMConvertToRadians(20.0f)); // Inner cone angle
    m_pcbMappedPass->m_SpotLight.m_fSpotLightOuterCone = cosf(XMConvertToRadians(30.0f)); // Outer cone angle
    m_pcbMappedPass->m_SpotLight.m_fPad5 = 0.0f;
    m_pcbMappedPass->m_SpotLight.m_fPad6 = 0.0f;

    // --------------------------------------------------------------------------
    // Create Interaction Cube (Blue Cube) - as global object (not in room)
    // --------------------------------------------------------------------------
    CRoom* pTempRoom = m_pCurrentRoom;
    m_pCurrentRoom = nullptr;  // Temporarily set to null so cube is created as global object
    m_pInteractionCube = CreateGameObject(pDevice, pCommandList);
    m_pCurrentRoom = pTempRoom;  // Restore

    m_pInteractionCube->GetTransform()->SetPosition(0.0f, 1.0f, 10.0f);  // In front of player
    m_pInteractionCube->GetTransform()->SetScale(2.0f, 2.0f, 2.0f);

    // Create blue cube mesh
    CubeMesh* pCubeMesh = new CubeMesh(pDevice, pCommandList, 1.0f, 1.0f, 1.0f);
    m_pInteractionCube->SetMesh(pCubeMesh);

    // Set blue material
    MATERIAL blueMaterial;
    blueMaterial.m_cAmbient = XMFLOAT4(0.0f, 0.0f, 0.2f, 1.0f);
    blueMaterial.m_cDiffuse = XMFLOAT4(0.2f, 0.4f, 1.0f, 1.0f);  // Blue color
    blueMaterial.m_cSpecular = XMFLOAT4(0.5f, 0.5f, 0.5f, 32.0f);
    blueMaterial.m_cEmissive = XMFLOAT4(0.0f, 0.1f, 0.3f, 1.0f);  // Slight glow
    m_pInteractionCube->SetMaterial(blueMaterial);

    // Add render component
    m_pInteractionCube->AddComponent<RenderComponent>()->SetMesh(pCubeMesh);
    m_vShaders[0]->AddRenderComponent(m_pInteractionCube->GetComponent<RenderComponent>());

    // Add interactable component
    auto* pInteractable = m_pInteractionCube->AddComponent<InteractableComponent>();
    pInteractable->SetPromptText(L"[F] Interact");
    pInteractable->SetInteractionDistance(m_fInteractionDistance);
    pInteractable->SetOnInteract([this](InteractableComponent* pComp) {
        // Activate room when interacted
        if (m_pCurrentRoom && m_pCurrentRoom->GetState() == RoomState::Inactive)
        {
            m_pCurrentRoom->SetState(RoomState::Active);
            OutputDebugString(L"[Scene] Room activated via InteractableComponent!\n");
        }
        pComp->Hide();
    });

    m_bInteractionCubeActive = true;
    m_bEnemiesSpawned = false;

    OutputDebugString(L"[Scene] Interaction cube created\n");
}

void Scene::LoadSceneFromFile(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, const char* pstrFileName)
{
    // This function is now primarily for loading other scene elements, not the player.
    // The player is loaded directly in Init.
    // If we want to load other objects, we can use this.
    // For now, it's empty as the player is handled separately.
}

void Scene::AddRenderComponentsToHierarchy(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
	GameObject* pGameObject, Shader* pShader)
{
	if (!pGameObject)
	{
		OutputDebugString(L"AddRenderComponentsToHierarchy called with a NULL game object!\n");
		return;
	}

	if (pGameObject->GetMesh())
	{
		pGameObject->AddComponent<RenderComponent>()->SetMesh(pGameObject->GetMesh());
		pShader->AddRenderComponent(pGameObject->GetComponent<RenderComponent>());
	}

	if (pGameObject->m_pChild)
	{
		AddRenderComponentsToHierarchy(pDevice, pCommandList, pGameObject->m_pChild, pShader);
	}
	if (pGameObject->m_pSibling)
	{
		AddRenderComponentsToHierarchy(pDevice, pCommandList, pGameObject->m_pSibling, pShader);
	}
}

void Scene::Update(float deltaTime, InputSystem* pInputSystem)
{
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
    XMVECTOR lightDir = XMVector3Normalize(XMVectorSet(-0.5f, -1.0f, 0.5f, 0.0f)); // Sun angle
    XMStoreFloat3(&m_pcbMappedPass->m_xmf3LightDirection, lightDir);
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
}

void Scene::Render(ID3D12GraphicsCommandList* pCommandList)
{
    // Set the descriptor heap
    ID3D12DescriptorHeap* ppHeaps[] = { m_pDescriptorHeap->GetHeap() };
    pCommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    // --------------------------------------------------------------------------
    // Dynamic Rendering List Update
    // --------------------------------------------------------------------------
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

    // --------------------------------------------------------------------------
    // Render
    // --------------------------------------------------------------------------
    // Iterate through shaders (groups) and render
    for (auto& shader : m_vShaders)
    {
        shader->Render(pCommandList, GetPassCBVAddress());
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
}

GameObject* Scene::CreateGameObject(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    // Note: We use raw pointer here, but ownership is transferred to unique_ptr below
    GameObject* newGameObject = new GameObject();

    // Create the constant buffer and its view
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_pDescriptorHeap->GetCPUHandle(m_nNextDescriptorIndex);
    newGameObject->CreateConstantBuffer(pDevice, pCommandList, sizeof(ObjectConstants), cpuHandle);

    // Set the GPU handle for rendering
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_pDescriptorHeap->GetGPUHandle(m_nNextDescriptorIndex);
    newGameObject->SetGpuDescriptorHandle(gpuHandle);

    // Increment for the next object
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

void Scene::TransitionToNextRoom()
{
    OutputDebugString(L"[Scene] Transitioning to next room...\n");

    m_nRoomCount++;

    // Create new room
    auto pNewRoom = std::make_unique<CRoom>();
    pNewRoom->SetBoundingBox(BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(100.0f, 100.0f, 100.0f)));

    // Configure spawn points - vary based on room count
    RoomSpawnConfig spawnConfig;
    int baseEnemies = 3;
    int extraEnemies = m_nRoomCount; // More enemies as rooms progress (capped at reasonable number)
    if (extraEnemies > 5) extraEnemies = 5;

    // Cycle through different enemy combinations
    const char* enemyTypes[] = { "RushAoEEnemy", "RushFrontEnemy", "RangedEnemy" };
    int numEnemies = baseEnemies + extraEnemies;
    if (numEnemies > 8) numEnemies = 8;

    for (int i = 0; i < numEnemies; ++i)
    {
        const char* type = enemyTypes[i % 3];
        float angle = (float)i * (6.28318f / (float)numEnemies);
        float radius = 10.0f + (i % 2) * 5.0f;
        float x = cosf(angle) * radius;
        float z = sinf(angle) * radius + 10.0f;
        spawnConfig.AddSpawn(type, x, 0.0f, z);
    }

    // Apply configuration to new room
    CRoom* pRoomPtr = pNewRoom.get();
    pRoomPtr->SetSpawnConfig(spawnConfig);
    pRoomPtr->SetEnemySpawner(m_pEnemySpawner.get());
    pRoomPtr->SetPlayerTarget(m_pPlayerGameObject);
    pRoomPtr->SetScene(this);

    // Add to room list and set as current
    m_vRooms.push_back(std::move(pNewRoom));
    m_pCurrentRoom = pRoomPtr;

    // Reset player position to origin
    if (m_pPlayerGameObject)
    {
        m_pPlayerGameObject->GetTransform()->SetPosition(0.0f, 0.0f, 0.0f);
    }

    // Activate the new room (enemies will spawn in Update)
    m_pCurrentRoom->SetState(RoomState::Active);

    wchar_t buffer[128];
    swprintf_s(buffer, L"[Scene] Room %d activated with %d enemies\n", m_nRoomCount + 1, numEnemies);
    OutputDebugString(buffer);
}