#include "stdafx.h"
#include "TailSweepAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "PlayerComponent.h"
#include "AnimationComponent.h"
#include "MathUtils.h"
#include "Room.h"
#include "Scene.h"
#include "Camera.h"

TailSweepAttackBehavior::TailSweepAttackBehavior(float fDamage,
                                                 float fWindupTime,
                                                 float fSweepTime,
                                                 float fRecoveryTime,
                                                 float fHitRange,
                                                 float fSweepArc,
                                                 bool bHitBehind,
                                                 const char* pClipOverride,
                                                 float fRectWidthHalf,
                                                 float fRectLength,
                                                 float fCameraShakeIntensity,
                                                 float fCameraShakeDuration,
                                                 float fAnimPlaybackSpeed)
    : m_fDamage(fDamage)
    , m_fWindupTime(fWindupTime)
    , m_fSweepTime(fSweepTime)
    , m_fRecoveryTime(fRecoveryTime)
    , m_fHitRange(fHitRange)
    , m_fSweepArc(fSweepArc)
    , m_bHitBehind(bHitBehind)
    , m_fRectWidthHalf(fRectWidthHalf)
    , m_fRectLength(fRectLength)
    , m_fCameraShakeIntensity(fCameraShakeIntensity)
    , m_fCameraShakeDuration(fCameraShakeDuration)
    , m_fAnimPlaybackSpeed(fAnimPlaybackSpeed)
{
    if (pClipOverride && pClipOverride[0] != '\0')
        m_strClipName = pClipOverride;
}

void TailSweepAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();

    if (pEnemy)
    {
        GameObject* pOwner = pEnemy->GetOwner();
        GameObject* pTarget = pEnemy->GetTarget();
        bool bRectMode = (m_fRectWidthHalf > 0.0f && m_fRectLength > 0.0f);

        // Rect 모드: stationary 보스라도 attack 시작 시 플레이어 방향으로 한 번 회전
        //   (회전이 완전 봉쇄된 고정형 보스에 "방향성 있는 직사각형 AOE" 를 주기 위함)
        //   회전 후 m_fInitialRotation 저장하므로 sweep/hit 판정도 올바른 방향으로 수행
        if (bRectMode && pEnemy->IsStationary() && pOwner && pTarget
            && pOwner->GetTransform() && pTarget->GetTransform())
        {
            XMFLOAT3 myPos  = pOwner->GetTransform()->GetPosition();
            XMFLOAT3 tgtPos = pTarget->GetTransform()->GetPosition();
            float dx = tgtPos.x - myPos.x;
            float dz = tgtPos.z - myPos.z;
            if (dx != 0.0f || dz != 0.0f)
            {
                float yaw = atan2f(dx, dz) * (180.0f / XM_PI);
                XMFLOAT3 rot = pOwner->GetTransform()->GetRotation();
                rot.y = yaw;
                pOwner->GetTransform()->SetRotation(rot);
            }
        }

        // Store initial rotation for sweep animation (rect 회전 반영 후)
        if (pOwner && pOwner->GetTransform())
        {
            m_fInitialRotation = pOwner->GetTransform()->GetRotation().y;
        }

        AnimationComponent* pAnimComp = pEnemy->GetAnimationComponent();
        if (pAnimComp)
        {
            pAnimComp->CrossFade(m_strClipName, 0.1f, false);
            // 애니 재생속도 override (휘두르기 처럼 느려져야 할 때)
            if (m_fAnimPlaybackSpeed > 0.0f)
                pAnimComp->SetPlaybackSpeed(m_fAnimPlaybackSpeed);
        }
    }

    m_ePhase = Phase::Windup;
}

void TailSweepAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
{
    if (m_bFinished) return;

    m_fTimer += dt;

    switch (m_ePhase)
    {
    case Phase::Windup:
        if (m_fTimer >= m_fWindupTime)
        {
            m_ePhase = Phase::Sweep;
            m_fTimer = 0.0f;
        }
        break;

    case Phase::Sweep:
        {
            // 사각형 모드에선 보스 몸을 돌리지 않음 (애니메이션이 촉수 휘두름을 표현 — 몸통 스핀은 어색함)
            bool bRectMode = (m_fRectWidthHalf > 0.0f && m_fRectLength > 0.0f);
            // Stationary 보스는 회전 금지 — 데미지 판정만 360° 수행
            bool bStationary = pEnemy && pEnemy->IsStationary();
            if (!bRectMode && !bStationary && pEnemy)
            {
                GameObject* pOwner = pEnemy->GetOwner();
                if (pOwner && pOwner->GetTransform())
                {
                    float t = m_fTimer / m_fSweepTime;
                    if (t > 1.0f) t = 1.0f;

                    // Rotate through the sweep arc
                    float sweepProgress = t * m_fSweepArc;
                    float newRotation = m_fInitialRotation + (m_bHitBehind ? sweepProgress : -sweepProgress);

                    XMFLOAT3 rot = pOwner->GetTransform()->GetRotation();
                    rot.y = newRotation;
                    pOwner->GetTransform()->SetRotation(rot);
                }
            }

            // 텔레그래프 fill 완료 타이밍(windup 끝 = sweep 시작)에 데미지 판정
            // → 사각형 방향이 초기 보스 yaw 기준이라 인디케이터와 정확히 일치
            if (!m_bHitDealt)
            {
                DealSweepDamage(pEnemy);
                m_bHitDealt = true;

                // Hit 순간 카메라 쉐이크 (강도 > 0 일 때만)
                if (m_fCameraShakeIntensity > 0.0f && pEnemy->GetRoom())
                {
                    if (Scene* pScene = pEnemy->GetRoom()->GetScene())
                        if (CCamera* pCam = pScene->GetCamera())
                            pCam->StartShake(m_fCameraShakeIntensity, m_fCameraShakeDuration);
                }
            }

            if (m_fTimer >= m_fSweepTime)
            {
                m_ePhase = Phase::Recovery;
                m_fTimer = 0.0f;
            }
        }
        break;

    case Phase::Recovery:
        if (m_fTimer >= m_fRecoveryTime)
        {
            m_bFinished = true;
        }
        break;
    }
}

bool TailSweepAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void TailSweepAttackBehavior::Reset()
{
    m_ePhase = Phase::Windup;
    m_fTimer = 0.0f;
    m_fInitialRotation = 0.0f;
    m_bHitDealt = false;
    m_bFinished = false;
}

void TailSweepAttackBehavior::DealSweepDamage(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;

    GameObject* pOwner = pEnemy->GetOwner();
    GameObject* pTarget = pEnemy->GetTarget();
    if (!pOwner || !pTarget) return;

    TransformComponent* pMyTransform = pOwner->GetTransform();
    TransformComponent* pTargetTransform = pTarget->GetTransform();
    if (!pMyTransform || !pTargetTransform) return;

    // 전방 사각형 판정 모드 — 설정되어 있으면 원형 호 대신 사각형으로 체크
    if (m_fRectWidthHalf > 0.0f && m_fRectLength > 0.0f)
    {
        if (!pEnemy->IsTargetInForwardRect(m_fRectWidthHalf, m_fRectLength))
            return;
        // 사각형 내면 통과 → 바로 데미지 적용 (아래 원형 체크 스킵)
        PlayerComponent* pPlayer = pTarget->GetComponent<PlayerComponent>();
        if (pPlayer)
            pPlayer->TakeDamage(m_fDamage);
        return;
    }

    // 공격 원점 기준 거리 체크 (보스 앞쪽 촉수 위치)
    XMFLOAT3 origin = pEnemy->GetAttackOrigin();
    XMFLOAT3 targetPos = pTargetTransform->GetPosition();
    float odx = targetPos.x - origin.x;
    float odz = targetPos.z - origin.z;
    float distance = sqrtf(odx * odx + odz * odz);
    if (distance > m_fHitRange) return;

    // Check angle - tail sweep hits in a wide arc
    XMFLOAT3 myPos = pMyTransform->GetPosition();
    XMFLOAT3 myRot = pMyTransform->GetRotation();

    // Direction to target (각도는 보스 중심에서 계산 — 방향성은 그대로)
    float dx = targetPos.x - myPos.x;
    float dz = targetPos.z - myPos.z;
    float angleToTarget = XMConvertToDegrees(atan2f(dx, dz));

    // Boss facing direction
    float facingAngle = myRot.y;

    // For tail sweep, check if target is within the sweep arc
    // If hitting behind, the arc is centered at 180 degrees from facing
    float centerAngle = m_bHitBehind ? facingAngle + 180.0f : facingAngle;

    // Normalize angles
    while (angleToTarget < 0.0f) angleToTarget += 360.0f;
    while (angleToTarget >= 360.0f) angleToTarget -= 360.0f;
    while (centerAngle < 0.0f) centerAngle += 360.0f;
    while (centerAngle >= 360.0f) centerAngle -= 360.0f;

    // Calculate angle difference
    float angleDiff = fabsf(angleToTarget - centerAngle);
    if (angleDiff > 180.0f) angleDiff = 360.0f - angleDiff;

    // Check if within sweep arc
    if (angleDiff > m_fSweepArc * 0.5f) return;

    // Deal damage
    PlayerComponent* pPlayer = pTarget->GetComponent<PlayerComponent>();
    if (pPlayer)
        pPlayer->TakeDamage(m_fDamage);
}
