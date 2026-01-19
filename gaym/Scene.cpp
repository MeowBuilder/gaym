#include "stdafx.h"
#include "Scene.h"
#include "RenderComponent.h"
#include "Shader.h"
#include "RotatorComponent.h"
#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "InputSystem.h" // Added for InputSystem
#include "PlayerComponent.h"
#include <functional> // Added for std::function

Scene::Scene()
{
    m_pCamera = std::make_unique<CCamera>();
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
    m_pDescriptorHeap->Create(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 256, true);

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
    // 1. Create Rooms
    // --------------------------------------------------------------------------
    
    // Room 1 (Start Area: Red Zone)
    auto pRoom1 = std::make_unique<CRoom>();
    pRoom1->SetState(RoomState::Active);
    pRoom1->SetBoundingBox(BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(60.0f, 100.0f, 100.0f))); // x: -30 to 30
    m_vRooms.push_back(std::move(pRoom1));

    // Room 2 (Next Area: Blue Zone)
    auto pRoom2 = std::make_unique<CRoom>();
    pRoom2->SetState(RoomState::Inactive);
    pRoom2->SetBoundingBox(BoundingBox(XMFLOAT3(60.0f, 0, 0), XMFLOAT3(60.0f, 100.0f, 100.0f))); // x: 30 to 90
    m_vRooms.push_back(std::move(pRoom2));


    // --------------------------------------------------------------------------
    // 2. Load Global Objects (Player)
    // --------------------------------------------------------------------------
    m_pCurrentRoom = nullptr; 

    GameObject* pPlayer = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Animation/Vampire A Lusth.bin");
    if (pPlayer)
    {
        OutputDebugString(L"Player model loaded successfully!\n");
        pPlayer->GetTransform()->SetPosition(0.0f, 0.0f, 20.0f);
        pPlayer->GetTransform()->SetScale(10.0f, 10.0f, 10.0f);
        pPlayer->AddComponent<PlayerComponent>();
        m_pPlayerGameObject = pPlayer;
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


    // --------------------------------------------------------------------------
    // 3. Setup Room 1 Objects (RED Cubes)
    // --------------------------------------------------------------------------
    m_pCurrentRoom = m_vRooms[0].get(); 

    for(int i=0; i<5; ++i)
    {
        GameObject* pCube = CreateGameObject(pDevice, pCommandList);
        pCube->GetTransform()->SetPosition(-15.0f + i*5.0f, 5.0f, 10.0f); // Row of cubes
        pCube->AddComponent<RotatorComponent>(); // Make them spin!
        
        // Load a simple cube mesh (using existing MeshLoader or creating one manually if possible)
        // Since we don't have a simple cube bin handy, we'll reuse the Apache for now but tint it?
        // Actually, let's use the Apache but make it spin fast to show it's "Active".
        GameObject* pModel = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Model/Apache.bin");
        if(pModel) 
        {
            pCube->SetChild(pModel);
            AddRenderComponentsToHierarchy(pDevice, pCommandList, pModel, pShader.get());
        }
    }


    // --------------------------------------------------------------------------
    // 4. Setup Room 2 Objects (BLUE/Different Cubes)
    // --------------------------------------------------------------------------
    m_pCurrentRoom = m_vRooms[1].get(); 
    
    for(int i=0; i<5; ++i)
    {
        GameObject* pCube = CreateGameObject(pDevice, pCommandList);
        pCube->GetTransform()->SetPosition(40.0f + i*5.0f, 5.0f, 10.0f); // Row of cubes in next room
        // Make these spin differently (faster?)
        auto rotator = pCube->AddComponent<RotatorComponent>();
        // We can't easily set speed on RotatorComponent without modifying it, but it defaults to rotating.
        
        // Use a different model to distinguish (SuperCobra)
        GameObject* pModel = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Model/SuperCobra.bin");
        if(pModel) 
        {
            pCube->SetChild(pModel);
            AddRenderComponentsToHierarchy(pDevice, pCommandList, pModel, pShader.get());
        }
    }

    // Reset Context to Room 1
    m_pCurrentRoom = m_vRooms[0].get(); 


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
        // --------------------------------------------------------------------------
        // Room Transition Logic
        // --------------------------------------------------------------------------
        if (m_pPlayerGameObject)
        {
            XMFLOAT3 playerPos = m_pPlayerGameObject->GetTransform()->GetPosition();

            // Transition: Room 1 -> Room 2
            if (m_pCurrentRoom == m_vRooms[0].get() && playerPos.x > 30.0f)
            {
                OutputDebugString(L"[Scene] Transition: Leaving Room 1, Entering Room 2\n");
                
                m_pCurrentRoom->SetState(RoomState::Inactive); // Disable Room 1
                m_pCurrentRoom = m_vRooms[1].get();            // Switch pointer
                m_pCurrentRoom->SetState(RoomState::Active);   // Enable Room 2
            }
            // Transition: Room 2 -> Room 1 (Optional: if we allow backtracking)
            else if (m_pCurrentRoom == m_vRooms[1].get() && playerPos.x < 30.0f)
            {
                OutputDebugString(L"[Scene] Transition: Leaving Room 2, Entering Room 1\n");

                m_pCurrentRoom->SetState(RoomState::Inactive);
                m_pCurrentRoom = m_vRooms[0].get();
                m_pCurrentRoom->SetState(RoomState::Active);
            }
        }
        
        m_pCurrentRoom->Update(deltaTime);
    }

    // Update player specific logic
    if (m_pPlayerGameObject && m_pPlayerGameObject->GetComponent<PlayerComponent>())
    {
        m_pPlayerGameObject->GetComponent<PlayerComponent>()->PlayerUpdate(deltaTime, pInputSystem, m_pCamera.get());
    }

    // 2. Check for collisions
    // ... (collision code is unchanged)
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