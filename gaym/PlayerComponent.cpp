#include "stdafx.h"
#include "PlayerComponent.h"
#include "InputSystem.h" // Needed for InputSystem
#include "GameObject.h" // Needed for GameObject
#include "TransformComponent.h" // Needed for TransformComponent
#include "Camera.h" // Needed for CCamera

PlayerComponent::PlayerComponent(GameObject* pOwner)
    : Component(pOwner)
{
}

void PlayerComponent::PlayerUpdate(float deltaTime, InputSystem* pInputSystem, CCamera* pCamera)
{
    if (!m_pOwner) return;

    TransformComponent* pTransform = m_pOwner->GetTransform();
    if (!pTransform) return;

    // --- Aim-at-cursor Rotation Logic ---

    // 1. Get mouse position in screen space
    XMFLOAT2 mousePos = pInputSystem->GetMousePosition();

    // 2. Convert to Normalized Device Coordinates (NDC)
    float ndcX = (2.0f * mousePos.x / kWindowWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mousePos.y / kWindowHeight);

    // 3. Unproject from NDC to World Space to form a ray
    XMMATRIX viewMatrix = XMLoadFloat4x4(&pCamera->GetViewMatrix());
    XMMATRIX projMatrix = XMLoadFloat4x4(&pCamera->GetProjectionMatrix());
    XMMATRIX viewProjMatrix = viewMatrix * projMatrix;
    XMMATRIX invViewProjMatrix = XMMatrixInverse(nullptr, viewProjMatrix);

    XMVECTOR rayOrigin = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invViewProjMatrix); // Near plane
    XMVECTOR rayEnd = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invViewProjMatrix);    // Far plane
    XMVECTOR rayDir = XMVector3Normalize(rayEnd - rayOrigin);

    // 4. Define the ground plane (Y=0)
    XMVECTOR groundPlane = XMPlaneFromPointNormal(XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

    // 5. Find intersection of the ray and the ground plane
	// XMPlaneIntersectLine requires two points on the line, not a point and a direction.
    XMVECTOR intersectionPoint = XMPlaneIntersectLine(groundPlane, rayOrigin, rayOrigin + rayDir * 1000.0f);


    // 6. Make the player look at the intersection point
    XMVECTOR playerPos = XMLoadFloat3(&pTransform->GetPosition());
    XMVECTOR lookDir = intersectionPoint - playerPos;
    
    // Set Y-component of look direction to 0 to prevent character from tilting up/down
    lookDir = XMVectorSetY(lookDir, 0.0f); 
    lookDir = XMVector3Normalize(lookDir);

	// Check if the look direction is valid before setting it, to prevent generating NaNs
	if (XMVectorGetX(XMVector3LengthSq(lookDir)) > 0.001f)
	{
		// Convert look direction to a yaw angle
        float yawRad = atan2f(XMVectorGetX(lookDir), XMVectorGetZ(lookDir));
        float yawDeg = XMConvertToDegrees(yawRad);

        // Get current rotation, only overwrite yaw
        const XMFLOAT3& currentRot = pTransform->GetRotation();
        pTransform->SetRotation(currentRot.x, yawDeg, currentRot.z);
	}


    // --- Movement Logic (Camera-Relative) ---

    float moveSpeed = 20.0f; // Increased speed for better feel

    // Get camera's axes and flatten them to the XZ plane (ground)
    // This makes WASD movement relative to the screen/camera view.
    XMVECTOR camLook = pCamera->GetLookDirection();
    XMVECTOR camRight = pCamera->GetRightDirection();

    camLook = XMVectorSetY(camLook, 0.0f);
    camRight = XMVectorSetY(camRight, 0.0f);

    camLook = XMVector3Normalize(camLook);
    camRight = XMVector3Normalize(camRight);

    XMVECTOR currentPosition = XMLoadFloat3(&pTransform->GetPosition());
    XMVECTOR displacement = XMVectorZero();

    // Keyboard input for movement (Camera-relative)
    XMVECTOR moveDir = XMVectorZero();
    if (pInputSystem->IsKeyDown('W')) moveDir += camLook;
    if (pInputSystem->IsKeyDown('S')) moveDir -= camLook;
    if (pInputSystem->IsKeyDown('A')) moveDir -= camRight;
    if (pInputSystem->IsKeyDown('D')) moveDir += camRight;

    // Normalize movement direction to keep speed consistent (even diagonally)
    if (XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.001f)
    {
        moveDir = XMVector3Normalize(moveDir);
        displacement = moveDir * moveSpeed * deltaTime;
    }

    // Apply displacement
    currentPosition += displacement;
    pTransform->SetPosition(XMFLOAT3(XMVectorGetX(currentPosition), XMVectorGetY(currentPosition), XMVectorGetZ(currentPosition)));
}
