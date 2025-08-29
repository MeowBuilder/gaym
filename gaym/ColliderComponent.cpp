#include "stdafx.h"
#include "ColliderComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"

ColliderComponent::ColliderComponent(GameObject* pOwner) : Component(pOwner)
{
    m_pTransform = GetOwner()->GetTransform();
}

void ColliderComponent::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    // Create a local-space bounding box, which will be transformed each frame.
    m_initialBox.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
    m_initialBox.Extents = XMFLOAT3(0.5f, 0.5f, 0.5f);
}

void ColliderComponent::Update(float deltaTime)
{
    if (m_pTransform)
    {
        // An alternative way to transform the box to avoid overload resolution issues.
        DirectX::BoundingOrientedBox tempObb;
        DirectX::BoundingOrientedBox::CreateFromBoundingBox(tempObb, m_initialBox);

        const XMMATRIX worldMatrix = XMLoadFloat4x4(&m_pTransform->GetWorldMatrix());
        tempObb.Transform(m_boundingBox, worldMatrix);
    }
}

bool ColliderComponent::Intersects(const ColliderComponent& other) const
{
    return m_boundingBox.Intersects(other.GetBoundingBox());
}