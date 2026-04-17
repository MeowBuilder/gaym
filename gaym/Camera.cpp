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

void CCamera::ToggleFreeCam()
{
    m_bFreeCam = !m_bFreeCam;
    if (m_bFreeCam)
    {
        // FreeCam 진입 시 현재 카메라 위치 및 방향 이어받기
        m_freeCamPos = m_position;
        m_freeYaw   = m_yaw;
        m_freePitch = -m_pitch; // orbit pitch는 위를 양수로, freecam은 아래를 양수로
        OutputDebugString(L"[Camera] FreeCam ON (F2 to toggle)\n");
    }
    else
    {
        OutputDebugString(L"[Camera] FreeCam OFF\n");
    }
}

void CCamera::Update(float mouseDeltaX, float mouseDeltaY, float scrollDelta, float deltaTime,
                     bool bForward, bool bBackward, bool bLeft, bool bRight, bool bUp, bool bDown)
{
    // === Free Camera Mode ===
    if (m_bFreeCam)
    {
        // Mouse rotation
        m_freeYaw   += mouseDeltaX * m_freeRotSpeed;
        m_freePitch += mouseDeltaY * m_freeRotSpeed;
        m_freePitch  = max(-89.0f, min(89.0f, m_freePitch));

        // Build look/right/up from yaw+pitch
        float yawRad   = XMConvertToRadians(m_freeYaw);
        float pitchRad = XMConvertToRadians(m_freePitch);

        XMVECTOR look = XMVector3Normalize(XMVectorSet(
            cosf(pitchRad) * sinf(yawRad),
           -sinf(pitchRad),
            cosf(pitchRad) * cosf(yawRad),
            0.0f));
        XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
        XMVECTOR right   = XMVector3Normalize(XMVector3Cross(worldUp, look));
        XMVECTOR up      = XMVector3Cross(look, right);

        // WASD + QE movement
        float speed = m_freeMoveSpeed * deltaTime;
        XMVECTOR pos = XMLoadFloat3(&m_freeCamPos);
        if (bForward)  pos = pos + look  * speed;
        if (bBackward) pos = pos - look  * speed;
        if (bRight)    pos = pos + right * speed;
        if (bLeft)     pos = pos - right * speed;
        if (bUp)       pos = pos + worldUp * speed;
        if (bDown)     pos = pos - worldUp * speed;
        XMStoreFloat3(&m_freeCamPos, pos);
        XMStoreFloat3(&m_position, pos);

        // Build view matrix
        XMVECTOR target = pos + look;
        XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
        XMStoreFloat4x4(&m_viewMatrix, view);
        return;
    }

    // === Orbit Camera Mode (original) ===
    // Update yaw and pitch from mouse input
    //m_yaw += mouseDeltaX * m_rotationSpeed;
    //m_pitch += mouseDeltaY * m_rotationSpeed;

    // Clamp pitch to avoid flipping
    m_pitch = max(-89.0f, min(89.0f, m_pitch));

    // Update distance from scroll wheel input
    m_distance -= scrollDelta * m_zoomSpeed;
    m_distance = max(m_minDistance, min(m_maxDistance, m_distance));

    // Update camera shake
    if (m_bShaking && deltaTime > 0.0f)
    {
        m_fShakeTimer += deltaTime;
        if (m_fShakeTimer >= m_fShakeDuration)
        {
            StopShake();
        }
        else
        {
            // Shake intensity decreases over time
            float fRemainingRatio = 1.0f - (m_fShakeTimer / m_fShakeDuration);
            float fCurrentIntensity = m_fShakeIntensity * fRemainingRatio;

            // Random offset
            m_shakeOffset.x = ((float)(rand() % 1000) / 500.0f - 1.0f) * fCurrentIntensity;
            m_shakeOffset.y = ((float)(rand() % 1000) / 500.0f - 1.0f) * fCurrentIntensity;
            m_shakeOffset.z = ((float)(rand() % 1000) / 500.0f - 1.0f) * fCurrentIntensity * 0.5f;
        }
    }

    // Recalculate the view matrix
    UpdateViewMatrix();
}

void CCamera::StartShake(float fIntensity, float fDuration)
{
    m_bShaking = true;
    m_fShakeIntensity = fIntensity;
    m_fShakeDuration = fDuration;
    m_fShakeTimer = 0.0f;
}

void CCamera::StopShake()
{
    m_bShaking = false;
    m_fShakeTimer = 0.0f;
    m_shakeOffset = { 0.0f, 0.0f, 0.0f };
}

void CCamera::UpdateViewMatrix()
{
    XMVECTOR lookAtPoint;
    float yawRad, pitchRad, dist;

    if (m_bCinematic)
    {
        // Orbit around fixed world-space point
        lookAtPoint = XMLoadFloat3(&m_cinLookAt);
        yawRad   = XMConvertToRadians(m_fCinYaw);
        pitchRad = XMConvertToRadians(m_fCinPitch);
        dist     = m_fCinDist;
    }
    else
    {
        if (!m_pTarget) return;
        yawRad   = XMConvertToRadians(m_yaw);
        pitchRad = XMConvertToRadians(m_pitch);
        dist     = m_distance;

        TransformComponent* pTransform = m_pTarget->GetTransform();
        XMVECTOR targetPos = XMLoadFloat3(&pTransform->GetPosition());
        lookAtPoint = targetPos + XMLoadFloat3(&m_lookAtOffset);
    }

    // Calculate the camera's position based on orbit parameters
    float x = dist * cosf(pitchRad) * sinf(yawRad);
    float y = dist * sinf(pitchRad);
    float z = dist * cosf(pitchRad) * cosf(yawRad);

    XMVECTOR cameraPos = lookAtPoint + XMVectorSet(x, y, z, 0.0f);

    // Apply shake offset
    if (m_bShaking)
    {
        cameraPos = cameraPos + XMLoadFloat3(&m_shakeOffset);
    }

    XMStoreFloat3(&m_position, cameraPos);

    // Build the view matrix
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(cameraPos, lookAtPoint, up);
    XMStoreFloat4x4(&m_viewMatrix, view);
}

void CCamera::StartCinematic(const XMFLOAT3& lookAt, float distance, float pitch, float yaw)
{
    m_bCinematic = true;
    m_cinLookAt  = lookAt;
    m_fCinDist   = distance;
    m_fCinPitch  = pitch;
    m_fCinYaw    = yaw;
}

void CCamera::StopCinematic()
{
    m_bCinematic = false;
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