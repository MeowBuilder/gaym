#include "stdafx.h"
#include "FluidSkillVFXManager.h"
#include "DescriptorHeap.h"
#include <algorithm>
#include <cmath>

void FluidSkillVFXManager::Init(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList,
                                 CDescriptorHeap* pDescriptorHeap, UINT nStartDescIndex)
{
    for (int i = 0; i < MAX_EFFECTS; ++i)
    {
        m_Slots[i].pSystem = std::make_unique<FluidParticleSystem>();
        m_Slots[i].pSystem->Init(pDevice, pCommandList, pDescriptorHeap, nStartDescIndex + i);
    }
    OutputDebugStringA("[FluidSkillVFXManager] Initialized\n");
}

int FluidSkillVFXManager::SpawnEffect(const XMFLOAT3& origin, const XMFLOAT3& direction,
                                       const FluidSkillVFXDef& def)
{
    for (int i = 0; i < MAX_EFFECTS; ++i)
    {
        if (!m_Slots[i].isActive)
        {
            FluidVFXSlot& slot = m_Slots[i];
            slot.isActive    = true;
            slot.isFadingOut = false;
            slot.elapsed     = 0.0f;
            slot.origin      = origin;
            slot.prevOrigin  = origin;
            slot.direction   = direction;
            slot.def         = def;
            slot.useSequence = false; // 기존 모드

            FluidParticleConfig cfg;
            cfg.element           = def.element;
            cfg.particleCount     = def.particleCount;
            cfg.spawnRadius       = def.spawnRadius;
            cfg.smoothingRadius   = 1.2f;
            cfg.restDensity       = 7.0f;
            cfg.stiffness         = 50.0f;
            cfg.viscosity         = 0.25f;
            cfg.boundaryStiffness = 150.0f;
            cfg.particleSize      = 0.25f;

            slot.pSystem->Spawn(origin, cfg);
            PushControlPoints(slot);

            wchar_t buf[128];
            swprintf_s(buf, 128, L"[FluidSkillVFXManager] SpawnEffect slot %d\n", i);
            OutputDebugString(buf);
            return i;
        }
    }
    OutputDebugStringA("[FluidSkillVFXManager] No free slot!\n");
    return -1;
}

int FluidSkillVFXManager::SpawnSequenceEffect(const XMFLOAT3& origin, const XMFLOAT3& direction,
                                               const VFXSequenceDef& seqDef)
{
    for (int i = 0; i < MAX_EFFECTS; ++i)
    {
        if (!m_Slots[i].isActive)
        {
            FluidVFXSlot& slot = m_Slots[i];
            slot.isActive         = true;
            slot.isFadingOut      = false;
            slot.elapsed          = 0.0f;
            slot.origin           = origin;
            slot.prevOrigin       = origin;
            slot.direction        = direction;
            slot.useSequence      = true;
            slot.sequenceDef      = seqDef;
            slot.currentPhaseIndex = -1; // 아직 페이즈 시작 전 (UpdatePhase에서 0으로 전환)

            // 메테오용 마스터 CP 초기 위치: origin 위 높은 곳
            slot.masterCPPos      = { origin.x, origin.y + 50.f, origin.z };
            slot.masterCPFallSpeed = 15.f;

            // FluidParticleConfig 설정
            FluidParticleConfig cfg;
            cfg.element           = seqDef.element;
            cfg.particleCount     = seqDef.particleCount;
            cfg.spawnRadius       = seqDef.spawnRadius;
            cfg.smoothingRadius   = 1.2f;
            cfg.restDensity       = 7.0f;
            cfg.stiffness         = 50.0f;
            cfg.viscosity         = 0.25f;
            cfg.boundaryStiffness = 150.0f;
            cfg.particleSize      = 0.25f;

            // 첫 페이즈의 모드 설정
            if (!seqDef.phases.empty()) {
                slot.pSystem->SetMotionMode(seqDef.phases[0].motionMode);
            }

            slot.pSystem->Spawn(origin, cfg);

            wchar_t buf[128];
            swprintf_s(buf, 128, L"[FluidSkillVFXManager] SpawnSequenceEffect slot %d\n", i);
            OutputDebugString(buf);
            return i;
        }
    }
    OutputDebugStringA("[FluidSkillVFXManager] No free slot for sequence!\n");
    return -1;
}

void FluidSkillVFXManager::TrackEffect(int id, const XMFLOAT3& origin, const XMFLOAT3& direction)
{
    if (id < 0 || id >= MAX_EFFECTS || !m_Slots[id].isActive) return;
    m_Slots[id].origin    = origin;
    m_Slots[id].direction = direction;
}

void FluidSkillVFXManager::StopEffect(int id)
{
    if (id < 0 || id >= MAX_EFFECTS) return;
    m_Slots[id].isActive    = false;
    m_Slots[id].useSequence = false;
    m_Slots[id].pSystem->Clear();
}

void FluidSkillVFXManager::ImpactEffect(int id, const XMFLOAT3& impactPos)
{
    if (id < 0 || id >= MAX_EFFECTS || !m_Slots[id].isActive) return;
    auto& slot = m_Slots[id];

    // 단일 제어점을 충돌 위치에 배치 - 파티클이 충돌점으로 수렴
    FluidControlPoint cp;
    cp.position           = impactPos;
    cp.attractionStrength = 35.0f;   // 강한 인력으로 빠르게 집결
    cp.sphereRadius       = 0.25f;   // 매우 좁은 구체 -> 밀집
    slot.pSystem->SetControlPoints({ cp });

    // 시퀀스 모드 해제 (충돌 후에는 기존 fade-out 로직)
    slot.useSequence = false;
    slot.pSystem->SetMotionMode(ParticleMotionMode::ControlPoint);
    ConfinementBoxDesc emptyBox;
    emptyBox.active = false;
    slot.pSystem->SetConfinementBox(emptyBox);

    slot.isFadingOut = true;
    slot.fadeTimer   = 0.7f;  // 0.7초 후 소멸
}

void FluidSkillVFXManager::Update(float deltaTime)
{
    for (auto& slot : m_Slots)
    {
        if (!slot.isActive) continue;

        if (slot.isFadingOut) {
            // fade-out 중: 제어점은 이미 충돌 위치에 고정됨, SPH만 업데이트
            slot.pSystem->Update(deltaTime);
            slot.fadeTimer -= deltaTime;
            if (slot.fadeTimer <= 0.0f) {
                slot.isActive    = false;
                slot.isFadingOut = false;
                slot.useSequence = false;
                slot.pSystem->Clear();
            }
            continue;
        }

        // 시퀀스 모드: 페이즈 전환 + 박스 lerp 로직
        if (slot.useSequence) {
            slot.elapsed += deltaTime;
            UpdatePhase(slot, deltaTime);
            slot.pSystem->Update(deltaTime);
            continue;
        }

        // 기존 모드: 투사체 이동 델타만큼 파티클 전체를 같이 이동 (공동이동 프레임)
        XMFLOAT3 delta = {
            slot.origin.x - slot.prevOrigin.x,
            slot.origin.y - slot.prevOrigin.y,
            slot.origin.z - slot.prevOrigin.z
        };
        slot.prevOrigin = slot.origin;
        if (fabsf(delta.x) + fabsf(delta.y) + fabsf(delta.z) > 0.0001f)
            slot.pSystem->OffsetParticles(delta);

        slot.elapsed += deltaTime;
        PushControlPoints(slot);
        slot.pSystem->Update(deltaTime);
    }
}

void FluidSkillVFXManager::Render(ID3D12GraphicsCommandList* pCommandList,
                                   const XMFLOAT4X4& viewProj, const XMFLOAT3& camRight, const XMFLOAT3& camUp)
{
    for (auto& slot : m_Slots)
    {
        if (!slot.isActive || !slot.pSystem->IsActive()) continue;
        slot.pSystem->Render(pCommandList, viewProj, camRight, camUp);
    }
}

void FluidSkillVFXManager::PushControlPoints(FluidVFXSlot& slot) const
{
    if (slot.def.cpDescs.empty()) return;

    XMVECTOR fwd = XMVector3Normalize(XMLoadFloat3(&slot.direction));
    XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
    float dot = XMVectorGetX(XMVector3Dot(fwd, worldUp));
    XMVECTOR right = (fabsf(dot) > 0.99f)
        ? XMVectorSet(1, 0, 0, 0)
        : XMVector3Normalize(XMVector3Cross(worldUp, fwd));
    XMVECTOR up = XMVector3Cross(fwd, right);

    XMVECTOR originV = XMLoadFloat3(&slot.origin);

    std::vector<FluidControlPoint> cps;
    for (const auto& cpd : slot.def.cpDescs)
    {
        float angle = slot.elapsed * cpd.orbitSpeed + cpd.orbitPhase;
        XMVECTOR worldPos = originV
            + right * (cosf(angle) * cpd.orbitRadius)
            + up    * (sinf(angle) * cpd.orbitRadius)
            + fwd   * cpd.forwardBias;

        FluidControlPoint cp;
        XMStoreFloat3(&cp.position, worldPos);
        cp.attractionStrength = cpd.attractionStrength;
        cp.sphereRadius       = cpd.sphereRadius;
        cps.push_back(cp);
    }
    slot.pSystem->SetControlPoints(cps);
}

// ============================================================================
// 시퀀스 기반 페이즈 전환 로직
// ============================================================================
void FluidSkillVFXManager::UpdatePhase(FluidVFXSlot& slot, float dt)
{
    if (slot.sequenceDef.phases.empty()) return;

    // 현재 페이즈 찾기
    int targetPhase = 0;
    for (int i = 0; i < static_cast<int>(slot.sequenceDef.phases.size()); i++) {
        if (slot.elapsed >= slot.sequenceDef.phases[i].startTime) {
            targetPhase = i;
        }
    }

    // 페이즈 전환 발생
    if (targetPhase != slot.currentPhaseIndex) {
        slot.currentPhaseIndex = targetPhase;
        const auto& phase = slot.sequenceDef.phases[targetPhase];

        // 모드 전환
        slot.pSystem->SetMotionMode(phase.motionMode);
        slot.pSystem->SetConfinementBox(phase.boxDesc);

        if (phase.motionMode == ParticleMotionMode::Beam) {
            // Beam: 시작점/끝점 설정
            BeamDesc bd = phase.beamDesc;
            bd.startPos = slot.origin;
            XMVECTOR endV = XMVectorAdd(
                XMLoadFloat3(&slot.origin),
                XMVectorScale(XMLoadFloat3(&slot.direction), 20.f)
            );
            XMStoreFloat3(&bd.endPos, endV);
            slot.pSystem->SetBeamDesc(bd);
            slot.pSystem->InitBeamParticles();
        }

        if (phase.motionMode == ParticleMotionMode::Gravity) {
            // 메테오 폭발: 방사형 초기 velocity 부여
            slot.pSystem->SetGravityDesc(phase.gravityDesc);
            slot.pSystem->ApplyRadialBurst(slot.origin,
                phase.gravityDesc.initialSpeedMin,
                phase.gravityDesc.initialSpeedMax);
        }

        // ConfinementBox의 center를 현재 origin 기준으로 설정
        if (phase.boxDesc.active) {
            ConfinementBoxDesc bd = phase.boxDesc;
            XMVECTOR centerV = XMVectorAdd(
                XMLoadFloat3(&slot.origin),
                XMVectorScale(XMLoadFloat3(&slot.direction), 2.5f)
            );
            XMStoreFloat3(&bd.center, centerV);

            // 방향 기준으로 박스 축 설정
            XMVECTOR fwd = XMVector3Normalize(XMLoadFloat3(&slot.direction));
            XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
            float dotUp = XMVectorGetX(XMVector3Dot(fwd, worldUp));
            XMVECTOR rightV = (fabsf(dotUp) > 0.99f)
                ? XMVectorSet(1, 0, 0, 0)
                : XMVector3Normalize(XMVector3Cross(worldUp, fwd));
            XMVECTOR upV = XMVector3Cross(fwd, rightV);

            XMStoreFloat3(&bd.axisX, rightV);
            XMStoreFloat3(&bd.axisY, upV);
            XMStoreFloat3(&bd.axisZ, fwd);

            slot.pSystem->SetConfinementBox(bd);
        }
    }

    // ─── 매 프레임 업데이트 ───

    const auto& curPhase = slot.sequenceDef.phases[slot.currentPhaseIndex];

    // OrbitalCP 매 프레임 위성 CP 갱신
    if (curPhase.motionMode == ParticleMotionMode::OrbitalCP) {
        UpdateOrbitalCPs(slot, dt);
    }

    // Phase 내 진행률 계산 (박스 halfExtents lerp용)
    float phaseProgress = (slot.elapsed - curPhase.startTime) / curPhase.duration;
    phaseProgress = std::clamp(phaseProgress, 0.f, 1.f);

    // 박스 halfExtents lerp (이전 페이즈와 현재 페이즈 간)
    if (curPhase.boxDesc.active) {
        XMFLOAT3 prev = (slot.currentPhaseIndex > 0)
            ? slot.sequenceDef.phases[slot.currentPhaseIndex - 1].boxDesc.halfExtents
            : XMFLOAT3{ 0.5f, 0.5f, 0.5f };
        XMFLOAT3 curr = curPhase.boxDesc.halfExtents;

        ConfinementBoxDesc bd = curPhase.boxDesc;
        bd.halfExtents.x = prev.x + (curr.x - prev.x) * phaseProgress;
        bd.halfExtents.y = prev.y + (curr.y - prev.y) * phaseProgress;
        bd.halfExtents.z = prev.z + (curr.z - prev.z) * phaseProgress;

        // center: 박스가 앞으로 이동하는 경우 direction 기준으로 offset
        XMVECTOR centerV = XMVectorAdd(
            XMLoadFloat3(&slot.origin),
            XMVectorScale(XMLoadFloat3(&slot.direction), bd.halfExtents.z)
        );
        XMStoreFloat3(&bd.center, centerV);

        // 방향 기준으로 박스 축 설정
        XMVECTOR fwd = XMVector3Normalize(XMLoadFloat3(&slot.direction));
        XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
        float dotUp = XMVectorGetX(XMVector3Dot(fwd, worldUp));
        XMVECTOR rightV = (fabsf(dotUp) > 0.99f)
            ? XMVectorSet(1, 0, 0, 0)
            : XMVector3Normalize(XMVector3Cross(worldUp, fwd));
        XMVECTOR upV = XMVector3Cross(fwd, rightV);

        XMStoreFloat3(&bd.axisX, rightV);
        XMStoreFloat3(&bd.axisY, upV);
        XMStoreFloat3(&bd.axisZ, fwd);

        slot.pSystem->SetConfinementBox(bd);
    }
}

void FluidSkillVFXManager::UpdateOrbitalCPs(FluidVFXSlot& slot, float dt)
{
    // 마스터 CP: 낙하 처리
    slot.masterCPPos.y -= slot.masterCPFallSpeed * dt;

    std::vector<FluidControlPoint> cps;
    // 마스터 CP 추가
    FluidControlPoint masterCP;
    masterCP.position           = slot.masterCPPos;
    masterCP.attractionStrength = 25.f;
    masterCP.sphereRadius       = 5.f;
    cps.push_back(masterCP);

    // 위성 CP들 공전
    for (const auto& sat : slot.sequenceDef.satelliteCPs) {
        float angle = sat.orbitPhase + slot.elapsed * sat.orbitSpeed;
        FluidControlPoint satCP;
        satCP.position.x           = slot.masterCPPos.x + cosf(angle) * sat.orbitRadius;
        satCP.position.y           = slot.masterCPPos.y + sat.verticalOffset;
        satCP.position.z           = slot.masterCPPos.z + sinf(angle) * sat.orbitRadius;
        satCP.attractionStrength   = sat.attractionStrength;
        satCP.sphereRadius         = sat.sphereRadius;
        cps.push_back(satCP);
    }

    slot.pSystem->SetControlPoints(cps);
}

FluidSkillVFXDef FluidSkillVFXManager::GetVFXDef(ElementType element, const RuneCombo& combo, float chargeRatio)
{
    FluidSkillVFXDef def;
    def.element = element;
    def.particleCount = 80;
    def.spawnRadius   = 0.8f;

    switch (element)
    {
    case ElementType::Fire:
    {
        // 혜성 꼬리: CP 3개를 투사체 뒤쪽에 배치, 뒤로 갈수록 퍼짐
        for (int i = 0; i < 3; ++i)
        {
            FluidCPDesc cp;
            cp.orbitRadius        = 0.12f + i * 0.08f;      // 뒤로 갈수록 약간 넓어짐
            cp.orbitSpeed         = 4.5f;
            cp.orbitPhase         = (float)i * (2.0f * XM_PI / 3.0f);
            cp.forwardBias        = -0.35f - i * 0.75f;     // -0.35, -1.1, -1.85 (모두 뒤쪽)
            cp.attractionStrength = 22.0f - i * 2.5f;       // 뒤로 갈수록 약해짐
            cp.sphereRadius       = 0.75f + i * 0.3f;       // 뒤로 갈수록 넓게
            def.cpDescs.push_back(cp);
        }
        break;
    }
    case ElementType::Water:
    {
        // 3개 CP, 느린 삼각형 궤도 - 유선형 구체
        for (int i = 0; i < 3; ++i)
        {
            FluidCPDesc cp;
            cp.orbitRadius = 0.7f; cp.orbitSpeed = 2.5f;
            cp.orbitPhase  = (float)i * (2.0f * XM_PI / 3.0f);
            cp.forwardBias = 0.0f;
            cp.attractionStrength = 14.0f; cp.sphereRadius = 2.0f;
            def.cpDescs.push_back(cp);
        }
        break;
    }
    case ElementType::Wind:
    {
        // 2개 CP, 매우 빠른 궤도 - 와류형
        FluidCPDesc cp0;
        cp0.orbitRadius = 0.5f; cp0.orbitSpeed = 8.0f;
        cp0.orbitPhase = 0.0f;  cp0.forwardBias = 0.4f;
        cp0.attractionStrength = 12.0f; cp0.sphereRadius = 1.5f;

        FluidCPDesc cp1 = cp0;
        cp1.orbitPhase = XM_PI; cp1.forwardBias = -0.4f;

        def.cpDescs = { cp0, cp1 };
        break;
    }
    case ElementType::Earth:
    {
        // 1개 CP, 느린 궤도 - 밀집 구체
        FluidCPDesc cp0;
        cp0.orbitRadius = 0.3f; cp0.orbitSpeed = 1.5f;
        cp0.orbitPhase = 0.0f;  cp0.forwardBias = 0.2f;
        cp0.attractionStrength = 22.0f; cp0.sphereRadius = 1.6f;

        def.cpDescs = { cp0 };
        def.particleCount = 100;
        def.spawnRadius   = 0.6f;
        break;
    }
    default:
    {
        FluidCPDesc cp0;
        cp0.orbitRadius = 0.6f; cp0.orbitSpeed = 3.5f;
        cp0.orbitPhase = 0.0f;  cp0.forwardBias = 0.0f;
        cp0.attractionStrength = 15.0f; cp0.sphereRadius = 1.8f;
        def.cpDescs = { cp0 };
        break;
    }
    }

    // Shape override: Place > Channel > Charge (mutually exclusive)
    if (combo.hasPlace) {
        // Mine: single CP at center, no orbit -- dense ball
        def.cpDescs.clear();
        FluidCPDesc cp;
        cp.orbitRadius        = 0.0f;
        cp.orbitSpeed         = 0.0f;
        cp.orbitPhase         = 0.0f;
        cp.forwardBias        = 0.0f;
        cp.attractionStrength = 40.0f;
        cp.sphereRadius       = 0.9f;
        def.cpDescs           = { cp };
        def.particleCount     = 120;
        def.spawnRadius       = 1.2f;
    }
    else if (combo.hasChannel) {
        // Beam: 5 CPs along forward axis, no orbit
        def.cpDescs.clear();
        for (int i = 0; i < 5; ++i)
        {
            FluidCPDesc cp;
            cp.orbitRadius        = 0.05f;
            cp.orbitSpeed         = 0.0f;
            cp.orbitPhase         = 0.0f;
            cp.forwardBias        = -1.5f + i * 0.75f;
            cp.attractionStrength = 22.0f;
            cp.sphereRadius       = 0.8f;
            def.cpDescs.push_back(cp);
        }
        def.particleCount = (int)(def.particleCount * 1.2f);
        def.spawnRadius   = 0.5f;
    }
    else if (combo.hasCharge) {
        // Charge: orbit radius grows with charge ratio
        float cr = chargeRatio;
        for (auto& cpd : def.cpDescs) {
            cpd.orbitRadius        *= 0.5f + 1.5f * cr;
            cpd.orbitSpeed         *= 1.0f + 0.6f * cr;
            cpd.attractionStrength *= 1.2f + 0.4f * cr;
            cpd.sphereRadius       *= 0.8f + 0.5f * cr;
        }
        def.particleCount = (int)(def.particleCount * (1.0f + 0.5f * cr));
        def.spawnRadius   *= 0.7f + 0.5f * cr;
    }

    // Enhance: additional scale regardless of shape
    if (combo.hasEnhance) {
        for (auto& cpd : def.cpDescs) {
            cpd.orbitRadius        *= 1.4f;
            cpd.attractionStrength *= 1.2f;
            cpd.sphereRadius       *= 1.35f;
        }
        def.particleCount = (int)(def.particleCount * 1.5f);
    }

    if (def.particleCount > 128) def.particleCount = 128;

    return def;
}
