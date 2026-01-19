#include "stdafx.h"
#include "TransformComponent.h"
#include "GameObject.h" // Added for full GameObject definition

TransformComponent::TransformComponent(GameObject* pOwner) 
    : Component(pOwner)
    , m_position(0.0f, 0.0f, 0.0f)
    , m_rotation(0.0f, 0.0f, 0.0f)
    , m_scale(1.0f, 1.0f, 1.0f)
{
    XMStoreFloat4x4(&m_matWorld, XMMatrixIdentity());
    XMStoreFloat4x4(&m_matLocal, XMMatrixIdentity());
}

void TransformComponent::Update(float deltaTime)
{
    XMMATRIX matScale = XMMatrixScaling(m_scale.x, m_scale.y, m_scale.z);
    
    XMMATRIX matRotation;
    if (m_bUseQuaternion)
    {
        matRotation = XMMatrixRotationQuaternion(XMLoadFloat4(&m_rotationQuaternion));
    }
    else
    {
        matRotation = XMMatrixRotationRollPitchYaw(XMConvertToRadians(m_rotation.x),
            XMConvertToRadians(m_rotation.y), XMConvertToRadians(m_rotation.z));
    }

    XMMATRIX matTranslate = XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
    XMMATRIX matLocal = XMLoadFloat4x4(&m_matLocal);

    XMMATRIX matWorld = matScale * matRotation * matTranslate * matLocal;

    // Get parent's world matrix and multiply it
    if (m_pOwner && m_pOwner->m_pParent)
    {
        TransformComponent* pParentTransform = m_pOwner->m_pParent->GetTransform();
        if (pParentTransform)
        {
            matWorld *= XMLoadFloat4x4(&pParentTransform->GetWorldMatrix());
        }
    }

    XMStoreFloat4x4(&m_matWorld, matWorld);
}

XMVECTOR TransformComponent::GetLook() const
{
    XMMATRIX worldMatrix = XMLoadFloat4x4(&m_matWorld);
    return worldMatrix.r[2]; // Z-axis of the world matrix is the look direction
}

XMVECTOR TransformComponent::GetRight() const
{
    XMMATRIX worldMatrix = XMLoadFloat4x4(&m_matWorld);
    return worldMatrix.r[0]; // X-axis of the world matrix is the right direction
}

XMVECTOR TransformComponent::GetUp() const
{
    XMMATRIX worldMatrix = XMLoadFloat4x4(&m_matWorld);
    return worldMatrix.r[1]; // Y-axis of the world matrix is the up direction
}

void TransformComponent::Rotate(float pitch, float yaw, float roll)
{
    m_rotation.x += pitch;
    m_rotation.y += yaw;
    m_rotation.z += roll;

    // Clamp pitch to avoid flipping
    m_rotation.x = max(-89.0f, min(89.0f, m_rotation.x));
}