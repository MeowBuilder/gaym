#pragma once
#include "stdafx.h"
#include "FluidParticle.h"
#include "FluidParticleSystem.h"
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
    XMFLOAT3         direction   = {0, 0, 1};
    FluidSkillVFXDef def;
};

class FluidSkillVFXManager
{
public:
    static constexpr int MAX_EFFECTS = 8;

    void Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
              CDescriptorHeap* pDescriptorHeap, UINT nStartDescIndex);

    // 새 이펙트 생성, 슬롯 ID 반환 (-1: 실패)
    int SpawnEffect(const XMFLOAT3& origin, const XMFLOAT3& direction,
                    const FluidSkillVFXDef& def);

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
    static FluidSkillVFXDef GetVFXDef(ElementType element, const RuneCombo& combo = {});

private:
    void PushControlPoints(FluidVFXSlot& slot) const;

    std::array<FluidVFXSlot, MAX_EFFECTS> m_Slots;
};
