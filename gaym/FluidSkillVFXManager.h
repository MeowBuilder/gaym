#pragma once
#include "stdafx.h"
#include "FluidParticle.h"
#include "FluidParticleSystem.h"
#include "VFXTypes.h"
#include "VFXLibrary.h"
#include <array>
#include <memory>

class CDescriptorHeap;

// 한 슬롯 = 하나의 활성 투사체 유체 이펙트
struct FluidVFXSlot {
    std::unique_ptr<FluidParticleSystem> pSystem;
    bool             isActive    = false;
    bool             isFadingOut = false;   // 충돌 후 수렴 중
    float            fadeTimer   = 0.0f;    // 수렴 후 소멸까지 남은 시간
    float            elapsed     = 0.0f;
    XMFLOAT3         origin      = {0, 0, 0};
    XMFLOAT3         prevOrigin  = {0, 0, 0};  // 이전 프레임 origin (파티클 공동이동용)
    XMFLOAT3         direction   = {0, 0, 1};
    FluidSkillVFXDef def;

    // 시퀀스 기반 VFX 확장 멤버
    VFXSequenceDef   sequenceDef;           // 현재 재생 중인 시퀀스 정의
    int              currentPhaseIndex = 0; // 현재 페이즈 인덱스
    bool             useSequence = false;   // 시퀀스 모드 활성 여부
    float            masterCPFallY = 0.f;   // 메테오용 마스터 CP Y 위치 (낙하 추적)
    XMFLOAT3         masterCPPos = {};      // 메테오 마스터 CP 현재 위치
    float            masterCPFallSpeed = 15.f; // 낙하 속도 (units/s)
};

class FluidSkillVFXManager
{
public:
    static constexpr int MAX_EFFECTS = 32;

    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
              CDescriptorHeap* pDescriptorHeap, UINT nStartDescIndex);

    // 새 이펙트 생성, 슬롯 ID 반환 (-1: 실패)
    int SpawnEffect(const XMFLOAT3& origin, const XMFLOAT3& direction,
                    const FluidSkillVFXDef& def);

    // 시퀀스 기반 이펙트 생성 (VFXLibrary 연동)
    int SpawnSequenceEffect(const XMFLOAT3& origin, const XMFLOAT3& direction,
                            const VFXSequenceDef& seqDef);

    // 매 프레임 투사체 위치/방향 추적
    void TrackEffect(int id, const XMFLOAT3& origin, const XMFLOAT3& direction);

    // 투사체 소멸 시 이펙트 정지
    void StopEffect(int id);

    // 피격 시 파티클을 충돌 위치로 수렴시킨 뒤 소멸
    void ImpactEffect(int id, const XMFLOAT3& impactPos);

    void Update(float deltaTime);
    void Render(ID3D12GraphicsCommandList* pCommandList,
                const XMFLOAT4X4& viewProj, const XMFLOAT3& camRight, const XMFLOAT3& camUp);

    // 원소별 내장 VFX 정의 반환 (룬 combo에 따라 파라미터 조정)
    static FluidSkillVFXDef GetVFXDef(ElementType element, const RuneCombo& combo = {}, float chargeRatio = 0.0f);

private:
    void PushControlPoints(FluidVFXSlot& slot) const;

    // 시퀀스 기반 페이즈 전환 로직
    void UpdatePhase(FluidVFXSlot& slot, float dt);
    void UpdateOrbitalCPs(FluidVFXSlot& slot, float dt);

    std::array<FluidVFXSlot, MAX_EFFECTS> m_Slots;
};
