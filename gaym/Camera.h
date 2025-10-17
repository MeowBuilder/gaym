#pragma once

#include <DirectXMath.h>

class GameObject; // Forward declaration to avoid circular dependency

class CCamera
{
public:
    CCamera();
    ~CCamera();

    // Set the object for the camera to follow
    void SetTarget(GameObject* pTarget) { m_pTarget = pTarget; }

    // Set camera lens properties
    void SetLens(float fovY, float aspect, float zn, float zf);

    // Update camera based on mouse input
    void Update(float mouseDeltaX, float mouseDeltaY, float scrollDelta);

    // Get camera matrices
    const DirectX::XMFLOAT4X4& GetViewMatrix() const { return m_viewMatrix; }
    const DirectX::XMFLOAT4X4& GetProjectionMatrix() const { return m_projectionMatrix; }

    // Camera direction vectors
    DirectX::XMVECTOR GetLookDirection() const;
    DirectX::XMVECTOR GetRightDirection() const;

private:
    void UpdateViewMatrix();

private:
    // Camera matrices
    DirectX::XMFLOAT4X4 m_viewMatrix;
    DirectX::XMFLOAT4X4 m_projectionMatrix;

    // Camera position and orientation
    DirectX::XMFLOAT3 m_position;

    // Target to follow
    GameObject* m_pTarget = nullptr;
    
    // Offset from the target's origin where the camera should look
    DirectX::XMFLOAT3 m_lookAtOffset = { 0.0f, 1.0f, 0.0f };

    // Camera orbit parameters
    float m_distance = 10.0f;
    float m_minDistance = 2.0f;
    float m_maxDistance = 20.0f;
    float m_pitch = 20.0f; // Angle in degrees
    float m_yaw = 0.0f;   // Angle in degrees

    // Mouse sensitivity
    float m_rotationSpeed = 0.2f;
    float m_zoomSpeed = 0.01f;
};
