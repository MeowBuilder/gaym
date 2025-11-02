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
    m_pCamera->SetLens(XMConvertToRadians(60.0f), (float)kWindowWidth / (float)kWindowHeight, 0.1f, 100.0f);

    // Create Shader
    auto pShader = std::make_unique<Shader>();
    pShader->Build(pDevice);

    // Player GameObject
    GameObject* pPlayer = CreateGameObject(pDevice, pCommandList);
    m_vGameObjects.push_back(std::unique_ptr<GameObject>(pPlayer));
    pPlayer->GetTransform()->SetPosition(0.0f, 0.0f, 20.0f);
    pPlayer->AddComponent<PlayerComponent>();
    m_pPlayerGameObject = pPlayer;

    // Load Apache model for the player
    GameObject* pPlayerModel = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Model/Apache.bin");
    if (pPlayerModel)
    { 
        OutputDebugString(L"Model loaded successfully!\n");
        pPlayer->SetChild(pPlayerModel);
        AddRenderComponentsToHierarchy(pDevice, pCommandList, pPlayerModel, pShader.get());
    }
    else
    {
        OutputDebugString(L"Failed to load model!\n");
    }

    // Set camera target to player
    m_pCamera->SetTarget(m_pPlayerGameObject);

    // Add two static Apache models for testing
    {
        GameObject* pStaticApache1 = CreateGameObject(pDevice, pCommandList);
        m_vGameObjects.push_back(std::unique_ptr<GameObject>(pStaticApache1));
        pStaticApache1->GetTransform()->SetPosition(-30.0f, 0.0f, 0.0f);
        GameObject* pStaticModel1 = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Model/Apache.bin");
        if (pStaticModel1)
        {
            pStaticApache1->SetChild(pStaticModel1);
            AddRenderComponentsToHierarchy(pDevice, pCommandList, pStaticModel1, pShader.get());
        }

        GameObject* pStaticApache2 = CreateGameObject(pDevice, pCommandList);
        m_vGameObjects.push_back(std::unique_ptr<GameObject>(pStaticApache2));
        pStaticApache2->GetTransform()->SetPosition(30.0f, 0.0f, 0.0f);
        GameObject* pStaticModel2 = MeshLoader::LoadGeometryFromFile(this, pDevice, pCommandList, NULL, "Model/Apache.bin");
        if (pStaticModel2)
        {
            pStaticApache2->SetChild(pStaticModel2);
            AddRenderComponentsToHierarchy(pDevice, pCommandList, pStaticModel2, pShader.get());
        }
    }

    // Store the shader

    // Store the shader
    m_vShaders.push_back(std::move(pShader));

    // Initialize all components for all game objects
    for (auto& gameObject : m_vGameObjects)
    {
        gameObject->Init(pDevice, pCommandList);
    }

    // Temporarily set the first cube as the camera target for testing
    // This will be replaced with the player GameObject later
    if (!m_vGameObjects.empty())
    {
        m_pCamera->SetTarget(m_vGameObjects[0].get());
    }
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

    // 1. Update all components (e.g., rotator, transform)
    for (auto& gameObject : m_vGameObjects)
    {
        gameObject->Update(deltaTime);
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

    // Iterate through shaders (groups) and render
    for (auto& shader : m_vShaders)
    {
        shader->Render(pCommandList, GetPassCBVAddress());
    }
}

GameObject* Scene::CreateGameObject(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    GameObject* newGameObject = new GameObject();

    // Create the constant buffer and its view
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = m_pDescriptorHeap->GetCPUHandle(m_nNextDescriptorIndex);
    newGameObject->CreateConstantBuffer(pDevice, pCommandList, sizeof(ObjectConstants), cpuHandle);

    // Set the GPU handle for rendering
    D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_pDescriptorHeap->GetGPUHandle(m_nNextDescriptorIndex);
    newGameObject->SetGpuDescriptorHandle(gpuHandle);

    // Increment for the next object
    m_nNextDescriptorIndex++;

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