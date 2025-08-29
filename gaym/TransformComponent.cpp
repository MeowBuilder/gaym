#include "stdafx.h"
#include "TransformComponent.h"

TransformComponent::TransformComponent(GameObject* pOwner) 
    : Component(pOwner)
    , m_position(0.0f, 0.0f, 0.0f)
    , m_rotation(0.0f, 0.0f, 0.0f)
    , m_scale(1.0f, 1.0f, 1.0f)
{
    XMStoreFloat4x4(&m_matWorld, XMMatrixIdentity());
}

void TransformComponent::Update(float deltaTime)
{
    XMMATRIX matScale = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
    XMMATRIX matRotation = XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_rotation.x), XMConvertToRadians(m_rotation.y), XMConvertToRadians(m_rotation.z));
    XMMATRIX matTranslate = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);

    XMStoreFloat4x4(&m_matWorld, matScale * matRotation * matTranslate);
}