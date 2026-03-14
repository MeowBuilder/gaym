#pragma once

#include "stdafx.h"
#include "FluidParticle.h"
#include "SkillTypes.h"

class FluidParticleSystem;
class SkillComponent;

// ============================================================================
// FluidSkillEffect
// ============================================================================
// 플레이어의 SkillComponent 상태를 읽어 FluidParticleSystem의 제어점을 매 프레임
// 자동으로 업데이트한다.
//
// 제어점 개수: 장착된 총 룬 수 + 1 (최소 1, 최대 5)
// 위치        : 플레이어 중심으로 궤도 회전
// 속도        : 시전 상태에 따라 변화 (채널 = 빠름, 차지 = 느림)
// 색상        : Q 슬롯 스킬의 ElementType 으로 결정
// ============================================================================
class FluidSkillEffect
{
public:
    // pFluid : 구동할 파티클 시스템
    // pSkill : 플레이어의 SkillComponent
    void Init(FluidParticleSystem* pFluid, SkillComponent* pSkill);

    // 매 프레임 호출 — playerPos 는 플레이어 월드 위치
    void Update(float deltaTime, const XMFLOAT3& playerPos);

private:
    // 장착된 룬 총합에서 제어점 개수 산출 (1-5)
    int          ComputeCPCount()  const;
    // Q 슬롯 스킬의 ElementType 반환
    ElementType  ComputeElement()  const;

    // 현재 상태로 제어점 목록을 계산하고 시스템에 밀어넣는다
    void PushControlPoints(const XMFLOAT3& center);

    // ── 참조 ─────────────────────────────────────────────────────────────────
    FluidParticleSystem* m_pFluid = nullptr;
    SkillComponent*      m_pSkill = nullptr;

    // ── 애니메이션 상태 ───────────────────────────────────────────────────────
    float m_fOrbitAngle = 0.0f;   // 지속 증가하는 궤도 각도 (라디안)

    // ── 초기화 추적 ──────────────────────────────────────────────────────────
    bool        m_bInitialized = false;
    ElementType m_eLastElement = ElementType::None;
    int         m_nLastCPCount = -1;
};
