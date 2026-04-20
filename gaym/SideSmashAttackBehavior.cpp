#include "stdafx.h"
#include "SideSmashAttackBehavior.h"
#include "EnemyComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "PlayerComponent.h"
#include "AnimationComponent.h"
#include "Room.h"
#include "Scene.h"
#include "Camera.h"

SideSmashAttackBehavior::SideSmashAttackBehavior(float fDamage,
                                                 float fTiltAngle,
                                                 float fRectWidthHalf,
                                                 float fRectLength,
                                                 float fWindupTime,
                                                 float fSlamTime,
                                                 float fRecoveryTime,
                                                 float fCameraShakeIntensity,
                                                 float fCameraShakeDuration,
                                                 float fAnimPlaybackSpeed)
    : m_fDamage(fDamage)
    , m_fTiltAngle(fTiltAngle)
    , m_fRectWidthHalf(fRectWidthHalf)
    , m_fRectLength(fRectLength)
    , m_fWindupTime(fWindupTime)
    , m_fSlamTime(fSlamTime)
    , m_fRecoveryTime(fRecoveryTime)
    , m_fCameraShakeIntensity(fCameraShakeIntensity)
    , m_fCameraShakeDuration(fCameraShakeDuration)
    , m_fAnimPlaybackSpeed(fAnimPlaybackSpeed)
{
}

void SideSmashAttackBehavior::Execute(EnemyComponent* pEnemy)
{
    Reset();
    if (!pEnemy) return;

    GameObject* pOwner  = pEnemy->GetOwner();
    GameObject* pTarget = pEnemy->GetTarget();
    if (!pOwner || !pTarget) return;

    TransformComponent* pMyT = pOwner->GetTransform();
    TransformComponent* pTgT = pTarget->GetTransform();
    if (!pMyT || !pTgT) return;

    XMFLOAT3 myPos  = pMyT->GetPosition();
    XMFLOAT3 tgtPos = pTgT->GetPosition();

    // 타겟을 향하는 yaw (보스 forward = (sin(yaw), cos(yaw)) 규약)
    float dx = tgtPos.x - myPos.x;
    float dz = tgtPos.z - myPos.z;
    float faceYaw = (dx == 0.0f && dz == 0.0f)
                    ? pMyT->GetRotation().y
                    : XMConvertToDegrees(atan2f(dx, dz));

    // 좌/우 선택 (현재 보스 yaw 기준 플레이어 localX)
    m_eSide = PickSide(pEnemy);

    // 최종 yaw = 타겟 방향 ± tilt (Left = -, Right = +)
    float tilt = (m_eSide == Side::Left) ? -m_fTiltAngle : m_fTiltAngle;
    m_fStartYaw  = pMyT->GetRotation().y;
    m_fTargetYaw = faceYaw + tilt;
    // shortest-path 정규화 (눈에 보이는 회전이 270° 가 아니라 -90° 되도록)
    while (m_fTargetYaw - m_fStartYaw >  180.0f) m_fTargetYaw -= 360.0f;
    while (m_fTargetYaw - m_fStartYaw < -180.0f) m_fTargetYaw += 360.0f;

    // 애니 클립 선택
    m_strClipName = (m_eSide == Side::Left)
                    ? "turn_45_left_Smash_Attack"
                    : "turn_45_Right_Smash_Attack";

    if (auto* pAnim = pEnemy->GetAnimationComponent())
    {
        pAnim->CrossFade(m_strClipName, 0.1f, false);
        if (m_fAnimPlaybackSpeed > 0.0f)
            pAnim->SetPlaybackSpeed(m_fAnimPlaybackSpeed);
    }

    m_ePhase = Phase::Windup;
}

void SideSmashAttackBehavior::Update(float dt, EnemyComponent* pEnemy)
{
    if (m_bFinished || !pEnemy) return;

    GameObject* pOwner = pEnemy->GetOwner();
    TransformComponent* pT = pOwner ? pOwner->GetTransform() : nullptr;

    m_fTimer += dt;

    switch (m_ePhase)
    {
    case Phase::Windup:
    {
        // 보스 몸을 부드럽게 tilt 된 방향으로 회전 — 인디케이터가 같이 기울며 회피 방향 시각화
        if (pT)
        {
            float t = (m_fWindupTime > 0.0f) ? (m_fTimer / m_fWindupTime) : 1.0f;
            if (t > 1.0f) t = 1.0f;
            float eased = t * t * (3.0f - 2.0f * t);   // smoothstep
            XMFLOAT3 rot = pT->GetRotation();
            rot.y = m_fStartYaw + (m_fTargetYaw - m_fStartYaw) * eased;
            pT->SetRotation(rot);
        }

        if (m_fTimer >= m_fWindupTime)
        {
            // 최종 yaw 스냅 — 사각형 판정/인디케이터 정확도 보장
            if (pT)
            {
                XMFLOAT3 rot = pT->GetRotation();
                rot.y = m_fTargetYaw;
                pT->SetRotation(rot);
            }
            m_ePhase = Phase::Slam;
            m_fTimer = 0.0f;
        }
        break;
    }

    case Phase::Slam:
    {
        if (!m_bHitDealt)
        {
            DealSmashDamage(pEnemy);
            m_bHitDealt = true;

            if (m_fCameraShakeIntensity > 0.0f && pEnemy->GetRoom())
            {
                if (Scene* pScene = pEnemy->GetRoom()->GetScene())
                    if (CCamera* pCam = pScene->GetCamera())
                        pCam->StartShake(m_fCameraShakeIntensity, m_fCameraShakeDuration);
            }
        }
        if (m_fTimer >= m_fSlamTime)
        {
            m_ePhase = Phase::Recovery;
            m_fTimer = 0.0f;
        }
        break;
    }

    case Phase::Recovery:
        if (m_fTimer >= m_fRecoveryTime)
            m_bFinished = true;
        break;
    }
}

bool SideSmashAttackBehavior::IsFinished() const
{
    return m_bFinished;
}

void SideSmashAttackBehavior::Reset()
{
    m_eSide      = Side::Right;
    m_ePhase     = Phase::Windup;
    m_fTimer     = 0.0f;
    m_fStartYaw  = 0.0f;
    m_fTargetYaw = 0.0f;
    m_bHitDealt  = false;
    m_bFinished  = false;
    m_strClipName = "turn_45_Right_Smash_Attack";
}

SideSmashAttackBehavior::Side SideSmashAttackBehavior::PickSide(EnemyComponent* pEnemy) const
{
    if (!pEnemy) return Side::Right;

    GameObject* pOwner  = pEnemy->GetOwner();
    GameObject* pTarget = pEnemy->GetTarget();
    if (!pOwner || !pTarget) return Side::Right;

    TransformComponent* pMyT = pOwner->GetTransform();
    TransformComponent* pTgT = pTarget->GetTransform();
    if (!pMyT || !pTgT) return Side::Right;

    XMFLOAT3 myPos  = pMyT->GetPosition();
    XMFLOAT3 tgtPos = pTgT->GetPosition();
    float dx = tgtPos.x - myPos.x;
    float dz = tgtPos.z - myPos.z;

    // 보스 forward = (sin(yaw), cos(yaw)) → localX = dx*cos - dz*sin (양수 = 우측)
    float yawRad = XMConvertToRadians(pMyT->GetRotation().y);
    float c = cosf(yawRad);
    float s = sinf(yawRad);
    float localX = dx * c - dz * s;

    // 거의 중앙이면 50/50 랜덤 — 예측 불가능성 확보
    if (fabsf(localX) < 2.0f)
        return (rand() % 2) ? Side::Left : Side::Right;
    return (localX < 0.0f) ? Side::Left : Side::Right;
}

void SideSmashAttackBehavior::DealSmashDamage(EnemyComponent* pEnemy)
{
    if (!pEnemy) return;
    GameObject* pTarget = pEnemy->GetTarget();
    if (!pTarget) return;

    // 보스 yaw 가 tilt 된 상태이므로 ForwardRect 가 자동으로 ±45° 기울어진 판정
    if (!pEnemy->IsTargetInForwardRect(m_fRectWidthHalf, m_fRectLength))
        return;

    if (auto* pPlayer = pTarget->GetComponent<PlayerComponent>())
        pPlayer->TakeDamage(m_fDamage);
}
