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
#include "MapLoader.h"
#include "FluidSkillVFXManager.h"
#include "VFXLibrary.h"

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

    // 시작 위치 저장 (이 필드는 단계별로 덮어써짐)
    m_xmf3StartPosition = pTransform->GetPosition();
    // 원래 위치 — 공격 종료 후 복귀 목표 (덮어쓰지 않음)
    m_xmf3OriginalPosition = m_xmf3StartPosition;

    // 벽 위치 계산
    m_xmf3WallPosition = CalculateWallPosition();

    // 무적 설정
    pEnemy->SetInvincible(true);

    // 컷씬 중 용암 기믹 일시 중지 (화염 맵 전용 기믹이지만 컷씬 중엔 방해되므로 비활성화)
    m_pRoom->SetLavaGeyserEnabled(false);

    // 이륙 애니메이션 시작
    if (AnimationComponent* pAnim = pEnemy->GetAnimationComponent())
    {
        pAnim->CrossFade("Take Off", 0.15f, false);
    }

    m_ePhase = Phase::TakeOff;

}

void MegaBreathAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
{
    if (m_bFinished) return;
    if (!pEnemy) return;

    m_fTimer += dt;

    // 매 프레임 시네마틱 카메라 업데이트 (페이즈에 맞춰 앵글/타겟 결정 + 부드럽게 블렌드)
    UpdateCinematicCamera(dt, pEnemy);

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
        }
        break;

    case Phase::SpawnCover:
        if (!m_bCoverSpawned)
        {
            SpawnCoverObjects(pEnemy);
            m_bCoverSpawned = true;
        }
        // 엄폐 기둥이 눈에 보이도록 짧게 대기 (카메라가 기둥 생성 잡아줌)
        if (m_fTimer >= m_fCoverRevealTime)
        {
            m_ePhase = Phase::Windup;
            m_fTimer = 0.0f;
        }
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

        // Windup 진입 직후 집결 VFX 스폰 — 플레이어에게 "곧 터진다" 예고
        if (m_nChargeVFXId < 0) SpawnChargeVFX(pEnemy);

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

            // 카메라 쉐이킹 시작 + Fire Wave 스폰
            if (Scene* pScene = m_pRoom->GetScene())
            {
                if (CCamera* pCamera = pScene->GetCamera())
                {
                    pCamera->StartShake(2.5f, m_fBreathDuration);  // 거대 파도 강렬한 진동
                }
            }
            // 집결 VFX 정리 — 터진 순간 사라지고 Beam으로 대체
            DestroyChargeVFX();
            SpawnFireWave(pEnemy);

        }
        break;

    case Phase::Breath:
        // Fire Wave 진행 업데이트 (위치 전진 + ember 파티클 에미터 추적)
        UpdateFireWave(dt, pEnemy);

        // 데미지 틱: 파도 AABB 안에 있으면 맞음 (엄폐물이 파도 전면↔플레이어 사이 차단하면 세이프)
        m_fDamageTickTimer += dt;
        if (m_fDamageTickTimer >= m_fTickInterval)
        {
            ApplyBreathDamage(pEnemy);
            m_fDamageTickTimer = 0.0f;
        }

        if (m_fTimer >= m_fBreathDuration)
        {
            m_ePhase = Phase::Recovery;
            m_fTimer = 0.0f;
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

            // Fire Wave 정리 (벽 mesh 숨김 + ember 에미터 정지)
            DestroyFireWave();

            // 중앙 기둥 제거 — 브레스 끝난 직후 바로 사라지게
            DestroyCoverObjects();

            m_bBreathAnimStarted = false;
        }

        if (m_fTimer >= m_fRecoveryTime)
        {
            // 무적 해제 — 복귀 비행 중엔 보스가 노출 (플레이어가 공격 기회)
            pEnemy->SetInvincible(false);

            // ReturnTakeOff 로 진입 — 복귀 비행 시작
            if (AnimationComponent* pAnim = pEnemy->GetAnimationComponent())
            {
                pAnim->CrossFade("Take Off", 0.15f, false);
            }

            // 이륙 시작 위치 저장 (현재 지면 위치)
            if (GameObject* pOwner = pEnemy->GetOwner())
                if (TransformComponent* pTransform = pOwner->GetTransform())
                    m_xmf3StartPosition = pTransform->GetPosition();

            m_ePhase = Phase::ReturnTakeOff;
            m_fTimer = 0.0f;
        }
        break;

    case Phase::ReturnTakeOff:
        {
            // 현재 위치에서 수직으로 비행 높이까지 상승
            GameObject* pOwner = pEnemy->GetOwner();
            if (pOwner)
            {
                TransformComponent* pTransform = pOwner->GetTransform();
                if (pTransform)
                {
                    float t = m_fTimer / m_fTakeOffTime;
                    t = min(t, 1.0f);
                    float smoothT = t * t;

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
                pAnim->CrossFade("Fly Glide", 0.2f, true);

            // 비행 시작점 = 현재 공중 위치
            if (GameObject* pOwner = pEnemy->GetOwner())
                if (TransformComponent* pTransform = pOwner->GetTransform())
                    m_xmf3StartPosition = pTransform->GetPosition();

            m_ePhase = Phase::ReturnFly;
            m_fTimer = 0.0f;
        }
        break;

    case Phase::ReturnFly:
        {
            GameObject* pOwner = pEnemy->GetOwner();
            if (pOwner)
            {
                TransformComponent* pTransform = pOwner->GetTransform();
                if (pTransform)
                {
                    float t = m_fTimer / m_fMoveToWallTime;
                    t = min(t, 1.0f);
                    float smoothT = 1.0f - (1.0f - t) * (1.0f - t);  // easeOut

                    // 공중에서 원위치(의 공중 높이)까지 이동
                    XMFLOAT3 targetAir = m_xmf3OriginalPosition;
                    targetAir.y = m_xmf3StartPosition.y;  // 비행 높이 유지

                    XMFLOAT3 pos;
                    pos.x = m_xmf3StartPosition.x + (targetAir.x - m_xmf3StartPosition.x) * smoothT;
                    pos.y = m_xmf3StartPosition.y + (targetAir.y - m_xmf3StartPosition.y) * smoothT;
                    pos.z = m_xmf3StartPosition.z + (targetAir.z - m_xmf3StartPosition.z) * smoothT;
                    pTransform->SetPosition(pos);

                    // 이동 방향 바라보기
                    if (t < 1.0f)
                    {
                        XMFLOAT2 dir = MathUtils::Direction2D(pos, targetAir);
                        if (dir.x != 0.0f || dir.y != 0.0f)
                        {
                            float yaw = atan2f(dir.x, dir.y) * (180.0f / XM_PI);
                            pTransform->SetRotation(0.0f, yaw, 0.0f);
                        }
                    }
                }
            }
        }
        if (m_fTimer >= m_fMoveToWallTime)
        {
            if (AnimationComponent* pAnim = pEnemy->GetAnimationComponent())
                pAnim->CrossFade("Land", 0.15f, false);

            if (GameObject* pOwner = pEnemy->GetOwner())
                if (TransformComponent* pTransform = pOwner->GetTransform())
                    m_xmf3StartPosition = pTransform->GetPosition();

            m_ePhase = Phase::ReturnLand;
            m_fTimer = 0.0f;
        }
        break;

    case Phase::ReturnLand:
        {
            GameObject* pOwner = pEnemy->GetOwner();
            if (pOwner)
            {
                TransformComponent* pTransform = pOwner->GetTransform();
                if (pTransform)
                {
                    float t = m_fTimer / m_fLandingTime;
                    t = min(t, 1.0f);
                    float smoothT = 1.0f - (1.0f - t) * (1.0f - t);

                    XMFLOAT3 pos = m_xmf3StartPosition;
                    pos.y = m_xmf3StartPosition.y - m_fFlyHeight * smoothT;
                    pTransform->SetPosition(pos);
                }
            }
        }
        if (m_fTimer >= m_fLandingTime)
        {
            // 완전히 원위치로 스냅
            if (GameObject* pOwner = pEnemy->GetOwner())
                if (TransformComponent* pTransform = pOwner->GetTransform())
                    pTransform->SetPosition(m_xmf3OriginalPosition);

            // 용암 기믹 재활성화
            if (m_pRoom) m_pRoom->SetLavaGeyserEnabled(true);

            // 시네마틱 카메라 종료 (보험)
            if (m_bCinematicActive && m_pRoom)
            {
                if (Scene* pScene = m_pRoom->GetScene())
                    if (CCamera* pCam = pScene->GetCamera())
                        pCam->StopCinematic();
                m_bCinematicActive = false;
            }

            m_bFinished = true;
        }
        break;
    }
}

// ─── 시네마틱 카메라 연출 ───────────────────────────────────────────────────
//  Phase 별로 3단 구성:
//   1) TakeOff/MoveToWall/Landing  → 하늘의 용을 추적하는 와이드 숏 (용이 프레임에 다 들어오게)
//   2) SpawnCover                  → 방 중앙 기둥 위로 내려다보는 숏
//   3) Windup/Breath               → 플레이어 뒤 어깨 너머 (용 쪽을 향해)
//  목표값을 매 프레임 계산해 현재 카메라 값에서 지수 블렌드 → 급격한 점프 제거
void MegaBreathAttackBehavior::UpdateCinematicCamera(float dt, EnemyComponent* pEnemy)
{
    if (!pEnemy || !m_pRoom) return;
    Scene* pScene = m_pRoom->GetScene();
    if (!pScene) return;
    CCamera* pCam = pScene->GetCamera();
    if (!pCam) return;

    GameObject* pOwner  = pEnemy->GetOwner();
    GameObject* pPlayer = pEnemy->GetTarget();
    if (!pOwner) return;
    TransformComponent* pOwnerT = pOwner->GetTransform();
    if (!pOwnerT) return;

    XMFLOAT3 dragonPos = pOwnerT->GetPosition();
    XMFLOAT3 playerPos = dragonPos;
    if (pPlayer)
        if (auto* pPT = pPlayer->GetTransform())
            playerPos = pPT->GetPosition();

    const BoundingBox& roomBox = m_pRoom->GetBoundingBox();

    // Recovery: cinematic 유지한 채 기본 orbit 값으로 부드럽게 블렌드 → Recovery 막바지에 StopCinematic
    //   기본 orbit: dist 50, pitch 60, yaw 45, lookAt = player
    if (m_ePhase == Phase::Recovery)
    {
        if (!m_bCinematicActive) return;

        // 남은 시간 0.15s 이하면 StopCinematic (막판에 부드럽게 끊기)
        if (m_fTimer >= m_fRecoveryTime - 0.15f)
        {
            pCam->StopCinematic();
            m_bCinematicActive = false;
            return;
        }

        XMFLOAT3 tgtLookAt = { playerPos.x, playerPos.y + 1.0f, playerPos.z };
        float tgtDist  = 50.f;
        float tgtPitch = 60.f;
        float tgtYaw   = 45.f;

        // Recovery 는 빠르게 수렴해야 해서 더 높은 rate 사용
        const float kBlendOut = 5.5f;
        float rate = 1.0f - expf(-dt * kBlendOut);
        rate = (rate < 0.f) ? 0.f : ((rate > 1.f) ? 1.f : rate);

        m_xmf3CamLookAt.x += (tgtLookAt.x - m_xmf3CamLookAt.x) * rate;
        m_xmf3CamLookAt.y += (tgtLookAt.y - m_xmf3CamLookAt.y) * rate;
        m_xmf3CamLookAt.z += (tgtLookAt.z - m_xmf3CamLookAt.z) * rate;
        m_fCamDist  += (tgtDist  - m_fCamDist)  * rate;
        m_fCamPitch += (tgtPitch - m_fCamPitch) * rate;
        float yawDelta = tgtYaw - m_fCamYaw;
        while (yawDelta >  180.f) yawDelta -= 360.f;
        while (yawDelta < -180.f) yawDelta += 360.f;
        m_fCamYaw += yawDelta * rate;

        pCam->SetCinematicLookAt(m_xmf3CamLookAt);
        pCam->SetCinematicOrbit(m_fCamDist, m_fCamPitch, m_fCamYaw);
        return;
    }

    // 복귀 비행 페이즈들: cinematic off — 일반 탑뷰로 진행
    if (m_ePhase == Phase::ReturnTakeOff
     || m_ePhase == Phase::ReturnFly
     || m_ePhase == Phase::ReturnLand)
    {
        if (m_bCinematicActive)
        {
            pCam->StopCinematic();
            m_bCinematicActive = false;
        }
        return;
    }

    // ── 페이즈별 목표 파라미터 결정 ──
    XMFLOAT3 tgtLookAt = { 0, 0, 0 };
    float tgtDist  = 30.f;
    float tgtPitch = 25.f;
    float tgtYaw   = 0.f;

    switch (m_ePhase)
    {
    case Phase::TakeOff:
    case Phase::MoveToWall:
    case Phase::Landing:
    {
        // 용이 화면에 충분히 크게, 하늘이 배경이 되도록 카메라 높이 크게 + 거리 확보
        tgtLookAt = { dragonPos.x, dragonPos.y + 4.0f, dragonPos.z };

        // 방 중앙 기준 용 방향을 기준 yaw로 (카메라는 방 바깥쪽에 위치)
        float dx = dragonPos.x - roomBox.Center.x;
        float dz = dragonPos.z - roomBox.Center.z;
        float radius = sqrtf(dx * dx + dz * dz);
        float baseYaw = (radius > 0.5f)
                      ? atan2f(dx, dz) * (180.0f / XM_PI)
                      : 0.f;

        // 다음 단계(SpawnCover/Windup) 에서 쓸 playerDir yaw
        float ddx = playerPos.x - dragonPos.x;
        float ddz = playerPos.z - dragonPos.z;
        float playerDirYaw = atan2f(ddx, ddz) * (180.0f / XM_PI);

        // 비행 진행도 전반에 걸쳐 완만한 30도 팬 (-15 → +15)
        float totalFlight = m_fTakeOffTime + m_fMoveToWallTime + m_fLandingTime;
        float stageBase   = 0.f;
        if (m_ePhase == Phase::MoveToWall) stageBase = m_fTakeOffTime;
        else if (m_ePhase == Phase::Landing) stageBase = m_fTakeOffTime + m_fMoveToWallTime;
        float globalT = (stageBase + m_fTimer) / (totalFlight > 0.001f ? totalFlight : 1.f);
        float yawOffset = (globalT - 0.5f) * 30.0f;
        float flightYaw = baseYaw + 45.0f + yawOffset;

        // Landing 단계: flightYaw → playerDirYaw 로 부드럽게 수렴 (다음 SpawnCover 와 yaw 일치)
        //   landingT = 0 이면 flightYaw, 1 이면 playerDirYaw
        if (m_ePhase == Phase::Landing)
        {
            float landingT = m_fTimer / (m_fLandingTime > 0.001f ? m_fLandingTime : 1.f);
            landingT = (landingT < 0.f) ? 0.f : ((landingT > 1.f) ? 1.f : landingT);
            // 최단 각도 경로로 보간
            float diff = playerDirYaw - flightYaw;
            while (diff >  180.f) diff -= 360.f;
            while (diff < -180.f) diff += 360.f;
            tgtYaw = flightYaw + diff * landingT;
        }
        else
        {
            tgtYaw = flightYaw;
        }

        tgtPitch = 28.0f;
        tgtDist  = 48.0f;
        break;
    }

    case Phase::SpawnCover:
    {
        // "와이드 establishing 숏" — Windup 과 동일 구도지만 거리/각도 큼
        //   플레이어 중심 + 용 반대편에서 내려다봐 기둥/용/플레이어 전부 프레임에 들어옴
        //   Windup 에 그대로 이어지면서 pitch/dist 만 조금 바뀌는 "zoom in" 느낌으로 매끄러움
        float ddx = playerPos.x - dragonPos.x;
        float ddz = playerPos.z - dragonPos.z;
        float yaw = atan2f(ddx, ddz) * (180.0f / XM_PI);

        // lookAt 은 플레이어 — Windup 과 연속. y 는 조금 더 띄워 와이드감 보강
        tgtLookAt = { playerPos.x, playerPos.y + 2.5f, playerPos.z };
        tgtDist   = 78.0f;   // Windup(62) 보다 훨씬 멀리 — 방 전체 조감
        tgtPitch  = 50.0f;   // Windup(55) 보다 살짝 낮게 → SpawnCover→Windup 은 pitch 상승 + zoom-in 으로 연출
        tgtYaw    = yaw;
        break;
    }

    case Phase::Windup:
    case Phase::Breath:
    {
        // 어깨 너머 숏: 카메라 offset 방향 = (player - dragon) — 플레이어 뒤편
        //   pitch 높여서 브레스 전체 모양이 화면에 담기도록, dist 도 살짝 증가
        //   lookAt 을 플레이어와 용 사이 25% 지점으로 옮겨 프레이밍 중심을 브레스 쪽으로
        float ddx = playerPos.x - dragonPos.x;
        float ddz = playerPos.z - dragonPos.z;
        float yaw = atan2f(ddx, ddz) * (180.0f / XM_PI);

        // 기본 탑뷰와 동일한 프레이밍(플레이어 중심), 거리만 길게 + 각도는 살짝만 조절
        //   기본 orbit: dist 50, pitch 60, yaw 45
        //   여기선: dist 62, pitch 55 (조금 덜 수직 → 전방 시야 살짝 확보), yaw는 용 반대편
        tgtLookAt = { playerPos.x, playerPos.y + 1.0f, playerPos.z };
        tgtDist   = 62.0f;
        tgtPitch  = 55.0f;
        tgtYaw    = yaw;
        break;
    }

    default: break;
    }

    // ── 첫 프레임: 현재 상태를 목표값으로 초기화 + StartCinematic ──
    if (!m_bCinematicActive || !m_bCamStateInit)
    {
        m_xmf3CamLookAt = tgtLookAt;
        m_fCamDist      = tgtDist;
        m_fCamPitch     = tgtPitch;
        m_fCamYaw       = tgtYaw;
        pCam->StartCinematic(m_xmf3CamLookAt, m_fCamDist, m_fCamPitch, m_fCamYaw);
        m_bCinematicActive = true;
        m_bCamStateInit    = true;
        return;
    }

    // ── 지수 블렌드 (반감기 ~0.35s) ──
    //   rate = 1 - exp(-dt * k), k 클수록 빠르게 따라옴
    //   SpawnCover (1.2s) 와 Windup 초반에 걸쳐 완만한 pan/zoom 이 보이도록 k=2
    const float kBlend = 2.0f;
    float rate = 1.0f - expf(-dt * kBlend);
    if (rate < 0.0f) rate = 0.0f;
    if (rate > 1.0f) rate = 1.0f;

    m_xmf3CamLookAt.x += (tgtLookAt.x - m_xmf3CamLookAt.x) * rate;
    m_xmf3CamLookAt.y += (tgtLookAt.y - m_xmf3CamLookAt.y) * rate;
    m_xmf3CamLookAt.z += (tgtLookAt.z - m_xmf3CamLookAt.z) * rate;
    m_fCamDist  += (tgtDist  - m_fCamDist)  * rate;
    m_fCamPitch += (tgtPitch - m_fCamPitch) * rate;

    // yaw: 360도 래핑 처리 — 최단 각도로 보간
    float yawDelta = tgtYaw - m_fCamYaw;
    while (yawDelta >  180.f) yawDelta -= 360.f;
    while (yawDelta < -180.f) yawDelta += 360.f;
    m_fCamYaw += yawDelta * rate;

    pCam->SetCinematicLookAt(m_xmf3CamLookAt);
    pCam->SetCinematicOrbit(m_fCamDist, m_fCamPitch, m_fCamYaw);
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
    m_nWallDirection = 0;
    m_bBreathAnimStarted = false;
    m_bCoverSpawned = false;
    m_bCinematicActive = false;
    m_bCamStateInit    = false;

    // 이전 공격이 중단됐을 수도 있으니 용암 기믹을 안전하게 복구
    if (m_pRoom) m_pRoom->SetLavaGeyserEnabled(true);

    // Fire Wave / Charge VFX 정리
    DestroyFireWave();
    DestroyChargeVFX();

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

                // 나중에 Destroy 할 수 있도록 트랙 (이전엔 추가 안 해서 기둥이 안 사라졌음)
                m_vCoverObjects.push_back(pCover);

                pScene->SetCurrentRoom(pPrevRoom);
            }
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

    }
}
void MegaBreathAttackBehavior::DestroyCoverObjects()
{
    // Scene::MarkForDeletion 으로 프레임 끝에서 안전하게 삭제 (iteration 중 Room vector erase 시 UB 방지)
    //   부가로 Shader render component 도 해제해서 남은 프레임 잔상 렌더 방지
    Scene* pScene = m_pRoom ? m_pRoom->GetScene() : nullptr;
    Shader* pShader = pScene ? pScene->GetDefaultShader() : nullptr;

    for (GameObject* pCover : m_vCoverObjects)
    {
        if (!pCover) continue;

        if (pShader)
        {
            if (auto* pRenderComp = pCover->GetComponent<RenderComponent>())
                pShader->RemoveRenderComponent(pRenderComp);
        }
        // 시각적으로 즉시 숨기기 위해 땅 아래로 (이번 프레임 남은 렌더 보험)
        if (auto* pT = pCover->GetTransform())
            pT->SetPosition(0.0f, -1000.0f, 0.0f);

        if (pScene) pScene->MarkForDeletion(pCover);
    }

    m_vCoverObjects.clear();
    m_vObstacles.clear();

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
    playerPos.y += 1.0f;

    // ── 3-Fan Beam 판정: 메인 + 좌우 15° 빔 중 하나라도 걸리면 히트 ──────────
    if (m_fBeamLength <= 0.0f) return;

    float rx = playerPos.x - m_xmf3BeamOrigin.x;
    float rz = playerPos.z - m_xmf3BeamOrigin.z;
    float vertDist = fabsf(playerPos.y - m_xmf3BeamOrigin.y);

    auto CheckConeHit = [&](float dirX, float dirZ, float endR) -> bool
    {
        float alongDir = rx * dirX + rz * dirZ;
        if (alongDir < 0.0f || alongDir > m_fBeamLength) return false;
        float perpX = rx - dirX * alongDir;
        float perpZ = rz - dirZ * alongDir;
        float horizDist = sqrtf(perpX * perpX + perpZ * perpZ);
        float tCone = alongDir / m_fBeamLength;
        float coneR = endR * tCone;
        if (horizDist > coneR) return false;
        if (vertDist > coneR * 0.5f + 2.0f) return false;
        return true;
    };

    // 5-fan 판정 — SpawnFireWave 와 동일 각도/spread
    const float kFanAnglesDeg[5] = { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f };
    const float kSpreadMults[5]  = {  0.85f, 0.95f, 1.00f, 0.95f, 0.85f };

    bool bHit = false;
    for (int i = 0; i < 5 && !bHit; ++i)
    {
        float a = XMConvertToRadians(kFanAnglesDeg[i]);
        float cs = cosf(a), sn = sinf(a);
        float dirX = m_xmf3BeamDirection.x * cs - m_xmf3BeamDirection.z * sn;
        float dirZ = m_xmf3BeamDirection.x * sn + m_xmf3BeamDirection.z * cs;
        if (CheckConeHit(dirX, dirZ, m_fBeamEndRadius * kSpreadMults[i]))
            bHit = true;
    }
    if (!bHit) return;

    // 엄폐 판정 — 입에서 플레이어 방향 ray 가 장애물에 막히면 safe
    XMFLOAT3 mouthForRay = m_xmf3BeamOrigin;
    if (IsPlayerBehindCover(mouthForRay, playerPos))
    {
        return;
    }

    // ── 3. 데미지 적용 ───────────────────────────────────────────────
    PlayerComponent* pPlayer = pTarget->GetComponent<PlayerComponent>();
    if (pPlayer)
    {
        pPlayer->TakeDamage(m_fDamagePerTick);
    }
}

// ─── Fire Wave (SPH 플루이드) ──────────────────────────────────────────────
void MegaBreathAttackBehavior::SpawnFireWave(EnemyComponent* pEnemy)
{
    if (!pEnemy || !m_pRoom) return;

    Scene* pScene = m_pRoom->GetScene();
    if (!pScene) return;

    m_pFluidVFXManager = pScene->GetEnemyFluidVFXManager();
    if (!m_pFluidVFXManager) return;

    // 방 바운드 + 벽 방향 기반 파도 경로 계산
    const BoundingBox& rb = m_pRoom->GetBoundingBox();
    // (방 AABB 는 m_pRoom->GetBoundingBox() 로 필요 시 참조)

    // 보스 실제 위치 + 방향 (Windup 단계에서 보스는 방 중앙을 향해 회전되어 있음)
    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;
    auto* pT = pOwner->GetTransform();
    if (!pT) return;

    XMFLOAT3 bossPos = pT->GetPosition();
    XMFLOAT3 bossRot = pT->GetRotation();
    float yawRad = XMConvertToRadians(bossRot.y);

    // 보스 입 위치 (전방 13u, 머리 높이 6u — charge VFX 와 동일 지점에서 발사)
    m_xmf3BeamOrigin.x = bossPos.x + sinf(yawRad) * 13.0f;
    m_xmf3BeamOrigin.y = bossPos.y + 6.0f;
    m_xmf3BeamOrigin.z = bossPos.z + cosf(yawRad) * 13.0f;

    m_xmf3BeamDirection = { sinf(yawRad), 0.0f, cosf(yawRad) };

    // 빔 길이: 보스 입 → 방 반대편 경계까지
    //   방 AABB 내 정확한 경계 거리 계산 (2D slab method 생략 — 간단히 대각선 전체 거리 상한 사용)
    float diagX = rb.Extents.x * 2.0f;
    float diagZ = rb.Extents.z * 2.0f;
    m_fBeamLength   = sqrtf(diagX * diagX + diagZ * diagZ) * 0.9f;
    // cone 끝 반경 — 방 반폭의 1.4배로 복원 (1.8은 너무 과함)
    float perpExtent = (fabsf(m_xmf3BeamDirection.x) > 0.5f) ? rb.Extents.z : rb.Extents.x;
    m_fBeamEndRadius = perpExtent * 1.4f;

    // ── Beam 모드 VFXSequenceDef — 입에서 끝까지 연속 분사 ─────────────────
    // 이름 "Dragon_MegaBreath" 매칭 시 enableFlow 자동 + 120m 하드코딩이었으나
    // swirlFadeEnd 를 빔 길이로 쓰도록 수정 완료 — 어떤 이름이든 맵 전체 커버 가능
    VFXSequenceDef def;
    def.name          = "Dragon_MegaBreath";      // 기존 보스 브레스 시스템 호환 (enableFlow)
    def.element       = ElementType::Fire;
    def.particleCount = 6000;                     // 끝으로 갈수록 cone 단면 증가 → 밀도 유지 위해 대폭 증가
    def.spawnRadius   = 3.0f;                     // 입쪽 생성 영역
    def.particleSize  = 1.8f;                     // 큰 불덩이 입자 (겹침으로 빈칸 상쇄)

    VFXPhase p;
    p.startTime             = 0.f;
    p.duration              = m_fBreathDuration + 0.5f;
    p.motionMode            = ParticleMotionMode::Beam;
    // 흐름 속도 — 전체 거리를 duration 50~70% 안에 도달 (자연스러운 채움)
    p.beamDesc.speedMin     = m_fBeamLength / (m_fBreathDuration * 0.7f);
    p.beamDesc.speedMax     = p.beamDesc.speedMin * 1.4f;
    p.beamDesc.spreadRadius = m_fBeamEndRadius;   // cone 최대 반경
    p.beamDesc.swirlExpand  = true;               // 입에서 좁게 → 끝에서 넓게 (순수 원뿔)
    p.beamDesc.swirlSpeed   = 0.8f;               // 살짝 회전 (불길 꼬임)
    p.beamDesc.beamLength   = m_fBeamLength;      // 빔 기하 길이 (endPos 계산용)
    p.beamDesc.swirlFadeEnd = 0.f;                // 크기 페이드 끄기 (끝 사이즈 증가는 upload 단계에서 처리)
    p.beamDesc.enableFlow   = true;               // 입에서 계속 새 파티클 분출
    p.beamDesc.verticalScale = 0.25f;             // 수직 납작 (0.5 → 0.25) — 위로 샘솟는 느낌 제거
    def.phases.push_back(p);

    // 불 색상 오버라이드 — 따뜻한 주황노랑(입, 중심) ↔ 깊은 암적색(끝, 외곽)
    //  UploadRenderData 에서 beamT + 반경 기반 그라디언트로 보간 → 자연스러운 불길 색 변이
    def.overrideColors    = true;
    def.overrideCoreColor = { 1.0f, 0.65f, 0.15f, 1.0f };  // 주황노랑 코어 (백열 회피)
    def.overrideEdgeColor = { 0.55f, 0.08f, 0.02f, 0.9f }; // 어두운 붉은 가장자리

    def.maxParticleSpeed = p.beamDesc.speedMax * 1.2f;
    def.useSSFBlur       = true;

    // 5개 빔을 팬-아웃 스폰 — -12°, -6°, 0°, +6°, +12°
    //  작은 간격으로 촘촘히 배치해 빔 사이 빈틈 제거 (뒤에서 봐도 연속된 부채 모양)
    //  각 빔 spread 를 main 과 비슷하게 유지해 서로 충분히 겹치게
    const float kFanAnglesDeg[NUM_BEAMS] = { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f };
    const int   kParticleCounts[NUM_BEAMS] = { 2800, 3600, 4400, 3600, 2800 };
    const float kSpreadMults[NUM_BEAMS]    = { 0.85f, 0.95f, 1.00f, 0.95f, 0.85f };

    for (int i = 0; i < NUM_BEAMS; ++i)
    {
        float ang = XMConvertToRadians(kFanAnglesDeg[i]);
        float cs = cosf(ang), sn = sinf(ang);
        XMFLOAT3 dir;
        dir.x = m_xmf3BeamDirection.x * cs - m_xmf3BeamDirection.z * sn;
        dir.y = 0.f;
        dir.z = m_xmf3BeamDirection.x * sn + m_xmf3BeamDirection.z * cs;

        VFXSequenceDef beamDef = def;
        beamDef.particleCount                     = kParticleCounts[i];
        beamDef.spawnRadius                       = (i == NUM_BEAMS / 2) ? 3.0f : 2.5f;
        beamDef.phases[0].beamDesc.spreadRadius   = m_fBeamEndRadius * kSpreadMults[i];

        m_nFluidVFXIds[i] = m_pFluidVFXManager->SpawnSequenceEffect(
            m_xmf3BeamOrigin, dir, beamDef, false); // 적 스킬 — SSF 분리
    }

}

void MegaBreathAttackBehavior::UpdateFireWave(float dt, EnemyComponent* pEnemy)
{
    // SPH wave 는 자체 waveSpeed + wavePushForce 로 자율 전진 —
    // 여기서는 데미지 판정용 Wave 진행 위치만 FluidSkillVFXManager 로부터 조회
    // (ApplyBreathDamage 는 IsPointInWave API 사용)
}

// ─── Charge VFX: 입 주변 파티클 집결 "곧 뭔가 온다" ────────────────────────
void MegaBreathAttackBehavior::SpawnChargeVFX(EnemyComponent* pEnemy)
{
    if (!pEnemy || !m_pRoom) return;
    if (m_nChargeVFXId >= 0) return;  // 이미 스폰됨

    Scene* pScene = m_pRoom->GetScene();
    if (!pScene) return;

    if (!m_pFluidVFXManager)
        m_pFluidVFXManager = pScene->GetEnemyFluidVFXManager();
    if (!m_pFluidVFXManager) return;

    GameObject* pOwner = pEnemy->GetOwner();
    if (!pOwner) return;
    auto* pT = pOwner->GetTransform();
    if (!pT) return;

    // 입 위치 근사 (Windup 단계의 보스 회전 기준) — 몸통 뒤쪽이 아닌 실제 입 앞쪽
    XMFLOAT3 bossPos = pT->GetPosition();
    XMFLOAT3 bossRot = pT->GetRotation();
    float yawRad = XMConvertToRadians(bossRot.y);

    XMFLOAT3 mouth;
    mouth.x = bossPos.x + sinf(yawRad) * 13.0f;   // 8 → 13 — 머리 위치까지 더 앞으로
    mouth.y = bossPos.y + 6.0f;                   // 5 → 6 — 머리 높이 살짝 올림
    mouth.z = bossPos.z + cosf(yawRad) * 13.0f;
    XMFLOAT3 forward = { sinf(yawRad), 0.0f, cosf(yawRad) };

    // ControlPoint 모드 — 파티클이 멀리서 천천히 입쪽으로 흘러들어옴
    //   cardinalSpawnRadius 를 크게 + 내향 속도 낮춰 Windup 전반에 걸쳐 visible 한 이동 궤적 형성
    VFXSequenceDef def;
    def.name          = "Dragon_MegaBreath_Charge";
    def.element       = ElementType::Fire;
    def.particleCount = 3500;         // 크기를 더 줄인 만큼 개수로 보완
    def.spawnRadius   = 0.7f;
    def.particleSize  = 0.55f;        // 더 작게 — 모래알 같은 불티가 빨려드는 느낌
    def.useSSFBlur    = false;

    // cardinal 스폰 — 멀리서(8m) 낮은 속도(2.0)로 발사 → 중심 도달까지 ~4초 걸려 windup 내내 흐름 보임
    def.cardinalSpawnRadius = 8.0f;
    def.cardinalInwardSpeed = 2.0f;

    // 색상 — 불꽃 집결 느낌
    def.overrideColors    = true;
    def.overrideCoreColor = { 1.0f, 0.7f,  0.18f, 1.0f };
    def.overrideEdgeColor = { 1.0f, 0.12f, 0.0f,  0.85f };

    // 중심 attractor CP — 약하게: 스폰 초반 관성 손상 없이 천천히 빨려듦
    FluidCPDesc cp;
    cp.forwardBias        = 0.0f;
    cp.attractionStrength = 2.8f;     // 기존 5.5 → 더 약하게
    cp.sphereRadius       = 1.2f;
    def.cpDescs.push_back(cp);

    VFXPhase p;
    p.startTime  = 0.f;
    p.duration   = 99.f;
    p.motionMode = ParticleMotionMode::ControlPoint;
    p.offsetParticlesWithOrigin = true;
    def.phases.push_back(p);

    m_nChargeVFXId = m_pFluidVFXManager->SpawnSequenceEffect(mouth, forward, def, false); // 적 스킬 — SSF 분리
}

void MegaBreathAttackBehavior::UpdateChargeVFX(EnemyComponent* /*pEnemy*/)
{
    // origin이 offsetParticlesWithOrigin 으로 따라감 — 여기선 별도 처리 없음
    // (필요 시 SetEffectOrigin 같은 API 통해 보스 머리 따라갈 수 있음)
}

void MegaBreathAttackBehavior::DestroyChargeVFX()
{
    if (m_pFluidVFXManager && m_nChargeVFXId >= 0)
    {
        m_pFluidVFXManager->StopEffect(m_nChargeVFXId);
        m_nChargeVFXId = -1;
    }
}

void MegaBreathAttackBehavior::DestroyFireWave()
{
    if (m_pFluidVFXManager)
    {
        for (int i = 0; i < NUM_BEAMS; ++i)
        {
            if (m_nFluidVFXIds[i] >= 0)
            {
                m_pFluidVFXManager->StopEffect(m_nFluidVFXIds[i]);
                m_nFluidVFXIds[i] = -1;
            }
        }
    }
    m_pFluidVFXManager = nullptr;
}
