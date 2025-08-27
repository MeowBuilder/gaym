#include "stdafx.h"
#include "GameObject.h"
#include "Component.h"
#include "TransformComponent.h"

GameObject::GameObject()
{
    m_pTransform = AddComponent<TransformComponent>();
}

GameObject::~GameObject()
{
}

void GameObject::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    for (auto& component : m_vComponents)
    {
        component->Init(pDevice, pCommandList);
    }
}

void GameObject::Update(float deltaTime)
{
    for (auto& component : m_vComponents)
    {
        component->Update(deltaTime);
    }
}

void GameObject::Render(ID3D12GraphicsCommandList* pCommandList)
{
    for (auto& component : m_vComponents)
    {
        component->Render(pCommandList);
    }
}