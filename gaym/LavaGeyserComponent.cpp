#include "stdafx.h"
#include "LavaGeyserComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "FluidParticleSystem.h"
#include "FluidParticle.h"
#include "ParticleSystem.h"
#include "Particle.h"
#include "Room.h"
#include "Scene.h"
#include "PlayerComponent.h"

LavaGeyserComponent::LavaGeyserComponent(GameObject* pOwner)
    : Component(pOwner)
{
}

LavaGeyserComponent::~LavaGeyserComponent()
{
}

void LavaGeyserComponent::Update(float deltaTime)
{
    switch (m_eState)
    {
    case GeyserState::Idle:
        // 대기 상태 - 아무것도 하지 않음
        break;

    case GeyserState::Warning:
        m_fTimer += deltaTime;

        // 점점 차오르는 효과: 스케일이 0에서 최대까지 증가
        {
            float progress = m_fTimer / m_fWarningDuration;
            if (progress > 1.0f) progress = 1.0f;

            // 0 → m_fRadius로 점점 커짐
            float currentScale = m_fRadius * progress;
            if (m_pIndicator)
            {
                TransformComponent* pT = m_pIndicator->GetTransform();
                if (pT)
                {
                    pT->SetScale(currentScale, 1.0f, currentScale);
                }
            }
        }

        if (m_fTimer >= m_fWarningDuration)
        {
            // 경고 종료 → 폭발
            m_fTimer = 0.0f;
            m_eState = GeyserState::Erupting;
            Erupt();
        }
        break;

    case GeyserState::Erupting:
        m_fTimer += deltaTime;

        // 0.7초 후 방출 중단 (기둥이 서서히 사라짐)
        if (m_fTimer >= 0.7f && m_pParticleSystem && m_nEmitterId >= 0)
        {
            ParticleEmitter* pEmitter = m_pParticleSystem->GetEmitter(m_nEmitterId);
            if (pEmitter && pEmitter->IsEmitting())
            {
                pEmitter->Stop();  // 방출 중단
            }
        }

        if (m_fTimer >= m_fEruptDuration)
        {
            // 폭발 종료 → 대기 상태로 복귀
            m_fTimer = 0.0f;
            m_eState = GeyserState::Idle;
            HideIndicator();

            // 이미터 정리
            if (m_pParticleSystem && m_nEmitterId >= 0)
            {
                m_pParticleSystem->RemoveEmitter(m_nEmitterId);
                m_nEmitterId = -1;
            }
        }
        break;
    }
}

void LavaGeyserComponent::Activate(const XMFLOAT3& position)
{
    if (m_eState != GeyserState::Idle) return;

    m_vTargetPosition = position;
    m_fTimer = 0.0f;
    m_eState = GeyserState::Warning;

    ShowIndicator();

    OutputDebugString(L"[LavaGeyser] Activated at position\n");
}

void LavaGeyserComponent::ShowIndicator()
{
    if (!m_pIndicator) return;

    TransformComponent* pT = m_pIndicator->GetTransform();
    if (pT)
    {
        // 위치 설정 (지면 살짝 위)
        pT->SetPosition(m_vTargetPosition.x, m_vTargetPosition.y + 0.1f, m_vTargetPosition.z);

        // 처음에는 스케일 0으로 시작 (점점 차오름)
        pT->SetScale(0.0f, 1.0f, 0.0f);
    }
}

void LavaGeyserComponent::HideIndicator()
{
    if (!m_pIndicator) return;

    TransformComponent* pT = m_pIndicator->GetTransform();
    if (pT)
    {
        // 카메라 아래로 숨김
        pT->SetPosition(0.0f, -1000.0f, 0.0f);
    }
}

void LavaGeyserComponent::Erupt()
{
    OutputDebugString(L"[LavaGeyser] Erupting!\n");

    // 1. 데미지 처리
    DealDamage();

    // 2. 인디케이터 숨기기 (폭발 시 사라짐)
    HideIndicator();

    // 3. 일반 파티클 시스템으로 용암 기둥 폭발!
    if (m_pParticleSystem)
    {
        // 용암 기둥 파티클 설정 - 피격 범위에 맞는 큰 기둥
        ParticleEmitterConfig cfg;
        cfg.emissionRate = 12000.0f;          // 초당 12000개! (넓은 범위 커버)
        cfg.burstCount = 0;

        // 수명 범위를 넓게 - 다양한 높이에 분포
        cfg.minLifetime = 0.08f;
        cfg.maxLifetime = 0.6f;

        // 파티클 크기
        cfg.minStartSize = 0.8f;
        cfg.maxStartSize = 1.5f;
        cfg.minEndSize = 0.4f;
        cfg.maxEndSize = 0.8f;

        // 속도 범위 - 더 높이 솟구침
        cfg.minVelocity = { -1.5f, 10.0f, -1.5f };
        cfg.maxVelocity = { 1.5f, 75.0f, 1.5f };

        // 진한 주황색 → 어두운 빨강으로 페이드
        cfg.startColor = { 1.0f, 0.6f, 0.1f, 1.0f };   // 진한 주황
        cfg.endColor = { 0.8f, 0.15f, 0.0f, 0.0f };    // 어두운 빨강

        // 중력 없음 (기둥이 똑바로 서있음)
        cfg.gravity = { 0.0f, 0.0f, 0.0f };

        // 둘레를 줄인 스폰 영역
        cfg.spawnRadius = m_fRadius * 0.5f;  // 5.0

        // 이미터 생성 및 시작
        m_nEmitterId = m_pParticleSystem->CreateEmitter(cfg, m_vTargetPosition);
        if (m_nEmitterId >= 0)
        {
            ParticleEmitter* pEmitter = m_pParticleSystem->GetEmitter(m_nEmitterId);
            if (pEmitter)
            {
                pEmitter->Start();  // 연속 방출 시작
            }
        }
    }
}

void LavaGeyserComponent::DealDamage()
{
    if (!m_pRoom) return;

    Scene* pScene = m_pRoom->GetScene();
    if (!pScene) return;

    GameObject* pPlayer = pScene->GetPlayer();
    if (!pPlayer) return;

    // 플레이어 위치 확인
    TransformComponent* pPlayerTransform = pPlayer->GetTransform();
    if (!pPlayerTransform) return;

    XMFLOAT3 playerPos = pPlayerTransform->GetPosition();

    // 거리 계산 (XZ 평면)
    float dx = playerPos.x - m_vTargetPosition.x;
    float dz = playerPos.z - m_vTargetPosition.z;
    float distSq = dx * dx + dz * dz;
    float radiusSq = m_fRadius * m_fRadius;

    // 범위 내에 있으면 데미지
    if (distSq <= radiusSq)
    {
        PlayerComponent* pPlayerComp = pPlayer->GetComponent<PlayerComponent>();
        if (pPlayerComp)
        {
            pPlayerComp->TakeDamage(m_fDamage);
            OutputDebugString(L"[LavaGeyser] Player hit! Dealing damage.\n");
        }
    }
}
