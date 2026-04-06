#include "stdafx.h"
#include "MegaBreathAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "PlayerComponent.h"
#include "ColliderComponent.h"
#include "RenderComponent.h"
#include "AnimationComponent.h"
#include "Room.h"
#include "Scene.h"
#include "Shader.h"
#include "Mesh.h"
#include "MathUtils.h"
#include "Dx12App.h"
#include "Camera.h"
#include "FluidSkillVFXManager.h"
#include "VFXLibrary.h"
#include "MapLoader.h"

MegaBreathAttackBehavior::MegaBreathAttackBehavior(
    float fDamagePerTick,
    float fTickInterval,
    float fMoveSpeed,
    float fMoveToWallTime,
    float fWindupTime,
    float fBreathDuration,
    float fRecoveryTime,
    float fCoverObjectSize)
    : m_fDamagePerTick(fDamagePerTick)
    , m_fTickInterval(fTickInterval)
    , m_fMoveSpeed(fMoveSpeed)
    , m_fMoveToWallTime(fMoveToWallTime)
    , m_fWindupTime(fWindupTime)
    , m_fBreathDuration(fBreathDuration)
    , m_fRecoveryTime(fRecoveryTime)
    , m_fCoverObjectSize(fCoverObjectSize)
{
}

void MegaBreathAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (!pEnemy) return;

    // EnemyComponent에서 Room 참조 가져오기
    m_pRoom = pEnemy->GetRoom();
    if (!m_pRoom) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    // 시작 위치 저장
    m_xmf3StartPosition = pTransform->GetPosition();

    // 벽 위치 계산
    m_xmf3WallPosition = CalculateWallPosition();

    // 무적 설정
    pEnemy->SetInvincible(true);

    // 이륙 애니메이션 시작
    if (AnimationComponent* pAnim = pEnemy->GetAnimationComponent())
    {
        pAnim->CrossFade("Take Off", 0.15f, false);
    }

    m_ePhase = Phase::TakeOff;

    OutputDebugString(L"[MegaBreath] Attack started - taking off\n");
}

void MegaBreathAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
{
    if (m_bFinished) return;
    if (!pEnemy) return;

    m_fTimer += dt;

    // 현재 페이즈 로그 (매 프레임 출력하면 너무 많으니 타이머 기반)
    static float logTimer = 0.0f;
    logTimer += dt;
    if (logTimer >= 0.5f)
    {
        wchar_t buf[128];
        swprintf_s(buf, L"[MegaBreath] Update: Phase=%d, Timer=%.2f\n", (int)m_ePhase, m_fTimer);
        OutputDebugString(buf);
        logTimer = 0.0f;
    }

    switch (m_ePhase)
    {
    case Phase::TakeOff:
        // 이륙 중 - 높이 상승
        {
            GameObject* pOwner = pEnemy->GetOwner();
            if (pOwner)
            {
                TransformComponent* pTransform = pOwner->GetTransform();
                if (pTransform)
                {
                    float t = m_fTimer / m_fTakeOffTime;
                    t = min(t, 1.0f);
                    float smoothT = t * t;  // easeIn

                    XMFLOAT3 pos = m_xmf3StartPosition;
                    pos.y = m_xmf3StartPosition.y + m_fFlyHeight * smoothT;
                    pTransform->SetPosition(pos);
                }
            }
        }

        if (m_fTimer >= m_fTakeOffTime)
        {
            // 비행 애니메이션으로 전환
            if (AnimationComponent* pAnim = pEnemy->GetAnimationComponent())
            {
                pAnim->CrossFade("Fly Glide", 0.2f, true);
            }

            m_ePhase = Phase::MoveToWall;
            m_fTimer = 0.0f;

            // 시작 위치 업데이트 (현재 높이 포함)
            if (GameObject* pOwner = pEnemy->GetOwner())
            {
                if (TransformComponent* pTransform = pOwner->GetTransform())
                {
                    m_xmf3StartPosition = pTransform->GetPosition();
                    m_xmf3WallPosition.y = m_xmf3StartPosition.y;  // 비행 높이 유지
                }
            }

            OutputDebugString(L"[MegaBreath] Flying to wall\n");
        }
        break;

    case Phase::MoveToWall:
        UpdateMoveToWall(dt, pEnemy);
        if (m_fTimer >= m_fMoveToWallTime)
        {
            // 착륙 애니메이션
            if (AnimationComponent* pAnim = pEnemy->GetAnimationComponent())
            {
                pAnim->CrossFade("Land", 0.15f, false);
            }

            // 착륙 시작 위치 저장
            if (GameObject* pOwner = pEnemy->GetOwner())
            {
                if (TransformComponent* pTransform = pOwner->GetTransform())
                {
                    m_xmf3StartPosition = pTransform->GetPosition();
                }
            }

            m_ePhase = Phase::Landing;
            m_fTimer = 0.0f;
            OutputDebugString(L"[MegaBreath] Landing\n");
        }
        break;

    case Phase::Landing:
        // 착륙 중 - 높이 하강
        {
            GameObject* pOwner = pEnemy->GetOwner();
            if (pOwner)
            {
                TransformComponent* pTransform = pOwner->GetTransform();
                if (pTransform)
                {
                    float t = m_fTimer / m_fLandingTime;
                    t = min(t, 1.0f);
                    float smoothT = 1.0f - (1.0f - t) * (1.0f - t);  // easeOut

                    XMFLOAT3 pos = m_xmf3StartPosition;
                    pos.y = m_xmf3StartPosition.y - m_fFlyHeight * smoothT;
                    pTransform->SetPosition(pos);
                }
            }
        }

        if (m_fTimer >= m_fLandingTime)
        {
            m_ePhase = Phase::SpawnCover;
            m_fTimer = 0.0f;
            OutputDebugString(L"[MegaBreath] Spawning cover objects\n");
        }
        break;

    case Phase::SpawnCover:
        SpawnCoverObjects(pEnemy);
        m_ePhase = Phase::Windup;
        m_fTimer = 0.0f;

        // 브레스 원점 설정 (현재 보스 위치)
        if (GameObject* pOwner = pEnemy->GetOwner())
        {
            if (TransformComponent* pTransform = pOwner->GetTransform())
            {
                m_xmf3BreathOrigin = pTransform->GetPosition();
                // 브레스 원점을 약간 앞으로 (보스 입에서 발사하는 느낌)
                m_xmf3BreathOrigin.y += 3.0f;
            }
        }

        OutputDebugString(L"[MegaBreath] Windup phase - preparing breath\n");
        break;

    case Phase::Windup:
        // 플레이어를 바라봄 (방 중앙 방향)
        {
            const BoundingBox& roomBox = m_pRoom->GetBoundingBox();
            GameObject* pOwner = pEnemy->GetOwner();
            if (pOwner)
            {
                TransformComponent* pTransform = pOwner->GetTransform();
                if (pTransform)
                {
                    XMFLOAT3 pos = pTransform->GetPosition();
                    XMFLOAT3 center = { roomBox.Center.x, pos.y, roomBox.Center.z };
                    XMFLOAT2 dir = MathUtils::Direction2D(pos, center);
                    float yaw = atan2f(dir.x, dir.y) * (180.0f / XM_PI);
                    pTransform->SetRotation(0.0f, yaw, 0.0f);
                }
            }
        }

        if (m_fTimer >= m_fWindupTime)
        {
            m_ePhase = Phase::Breath;
            m_fTimer = 0.0f;
            m_fDamageTickTimer = 0.0f;

            // 브레스 애니메이션 시작 (루프)
            AnimationComponent* pAnim = pEnemy->GetAnimationComponent();
            if (pAnim)
            {
                pAnim->CrossFade("Flame Attack", 0.2f, true);  // 루프로 재생
                m_bBreathAnimStarted = true;
            }

            // 카메라 쉐이킹 시작
            if (Scene* pScene = m_pRoom->GetScene())
            {
                if (CCamera* pCamera = pScene->GetCamera())
                {
                    pCamera->StartShake(1.5f, m_fBreathDuration);
                }

                // 입에서 불 내뿜는 유체 VFX 생성
                m_pFluidVFXManager = pScene->GetFluidVFXManager();
                if (m_pFluidVFXManager)
                {
                    // VFXLibrary에서 Boss Mega Breath 정의 가져오기 (키 999 사용)
                    VFXSequenceDef bossDef = VFXLibrary::Get().GetDef((SkillSlot)0, 999, ElementType::Fire);
                    
                    GameObject* pOwner = pEnemy->GetOwner();
                    TransformComponent* pTransform = pOwner->GetTransform();
                    XMFLOAT3 pos = pTransform->GetPosition();
                    XMFLOAT3 rot = pTransform->GetRotation();
                    float yawRad = XMConvertToRadians(rot.y);

                    // 입 위치: 지면에 더 가깝게 (1.0f)
                    XMFLOAT3 mouthPos;
                    mouthPos.x = pos.x + sinf(yawRad) * 12.0f;
                    mouthPos.y = pos.y + 1.0f;
                    mouthPos.z = pos.z + cosf(yawRad) * 12.0f;

                    // 전방 방향: 지면을 향해 더 깊게 (-0.4f) 꺾음
                    XMVECTOR fwdV = XMVectorSet(sinf(yawRad), -0.4f, cosf(yawRad), 0.0f);
                    XMFLOAT3 fwd;
                    XMStoreFloat3(&fwd, XMVector3Normalize(fwdV));

                    m_nFluidVFXId = m_pFluidVFXManager->SpawnSequenceEffect(mouthPos, fwd, bossDef);
                    OutputDebugString(L"[MegaBreath] Fluid MegaBreath VFX started!\n");
                }
            }

            OutputDebugString(L"[MegaBreath] Breath phase - FIRING!\n");
        }
        break;

    case Phase::Breath:
        m_fDamageTickTimer += dt;
        if (m_fDamageTickTimer >= m_fTickInterval)
        {
            ApplyBreathDamage(pEnemy);
            m_fDamageTickTimer = 0.0f;
        }

        // 유체 VFX 위치/방향 업데이트 (보스 고개 방향 추적)
        if (m_pFluidVFXManager && m_nFluidVFXId >= 0)
        {
            GameObject* pOwner = pEnemy->GetOwner();
            if (pOwner)
            {
                TransformComponent* pTransform = pOwner->GetTransform();
                if (pTransform)
                {
                    XMFLOAT3 pos = pTransform->GetPosition();
                    XMFLOAT3 rot = pTransform->GetRotation();
                    float yawRad = XMConvertToRadians(rot.y);

                    // 입 위치
                    XMFLOAT3 mouthPos;
                    mouthPos.x = pos.x + sinf(yawRad) * 8.0f;
                    mouthPos.y = pos.y + 4.0f;
                    mouthPos.z = pos.z + cosf(yawRad) * 8.0f;

                    // 전방 방향
                    XMFLOAT3 fwd = { sinf(yawRad), 0.0f, cosf(yawRad) };

                    m_pFluidVFXManager->TrackEffect(m_nFluidVFXId, mouthPos, fwd);
                }
            }
        }

        if (m_fTimer >= m_fBreathDuration)
        {
            m_ePhase = Phase::Recovery;
            m_fTimer = 0.0f;
            OutputDebugString(L"[MegaBreath] Recovery phase\n");
        }
        break;

    case Phase::Recovery:
        // 첫 프레임에만 정리 작업 수행
        if (m_bBreathAnimStarted)
        {
            // 카메라 쉐이킹 중지
            if (Scene* pScene = m_pRoom->GetScene())
            {
                if (CCamera* pCamera = pScene->GetCamera())
                {
                    pCamera->StopShake();
                }
            }

            // 브레스 유체 VFX 중지
            if (m_pFluidVFXManager && m_nFluidVFXId >= 0)
            {
                m_pFluidVFXManager->StopEffect(m_nFluidVFXId);
                m_nFluidVFXId = -1;
                OutputDebugString(L"[MegaBreath] Fluid MegaBreath VFX stopped\n");
            }

            m_bBreathAnimStarted = false;
        }

        if (m_fTimer >= m_fRecoveryTime)
        {
            // 엄폐물 제거
            DestroyCoverObjects();

            // 무적 해제
            pEnemy->SetInvincible(false);

            m_bFinished = true;
            OutputDebugString(L"[MegaBreath] Attack finished\n");
        }
        break;
    }
}

bool MegaBreathAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void MegaBreathAttackBehavior::Reset()
{
    m_ePhase = Phase::TakeOff;
    m_fTimer = 0.0f;
    m_fDamageTickTimer = 0.0f;
    m_bFinished = false;
    m_xmf3WallPosition = { 0.0f, 0.0f, 0.0f };
    m_xmf3StartPosition = { 0.0f, 0.0f, 0.0f };
    m_xmf3BreathOrigin = { 0.0f, 0.0f, 0.0f };
    m_nWallDirection = 0;
    m_bBreathAnimStarted = false;

    // 유체 VFX 정리
    if (m_pFluidVFXManager && m_nFluidVFXId >= 0)
    {
        m_pFluidVFXManager->StopEffect(m_nFluidVFXId);
        m_nFluidVFXId = -1;
    }
    m_pFluidVFXManager = nullptr;

    // 남아있는 엄폐물이 있으면 정리
    DestroyCoverObjects();
}

XMFLOAT3 MegaBreathAttackBehavior::CalculateWallPosition()
{
    if (!m_pRoom) return XMFLOAT3(0.0f, 0.0f, 0.0f);

    const BoundingBox& roomBox = m_pRoom->GetBoundingBox();

    // 방의 경계 계산
    float minX = roomBox.Center.x - roomBox.Extents.x;
    float maxX = roomBox.Center.x + roomBox.Extents.x;
    float minZ = roomBox.Center.z - roomBox.Extents.z;
    float maxZ = roomBox.Center.z + roomBox.Extents.z;

    // 4방향 중 랜덤 선택
    m_nWallDirection = rand() % 4;

    XMFLOAT3 wallPos = { roomBox.Center.x, 0.0f, roomBox.Center.z };
    const float WALL_OFFSET = 15.0f;  // 벽에서 충분히 안쪽

    switch (m_nWallDirection)
    {
    case 0: // +X (오른쪽 벽)
        wallPos.x = maxX - WALL_OFFSET;
        break;
    case 1: // -X (왼쪽 벽)
        wallPos.x = minX + WALL_OFFSET;
        break;
    case 2: // +Z (앞쪽 벽)
        wallPos.z = maxZ - WALL_OFFSET;
        break;
    case 3: // -Z (뒤쪽 벽)
        wallPos.z = minZ + WALL_OFFSET;
        break;
    }

    wchar_t buf[128];
    swprintf_s(buf, L"[MegaBreath] Wall position: (%.1f, %.1f, %.1f) direction=%d\n",
               wallPos.x, wallPos.y, wallPos.z, m_nWallDirection);
    OutputDebugString(buf);

    return wallPos;
}

void MegaBreathAttackBehavior::UpdateMoveToWall(float dt, EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;

    TransformComponent* pTransform = pOwner->GetTransform();
    if (!pTransform) return;

    // 현재 위치에서 목표 위치로 보간
    float t = m_fTimer / m_fMoveToWallTime;
    t = min(t, 1.0f);

    // 부드러운 이동 (easeOutQuad)
    float smoothT = 1.0f - (1.0f - t) * (1.0f - t);

    XMFLOAT3 newPos;
    newPos.x = m_xmf3StartPosition.x + (m_xmf3WallPosition.x - m_xmf3StartPosition.x) * smoothT;
    newPos.y = m_xmf3StartPosition.y + (m_xmf3WallPosition.y - m_xmf3StartPosition.y) * smoothT;
    newPos.z = m_xmf3StartPosition.z + (m_xmf3WallPosition.z - m_xmf3StartPosition.z) * smoothT;

    pTransform->SetPosition(newPos);

    // 이동 방향을 바라봄
    if (t < 1.0f)
    {
        XMFLOAT2 dir = MathUtils::Direction2D(pTransform->GetPosition(), m_xmf3WallPosition);
        if (dir.x != 0.0f || dir.y != 0.0f)
        {
            float yaw = atan2f(dir.x, dir.y) * (180.0f / XM_PI);
            pTransform->SetRotation(0.0f, yaw, 0.0f);
        }
    }
}

void MegaBreathAttackBehavior::SpawnCoverObjects(EnemyComponent* pEnemy)
{
    if (!m_pRoom) return;

    Scene* pScene = m_pRoom->GetScene();
    if (!pScene) return;

    Dx12App* pApp = Dx12App::GetInstance();
    if (!pApp) return;

    ID3D12Device* pDevice = pApp->GetDevice();
    ID3D12GraphicsCommandList* pCommandList = pApp->GetCommandList();
    Shader* pShader = pScene->GetDefaultShader();

    const BoundingBox& roomBox = m_pRoom->GetBoundingBox();
    XMFLOAT3 center = { roomBox.Center.x, 0.0f, roomBox.Center.z };

    // 방 크기에 비례한 엄폐물 배치 거리
    float coverDist = min(roomBox.Extents.x, roomBox.Extents.z) * 0.5f;

    // 4개의 엄폐물 위치 (방 중앙 주변)
    XMFLOAT3 coverPositions[4] = {
        { center.x + coverDist, 0.0f, center.z },
        { center.x - coverDist, 0.0f, center.z },
        { center.x, 0.0f, center.z + coverDist },
        { center.x, 0.0f, center.z - coverDist }
    };

    m_vObstacles.clear();
    m_vCoverObjects.clear();

    for (int i = 0; i < 4; ++i)
    {
        // 엄폐물의 BoundingBox를 장애물 목록에 추가
        BoundingBox coverBox;
        coverBox.Center = { coverPositions[i].x, coverPositions[i].y + m_fCoverObjectSize, coverPositions[i].z };
        coverBox.Extents = { m_fCoverObjectSize, m_fCoverObjectSize * 2.0f, m_fCoverObjectSize };
        m_vObstacles.push_back(coverBox);

        // 실제 GameObject 생성
        if (pDevice && pCommandList && pShader)
        {
            // Room을 현재 방으로 설정하여 오브젝트가 방에 추가되도록 함
            CRoom* pPrevRoom = pScene->GetCurrentRoom();
            pScene->SetCurrentRoom(m_pRoom);

            GameObject* pCover = pScene->CreateGameObject(pDevice, pCommandList);
            if (pCover)
            {
                // 위치/스케일 설정
                TransformComponent* pTransform = pCover->GetTransform();
                if (pTransform)
                {
                    pTransform->SetPosition(coverPositions[i].x, coverPositions[i].y, coverPositions[i].z);
                    // 우물 모델에 맞는 거대한 스케일 설정
                    pTransform->SetScale(5.0f, 5.0f, 5.0f);
                }

                // WellSmall_001.obj 모델 로드
                Mesh* pWellMesh = MapLoader::LoadMesh("Assets/MapData/meshes/ColumnBig_001.obj", pDevice, pCommandList);
                if (pWellMesh)
                {
                    pWellMesh->AddRef();
                    pCover->SetMesh(pWellMesh);
                }

                // 돌 느낌 재질
                MATERIAL material;
                material.m_cAmbient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
                material.m_cDiffuse = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
                material.m_cSpecular = XMFLOAT4(0.2f, 0.2f, 0.2f, 8.0f);
                material.m_cEmissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
                pCover->SetMaterial(material);

                // 렌더 컴포넌트
                auto* pRenderComp = pCover->AddComponent<RenderComponent>();
                if (pWellMesh) pRenderComp->SetMesh(pWellMesh);
                pShader->AddRenderComponent(pRenderComp);

                // 콜라이더 (Wall 레이어) - 모델 비주얼에 맞춰 히트박스 축소
                auto* pCollider = pCover->AddComponent<ColliderComponent>();
                // 가로 폭을 4.5 -> 1.5로 줄여 이동 방해 최소화, 높이는 유지
                pCollider->SetExtents(1.5f, 6.0f, 1.5f);
                pCollider->SetCenter(0.0f, 3.0f, 0.0f);
                pCollider->SetLayer(CollisionLayer::Wall);
                pCollider->SetCollisionMask(CollisionMask::Wall);

                // 엄폐 판정용 리스트에도 업데이트된 크기 반영
                coverBox.Extents = { 1.5f, 6.0f, 1.5f };
                m_vObstacles.back() = coverBox;

                pScene->SetCurrentRoom(pPrevRoom);
            }

            wchar_t buf[128];
            swprintf_s(buf, L"[MegaBreath] Cover %d at (%.1f, %.1f, %.1f)\n",
                i, coverPositions[i].x, coverPositions[i].y, coverPositions[i].z);
            OutputDebugString(buf);
        }

        // 방 내 기존 장애물(Wall 레이어)도 장애물 목록에 추가
        const auto& gameObjects = m_pRoom->GetGameObjects();
        for (const auto& pObj : gameObjects)
        {
            if (!pObj) continue;

            ColliderComponent* pCollider = pObj->GetComponent<ColliderComponent>();
            if (pCollider && pCollider->GetLayer() == CollisionLayer::Wall)
            {
                const BoundingOrientedBox& obb = pCollider->GetBoundingBox();
                // OBB를 AABB로 변환 (간단화)
                BoundingBox aabb;
                aabb.Center = obb.Center;
                aabb.Extents = obb.Extents;
                m_vObstacles.push_back(aabb);
            }
        }

        wchar_t buf2[64];
        swprintf_s(buf2, L"[MegaBreath] Total obstacles: %zu\n", m_vObstacles.size());
        OutputDebugString(buf2);
    }
}
void MegaBreathAttackBehavior::DestroyCoverObjects()
{
    // 엄폐물을 화면 밖으로 이동 (삭제 대신 - Update 루프 중 삭제 시 크래시 방지)
    for (GameObject* pCover : m_vCoverObjects)
    {
        if (pCover)
        {
            TransformComponent* pTransform = pCover->GetTransform();
            if (pTransform)
            {
                // 땅 밑으로 숨기기
                pTransform->SetPosition(0.0f, -1000.0f, 0.0f);
            }
        }
    }
    // 포인터는 유지 (Room이 소유권 가짐)
    m_vCoverObjects.clear();
    m_vObstacles.clear();

    OutputDebugString(L"[MegaBreath] Cover objects hidden\n");
}

bool MegaBreathAttackBehavior::IsPlayerBehindCover(const XMFLOAT3& breathOrigin, const XMFLOAT3& playerPos)
{
    // 브레스 원점에서 플레이어까지의 방향
    XMFLOAT3 dir;
    dir.x = playerPos.x - breathOrigin.x;
    dir.y = playerPos.y - breathOrigin.y;
    dir.z = playerPos.z - breathOrigin.z;

    float dist = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
    if (dist < 0.001f) return false;

    // 정규화
    dir.x /= dist;
    dir.y /= dist;
    dir.z /= dist;

    // 모든 장애물에 대해 Ray-AABB 교차 검사
    for (const BoundingBox& obstacle : m_vObstacles)
    {
        if (RayIntersectsAABB(breathOrigin, dir, obstacle, dist))
        {
            return true;  // 엄폐 성공
        }
    }

    return false;  // 노출됨
}

bool MegaBreathAttackBehavior::RayIntersectsAABB(const XMFLOAT3& rayOrigin, const XMFLOAT3& rayDir,
                                                  const BoundingBox& box, float maxDist)
{
    // Slab method를 사용한 Ray-AABB 교차 검사
    float tmin = 0.0f;
    float tmax = maxDist;

    // X축
    float boxMin = box.Center.x - box.Extents.x;
    float boxMax = box.Center.x + box.Extents.x;
    if (fabsf(rayDir.x) < 0.0001f)
    {
        if (rayOrigin.x < boxMin || rayOrigin.x > boxMax)
            return false;
    }
    else
    {
        float invD = 1.0f / rayDir.x;
        float t1 = (boxMin - rayOrigin.x) * invD;
        float t2 = (boxMax - rayOrigin.x) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tmin = max(tmin, t1);
        tmax = min(tmax, t2);
        if (tmin > tmax) return false;
    }

    // Y축
    boxMin = box.Center.y - box.Extents.y;
    boxMax = box.Center.y + box.Extents.y;
    if (fabsf(rayDir.y) < 0.0001f)
    {
        if (rayOrigin.y < boxMin || rayOrigin.y > boxMax)
            return false;
    }
    else
    {
        float invD = 1.0f / rayDir.y;
        float t1 = (boxMin - rayOrigin.y) * invD;
        float t2 = (boxMax - rayOrigin.y) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tmin = max(tmin, t1);
        tmax = min(tmax, t2);
        if (tmin > tmax) return false;
    }

    // Z축
    boxMin = box.Center.z - box.Extents.z;
    boxMax = box.Center.z + box.Extents.z;
    if (fabsf(rayDir.z) < 0.0001f)
    {
        if (rayOrigin.z < boxMin || rayOrigin.z > boxMax)
            return false;
    }
    else
    {
        float invD = 1.0f / rayDir.z;
        float t1 = (boxMin - rayOrigin.z) * invD;
        float t2 = (boxMax - rayOrigin.z) * invD;
        if (t1 > t2) std::swap(t1, t2);
        tmin = max(tmin, t1);
        tmax = min(tmax, t2);
        if (tmin > tmax) return false;
    }

    return true;
}

void MegaBreathAttackBehavior::ApplyBreathDamage(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pTarget = pEnemy->GetTarget();
    if (!pTarget) return;

    TransformComponent* pTargetTransform = pTarget->GetTransform();
    if (!pTargetTransform) return;

    XMFLOAT3 playerPos = pTargetTransform->GetPosition();
    playerPos.y += 1.0f;  // 플레이어 중심 높이 보정

    // 엄폐 판정
    if (IsPlayerBehindCover(m_xmf3BreathOrigin, playerPos))
    {
        // 엄폐 성공 - 데미지 없음
        OutputDebugString(L"[MegaBreath] Player behind cover - SAFE\n");
        return;
    }

    // 노출됨 - 데미지 적용
    PlayerComponent* pPlayer = pTarget->GetComponent<PlayerComponent>();
    if (pPlayer)
    {
        pPlayer->TakeDamage(m_fDamagePerTick);

        wchar_t buf[128];
        swprintf_s(buf, L"[MegaBreath] Player HIT! Dealt %.1f damage (HP: %.1f/%.1f)\n",
                   m_fDamagePerTick, pPlayer->GetCurrentHP(), pPlayer->GetMaxHP());
        OutputDebugString(buf);
    }
}
