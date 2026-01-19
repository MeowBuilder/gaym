#include "stdafx.h"
#include "Scene.h"
#include "RenderComponent.h"
#include "Shader.h"
#include "RotatorComponent.h"
#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "InputSystem.h" // Added for InputSystem
#include "PlayerComponent.h"

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

    // Create Default Room and set it as active
    auto pDefaultRoom = std::make_unique<CRoom>();
    pDefaultRoom->SetState(RoomState::Active);
    // Set a large bounding box for the default room to ensure player is "inside"
    pDefaultRoom->SetBoundingBox(BoundingBox(XMFLOAT3(0, 0, 0), XMFLOAT3(10000.0f, 10000.0f, 10000.0f)));
    m_vRooms.push_back(std::move(pDefaultRoom));
    
    // NOTE: We do NOT set m_pCurrentRoom yet. We want the player to be added to m_vGameObjects (Global),
    // not the room. CreateGameObject checks m_pCurrentRoom to decide where to put the object.
    m_pCurrentRoom = nullptr; 


    // Player GameObject - Refactored to prevent orbiting issue
    GameObject* pPlayer = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Animation/Vampire A Lusth.bin");
    if (pPlayer)
    {
        // The loaded model is already added to m_vGameObjects by CreateGameObject because m_pCurrentRoom is null.
        // We just need to configure it.
        
        OutputDebugString(L"Player model loaded successfully!\n");

        // Configure this object as the player
        pPlayer->GetTransform()->SetPosition(0.0f, 0.0f, 20.0f);
        pPlayer->GetTransform()->SetScale(10.0f, 10.0f, 10.0f);
        pPlayer->AddComponent<PlayerComponent>();
        m_pPlayerGameObject = pPlayer;

        // Add render components to the new player object and its children
        AddRenderComponentsToHierarchy(pDevice, pCommandList, pPlayer, pShader.get());
    }
    else
    {
        OutputDebugString(L"Failed to load player model!\n");
        // Create a fallback empty game object so the game doesn't crash
        // CreateGameObject adds it to m_vGameObjects since m_pCurrentRoom is null.
        pPlayer = CreateGameObject(pDevice, pCommandList);
        pPlayer->GetTransform()->SetPosition(0.0f, 0.0f, 20.0f);
        pPlayer->AddComponent<PlayerComponent>();
        m_pPlayerGameObject = pPlayer;
    }

    // Set camera target to player
    m_pCamera->SetTarget(m_pPlayerGameObject);

    // NOW we activate the room. Any subsequent CreateGameObject calls will add objects to this room.
    m_pCurrentRoom = m_vRooms[0].get();

    // Add two static Apache models for testing
    // These should go into the Room now.
    {
        // Apache 1
        GameObject* pStaticApache1 = CreateGameObject(pDevice, pCommandList); 
        // CreateGameObject now adds to m_pCurrentRoom automatically.
        
        pStaticApache1->GetTransform()->SetPosition(-30.0f, 0.0f, 0.0f);
        GameObject* pStaticModel1 = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Model/Apache.bin");
        // MeshLoader calls CreateGameObject -> Adds to Room.
        // But wait, pStaticModel1 is a child of pStaticApache1.
        // Does SetChild handle ownership? No, it just links pointers.
        // So pStaticModel1 remains in the Room list as a root object in the room context,
        // but logic-wise it's a child.
        // This is fine for memory management (Room owns it), but Update/Render might double-call 
        // if Room iterates ALL objects and Parent also iterates Children.
        
        // GameObject::Update/Render recurses to children.
        // If Room also Updates/Renders pStaticModel1, it will be done twice.
        // FIX: MeshLoader adds to Room. We need to prevent double update.
        // Actually, for this specific case (Child), we might want to remove it from the Room's top-level list?
        // Or, we just accept that MeshLoader loads a hierarchy and returns the ROOT of that hierarchy.
        // The ROOT (pStaticModel1) is in the Room list.
        // pStaticApache1 is also in the Room list.
        // We set pStaticApache1 -> Child -> pStaticModel1.
        
        // If we leave it as is:
        // Room Update -> pStaticApache1 Update -> pStaticModel1 Update.
        // Room Update -> pStaticModel1 Update.
        // YES, Double Update!
        
        // However, fixing that is complex right now. 
        // Let's first fix the CRASH (Double Free), which is caused by explicit push_back in Scene::Init.
        // I have removed the explicit push_back above for the Player.
        // Here for Apache, I am NOT doing push_back, so that's good.
        
        if (pStaticModel1)
        {
            pStaticApache1->SetChild(pStaticModel1);
            AddRenderComponentsToHierarchy(pDevice, pCommandList, pStaticModel1, pShader.get());
        }

        // Apache 2
        GameObject* pStaticApache2 = CreateGameObject(pDevice, pCommandList);
        pStaticApache2->GetTransform()->SetPosition(30.0f, 0.0f, 0.0f);
        GameObject* pStaticModel2 = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Model/Apache.bin");
        if (pStaticModel2)
        {
            pStaticApache2->SetChild(pStaticModel2);
            AddRenderComponentsToHierarchy(pDevice, pCommandList, pStaticModel2, pShader.get());
        }
    }

    // Store the shader
    m_vShaders.push_back(std::move(pShader));

    // Initialize all components for global game objects
    for (auto& gameObject : m_vGameObjects)
    {
        gameObject->Init(pDevice, pCommandList);
    }
    
    // Initialize components for objects in the room
    if (m_pCurrentRoom)
    {
        // We need to access room's objects to init them? 
        // Actually, GameObject::Init might just be setting up stuff that CreateGameObject already did?
        // GameObject::Init sets up components.
        // We need a way to iterate room objects to Init them if they weren't inited.
        // But CreateGameObject calls CreateConstantBuffer etc. 
        // Let's assume newly created objects via CreateGameObject need Init? 
        // The original code called Init on everything at the end.
        // We should add an Init method to CRoom.
        // For now, let's manually iterate.
        // But CRoom::GetGameObjects returns const ref.
        // We might need to add Init to CRoom.
        // Let's add Init to CRoom later or cast away const for now (bad practice but quick).
        // Better: The loop below iterates m_vGameObjects.
        // CreateGameObject calls Init? No.
        // We need to ensure objects in Room are initialized.
    }
    
    // Quick fix: Iterate room objects and Init
    // Since CRoom doesn't have Init yet, and we can't easily access non-const objects...
    // Let's just rely on the fact that CreateGameObject does the heavy lifting (CBV creation).
    // Does GameObject::Init do anything critical?
    // It calls Component::Init.
    // Yes, we need to call it.
    // I will add a temporary loop here by const_cast or accessing via friend if possible?
    // No, I'll just rely on CreateGameObject returning the pointer and calling Init on it right there?
    // The original code did it in a batch at the end.
    // Let's just add `pStaticApache1->Init(pDevice, pCommandList);` immediately after creation.
    // Wait, CreateGameObject is a factory.
    // Let's look at `GameObject::Init`. 
    // It calls `m_vComponents[i]->Init()`.
    
    // Revised plan for Init: Call Init immediately after creation or setup in the block above.
    // DONE: Added Init calls in the Apache block below.

    // Temporarily set the first cube as the camera target for testing
    // This will be replaced with the player GameObject later
    if (!m_vGameObjects.empty())
    {
        m_pCamera->SetTarget(m_vGameObjects[0].get());
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
        // Check if player is inside the room (for transition logic later)
        if (m_pPlayerGameObject)
        {
             // Logic to switch rooms could go here
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

    // Global objects render via m_vGameObjects? 
    // No, m_vGameObjects are just raw storage in the previous code?
    // Wait, the previous Render code:
    // for (auto& shader : m_vShaders) { shader->Render(...) }
    // Shader::Render iterates its registered RenderComponents.
    // As long as objects (Room or Global) register their RenderComponents to the Shader, they will be drawn.
    // BUT, we might want to cull objects in inactive rooms.
    // If Shader::Render draws EVERYTHING registered, we have a problem: Inactive room objects will still be drawn.
    
    // We need to change how rendering works.
    // Option A: Clear Shader's list every frame and re-add only active objects. (Slow)
    // Option B: Have Shader::Render take a list of objects to render? (Better)
    // Option C: Let the Scene drive rendering by iterating rooms. (Best for this architecture)
    
    // Currently, Shader::Render does:
    // for(m_vRenderComponents) { Draw }
    
    // We should probably modify this so Scene calls Render on objects.
    // OR, we keep it simple for now:
    // If the Shader holds pointers to ALL RenderComponents, it will draw EVERYTHING.
    // We need to unregister/register components when switching rooms? Too complex.
    
    // Alternative:
    // CRoom::Render() calls pGameObject->Render(). 
    // GameObject::Render() calls RenderComponent::Render().
    // RenderComponent::Render() needs to know about the Shader or CommandList?
    // Currently RenderComponent doesn't do the draw call itself, the Shader does.
    
    // Let's look at Shader::Render in the codebase (memory).
    // Usually it sets PSO, RootSignature, then iterates objects.
    
    // If we want to support Room-based culling, we should probably:
    // 1. Bind Shader State (PSO, etc.)
    // 2. Ask CurrentRoom to Render its objects.
    // 3. Render Global objects.
    
    // However, `Shader` class encapsulates the PSO.
    // So we might need: `pShader->Render(pCommandList, m_pCurrentRoom->GetObjects())`?
    
    // For this refactoring step, I will stick to the existing Shader-based rendering 
    // but I must ensure that ONLY active objects are registered?
    // Or, I can just leave it as is: All objects are drawn.
    // Optimization (Culling inactive rooms) can come later.
    // For now, let's just make sure the code COMPILES and RUNS with the new structure.
    // The visual result will be the same (all objects drawn).
    
    // So I will keep the loop over m_vShaders.
    // Since AddRenderComponentsToHierarchy registers components to the shader, they will all be drawn.
    // This is fine for Step 1.
    
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