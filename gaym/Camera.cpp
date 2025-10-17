#include "stdafx.h"
#include "Camera.h"
#include "GameObject.h"
#include "TransformComponent.h"

using namespace DirectX;

CCamera::CCamera()
{
    // Initialize matrices
    XMStoreFloat4x4(&m_viewMatrix, XMMatrixIdentity());
    XMStoreFloat4x4(&m_projectionMatrix, XMMatrixIdentity());

    // Initialize position
    m_position = XMFLOAT3(0.0f, 0.0f, 0.0f);
}

CCamera::~CCamera()
{
}

void CCamera::SetLens(float fovY, float aspect, float zn, float zf)
{
    XMMATRIX P = XMMatrixPerspectiveFovLH(fovY, aspect, zn, zf);
    XMStoreFloat4x4(&m_projectionMatrix, P);
}

void CCamera::Update(float mouseDeltaX, float mouseDeltaY, float scrollDelta)
{
    // Update yaw and pitch from mouse input
    m_yaw += mouseDeltaX * m_rotationSpeed;
    m_pitch += mouseDeltaY * m_rotationSpeed;

    // Clamp pitch to avoid flipping
    m_pitch = max(-89.0f, min(89.0f, m_pitch));

    // Update distance from scroll wheel input
    m_distance -= scrollDelta * m_zoomSpeed;
    m_distance = max(m_minDistance, min(m_maxDistance, m_distance));

    // Recalculate the view matrix
    UpdateViewMatrix();
}

void CCamera::UpdateViewMatrix()
{
    if (!m_pTarget)
        return;

    // Convert degrees to radians
    float yawRad = XMConvertToRadians(m_yaw);
    float pitchRad = XMConvertToRadians(m_pitch);

    // Get target's position
    TransformComponent* pTransform = m_pTarget->GetTransform();
    XMVECTOR targetPos = XMLoadFloat3(&pTransform->GetPosition());
    XMVECTOR lookAtPoint = targetPos + XMLoadFloat3(&m_lookAtOffset);

    // Calculate the camera's position based on orbit parameters
    float x = m_distance * cosf(pitchRad) * sinf(yawRad);
    float y = m_distance * sinf(pitchRad);
    float z = m_distance * cosf(pitchRad) * cosf(yawRad);

    XMVECTOR cameraPos = targetPos + XMVectorSet(x, y, z, 0.0f);
    XMStoreFloat3(&m_position, cameraPos);

    // Build the view matrix
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(cameraPos, lookAtPoint, up);
    XMStoreFloat4x4(&m_viewMatrix, view);
}

XMVECTOR CCamera::GetLookDirection() const
{
    XMMATRIX viewMatrix = XMLoadFloat4x4(&m_viewMatrix);
    XMMATRIX inverseViewMatrix = XMMatrixInverse(nullptr, viewMatrix);
    return inverseViewMatrix.r[2]; // Z-axis of the inverse view matrix is the look direction
}

XMVECTOR CCamera::GetRightDirection() const
{
    XMMATRIX viewMatrix = XMLoadFloat4x4(&m_viewMatrix);
    XMMATRIX inverseViewMatrix = XMMatrixInverse(nullptr, viewMatrix);
    return inverseViewMatrix.r[0]; // X-axis of the inverse view matrix is the right direction
}