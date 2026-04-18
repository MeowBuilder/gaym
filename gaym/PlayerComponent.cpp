#include "stdafx.h"
#include "PlayerComponent.h"
#include "InputSystem.h" // Needed for InputSystem
#include "GameObject.h" // Needed for GameObject
#include "TransformComponent.h" // Needed for TransformComponent
#include "Camera.h" // Needed for CCamera
#include "SkillComponent.h" // Needed for SkillComponent
#include "AnimationComponent.h"
#include "Dx12App.h" // For runtime window size
#include "NetworkManager.h" // For rotation sync

PlayerComponent::PlayerComponent(GameObject* pOwner)
    : Component(pOwner)
{
}

void PlayerComponent::PlayerUpdate(float deltaTime, InputSystem* pInputSystem, CCamera* pCamera)
{
    if (!m_pOwner) return;

    TransformComponent* pTransform = m_pOwner->GetTransform();
    if (!pTransform) return;

    // Apply gravity
    XMFLOAT3 pos = pTransform->GetPosition();

    // Fall zone: safe AABB 바깥 = 낙하 허용, 안쪽 = 수면에 뜸(차오르는 물 따라 상승)
    bool bOutsideSafe = false;
    float effectiveGroundY = GROUND_Y;  // 기본 바닥(타일 Y=0)
    if (m_bFallZoneActive)
    {
        float dx = pos.x - m_xmf3SafeCenter.x;
        float dz = pos.z - m_xmf3SafeCenter.z;
        bOutsideSafe = (fabsf(dx) > m_xmf3SafeExtents.x) || (fabsf(dz) > m_xmf3SafeExtents.z);
        if (bOutsideSafe)
        {
            m_bOnGround = false;  // 물 밖 = 지지 없음
        }
        else
        {
            // 안전존 내부에서는 수면이 타일 위로 올라오면 수면이 새 바닥
            effectiveGroundY = fmaxf(GROUND_Y, m_fFallZoneWaterY);
            if (pos.y < effectiveGroundY)
            {
                // 물이 플레이어 발밑을 넘었으니 수면에 띄움
                pos.y = effectiveGroundY;
                m_fVelocityY = 0.0f;
                m_bOnGround = true;
            }
        }
    }

    if (!m_bOnGround)
    {
        m_fVelocityY -= GRAVITY * deltaTime;
        pos.y += m_fVelocityY * deltaTime;

        if (!bOutsideSafe && pos.y <= effectiveGroundY)
        {
            pos.y = effectiveGroundY;
            m_fVelocityY = 0.0f;
            m_bOnGround = true;
        }
        pTransform->SetPosition(pos);

        // 낙사: 안전존 밖에서 사망 Y 이하 → 즉사
        if (bOutsideSafe && pos.y <= FALL_DEATH_Y && !IsDead())
        {
            TakeDamage(m_fMaxHP);
        }
    }
    else
    {
        // 수면 상승 반영 (이미 bOnGround=true 상태에서도 Y 업데이트)
        pTransform->SetPosition(pos);
    }

    // --- Aim-at-cursor Rotation Logic ---

    // 1. Get mouse position in screen space
    XMFLOAT2 mousePos = pInputSystem->GetMousePosition();

    // 2. Convert to Normalized Device Coordinates (NDC)
    // 런타임 윈도우 크기 사용 (고DPI/해상도 변경 대응)
    float windowWidth = static_cast<float>(Dx12App::GetInstance()->GetWindowWidth());
    float windowHeight = static_cast<float>(Dx12App::GetInstance()->GetWindowHeight());
    float ndcX = (2.0f * mousePos.x / windowWidth) - 1.0f;
    float ndcY = 1.0f - (2.0f * mousePos.y / windowHeight);

    // 3. Unproject from NDC to World Space to form a ray
    XMMATRIX viewMatrix = XMLoadFloat4x4(&pCamera->GetViewMatrix());
    XMMATRIX projMatrix = XMLoadFloat4x4(&pCamera->GetProjectionMatrix());
    XMMATRIX viewProjMatrix = viewMatrix * projMatrix;
    XMMATRIX invViewProjMatrix = XMMatrixInverse(nullptr, viewProjMatrix);

    XMVECTOR rayOrigin = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 0.0f, 1.0f), invViewProjMatrix); // Near plane
    XMVECTOR rayEnd = XMVector3TransformCoord(XMVectorSet(ndcX, ndcY, 1.0f, 1.0f), invViewProjMatrix);    // Far plane
    XMVECTOR rayDir = XMVector3Normalize(rayEnd - rayOrigin);

    // 4. Define the ground plane at player's floor height
    XMVECTOR groundPlane = XMPlaneFromPointNormal(XMVectorSet(0.0f, GROUND_Y, 0.0f, 0.0f), XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f));

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
    bool bRotationChanged = false;
	if (XMVectorGetX(XMVector3LengthSq(lookDir)) > 0.001f)
	{
		// Convert look direction to a yaw angle
        float yawRad = atan2f(XMVectorGetX(lookDir), XMVectorGetZ(lookDir));
        float yawDeg = XMConvertToDegrees(yawRad);

        // 회전 변경 감지 (임계값 이상 변화 시)
        float yawDiff = fabsf(yawDeg - m_fPrevYaw);
        // 360도 경계 처리 (예: 359 -> 1도 변화는 2도로 처리)
        if (yawDiff > 180.0f) yawDiff = 360.0f - yawDiff;
        if (yawDiff >= YAW_SYNC_THRESHOLD)
        {
            bRotationChanged = true;
            m_fPrevYaw = yawDeg;
        }

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

    bool bMoving = XMVectorGetX(XMVector3LengthSq(moveDir)) > 0.001f;

    // Normalize movement direction to keep speed consistent (even diagonally)
    if (bMoving)
    {
        moveDir = XMVector3Normalize(moveDir);
        displacement = moveDir * moveSpeed * deltaTime;
    }

    // Apply displacement (keep current Y from gravity system)
    currentPosition += displacement;
    float currentY = pTransform->GetPosition().y;
    pTransform->SetPosition(XMFLOAT3(XMVectorGetX(currentPosition), currentY, XMVectorGetZ(currentPosition)));

    // --- 네트워크 동기화: 이동 또는 회전 변경 시 전송 ---
    if (bMoving || bRotationChanged)
    {
        NetworkManager* pNetMgr = NetworkManager::GetInstance();
        if (pNetMgr && pNetMgr->IsConnected())
        {
            const XMFLOAT3& finalPos = pTransform->GetPosition();
            XMVECTOR lookVec = pTransform->GetLook();
            XMFLOAT3 lookDir3;
            XMStoreFloat3(&lookDir3, lookVec);

            pNetMgr->SendMove(finalPos.x, finalPos.y, finalPos.z, lookDir3.x, lookDir3.y, lookDir3.z);
        }
    }

    // --- Skill Input Processing ---
    bool bAttackTriggered = pInputSystem->IsKeyPressed('Q')
                         || pInputSystem->IsKeyPressed('E')
                         || pInputSystem->IsKeyPressed('R')
                         || pInputSystem->IsMouseButtonPressed(1);

    SkillComponent* pSkillComponent = m_pOwner->GetComponent<SkillComponent>();
    if (pSkillComponent)
    {
        pSkillComponent->ProcessSkillInput(pInputSystem, pCamera);
    }

    UpdateAnimation(deltaTime, bMoving, bAttackTriggered);
}

void PlayerComponent::EnableFallZone(const XMFLOAT3& safeCenter, const XMFLOAT3& safeExtents)
{
    m_bFallZoneActive = true;
    m_xmf3SafeCenter  = safeCenter;
    m_xmf3SafeExtents = safeExtents;
}

void PlayerComponent::TakeDamage(float fDamage)
{
    if (fDamage <= 0.0f || IsDead()) return;

    m_fCurrentHP -= fDamage;
    if (m_fCurrentHP < 0.0f)
    {
        m_fCurrentHP = 0.0f;
    }
}

void PlayerComponent::Heal(float fAmount)
{
    if (fAmount <= 0.0f || IsDead()) return;

    m_fCurrentHP += fAmount;
    if (m_fCurrentHP > m_fMaxHP)
    {
        m_fCurrentHP = m_fMaxHP;
    }
}

void PlayerComponent::UpdateAnimation(float deltaTime, bool bMoving, bool bAttackTriggered)
{
    AnimationComponent* pAnim = m_pOwner->GetComponent<AnimationComponent>();
    if (!pAnim) return;

    // Tick down attack timer
    if (m_fAttackTimer > 0.0f)
        m_fAttackTimer -= deltaTime;

    // If attack triggered, always restart attack animation
    if (bAttackTriggered)
    {
        m_fAttackTimer = kAttackAnimDuration;
        m_eAnimState = PlayerAnimState::Attack;
        pAnim->CrossFade("Attack1", 0.1f, false, true);  // forceRestart = true
        return;
    }

    // Determine desired state (Attack > Walk > Idle)
    PlayerAnimState desiredState;
    if (m_fAttackTimer > 0.0f)
    {
        desiredState = PlayerAnimState::Attack;
    }
    else if (bMoving)
    {
        desiredState = PlayerAnimState::Walk;
    }
    else
    {
        desiredState = PlayerAnimState::Idle;
    }

    if (desiredState == m_eAnimState) return;
    m_eAnimState = desiredState;

    switch (m_eAnimState)
    {
    case PlayerAnimState::Idle:
        pAnim->CrossFade("Idle", 0.2f, true);
        break;
    case PlayerAnimState::Walk:
        pAnim->CrossFade("Walk", 0.15f, true);
        break;
    case PlayerAnimState::Attack:
        pAnim->CrossFade("Attack1", 0.1f, false);
        break;
    }
}
