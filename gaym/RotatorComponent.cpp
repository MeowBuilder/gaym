#include "stdafx.h"
#include "RotatorComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"

RotatorComponent::RotatorComponent(GameObject* pOwner) 
    : Component(pOwner)
    , m_rotationSpeed(0.0f, 90.0f, 0.0f) // Default to 90 degrees per second on Y-axis
{
}

void RotatorComponent::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
    m_pTransform = GetOwner()->GetTransform();
}

void RotatorComponent::Update(float deltaTime)
{
    if (m_pTransform)
    {
        // Get current rotation
        XMFLOAT3 currentRotation = m_pTransform->GetRotation();

        // Add rotation
        currentRotation.x += m_rotationSpeed.x * deltaTime;
        currentRotation.y += m_rotationSpeed.y * deltaTime;
        currentRotation.z += m_rotationSpeed.z * deltaTime;

        // Apply new rotation
        m_pTransform->SetRotation(currentRotation);
    }
}
