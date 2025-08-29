#include "stdafx.h"
#include "Scene.h"
#include "RenderComponent.h"
#include "Shader.h"
#include "RotatorComponent.h"
#include "ColliderComponent.h"
#include "TransformComponent.h"
Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    // Create Shader
    auto pShader = std::make_unique<Shader>();
    pShader->Build(pDevice);

    // Create Mesh
    auto pCubeMesh = std::make_shared<Mesh>();
    pCubeMesh->BuildCube(pDevice, pCommandList);

    // --- Create GameObjects and add Components ---

    // Cube 1 (Rotates, has a collider)
    GameObject* pCube1 = CreateGameObject();
    pCube1->GetTransform()->SetPosition(-1.5f, 0.0f, 0.0f);
    pCube1->AddComponent<RenderComponent>()->SetMesh(pCubeMesh);
    pCube1->AddComponent<RotatorComponent>(); // Add rotator
    pCube1->AddComponent<ColliderComponent>(); // Add collider
    pShader->AddRenderComponent(pCube1->GetComponent<RenderComponent>());

    // Cube 2 (Stays still, has a collider)
    GameObject* pCube2 = CreateGameObject();
    pCube2->GetTransform()->SetPosition(-1.5f, 0.0f, 0.0f);
    pCube2->AddComponent<RenderComponent>()->SetMesh(pCubeMesh);
    pCube2->AddComponent<ColliderComponent>(); // Add collider
    pShader->AddRenderComponent(pCube2->GetComponent<RenderComponent>());

    // Store the shader
    m_vShaders.push_back(std::move(pShader));

    // Initialize all components for all game objects
    for (auto& gameObject : m_vGameObjects)
    {
        gameObject->Init(pDevice, pCommandList);
    }
}

void Scene::Update(float deltaTime)
{
    // 1. Update all components (e.g., rotator, transform)
    for (auto& gameObject : m_vGameObjects)
    {
        gameObject->Update(deltaTime);
    }

    // 2. Check for collisions
    for (size_t i = 0; i < m_vGameObjects.size(); ++i)
    {
        for (size_t j = i + 1; j < m_vGameObjects.size(); ++j)
        {
            GameObject* pObj1 = m_vGameObjects[i].get();
            GameObject* pObj2 = m_vGameObjects[j].get();

            auto pCollider1 = pObj1->GetComponent<ColliderComponent>();
            auto pCollider2 = pObj2->GetComponent<ColliderComponent>();

            if (pCollider1 && pCollider2)
            {
                if (pCollider1->Intersects(*pCollider2))
                {
                    // Collision detected! Print to debug output.
                    OutputDebugStringA("Collision Detected!\n");
                }
            }
        }
    }
}

void Scene::Render(ID3D12GraphicsCommandList* pCommandList)
{
    // Iterate through shaders (groups) and render
    for (auto& shader : m_vShaders)
    {
        shader->Render(pCommandList);
    }
}

GameObject* Scene::CreateGameObject()
{
    auto newGameObject = std::make_unique<GameObject>();
    GameObject* rawPtr = newGameObject.get();
    m_vGameObjects.push_back(std::move(newGameObject));
    return rawPtr;
}