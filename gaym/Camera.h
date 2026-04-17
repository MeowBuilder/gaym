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
    void Update(float mouseDeltaX, float mouseDeltaY, float scrollDelta, float deltaTime = 0.0f,
                bool bForward = false, bool bBackward = false, bool bLeft = false, bool bRight = false,
                bool bUp = false, bool bDown = false);

    // Free camera toggle
    void ToggleFreeCam();
    bool IsFreeCam() const { return m_bFreeCam; }

    // Get camera matrices
    const DirectX::XMFLOAT4X4& GetViewMatrix() const { return m_viewMatrix; }
    const DirectX::XMFLOAT4X4& GetProjectionMatrix() const { return m_projectionMatrix; }

    // Get camera position
    DirectX::XMFLOAT3 GetPosition() const { return m_position; }

    // Camera direction vectors
    DirectX::XMVECTOR GetLookDirection() const;
    DirectX::XMVECTOR GetRightDirection() const;

    // Camera shake
    void StartShake(float fIntensity, float fDuration);
    void StopShake();
    bool IsShaking() const { return m_bShaking; }

    // Cinematic mode: orbit around a world-space point instead of the follow target
    void StartCinematic(const DirectX::XMFLOAT3& lookAt, float distance, float pitch, float yaw);
    void StopCinematic();
    bool IsCinematic() const { return m_bCinematic; }
    // Smoothly adjust cinematic orbit params (call every frame during cutscene)
    void SetCinematicOrbit(float distance, float pitch, float yaw) { m_fCinDist = distance; m_fCinPitch = pitch; m_fCinYaw = yaw; }

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
    float m_distance = 50.0f;
    float m_minDistance = 2.0f;
    float m_maxDistance = 100.0f;
    float m_pitch = 60.0f; // Angle in degrees
    float m_yaw = 45.0f;   // Angle in degrees

    // Mouse sensitivity
    float m_rotationSpeed = 0.2f;
    float m_zoomSpeed = 0.01f;

    // Free camera mode
    bool m_bFreeCam = false;
    DirectX::XMFLOAT3 m_freeCamPos = { 0.0f, 30.0f, 0.0f };
    float m_freeYaw   = 45.0f;   // degrees
    float m_freePitch = -30.0f;  // degrees (looking down)
    float m_freeMoveSpeed = 40.0f;
    float m_freeRotSpeed  = 0.15f;

    // Cinematic mode
    bool m_bCinematic = false;
    DirectX::XMFLOAT3 m_cinLookAt = { 0.0f, 0.0f, 0.0f };
    float m_fCinDist  = 30.0f;
    float m_fCinPitch = 30.0f;
    float m_fCinYaw   = 45.0f;

    // Camera shake
    bool m_bShaking = false;
    float m_fShakeIntensity = 0.0f;
    float m_fShakeDuration = 0.0f;
    float m_fShakeTimer = 0.0f;
    DirectX::XMFLOAT3 m_shakeOffset = { 0.0f, 0.0f, 0.0f };
};
