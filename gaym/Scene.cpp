#include "stdafx.h"
#include "Scene.h"
#include "RenderComponent.h"

Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    GameObject* pCube = CreateGameObject();
    pCube->AddComponent<RenderComponent>();

    for (auto& gameObject : m_vGameObjects)
    {
        gameObject->Init(pDevice, pCommandList);
    }
}

void Scene::Update(float deltaTime)
{
    for (auto& gameObject : m_vGameObjects)
    {
        gameObject->Update(deltaTime);
    }
}

void Scene::Render(ID3D12GraphicsCommandList* pCommandList)
{
    for (auto& gameObject : m_vGameObjects)
    {
        gameObject->Render(pCommandList);
    }
}

GameObject* Scene::CreateGameObject()
{
    auto newGameObject = std::make_unique<GameObject>();
    GameObject* rawPtr = newGameObject.get();
    m_vGameObjects.push_back(std::move(newGameObject));
    return rawPtr;
}